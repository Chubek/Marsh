#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "marsh.h"

enum ProcessState {
  PROCSTATE_RUNNING,
  PROCSTATE_STOPPED,
  PROCSTATE_COMPLETED,
  PROCSTATE_TERMINATED,
};

enum JobState {
  JOBSTATE_RUNNING,
  JOBSTATE_STOPPED,
  JOBSTATE_COMPLETED,
  JOBSTATE_TERMINATED,
  JOBSTATE_REDIRECTED,
  JOBSTATE_PIPED,
};

struct Process {
  pid_t process_id;
  pid_t group_id;
  char *exec_command;
  char *argv[ARG_MAX];
  ProcessState state;
  int exit_status;
  bool is_async;
  Process *next;
};

struct Job {
  int job_id;
  pid_t group_id;
  Process *first_process;
  JobState state;
  FRedir *stdin_redir;
  FRedir *stdout_redir;
  FRedir *stderr_redir;
  bool is_foreground;
  Job *next;
};

struct ShellState {
  Job *foreground_job;
  Job **background_jobs;
  int shell_terminal;
  struct termios tmodes;
  bool is_interactive;
  FDescTable *fdesc_table;
};
