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

struct Process {
  pid_t pid;
  pid_t pgid;
  ProcessStatus state;
  ProcessIO *io;
  Command *cmd;
  bool is_async;
  Process *next_p;
  Arena *scratch;
};

struct Job {
  int job_id;
  pid_t job_pgid;
  JobStatus state;
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

Process *append_process_to_chain(Process **chain, ProcessIO *io, Command *cmd,
                                 bool is_async, Arena *scratch) {
  Process *p = (Process *)arena_alloc(scratch, sizeof(Process));

  if (p == NULL) {
    p = (Process *)cbu_alloc(sizeof(Process));
    if (p == NULL)
      return NULL;
  }

  p->scratch = arena_init(ARENA_INIT_SIZE_PROCESS);

  p->cmd = cmd;
  p->io = io;
  p->is_async = is_async;
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
