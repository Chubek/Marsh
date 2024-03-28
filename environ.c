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
  FunTbl *func_table;
  bool interactive;
};
