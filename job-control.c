#include <fcntl.h>
#include <limits.h>
#include <signal.h>
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

static GCHeap *job_heap = NULL;

static Process *waiting_process_list = NULL;

#define GC_INIT() job_heap = gc_heap_create()
#define GC_CALLOC(nmemb, size) gc_heap_alloc(job_heap, nmemb, size)
#define GC_ALLOC(size) GC_CALLOC(1, size)
#define GC_REALLOC(mem, new_size) gc_heap_realloc(job_heap, mem, new_size)
#define GC_FREE(mem) gc_heap_free(job_heap, mem)
#define GC_COLLECT() gc_heap_collect(job_heap)
#define GC_DUMP() gc_heap_dump(job_heap)
#define GC_MEMDUP(mem, size) gc_heap_memdup(job_heap, mem, size)
#define GC_VIZ(stream) gc_heap_visualize(job_heap, stream)

enum ProcessState {
  PROCSTATE_PENDING,
  PROCSTATE_RUNNING,
  PROCSTATE_STOPPED,
  PROCSTATE_SUSPENDED,
  PROCSTATE_COMPLETED,
  PROCSTATE_TERMINATED,
};

enum JobState {
  JOBSTATE_PENDING,
  JOBSTATE_RUNNING,
  JOBSTATE_STOPPED,
  JOBSTATE_SUSPENDED,
  JOBSTATE_COMPLETED,
  JOBSTATE_TERMINATED,
};

struct SignalHandlers {
  sighandler_t *sigint;
  sighandler_t *sigchld;
  sighandler_t *sigstp;
  sighandler_t *sigquit;
  sighandler_t *sigterm;
}

struct Process {
  pid_t process_id;
  pid_t group_id;
  char *exec_command;
  char *exec_arguments;
  size_t num_arguments;
  ProcessState state;
  int exit_status;
  bool is_async;
  int stdin_fileno;
  int stdout_fileno;
  int stderr_fileno;
  Process *next;
};

struct Job {
  int job_id;
  pid_t group_id;
  Process *process_list;
  JobState state;
  bool is_piped;
  bool exit_stat_negated;
  bool is_foreground;
  int exit_stat_coalesce;
  Job *next;
};

struct ShellState {
  Job *foreground_job_list;
  Job *background_job_list;
  int terminal_fdesc;
  struct termios tmodes;
  bool is_interactive;
  FDescTable *fdesc_table;
};

SignalHandlers *create_signal_handler(void) {
  SignalHandlers *handlers = GC_ALLOC(sizeof(SignalHandlers));
  if (!handlers) {
    perror("create_signal_handler: allocation failed");
    exit(EXIT_FAILURE);
  }
  handlers->sigint = SIG_IGN;
  handlers->sigchld = SIG_IGN;
  handlers->sigstp = SIG_IGN;
  handlers->sigquit = SIG_IGN;
  handlers->sigterm = SIG_IGN;
  return handlers;
}

void set_signal_handler_sigint(SignalHandlers *handlers, sighandler_t handler) {
  if (handlers) {
    handlers->sigint = handler;
  }
}

void set_signal_handler_sigchld(SignalHandlers *handlers,
                                sighandler_t handler) {
  if (handlers) {
    handlers->sigchld = handler;
  }
}

void set_signal_handler_sigstp(SignalHandlers *handlers, sighandler_t handler) {
  if (handlers) {
    handlers->sigstp = handler;
  }
}

void set_signal_handler_sigquit(SignalHandlers *handlers,
                                sighandler_t handler) {
  if (handlers) {
    handlers->sigquit = handler;
  }
}

void set_signal_handler_sigterm(SignalHandlers *handlers,
                                sighandler_t handler) {
  if (handlers) {
    handlers->sigterm = handler;
  }
}

