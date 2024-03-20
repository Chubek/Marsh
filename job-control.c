#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

typedef enum {
  PSTATE_PENDING,
  PSTATE_RUNNING,
  PSTATE_EXITED,
  PSTATE_TERMINATED,
  PSTATE_STOPPED,
  PSTATE_CONTINUED,
} pstate_t;

typedef struct Process {
  pid_t pid;
  char **argv;
  int argc;
  int status;
  int term_signal;
  int stop_signal;
  bool core_dumped;
  pstate_t state;
  struct Process *next;
} Process;

typedef struct Job {
  pid_t pgid;
  Process *first_process;
  struct termios tmodes;
  int stdin, stdout, stderr;
  struct Job *next;
} Job;

static struct termios shell_tmodes;
static Job *jobs = NULL;
static pid_t shell_pgid = -1;

void initialize_job_control(void);
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigstp_handler(int sig);
void sigcont_handler(int sig);
void setup_signal_handlers(void);
void launch_job(Job *job, int foreground);
void put_job_in_foreground(Job *job, int cont);
void put_job_in_background(Job *job, int cont);
void wait_for_job(Job *job);
bool job_is_stopped(Job *job);
bool job_is_completed(Job *job);
void update_status(void);
void mark_process_status(pid_t pid, int status);
Job *create_job(void);
Process *create_process(char **argv, int argc);
void append_process_to_job(Job *job, Process *process);
void append_job_to_list(Job *new_job);

void initialize_job_control(void) {
  pid_t pid = getpid();

  if (setpgid(pid, pid) < 0) {
    perror("Couldn't put the shell in its own process group");
    exit(1);
  }

  tcsetpgrp(STDIN_FILENO, pid);
  tcgetattr(STDIN_FILENO, &shell_tmodes);
  signal(SIGCHLD, sigchld_handler);
}

void sigchld_handler(int sig) {
  pid_t pid;
  int status;

  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
    mark_process_status(pid, status);
  }
}

void setup_signal_handlers(void) {
  struct sigaction sa;

  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  sa.sa_handler = sigstp_handler;
  if (sigaction(SIGTSTP, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }

  sa.sa_handler = sigchld_handler;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }
}

void launch_job(Job *job, int foreground) {
  pid_t pid;
  int joint_pipe[2], infile, outfile;
  Process *process;

  infile = job->stdin;
  for (process = job->first_process; process; process = process->next) {

    if (process->next) {
      if (pipe(joint_pipe) < 0) {
        perror("pipe");
        exit(1);
      }
      outfile = joint_pipe[1];
    } else {
      outfile = job->stdout;
    }

    pid = fork();
    if (pid == 0) {
      if (infile != STDIN_FILENO) {
        dup2(infile, STDIN_FILENO);
        close(infile);
      }
      if (outfile != STDOUT_FILENO) {
        dup2(outfile, STDOUT_FILENO);
        close(outfile);
      }
      execvp(process->argv[0], process->argv);
      perror("execvp");
      exit(EXIT_FAILURE);
    } else if (pid < 0) {

      perror("fork");
      exit(EXIT_FAILURE);
    } else {
      process->pid = pid;
      if (!job->pgid) {
        job->pgid = pid;
      }
      setpgid(pid, job->pgid);

      if (infile != STDIN_FILENO) {
        close(infile);
      }
      if (outfile != STDOUT_FILENO) {
        close(outfile);
      }
      infile = joint_pipe[0];
    }
  }

  if (foreground) {
    put_job_in_foreground(job, 0);
  } else {
    put_job_in_background(job, 0);
  }
}

