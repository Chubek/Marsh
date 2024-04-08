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

//=> alloc cbu_heap, cbu_alloc, cbu_realloc, cbu_dump
//=> hashfunc cbu_heap_hashfn

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
    REDIR_DUP_IN,
    REDIR_DUP_OUT,
    REDIR_DUP_ERR,
  } kind;

  String *file_path;
  String *here;
  int target_fd;
  int dup_fd;

  Redir *next;
}

struct Process {
  pid_t pid;
  pid_t pgid;
  Redir *redirs;
  Command *cmd;
  int fno_in, fno_out, fno_err;
  bool is_async;
  bool last_in_line;
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
  struct termios tmodes;
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

Process *append_process_to_chain(Process **chain, bool is_async,
                                 bool last_in_line, Command *cmd,
                                 Arena *scratch) {
  Process *p = (Process *)arena_alloc(scratch, sizeof(Process));

  if (p == NULL) {
    p = (Process *)cbu_alloc(sizeof(Process));
    if (p == NULL)
      return NULL;
  }

  p->scratch = arena_init(ARENA_INIT_SIZE_PROCESS);

  p->pid = -1;
  p->pgid = -1;
  p->cmd = cmd;
  p->is_async = is_async;
  p->last_in_line = last_in_line;
  p->fno_in = STDIN_FILENO;
  p->fno_out = STDOUT_FILENO;
  p->fno_err = STDERR_FILENO;
  p->state_code = -1;
  p->state = PSTATE_PENDING;
  p->next_p = NULL;
  p->redirs = NULL;

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
    j = (Job *)cbu_alloc(sizeof(Job));
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
  if (r == NULL)
    return;

  switch (r->kind) {
  case REDIR_IN:
    FILE *redirect =
        freopen(get_string_asciiz(r->file_path, p->scratch), "r", stdin);
    if (redirect == NULL)
      system_error("freopen");
    if (r->target_fd == -1) {
      stdin = redirect;
      p->fno_in = fileno(stdin);
    } else
      dup2(fileno(redirect), r->target_fd);
    break;
  case REDIR_OUT:
    FILE *redirect =
        fropen(get_string_asciiz(r->file_path, p->scratch), "w", stdout);
    if (redirect == NULL)
      system_error("freopen");
    if (r->target_fd == -1) {
      stdout = redirect;
      p->fno_out = fileno(stdout);
    } else
      dup2(fileno(redirect), r->target_fd);
    break;
  case REDIR_APPEND:
    FILE *redirect =
        fropen(get_string_asciiz(r->file_path, p->scratch), "a", stdout);
    if (redirect == NULL)
      system_error("freopen");
    if (r->target_fd == -1) {
      stdout = redirect;
      p->fno_out = fileno(stdout);
    } else
      dup2(fileno(redirect), r->target_fd);
    break;
    p->fno_out = fileno(stdout);
  case REDIR_HERE:
    int write =
        fwrite(r->here_str->buf, r->here_str->len, sizeof(uint8_t), stdin);
    if (write < 0)
      system_error("fwrite");
    break;
  case REDIR_DUP_IN:
    int dup_res = dup2(p->fno_in, r->dup_fd);
    if (dup_res < 0)
      system_error("dup2");
    break;
  case REDIR_DUP_OUT:
    int dup_res = dup2(p->fno_out, r->dup_fd);
    if (dup_res < 0)
      system_error("dup2");
    break;
  case REDIR_DUP_ERR:
    int dup_res = dup2(p->fno_err, r->dup_fd);
    if (dup_res < 0)
      system_error("dup2");
    break;
  case REDIR_DUP_TARGET:
    int dup_res = dup2(p->fno_target, r->dup_fd);
    if (dup_res < 0)
      system_error("dup2");
    break;
  default:
    /* unreachable */
    break;
  }
}

void handle_pipe(Process *p) {
  if (p->fno_in != STDIN_FILENO) {
    if (dup2(p->fno_in, STDIN_FILENO) < 0) {
      system_error("dup2");
    }
    close(p->fno_in);
  }

  if (p->fno_out != STDOUT_FILENO) {
    if (dup2(p->fno_out, STDOUT_FILENO) < 0) {
      system_error("dup2");
    }

    close(p->fno_out);
  }
}

void execute_command(Command *cmd, Process *p) {
  char *path = string_to_asciiz(cmd->cmd, p->scratch);
  char **args =
      strings_to_nullterm_asciiz(cmd->args_chain, cmd->num_args, p->scratch);

  execvpe(path, args, shell_env);
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
    handle_pipe(p);

    if (p->redirs != NULL)
      for (Redir *r = p->redirs; r != NULL; r = r->next)
        hook_redir(r, p);

    execute_command(p->cmd);
    system_error("execvpe");
  }

  if (p->fno_in != STDIN_FILENO)
    close(p->fno_in);

  if (p->fno_out != STDOUT_FILENO)
    close(p->fno_out);
}

void execute_job(Job *j) {
  int pipe_chain[2];

  for (Process *p = j->first_p; p != NULL; p = p->next_p) {
    if (p != j->first_p && !j->last_in_line) {
      if (pipe(pipe_chain) < 0)
        system_error("pipe");

      if (p != j->first_p) {
        p->fno_in = dup(pipe_chain[0]);
        close(pipe_chain[0]);
      }

      if (!p->last_in_line) {
        p->fno_out = dup(pipe_chain[1]);
        close(pipe_chain[1]);
      }
    }

    execute_process(p);
    wait_for_process(p);
  }
}