void sigchld_handler_async(int signum) {
  int saved_errno = errno;
  int status;
  pid_t pid;

  while ((pid = waitpid((pid_t)(-1), &status, WNOHANG)) > 0) {
    Process *proc = find_process_by_pid(waiting_process_list, pid);
    if (proc != NULL) {
      if (WIFEXITED(status)) {
        proc->exit_status = WEXITSTATUS(status);
        set_process_state(proc, PROCSTATE_COMPLETED);
        printf("Process %d completed with exit status %d\n", pid,
               proc->exit_status);
      } else if (WIFSIGNALED(status)) {
        proc->exit_status = 128 + WTERMSIG(status);
        set_process_state(proc, PROCSTATE_TERMINATED);
        printf("Process %d terminated by signal %d\n", pid, WTERMSIG(status));
      } else if (WIFSTOPPED(status)) {
        set_process_state(proc, PROCSTATE_STOPPED);
        printf("Process %d stopped by signal %d\n", pid, WSTOPSIG(status));
      } else if (WIFCONTINUED(status)) {
        set_process_state(proc, PROCSTATE_RUNNING);
        printf("Process %d continued\n", pid);
      }

    } else {
      fprintf(stderr, "Received SIGCHLD for unknown pid %d\n", pid);
    }
    errno = saved_errno;
    remove_from_waiting_process_list(pid);
  }
}

Process *create_process(char *exec_command, char **exec_arguments,
                        size_t num_arguments, bool is_async) {
  Process *process = GC_ALLOC(sizeof(Process));
  if (!process) {
    perror("create_process: allocation failed");
    exit(EXIT_FAILURE);
  }
  process->exec_command = GC_MEMDUP(exec_command, strlen(exec_command) + 1);
  for (size_t i = 0; i < num_arguments; i++) {
    process->argv[i] = GC_MEMDUP(argv[i], strlen(argv[i]) + 1);
  }
  process->exec_arguments[num_arguments] = NULL;
  process->is_async = is_async;
  process->state = PROCSTATE_PENDING;
  process->stdin_fileno = STDIN_FILENO;
  process->stdout_fileno = STDOUT_FILENO;
  process->stderr_fileno = STDERR_FILENO;
  return process;
}

void set_process_state(Process *process, ProcessState state) {
  if (process) {
    process->state = state;
  }
}

void set_process_exit_status(Process *process, int status) {
  if (process) {
    process->exit_status = status;
  }
}

void set_process_stdin_fileno(Process *process, char *shell_word) {}

void set_process_stdout_fileno(Process *process, char *shell_word) {}

void set_process_stderr_fileno(Process *process, char *shell_word) {}

void add_next_process(Process *process, Process *next) {}

Process *find_process_by_pid(Process *process_chain, pid_t process_id) {
  Process *current = process_chain;
  while (current) {
    if (current->process_id == process_id) {
      return current;
    }
    current = current->next;
  }
  return NULL;

  void init_waiting_process_list(void) { waiting_process_list = NULL; }

  void add_to_waiting_process_list(Process * waiting_process) {
    if (waiting_process == NULL)
      return;

    waiting_process->next = waiting_process_list;
    waiting_process_list = waiting_process;
  }
}

void remove_from_waiting_process_list(pid_t process_id) {
  Process **indirect = &waiting_process_list;

  while (*indirect) {
    Process *current = *indirect;
    if (current->process_id == process_id) {
      free_process(current);
      *indirect = current->next;
      return;
    }
    indirect = &current->next;
  }
}

void free_process(Process *process) {
  if (process == NULL)
    return;

  GC_FREE(process->exec_command);
  for (size_t i = 0; i < process->num_arguments; i++) {
    GC_FREE(process->exec_arguments[i]);
  }
  GC_FREE(process->exec_arguments);
  GC_FREE(process);
}

void free_process_list(Process *head) {
  Process *current = head;
  while (current) {
    Process *next = current->next;
    free_process(current);
    current = next;
  }
}

void free_waiting_process_list(void) {
  free_process_list(waiting_process_list);
  waiting_process_list = NULL;
}

void apply_redirections(Process *process) {
  if (process->stdin_fileno != STDIN_FILENO) {
    if (dup2(process->stdin_fileno, STDIN_FILENO) == -1) {
      perror("Failed to redirect stdin");
      exit(EXIT_FAILURE);
    }
    close(process->stdin_fileno);
  }

  if (process->stdout_fileno != STDOUT_FILENO) {
    if (dup2(process->stdout_fileno, STDOUT_FILENO) == -1) {
      perror("Failed to redirect stdout");
      exit(EXIT_FAILURE);
    }
    close(process->stdout_fileno);
  }

  if (process->stderr_fileno != STDERR_FILENO) {
    if (dup2(process->stderr_fileno, STDERR_FILENO) == -1) {
      perror("Failed to redirect stderr");
      exit(EXIT_FAILURE);
    }
    close(process->stderr_fileno);
  }
}