void put_job_in_foreground(Job *job, int cont) {
  tcsetpgrp(STDIN_FILENO, job->pgid);

  if (cont) {

    if (tcsetattr(STDIN_FILENO, TCSADRAIN, &job->tmodes) == -1) {
      perror("tcsetattr");
    }
    if (kill(-job->pgid, SIGCONT) < 0) {
      perror("kill (SIGCONT)");
    }
  }

  wait_for_job(job);

  if (tcsetpgrp(STDIN_FILENO, shell_pgid) == -1) {
    perror("tcsetpgrp");
  }

  if (tcgetattr(STDIN_FILENO, &job->tmodes) == -1) {
    perror("tcgetattr");
  }

  if (tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_tmodes) == -1) {
    perror("tcsetattr");
  }
}

void put_job_in_background(Job *job, int cont) {
  if (cont) {
    if (kill(-job->pgid, SIGCONT) < 0) {
      perror("kill (SIGCONT)");
    }
  }
}

void wait_for_job(Job *job) {
  int status;
  pid_t pid;

  do {
    pid = waitpid(WAIT_ANY, &status, WUNTRACED);
    if (pid > 0) {
      mark_process_status(pid, status);
    }
  } while (!job_is_stopped(job) && !job_is_completed(job));
}

void update_status(void) {
  int status;
  pid_t pid;

  do {
    pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
    if (pid > 0) {
      mark_process_status(pid, status);
    }
  } while (pid > 0);
}

void mark_process_status(pid_t pid, int status) {
  Job *j;
  Process *p;

  for (j = jobs; j; j = j->next) {
    for (p = j->first_process; p; p = p->next) {
      if (p->pid == pid) {
        p->status = status;
        if (WIFSTOPPED(status)) {
          p->state = PSTATE_STOPPED;
        } else if (WIFSIGNALED(status)) {
          p->state = PSTATE_TERMINATED;
          p->term_signal = WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
          p->state = PSTATE_STOPPED;
          p->stop_signal = WSTOPSIG(status);
        } else if (WIFCONTINUED(status)) {
          p->state = PSTATE_CONTINUED;
        }

#ifdef WCOREDUMP
        if (WCOREDUMP(status))
          p->core_dumped = true;
#endif

        return;
      }
    }
  }
}

bool job_is_stopped(Job *job) {
  Process *p;

  if (job->first_process == NULL) {
    return false;
  }

  for (p = job->first_process; p; p = p->next) {
    if (p->state != PSTATE_STOPPED) {
      return false;
    }
  }

  return true;
}

bool job_is_completed(Job *job) {
  Process *p;

  if (job->first_process == NULL) {
    return true;
  }

  for (p = job->first_process; p; p = p->next) {
    if (p->state != PSTATE_EXITED && p->state != PSTATE_TERMINATED) {
      return false;
    }
  }

  return true;
}

Job *create_job(void) {
  Job *job = malloc(sizeof(Job));

  if (job == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  job->pgid = 0;
  job->first_process = NULL;
  job->next = NULL;
  tcgetattr(STDIN_FILENO, &job->tmodes);

  job->stdin = STDIN_FILENO;
  job->stdout = STDOUT_FILENO;
  job->stderr = STDERR_FILENO;

  return job;
}

Process *create_process(char **argv, int argc) {
  Process *process = malloc(sizeof(Process));
  if (process == NULL) {
    perror("malloc");
    exit(EXIT_FAILURE);
  }

  process->argv = argv;
  process->argc = argc;
  process->pid = 0;
  process->state = PSTATE_PENDING;
  process->term_signal = -1;
  process->stop_signal = -1;
  process->core_dumped = false;
  process->next = NULL;

  return process;
}

void append_process_to_job(Job *job, Process *process) {
  Process *current_process = job->first_process;

  if (current_process == NULL) {
    job->first_process = process;
  } else {
    while (current_process->next != NULL) {
      current_process = current_process->next;
    }
    current_process->next = process;
  }
}

void append_job_to_list(Job *new_job) {
  if (jobs == NULL) {
    jobs = new_job;
  } else {
    Job *current_job = jobs;
    while (current_job->next != NULL) {
      current_job = current_job->next;
    }
    current_job->next = new_job;
  }
}
