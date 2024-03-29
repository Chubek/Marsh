#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "marsh.h"

// A. IO Definitions

typedef enum {
  REDIR_READ = (1 << 1),
  REDIR_WRITE = (1 << 2),
  REDIR_APPEND = (1 << 3),
  REDIR_CLOSE = (1 << 4),
} redirflags_t;

struct Redirection {
  char *target;
  int fdesc;
  bool target_fdesc;
  redirflags_t flags;
};

struct FDescState {
  int fno_in;
  int fno_out;
  int fno_err;
};

// B. Process & Command Definitions

typedef enum {
  PSTAT_PENDING,
  PSTAT_RUNNING,
  PSTAT_WAITING,
  PSTAT_COMPLETE,
  PSTAT_TERMINATED,
} pstat_t;

struct Command {
  char *util_name;
  char **arguments;
  size_t num_arguments;
  Redirection *redir;
};

struct Process {
  pid_t self_id;
  pid_t group_id;
  Command *command;
  pstate_t status;
  Process *next;
};

// C. Job & Environment Definitions

struct Job {
  pid_t group_id;
  bool foreground;
  bool async;
  FDescState fd_state;
  struct termios term_state;
  Process *first_process;
  Job *next;
};

struct Environ {
  Job *foreground_job;
  Job **background_jobs;
  size_t num_background_jobs;
  FDescTbl *fd_table;
  SymTbl *sym_table;
  FuncTbl *func_table;
  bool interactive;
  int shell_fdesc;
};

// D. Redirection Factory Functions

Redirection *create_redirection(redirflags_t flags, char *target,
                                bool target_fdesc, int fdesc) {
  Redirection *redir = (Redirection *)redir_alloc(sizeof(Redir));
  redir->target = duplicate_string(target);
  redir->fdesc = duplicate_string(fdesc);
  redir->target_fdesc = target_fdesc;
  redir->flags = flags;
  return redir;
}

Redirection *duplicate_redir(Redirection *redir) {
  Redirection *redir_copy = (Redirection *)redir_alloc(sizeof(Redirection));
  return memmove(redir_copy, redir, sizeof(Redirection));
}

// E. Process & Command Factory Functions

Command *create_command(char *util_name, char **arguments,
                        size_t num_arguments) {
  Command *command = (Command *)command_alloc(sizeof(Command));
  command->util_name = duplicate_string(util_name);
  command->arguments = duplicate_strings(arguments, num_arguments);
  command->num_arguments = num_arguments;
  command->io = NULL;
  return command;
}

Command *duplicate_command(Command *command) {
  Command *command_copy = (Command *)command_alloc(sizeof(Command));
  return (Command *)memmove(command_copy, command, sizeof(Command));
}

static inline void set_command_redir(Command *command, Redirection *redir) {
  command->io = duplicate_redirection(redir);
}

Process *create_process(Command *command) {
  Process *process = (Process *)process_alloc(sizeof(Process));
  process->self_id = process->group_id = -1;
  process->command = command;
  process->state = PSTAT_PENDING;
  process->next = NULL;
  return process;
}

Process *duplicate_process(Process *process) {
  Process *process_copy = (Process *)process_alloc(sizeof(Process));
  return (Process *)memmove(process_copy, process, sizeof(Process));
}

void push_next_process(Process *head, Process *next) {
  Process *p;
  for (p = *head; p->next != NULL; p = p->next)
    ;
  p->next = next;
}

static inline void set_process_self_id(Process *process, pid_t self_id) {
  process->self_id = self_id;
}

static inline void set_process_group_id(Process *process, pid_t group_id) {
  process->group_id = group_id;
}

// F. Job & Environment Factory Functions

Job *create_job(bool foreground, bool async) {
  Job *job = (Job *)job_alloc(sizeof(Job));
  job->group_id = -1;
  job->async = async;
  job->foreground = foreground;
  job->first_process = NULL;
  job->next = NULL;
  return job;
}

void push_next_job(Job *job, Job *next) {
  Job *j;
  for (j = job; j->next != NULL; j = j->next)
    ;
  j->next = next;
}

static inline void push_job_next_process(Job *job, Process *next_process) {
  push_next_process(job->first_process, next_process);
}

static inline void set_job_group_id(Job *job, pid_t group_id) {
  job->group_id = group_id;
}

static inline void set_job_terminal_state(Job *job, int shell_fd) {
  tcgetattr(shell_fd, &job->term_state);
}
