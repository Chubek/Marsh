#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "marsh.h"

// NOTE: Preprocess with allocpp.pl
//=> alloc io_heap, io_alloc, io_realloc, io_dump
//=> alloc cmd_heap, cmd_alloc, cmd_realloc, cmd_dump
//=> alloc proc_heap, proc_alloc, proc_realloc, proc_dump
//=> alloc job_heap, job_alloc, job_realloc, job_dump
//=> alloc env_heap, env_alloc, env_realloc, env_dump
//=> hashfunc environ_hash_func

typedef enum {
  PSTAT_PENDING,
  PSTAT_RUNNING,
  PSTAT_WAITING,
  PSTAT_COMPLETE,
  PSTAT_TERMINATED,
} pstat_t;

struct Stdio {
  int fno_in;
  int fno_out;
  int fno_err;
};

struct Command {
  char *util_sym;
  char **arguments;
  size_t num_arguments;
  Stdio *io;
};

struct Process {
  pid_t self_id;
  pid_t group_id;
  Command *command;
  bool async;
  bool piped;
  pstate_t status;
  Process *next;
};

struct Job {
  pid_t group_id;
  bool foreground;
  struct termios tmodes;
  Process *first_process;
  Job *next;
};

struct Environ {
  Job *foreground_job;
  Job **background_jobs;
  FDescTbl *fd_table;
  SymTbl *sym_table;
  FuncTbl *func_table;
  bool interactive;
};

Stdio *create_stdio(int fno_in, int fno_out, int fno_err) {
  Stdio *stdio = (Stdio *)io_alloc(sizeof(Stdio));
  stdio->fno_in = fno_in;
  stdio->fno_out = fno_out;
  stdio->fno_err = fno_err;
  return stdio;
}

Stdio *duplicate_stdio(Stdio *stdio) {
  Stdio *stdio_copy = (Stdio *)io_alloc(sizeof(Stdio));
  return (Stdio *)memmove(stdio_copy, stdio, sizeof(Stdio));
}

static inline Stdio *create_stdio_standard(void) {
  return create_stdio(STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO);
}

void set_stdio_in(Stdio *stdio, int fno_in) { stdio->fno_in = fno_in; }

void set_stdio_out(Stdio *stdio, int fno_out) { stdio->fno_out = fno_out; }

void set_stdio_err(Stdio *stdio, int fno_err) { stdio->fno_err = fno_err; }

Command *create_command(char *util_sym, char **arguments,
                        size_t num_arguments) {
  Command *command = (Command *)command_alloc(sizeof(Command));
  command->util_sym = duplicate_string(util_sym);
  command->arguments = duplicate_strings(arguments, num_arguments);
  command->num_arguments = num_arguments;
  command->io = NULL;
  return command;
}

Command *duplicate_command(Command *command) {
  Command *command_copy = (Command *)command_alloc(sizeof(Command));
  return (Command *)memmove(command_copy, command, sizeof(Command));
}

static inline void set_command_io(Command *command, Stdio *io) {
  command->io = duplicate_stdio(io);
}

Process *create_process(Command *command, bool async, bool piped) {
  Process *process = (Process *)process_alloc(sizeof(Process));
  process->self_id = -1;
  process->group_id = -1;
  process->command = command;
  process->async = async;
  process->piped = piped;
  process->state = PSTAT_PENDING;
  process->next = NULL;
  return process;
}

Process *duplicate_process(Process *process) {
  Process *process_copy = (Process *)process_alloc(sizeof(Process));
  return (Process *)memmove(process_copy, process, sizeof(Process));
}

void add_next_process(Process **head, Process *next) {
  next->next = *head;
  *head = next;
}

static inline void set_process_self_id(Process *process, pid_t self_id) {
  process->self_id = self_id;
}

static inline void set_process_group_id(Process *process, pid_t group_id) {
  process->group_id = group_id;
}
