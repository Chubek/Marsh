#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "jobexec.h"
#include "utils.h"

typedef enum {
  PSTATE_PENDING,
  PSTATE_RUNNING,
  PSTATE_COMPLETED,
  PSTATE_ABORTED,
  PSTATE_STOPPED,
  PSTATE_TERMINATED,
  PSTATE_EXITED,
  PSTATE_UNKNOWN,
} pstate_t;

typedef enum {
  JSTATE_PENDING,
  JSTATE_FOREGROUND,
  JSTATE_BACKGROUND,
  JSTATE_STOPPED,
} jobstate_t;

typedef enum SignalFlags {
  SIGFLAG_IGNORE_INT = 1 << 0,
  SIGFLAG_IGNORE_QUIT = 1 << 1,
  SIGFLAG_NOHUP = 1 << 2,
  SIGFLAG_CUSTOM_CHLD = 1 << 3,
  SIGFLAG_TSTP_TO_BG = 1 << 4,
  SIGFLAG_INT_TO_TERM = 1 << 5,
  SIGFLAG_QUIT_TO_TERM = 1 << 6,
  SIGFLAG_CLEANUP_ZOMBIES = 1 << 7,
  SIGFLAG_RESTORE_TTY = 1 << 8,
  SIGFLAG_HANDLE_WINCH = 1 << 9,
  SIGFLAG_EXIT_ON_TERM_CLOSE = 1 << 10,
} sigflags_t;

typedef enum IOFlags {
  IO_READ = 1 << 1,
  IO_WRITE = 1 << 2,
  IO_APPEND = 1 << 3,
  IO_STR = 1 << 4,
} ioflags_t;

struct Process {
  pid_t pid;
  pid_t pgid;
  pstate_t state;
  ioflags_t ioflags;
  char *in_path;
  char *out_path;
  char *append_path;
  char *cmd_path;
  char **argv;
  size_t argc;
  bool is_async;
  Process *next_p;
  Arena *scratch;
};

struct Job {
  int job_id;
  pid_t jpgid;
  jstate_t state;
  bool user_notified;
  struct termios tmodes;
  Process *first_p;
  int fno_in, fno_out, fno_err;
  Job *next_j;
  Arena *scratch;
};

struct Environ {
  pid_t session_id;
  int tty_fdesc;
  char *working_dir;
  char **env_vars;
  int last_bg_job_id;
  sigflags_t sigflags;
  Job *first_j_fg;
  Job *first_j_bg;
  Arena *scratch;
};

Process *get_process_by_pid(Job *job, pid_t pid) {
  Process *p;
  for (p = job->first_p; p != NULL; p = p->next)
    if (p->pid == pid)
      return p;

  return NULL;
}

Process *get_process_chain_by_pgid(Job *job, pid_t pgid) {
  Process *p, *dup, *chain = NULL;
  for (p = job->first_p; p != NULL; p = p->next) {
    if (p->pgid == pgid) {
      dup = (Process *)arena_alloc(job->scratch, sizeof(Process));
      memmove(dup, p, sizeof(Process));

      dup->next = chain;
      chain = dup;
    }
  }

  return chain;
}

Job *get_job_by_job_id(Environ *env, int job_id, bool bg) {
  Job *j;
  for (j = bg ? env->first_bg_j : env->first_fg_j; j != NULL; j = j->next)
    if (j->id == id)
      return j;

  return NULL;
}

Job *get_job_chain_by_jpgid(Environ *env, pid_t jpgid, bool bg) {
  Job *j, *dup, *chain = NULL;
  for (j = bg ? env->first_bg_j : env->first_fg_j; j != NULL; j = j->next) {
    if (j->jpgid == jpgid) {
      dup = (Job *)arena_alloc(env->scratch, sizeof(Job));
      memmove(dup, j, sizeof(Job));

      dup->next = chain;
      chain = dup;
    }
  }

  return chain;
}

Process *push_blank_process_to_job(Job *job, ioflags_t io_flags, char *io_word,
                                   int io_num, char *cmd_path, char **argv,
                                   size_t argc, bool is_async) {
  Process *p = (Process *)arena_alloc(job->scratch, sizeof(Process)), *pprime;
  p->scratch = arena_init(PROCESS_INIT_ARENA_SIZE);

  if (job->first_p == NULL)
    job->first_p = p;
  else {
    for (pprime = job->first_p; pprime->next != NULL; pprime = pprime->next)
      ;
    p->next_p = NULL;
    pprime->next_p = p;
  }

  p->io_flags = io_flags;
  p->io_word = duplicate_string(p->scratch, io_word);
  p->io_num = io_num;
  p->cmd_path = duplicate_string(p->scratch, cmd_path);
  p->argv = duplicate_strings(p->scratch, argv, argc);
  p->argc = argc;
  p->is_async = is_async;

  p->state = PSTATE_PENDING;
  p->pid = -1;
  p->pgid = job->jpgid;

  p->exit_stat = -1;
  p->stop_stat = -1;
  p->exit_stat = -1;

  p->fno_in = STDIN_FILENO;
  p->fno_out = STDOUT_FILENO;
  p->fno_err = STDERR_FILENO;

  return p;
}

