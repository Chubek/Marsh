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

Process *append_process_to_chain(Process **chain, bool is_async, Command *cmd,
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

void hook_redir(Redir *r) {
  if (r == NULL)
    return;

  switch (r->kind) {
  case REDIR_IN:
    stdin = freopen(get_string_asciiz(r->file_path), "r", stdin);
    if (stdin == NULL)
      perror("freopen");
    break;
  case REDIR_OUT:
    stdout = fropen(get_string_asciiz(r->file_path), "w", stdout);
    if (stdout == NULL)
      perror("freopen");
    break;
  case REDIR_APPEND:
    stdout = fropen(get_string_asciiz(r->file_path), "a", stdout);
    if (stdout == NULL)
      perror("freopen");
    break;
  case REDIR_HERE:
    int write =
        fwrite(r->here_str->buf, r->here_str->len, sizeof(uint8_t), stdin);
    if (write < 0)
      perror("fwrite");
    break;
  case REDIR_DUP_IN:
    int dup_res = dup2(STDIN_FILENO, r->dup_fd);
    if (dup_res < 0)
      perror("dup2");
    break;
  case REDIR_DUP_OUT:
    int dup_res = dup2(STDOUT_FILENO, r->dup_fd);
    if (dup_res < 0)
      perror("dup2");
    break;
  case REDIR_DUP_ERR:
    int dup_res = dup2(STDERR_FILENO, r->dup_fd);
    if (dup_res < 0)
      perror("dup2");
    break;
  default:
    /* unreachable */
    break;
  }
}

void execute_process(Process *p) {
  pid_t id;

  if ((p->pid = id = fork()) < 0) {
    perror("fork");
    return;
  }

  if (p->pgid == -1)
    p->pgid = getpgid(0);

  if (id == 0) {
    if (p->redirs != NULL)
      for (Redir *r = p->redirs; r != NULL; r = r->next)
        hook_redir(r);

    exec_command(p->cmd);
    perror("execvpe");
  }
}
