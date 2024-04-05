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

#include "control.h"
#include "io.h"
#include "memory.h"
#include "notify.h"

//=> alloc ctrl_backup_heap, ctrl_backup_alloc, ctrl_backup_realloc,
//ctrl_backup_dump
//=> hashfunc ctrl_backup_heap_hashfn

struct Process {
  pid_t pid;
  pid_t pgid;
  ProcessStatus state;
  ProcessIO *io;
  String *cmd_path;
  String *arguments;
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
  Job *fg_jobs;
  Job *bg_jobs;
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

  env->fg_jobs = NULL;
  env->bg_jobs = NULL;

  env->scratch = arena_init(ARENA_INIT_SIZE_ENVIRON);

  return env;
}

Process *append_process_to_chain(Process **chain, ProcessIO *io,
                                 String *cmd_path, String *arguments,
                                 bool is_async, Arena *scratch) {
  Process *p = (Process *)arena_alloc(scratch, sizeof(Process));

  if (p == NULL) {
    p = (Process *)ctrl_backup_alloc(sizeof(Process));
    if (p == NULL)
      return NULL;
  }

  p->scratch = arena_init(ARENA_INIT_SIZE_PROCESS);

  p->cmd_path = duplicate_string(cmd_path, p->scratch);
  p->arguments = duplicate_string_list(arguments, p->scratch);

  p->in_path = duplicate_string(in_path, p->scratch);
  p->out_path = duplcate_string(out_path, p->scratch);
  p->append_path = duplicate_string(append_path, p->scratch);

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
    j = (Job *)ctrl_backup_alloc(sizeof(Job));
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
