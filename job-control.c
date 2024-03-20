#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "marsh.h"

static GCHeap *job_heap = NULL;


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
  FRedir *stdin_redir;
  FRedir *stdout_redir;
  FRedir *stderr_redir;
  Process *next;
};

struct Job {
  int job_id;
  pid_t group_id;
  Process *process_list;
  JobState state;
  bool is_piped;
  bool is_foreground;
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

void set_signal_handler_sigchld(SignalHandlers *handlers, sighandler_t handler) {
    if (handlers) {
        handlers->sigchld = handler;
    }
}

void set_signal_handler_sigstp(SignalHandlers *handlers, sighandler_t handler) {
    if (handlers) {
        handlers->sigstp = handler;
    }
}

void set_signal_handler_sigquit(SignalHandlers *handlers, sighandler_t handler) {
    if (handlers) {
        handlers->sigquit = handler;
    }
}

void set_signal_handler_sigterm(SignalHandlers *handlers, sighandler_t handler) {
    if (handlers) {
        handlers->sigterm = handler;
    }
}


void sigchld_handler_async(int signum) {
    int saved_errno = errno;
    int status;
    pid_t pid;

    while ((pid = waitpid((pid_t)(-1), &status, WNOHANG)) > 0) {
	    // **TODO**: add operations here
    }

    errno = saved_errno;
}


Process *create_process(char *exec_command, char **exec_arguments, size_t num_arguments, bool is_async) {
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
    process->stdin_redir = NULL;
    process->stdout_redir = NULL;
    process->stderr_redir = NULL;
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

void set_process_stdin_redir(Process *process, FRedir *redir) {
	// implement
}

void set_process_stdout_redir(Process *process, FRedir *redir) {
	// implement
}

void set_process_stderr_redir(Process *process, FRedir *redir) {
	// implement
}


void add_next_process(Process *process, Process *next) {
	// implement
}





void wait_on_process_non_async(Process *process) {
    if (process == NULL || process->is_async) {
        fprintf(stderr, "wait_on_process_non_async: Invalid process or process is asynchronous.\n");
        return;
    }

    int status;
    while (waitpid(process->process_id, &status, 0) == -1) {
        if (errno != EINTR) { // Interrupted by a signal that was caught.
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
        fprintf(stderr, "launch_process_non_async: Invalid process or process is asynchronous.\n");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
 	// **TODO**: set up redirections

        if (execvp(process->exec_command, process->exec_arguments) == -1) {
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
    } else {
        process->process_id = pid;
        set_process_state(process, PROCSTATE_RUNNING);
        wait_on_process_non_async(process);
    }
}

void launch_process_async(Process *process) {
    if (process == NULL || !process->is_async) {
        fprintf(stderr, "launch_process_async: Invalid process or process is not asynchronous.\n");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
  	// **TODO**: set up redirections

        if (execvp(process->exec_command, process->exec_arguments) == -1) {
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
    } else { // Parent process
        process->process_id = pid;
        set_process_state(process, PROCSTATE_RUNNING);
    }
}


void hook_process_signals_handlers(Process *process, SignalHandlers *handlers) {
	// implement
}




Job *create_job(int job_id) {
	// implement
}

void set_job_group_id(Job *job, pid_t group_id) {
	// implement
}

void set_job_state(Job *job, JobState state) {
	// implement
}

void set_job_piped(Job *job, bool is_piped) {
	// implement
}

void set_job_foreground(Job *job, bool is_foreground) {
	// implement
}

void add_job_process(Job *job, Process *process) {
	// implement
}

void add_next_job(Job *job, Job *next) {
	// implement
}


void handle_job_redirection(Job *job) {
	// implement: this function should handle redirections of a job, based of FDRedir's
}


void handle_job_piping(Job *job) {
	// implement: this function should handle piping to the next job, if is_piped? = true
}



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



void set_shell_state_terminal_fdesc(ShellState *state, int terminal_fdesc) {
	// implement
}


void save_shell_state_termios(ShellShate *state) {
	// implement
}

void init_shell_state_fdesc_table(ShellState *state) {
	// implement
}

void shell_state_insert_fdesc(ShellState *state, FDesc *fdesc) {
	// implement
}



void shell_state_insert_foreground_job(ShellState *state, Job *job) {
	// implement
}


void shell_state_insert_background_job(ShellState *state, Job *job) {
	// implement
}
