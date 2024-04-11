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

#include "marsh.h"

#define NEW_FILE_STAT S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH

struct Command {
  String *cmd;
  String *args_chain;
  size_t args_num;
};

struct Redir {
  enum RedirKind {
    REDIR_IN,
    REDIR_OUT,
    REDIR_APPEND,
    REDIR_HERE,
    REDIR_DUP,
  } kind;

  String *file_path;
  String *here;
  int target_fd;
  int dup_fd;

  Redir *next;
};

struct Process {
  pid_t pid;
  pid_t pgid;
  Redir *redirs;
  FILE *redirect_stream;
  Command *cmd;
  int fno_in, fno_out, fno_err;
  bool is_async;
  Process *next_p;
  Arena *scratch;

  enum ProcessState {
    PSTATE_PENDING,
    PSTATE_RUNNING,
    PSTATE_TERM,
    PSTATE_STOP,
    PSTATE_EXIT,
  } state;

  int state_code;
};

struct Job {
  int job_id;
  pid_t job_pgid;
  Status state;
  Process *first_p;
  Job *next_j;
  Arena *scratch;
};

struct Environ {
  pid_t session_id;
  int tty_fdesc;
  String working_dir;
  String *env_vars;
  Job *jobs;
  Job *suspended_jobs;
  Arena *scratch;
};

Process *get_process_by_pid(Process *chain, pid_t pid) {
  for (Process *p = job->first_p; p != NULL; p = p->next)
    if (p->pid == pid)
      return p;

  return NULL;
}

Job *get_job_by_job_id(Job *chain, int job_id) {
  for (Job *j = chain; j != NULL; j = j->next)
    if (j->id == id)
      return j;

  return NULL;
}

Environ *init_environ(Environ *env, int tty_fdesc, String working_dir,
                      String *env_vars) {
  env->session_id = getsid(0);
  env->tty_fdesc = tty_fdesc;
  env->working_dir = working_dir;
  env->env_vars = env_vars;

  env->jobs = NULL;
  env->suspended_jobs = NULL;

  env->scratch = arena_init(ARENA_INIT_SIZE_ENVIRON);

  return env;
}

Process *append_process_to_chain(Process **chain, bool is_async, Redir *redirs,
                                 Command *cmd, Arena *scratch) {
  Process *p = (Process *)arena_alloc(scratch, sizeof(Process));

  if (p == NULL) {
    p = (Process *)backup_alloc(sizeof(Process));
    if (p == NULL)
      return NULL;
  }

  p->scratch = scratch;

  p->pid = -1;
  p->pgid = -1;
  p->cmd = cmd;
  p->redirs = redirs;
  p->redirect_stream = NULL;
  p->is_async = is_async;
  p->last_in_line = last_in_line;
  p->fno_in = STDIN_FILENO;
  p->fno_out = STDOUT_FILENO;
  p->fno_err = STDERR_FILENO;
  p->state_code = -1;
  p->state = PSTATE_PENDING;
  p->next_p = NULL;

  if (*chain == NULL) {
    *chain = p;
    return p;
  }

  for (Process *chain_deref = *chain; chain_deref->next != NULL;
       chain_deref = chain_deref->next)
    ;
  chain_deref->next = p;

  return p;
}

Job *append_job_to_chain(Job **chain, int job_id, Arena *scratch) {
  Job *j = (Job *)arena_alloc(scratch, sizeof(Job));

  if (j == NULL) {
    j = (Job *)backup_alloc(sizeof(Job));
    if (j == NULL)
      return NULL;
  }

  j->scratch = arena_init(ARENA_INIT_SIZE_JOB);

  j->job_id = job_id;
  j->first_p = NULL;
  j->next_j = NULL;
  if (*chain == NULL) {
    *chain = j;
    return j;
  }

  for (Job *chain_deref = *chain; chain_deref->next != NULL;
       chain_deref = chain_deref->next)
    ;
  chain_deref->next = j;

  return j;
}