void wait_on_process_non_async(Process *process) {
  if (process == NULL || process->is_async) {
    fprintf(stderr, "wait_on_process_non_async: Invalid process or process "
                    "is asynchronous.\n");
    return;
  }

  int status;
  while (waitpid(process->process_id, &status, 0) == -1) {
    if (errno != EINTR) {
      perror("waitpid failed");
      break;
    }
  }

  if (WIFEXITED(status)) {
    process->exit_status = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    process->exit_status = WTERMSIG(status);
  }

  set_process_state(process, PROCSTATE_COMPLETED);
}

void launch_process_non_async(Process *process) {
  if (process == NULL || process->is_async) {
    fprintf(stderr, "launch_process_non_async: Invalid process or process is "
                    "asynchronous.\n");
    return;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork failed");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    apply_redirections(process);

    if (execvp(process->exec_command, process->exec_arguments) == -1) {
      perror("execvp failed");
      exit(EXIT_FAILURE);
    }
  } else {
    process->process_id = pid;
    set_process_state(process, PROCSTATE_RUNNING);
    add_to_waiting_process_list(process);
    wait_on_process_non_async(process);
  }

  remove_from_waiting_process_list(pid);
}

void launch_process_async(Process *process) {
  if (process == NULL || !process->is_async) {
    fprintf(stderr, "launch_process_async: Invalid process or process is not "
                    "asynchronous.\n");
    return;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork failed");
    exit(EXIT_FAILURE);
  } else if (pid == 0) {
    apply_redirections(process);

    if (execvp(process->exec_command, process->exec_arguments) == -1) {
      perror("execvp failed");
      exit(EXIT_FAILURE);
    }
  } else {
    process->process_id = pid;
    add_to_waiting_process_list(process);
    set_process_state(process, PROCSTATE_RUNNING);
  }
}

void hook_process_signals_handlers(Process *process, SignalHandlers *handlers) {
}

Job *create_job(int job_id) {
  Job *job = GC_ALLOC(sizeof(Job));

  if (job == NULL) {
    perror("Failed to allocate memory for a new job");
    exit(EXIT_FAILURE);
  }

  job->job_id = job_id;
  job->group_id = -1;
  job->process_list = NULL;
  job->state = JOBSTATE_PENDING;
  job->is_piped = false;
  job->is_foreground = true;
  job->next = NULL;

  return job;
}

void set_job_group_id(Job *job, pid_t group_id) {
  if (!job)
    return;
  job->groupd_id = groupd_id;
}

void set_job_state(Job *job, JobState state) {
  if (!job)
    return;
  job->state = state;
}

void set_job_piped(Job *job, bool is_piped) {
  if (!job)
    return;
  job->is_piped = is_piped;
}

void set_job_exit_state_negated(Job *job, bool exit_stat_negated) {
  if (!job)
    return;
  job->exit_stat_negated = exit_stat_negated;
}

void set_job_foreground(Job *job, bool is_foreground) {
  if (!job)
    return;
  job->is_foreground = is_foreground;
}

void set_job_exit_stat_coalesce(Job *job, int exit_stat_coalesce) {
  if (!job)
    return;
  job->exit_stat_coalesce = exit_stat_coalesce;
}

void add_job_process(Job *job, Process *process) {}

void add_next_job(Job *job, Job *next) {}

void handle_job_redirection(Job *job) {}

void handle_job_piping(Job *job) {}

ShellState *create_shell_state(bool is_interactive) {
  ShellState *state = GC_ALLOC(sizeof(ShellState));
  if (!state) {
    perror("create_shell_state: allocation failed");
    exit(EXIT_FAILURE);
  }

  state->is_interactive = is_interactive;
  state->foreground_jobs = create_job_list();
  state->background_jobs = create_job_list();
  state->fdesc_table = create_fdesc_table();

  return state;
}

void set_shell_state_terminal_fdesc(ShellState *state, int terminal_fdesc) {}

void save_shell_state_termios(ShellShate *state) {}

void init_shell_state_fdesc_table(ShellState *state) {}

void shell_state_insert_fdesc(ShellState *state, FDesc *fdesc) {}

void shell_state_insert_foreground_job(ShellState *state, Job *job) {}

void shell_state_insert_background_job(ShellState *state, Job *job) {}
