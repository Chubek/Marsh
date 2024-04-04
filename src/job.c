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

struct Process {
  pid_t pid;
  pid_t pgid;
  pstate_t state;
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
  pid_t job_pgid;
  jstate_t state;
  bool user_notified;
  struct termios tmodes;
  Process *first_p;
  Job *next_j;
  Arena *scratch;
};

struct Environ {
  pid_t session_id;
  int tty_fdesc;
  char *working_dir;
  char **env_vars;
  Job *fg_jobs;
  Job *bg_jobs;
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
  Process *p, *chain = NULL;
  for (p = job->first_p; p != NULL; p = p->next) {
    if (p->pgid == pgid) {
	// TODO: Implement
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


Job *get_job_chain_by_job_pgid(Environ *env, pid_t job_pgid, bool bg) {
  Job *j, *chain = NULL;
  for (j = bg ? env->first_bg_j : env->first_fg_j; j != NULL; j = j->next) {
    if (j->job_pgid == job_pgid) {
    	// TODO: Implement
    }
  }

  return chain;
}




Environ *init_environ(Environ *env, int tty_fdesc, char *working_dir,
                      char **env_vars) {
  env->session_id = getsid(0);
  env->tty_fdesc = tty_fdesc;
  env->working_dir = working_dir;
  env->env_vars = env_vars;

  env->fg_jobs = NULL;
  env->bg_jobs = NULL;

  env->scratch = arena_init(ENVIRON_INIT_ARENA_SIZE);

  return env;
}






















void execute_process(Process *process, int in_fd, int out_fd, int err_fd,
                     char **env_vars) {
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
    if (in_fd != STDIN_FILENO) {
      dup2(in_fd, STDIN_FILENO);
      close(in_fd);
    }
    if (out_fd != STDOUT_FILENO) {
      dup2(out_fd, STDOUT_FILENO);
      close(out_fd);
    }
    if (err_fd != STDERR_FILENO) {
      dup2(err_fd, STDERR_FILENO);
      close(err_fd);
    }

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
  int in_fd = STDIN_FILENO, err_fd = STDERR_FILENO;
  Process *process = job->first_process;

  while (process != NULL) {
    if (process->next_p != NULL) {
      if (pipe(pipe_fds) == -1)
        perror("pipe");
    } else {
      pipe_fds[0] = -1;
      pipe_fds[1] = (process->out_path == NULL && process->append_path == NULL)
                        ? STDOUT_FILENO
                        : -1;
    }

    if (process == job->first_process && process->in_path == NULL)
      in_fd = STDIN_FILENO;

    int out_fd = (process->next_p != NULL) ? pipe_fds[1] : STDOUT_FILENO;
    if (process->out_path != NULL || process->append_path != NULL)
      out_fd = -1;

    execute_process(process, in_fd, out_fd, err_fd, env_vars);

    if (in_fd != STDIN_FILENO)
      close(in_fd);

    in_fd = pipe_fds[0];

    if (out_fd != STDOUT_FILENO && out_fd != -1)
      close(pipe_fds[1]);

    process = process->next_p;
  }
}