Job *push_blank_job_to_environ(Environ *env, int job_id) {
  Job *j = (Job *)arena_alloc(env->scratch, sizeof(Job)), *jprime;
  j->scratch = arena_init(JOB_INIT_ARENA_SIZE);

  if (env->first_j == NULL)
    env->first_j = j;
  else {
    for (jprime = env->first_j; jprime->next != NULL; jprime = jprime->next)
      ;
    j->next_j = NULL;
    jprime->next_j = j;
  }

  j->job_id = job_id;
  j->jpgid = -1;

  j->state = JSTATE_PENDING;
  j->user_notified = false;

  j->first_p = NULL;

  if (tcgetattr(env->tty_fdesc, &j->tmodes) == -1) {
    fprintf("Error: Could not get terminal attributes");
    exit(EXIT_FAILURE);
  }

  return j;
}

Environ *init_environ(Environ *env, int tty_fdesc, char *working_dir,
                      char **env_vars, sigflags_t sigflags) {
  env->session_id = getsid(0);
  env->tty_fdesc = tty_fdesc;
  env->working_dir = working_dir;
  env->env_vars = env_vars;

  env->last_bg_job_id = -1;
  env->sigflags = sigflags;

  env->first_j_fg = NULL;
  env->first_j_bg = NULL;

  env->scratch = arena_init(ENVIRON_INIT_ARENA_SIZE);

  return env;
}

void execute_process(Process *process, char **env_vars) {
  int in_fd = STDIN_FILENO, out_fd = STDOUT_FILENO, err_fd = STDERR_FILENO;

  if (process->in_path != NULL) {
    in_fd = open(process->in_path, O_RDONLY);
    if (in_fd == -1) {
      perror("Failed to open input file for redirection");
    }
  }

  if (process->out_path != NULL) {
    out_fd = open(process->out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) {
      perror("Failed to open output file for redirection");
    }
  }

  if (process->append_path != NULL) {
    out_fd = open(process->append_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (out_fd == -1) {
      perror("Failed to open file for appending");
    }
  }

  pid_t pid = fork();
  if (pid == 0) {
    if (in_fd != STDIN_FILENO)
      dup2(in_fd, STDIN_FILENO);
    if (out_fd != STDOUT_FILENO)
      dup2(out_fd, STDOUT_FILENO);
    if (err_fd != STDERR_FILENO)
      dup2(err_fd, STDERR_FILENO);

    if (in_fd != STDIN_FILENO)
      close(in_fd);
    if (out_fd != STDOUT_FILENO)
      close(out_fd);
    if (err_fd != STDERR_FILENO)
      close(err_fd);

    execvpe(process->cmd_path, process->argv, env_vars);
    perror("execvpe");
  } else if (pid < 0) {
    perror("fork");
  }

  if (in_fd != STDIN_FILENO)
    close(in_fd);
  if (out_fd != STDOUT_FILENO)
    close(out_fd);
  if (err_fd != STDERR_FILENO)
    close(err_fd);
}

void wait_on_process(Process *process, bool force) {
  if (process->is_async && !force)
    return;

  int waited = -1;
  do {
    waited = waitpid(process->pid, &process->exit_stat,
                     WEXITED | WCONTINUED | WSTOPPED | WNOHAND);
  } while (waited != process->pid);

  set_process_state(process);
}

void set_process_state(Process *process) {
  if (WIFEXITED(process->exit_stat)) {
    process->state = PSTATE_EXITED;
    process->exit_stat = WEXITSTAT(process->exit_stat);
  } else if (WIFSIGTERM(process->exit_stat)) {
    process->state = PSTATE_TERMINATED;
    process->term_stat = WTERMSIG(process->exit_stat);
  } else if (WIFSTOPPED(process->exit_stat)) {
    process->state = PSTATE_STOPPED;
    process->stop_stat = WSTOPSIG(process->exit_stat);
  } else
    process->state = PSTATE_UNKNOWN;
}

void execute_job(Job *job, char **env_vars) {
  int pipe_fds[2];
  int in_fd = STDIN_FILENO;
  Process *process = job->first_process;

  while (process != NULL) {
    if (process->next_p != NULL) {
      if (pipe(pipe_fds) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
      }
    } else {
      pipe_fds[0] = -1;
      pipe_fds[1] = (process->out_path == NULL && process->append_path == NULL)
                        ? STDOUT_FILENO
                        : -1;
    }

    if (process == job->first_process && process->in_path == NULL) {
      in_fd = STDIN_FILENO;
    }

    int out_fd = (process->next_p != NULL) ? pipe_fds[1] : STDOUT_FILENO;
    if (process->out_path != NULL || process->append_path != NULL) {

      out_fd = -1;
    }

    execute_process(process, env_vars);
    if (in_fd != STDIN_FILENO) {
      close(in_fd);
    }
    in_fd = pipe_fds[0];

    if (out_fd != STDOUT_FILENO && out_fd != -1) {
      close(pipe_fds[1]);
    }

    process = process->next_p;
  }
}