void hook_redir(Redir *r, Process *p) {
  if (r == NULL || p == NULL)
    return;

  int fd; // File descriptor for the opened file
  switch (r->kind) {
  case REDIR_IN:
    fd = open(get_string_asciiz(r->file_path, p->scratch), O_RDONLY);
    if (fd < 0) {
      perror("open");
      exit(EXIT_FAILURE);
    }
    if (dup2(fd, p->fno_in) < 0) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }
    close(fd);
    break;
  case REDIR_OUT:
    fd = open(get_string_asciiz(r->file_path, p->scratch),
              O_WRONLY | O_CREAT | O_TRUNC, NEW_FILE_STAT);
    if (fd < 0) {
      perror("open");
      exit(EXIT_FAILURE);
    }
    if (dup2(fd, p->fno_out) < 0) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }
    close(fd);
    break;
  case REDIR_APPEND:
    fd = open(get_string_asciiz(r->file_path, p->scratch),
              O_WRONLY | O_CREAT | O_APPEND, NEW_FILE_STAT);
    if (fd < 0) {
      perror("open");
      exit(EXIT_FAILURE);
    }
    if (dup2(fd, p->fno_out) < 0) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }
    close(fd);
    break;
  case REDIR_DUP:
    if (dup2(r->dup_fd, r->target_fd) < 0) {
      perror("dup2");
      exit(EXIT_FAILURE);
    }
    break;
  case REDIR_HERE:
    int here_pipe[2];
    if (pipe(here_pipe) < 0)
      system_error("pipe");
    if (write(here_pipe[1], r->here->buf, r->here->len) < 0)
      system_error("write");
    if (dup2(fd, p->fno_in) < 0)
      system_error("dup2");
    close(here_pipe[0]);
    close(here_pipe[1]);
    break;
  default:
    fprintf(stderr, "Unknown redirection kind.\n");
    exit(EXIT_FAILURE);
  }
}

void handle_pipe(Process *p) {
  if (p->fno_in != STDIN_FILENO) {
    if (dup2(p->fno_in, STDIN_FILENO) < 0)
      system_error("dup2");
    close(p->fno_in);
  }

  if (p->fno_out != STDOUT_FILENO) {
    if (dup2(p->fno_out, STDOUT_FILENO) < 0)
      system_error("dup2");
    close(p->fno_out);
  }
}

void execute_command(Command *cmd, Process *p) {
  char *path = string_to_asciiz(cmd->cmd, p->scratch);
  char **args =
      strings_to_nullterm_asciiz(cmd->args_chain, cmd->num_args, p->scratch);

  resolve_path(path, shell_env);
  execle(path, args, shell_env);
}

void execute_redirs(Process *p) {
  if (p->redirs != NULL)
    for (Redir *r = p->redirs; r != NULL; r = r->next)
      hook_redir(r, p);
}

void execute_process(Process *p) {
  pid_t id;

  if ((p->pid = id = fork()) < 0) {
    system_error("fork");
    return;
  }

  if (p->pgid == -1)
    p->pgid = getpgid(0);

  if (id == 0) {
    setpgid(p->pgid);

    handle_pipe(p);

    execute_redirs();

    execute_command(p->cmd);
    system_error("execle");
  }

  if (p->fno_in != STDIN_FILENO)
    close(p->fno_in);

  if (p->fno_out != STDOUT_FILENO)
    close(p->fno_out);
}

void wait_for_process(Process *p) {
  // wait for process, and stet p->state and p->state_code
}

void execute_job(Job *j) {
  int pipe_chain[2];
  int prev_in = -1;

  for (Process *p = j->first_p; p != NULL; p = p->next_p) {
    if (p != j->first_p && j->next_p != NULL) {
      if (p->next_p != NULL) {
        if (pipe(pipe_chain) < 0)
          system_error("pipe");
        if (dup2(pipe_chain[0], prev_in) < 0)
          system_error("dup2");
        close(pipe_chain[0]);
      }

      if (p != j->first_p && prev_in != -1)
        p->fno_in = prev_in;
      else
        p->fno_in = STDIN_FILENO;

      if (p->next_p != NULL) {
        p->fno_out = prev_in = dup(pipe_chain[1]);
        close(pipe_chain[1]);
      } else
        p->fno_out = STDOUT_FILENO;

      if (j->job_pgid != -1)
        p->pgid = j->job_pgdid;

      execute_process(p);

      if (p == j->first_p && p->pgid != -1)
        j->job_pgid = p->pgid;

      wait_for_process(p)
    }
  }

  void disable_raw_mode(struct termios tmode) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &tmode);
  }

  void enable_raw_mode(struct termios tmode) {
    struct termios raw;

    tcgetattr(STDIN_FILENO, &tmode);
    atexit(disableRawMode);

    raw = tmode;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  }

  void sigint_handler(int sig) {
    // CTRL+D
  }

  void sigtstp_handler(int sig) {
    // CTRL+Z
  }

  void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0)
      ;
  }
