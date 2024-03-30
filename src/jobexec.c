#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
  JSTATE_FOREGROUND,
  JSTATE_BACKGROUND,
  JSTATE_STOPPED,
} jobstate_t;

typedef enum {
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
  SIGFLAG_EXIT_ON_TERM_CLOSE = 1 << 10
} sigflags_t;

typedef enum {
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
