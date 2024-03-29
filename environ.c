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

// NOTE: Preprocess with allocpp.pl
//=> alloc redir_heap, redir_alloc, redir_realloc, redir_dump
//=> alloc command_heap, command_alloc, command_realloc, command_dump
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

typedef enum {
  REDIR_FILE_IN,
  REDIR_FDESC_IN,
  REDIR_FILE_OUT,
  REDIR_FDESC_OUT,
  REDIR_FILE_APPEND,
  REDIR_FDESC_APPEND,
  REDIR_HERE_NORMAL,
  REDIR_HERE_EXPAND,
  REDIR_FILE_RW,
} redirkind_t;

struct Redirection {
  char *source_word;
  char *target_word;
  redirkind_t kind;
};

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
  bool async;
  pstate_t status;
  int fno_in, fno_out, fno_err;
  Process *next;
};

struct Job {
  pid_t group_id;
  bool foreground;
  bool async;
  struct termios tmodes;
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

Redirection *create_redirection(redirkind_t kind, char *source_word,
                                char *target_word) {
  Redirection *redir = (Redirection *)redir_alloc(sizeof(Redir));
  redir->source_word = duplicate_string(source_word);
  redir->target_word = duplicate_string(target_word);
  redir->kind = kind;
  return redir;
}

Redirection *duplicate_redir(Redirection *redir) {
  Redirection *redir_copy = (Redirection *)redir_alloc(sizeof(Redirection));
  return memmove(redir_copy, redir, sizeof(Redirection));
}

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

Process *create_process(Command *command, bool async, bool piped) {
  Process *process = (Process *)process_alloc(sizeof(Process));
  process->self_id = process->group_id = -1;
  process->command = command;
  process->async = async;
  process->piped = piped;
  process->fno_in = process->fno_out = process->fno_err = -1;
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

static inline void set_job_terminal_attributes(Job *job, int shell_fd) {
  tcgetattr(shell_fd, &job->tmodes);
}

pid_t execute_command(Command *command, int prev_in, int *next_in) {
  int pipe_out[2], fno_in = -1, fno_out = -1, fno_err = -1;
  pid_t child_pid;

  if (pipe(pipe_out) < 0) {
    runtime_error(RTMERR_NONFATAL | RTMERR_IO, "Failed to create pipe");
    ABORT_FORK();
  }

  if ((child_pid = fork) < 0) {
    runtime_error(RTMERR_NONFATAL | RTMERR_PROCESS,
                  "Failed to fork the parent process");
    ABORT_FORK();

  } else if (!child_pid) {
    if (command->redir != NULL) {
      if (hook_redir(command->redir, &fno_in, &fno_out, &fno_err) == -1) {
        runtime_error(RTMERR_NONFATAL | RTMERR_IO, "Failed to redirect output");
        ABORT_EXEC();
      }
    }

    if (fno_in == -1 && prev_in == -1)
      fno_in = STDIN_FILENO;
    else if (fno_in == -1 && prev_in > 0)
      fno_in = prev_in;

    if (next_in != NULL) {
      if (dup2(pipe_out[1], fno_out) < 0) {
        runtime_error(RTMERR_NONFATAL | RTMERR_IO, "Failed to pipe output");
        ABORT_EXEC();
      }

      *next_in = pipe_out[0];
      close(pipe_out[0]);
      close(pipe_out[1]);
    }

    if (command->arguments[command->num_arguments - 1] != NULL) {
      command->arguments = (char **)command_realloc(
          command->arguments, (command->num_arguments + 1) * sizeof(char *));
      command->arguments[command->num_arguments++] = NULL;
    }

    execlp(command->util_name, command->arguments);
    runtime_error(RTMERR_FATAL | RTERR_EXEC, "Execution failed");
    ABORT_POSTEXEC();
  }

  close(pipe_out[0]);
  close(pipe_out[1]);

  if (prev_in != -1)
    close(prev_in);

  return child_pid;
}



