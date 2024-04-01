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

#include "jobexec.h"
#include "utils.h"

typedef enum {
  PSTATE_PENDING,
  PSTATE_RUNNING,
  PSTATE_COMPLETED,
  PSTATE_ABORTED,
} pstate_t;

typedef enum {
  JSTATE_PENDING,
  JSTATE_FOREGROUND,
  JSTATE_BACKGROUND,
  JSTATE_STOPPED,
} jobstate_t;

typedef enum SignalFlags {
  SIGFLAG_IGNORE_INT = 1 << 0,
  SIGFLAG_IGNORE_QUIT = 1 << 1,
  SIGFLAG_NOHUP = 1 << 2,
  SIGFLAG_CUSTOM_CHLD = 1 << 3,
  SIGFLAG_TSTP_TO_BG = 1 << 4,
  SIGFLAG_INT_TO_TERM = 1 << 5,
  SIGFLAG_QUIT_TO_TERM = 1 << 6,
  SIGFLAG_CLEANUP_ZOMBIES = 1 << 7,
  SIGFLAG_RESTORE_TTY = 1 << 8,
  SIGFLAG_HANDLE_WINCH = 1 << 9,
  SIGFLAG_EXIT_ON_TERM_CLOSE = 1 << 10,
} sigflags_t;

typedef enum IOFlags {
  IO_READ = 1 << 1,
  IO_WRITE = 1 << 2,
  IO_APPEND = 1 << 3,
  IO_STR = 1 << 4,
} ioflags_t;

struct Process {
  pid_t pid;
  pid_t pgrpid;
  pstate_t state;
  ioflags_t ioflags;
  char *io_word;
  int io_num;
  int exit_stat;
  char *cmd_path;
  char **argv;
  size_t argc;
  bool is_async;
  int fno_in, fno_out, fno_err;
  int channel[2];
  Process *next_p;
  Arena *scratch;
};

struct Job {
  int job_id;
  pid_t grpid;
  jstate_t state;
  bool user_notified;
  struct termios tmodes;
  Process *first_p;
  Job *next_j;
  Arena *scratch;
};

struct Environ {
  pid_t session_id;
  int tty_fdesc;
  char *working_dir;
  char **env_vars;
  int last_bg_job_id;
  sigflags_t sigflags;
  Job *first_j_fg;
  Job *first_j_bg;
  Arena *scratch;
};

Process *push_blank_process_to_job(Job *job, ioflags_t io_flags, char *io_word,
                                   int io_num, char *cmd_path, char **argv,
                                   size_t argc, bool is_async) {
  Process *p = (Process *)arena_alloc(job->scratch, sizeof(Process)), *pprime;
  p->scratch = arena_init(PROCESS_INIT_ARENA_SIZE);

  if (job->first_p == NULL)
    job->first_p = p;
  else {
    for (pprime = job->first_p; pprime->next != NULL; pprime = pprime->next)
      ;
    p->next_p = NULL;
    pprime->next_p = p;
  }

  p->io_flags = io_flags;
  p->io_word = duplicate_string(p->scratch, io_word);
  p->io_num = io_num;
  p->cmd_path = duplicate_string(p->scratch, cmd_path);
  p->argv = duplicate_strings(p->scratch, argv, argc);
  p->argc = argc;
  p->is_async = is_async;

  p->state = PSTATE_PENDING;
  p->pid = -1;
  p->pgrpid = job->grpid;
  p->exit_stat = -1;

  p->fno_in = STDIN_FILENO;
  p->fno_out = STDOUT_FILENO;
  p->fno_err = STDERR_FILENO;

  p->channel[0] = STDIN_FILENO;
  p->channel[1] = STDOUT_FILENO;

  return p;
}

Job *push_blank_job_to_environ(Environ *env, int job_id) {
  Job *j = (Job *)arena_alloc(env->scratch, sizeof(Job)), *jprime;
  j->scratch = arena_init(JOB_INIT_ARENA_SIZE);

  if (env->first_j == NULL)
    env->first_j = j;
  else {
    for (jprime = env->first_j; jprime->next != NULL; jprime = jprime->next)
      ;
    j->next_j = NULL;
    jprime->next_j = j;
  }

  j->job_id = job_id;
  j->grpid = -1;

  j->state = JSTATE_PENDING;
  j->user_notified = false;

  j->first_p = NULL;

  if (tcgetattr(env->tty_fdesc, &j->tmodes) == -1) {
    fprintf("Error: Could not get terminal attributes");
    exit(EXIT_FAILURE);
  }

  return j;
}

Environ *init_environ(Environ *env, int tty_fdesc, char *working_dir,
                      char **env_vars, sigflags_t sigflags) {
  env->session_id = getsid(0);
  env->tty_fdesc = tty_fdesc;
  env->working_dir = working_dir;
  env->env_vars = env_vars;

  env->last_bg_job_id = -1;
  env->sigflags = sigflags;

  env->first_j_fg = NULL;
  env->first_j_bg = NULL;

  env->scratch = arena_init(ENVIRON_INIT_ARENA_SIZE);

  return env;
}

void execute_process(Process *process, int prev_read_end, char **env_vars) {
  pid_t id = process->pid = fork();

  if (process->pgrpid == -1) {
    if ((process->pgrpid = getgpid(0)) == 0) {
      fputs("Runtime Error: Failed to obtain process' group id\n", stderr);
      process->state = PSTATE_ABORTED;
      return;
    }
  } else {
    if (setgpid(id, process->pgrpid) < 0) {
      fputs("Runtime Error: Failed to set process' group id\n", stderr);
      process->state = PSTATE_ABORTED;
      return;
    }
  }

  if (id < 0) {
    fputs("Runtime Error: Failed to spawn child process\n", stderr);
    process->state = PSTATE_ABORTED;
    return;
  }

  if (pipe(process->channel) < 0) {
    fputs(
        "Runtime Error: Failed to establish communication with child process\n",
        stderr);
    process->state = PSTATE_ABORTED;
    return;
  }

  if (prev_read_end != -1) {
    if (dup2(prev_read_end, process->channel[0]) < 0) {
      fputs("Runtime Error: Failed to read the previous process input\n",
            stderr);
      process->state = PSTATE_ABORTED;
      return;
    }

    if (prev_read_end != STDOUT_FILENO)
      close(prev_read_end);
  }

  if (!id) {
    close(process->channel[0]);

    process->fno_in = dup(process->chan[1]);

    if (process->fno_in < -1) {
      fputs("Runtime Error: Failed to establish process input file\n", stderr);
      process->state = PSTATE_ABORTED;
      return;
    }

    if (handle_process_redirect(process) == -1) {
      fputs("Runtime Error: Failed to redirect process\n", stderr);
      process->state = PSTATE_ABORTED;
      return;
    }

    null_terminate_list(process->argv, process->argc);
    execlpe(process->cmd_path, process->argv, env_vars);

    fprintf(stderr, "Runtime Error: Failed to launch %s\n", process->cmd_path);
    return;
  }

  close(process->channel[1]);

  process->state = PSTATE_RUNNING;
}
