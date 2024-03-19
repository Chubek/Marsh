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
  PROCSTATE_RUNNING,
  PROCSTATE_STOPPED,
  PROCSTATE_COMPLETED,
  PROCSTATE_TERMINATED,
};

enum JobState {
  JOBSTATE_RUNNING,
  JOBSTATE_STOPPED,
};

struct SignalHandlers {
    sighandler_t *sigint;
    sighandler_t *sigchld;
    sighandler_t *sigstp;
}

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

struct ProcessList {
  Process *first_process;
  Process *next_in_line;
};

struct Job {
  int job_id;
  pid_t group_id;
  ProcessList *processes;
  JobState state;
  FRedir *stdin_redir;
  FRedir *stdout_redir;
  FRedir *stderr_redir;
  bool is_piped;
  bool is_foreground;
  Job *next;
};

struct JobList {
  Job *first_job;
  Job *next_in_line;
};

struct ShellState {
  JobList *foreground_jobs;
  JobList *background_jobs;
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


Process *create_process(char *exec_command, char **argv, size_t num_args, bool is_async) {
    Process *process = GC_ALLOC(sizeof(Process));
    if (!process) {
        perror("create_process: allocation failed");
        exit(EXIT_FAILURE);
    }
    process->exec_command = GC_MEMDUP(exec_command, strlen(exec_command) + 1);
    for (size_t i = 0; i < num_args; i++) {
        process->argv[i] = GC_MEMDUP(argv[i], strlen(argv[i]) + 1);
    }
    process->argv[num_args] = NULL; // NULL-terminate the argv list
    process->is_async = is_async;
    process->state = PROCSTATE_RUNNING; // Default state
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

void launch_process_non_async(Process *process) {
	// implement
}

void launch_process_async(Process *process) {
	// implement
}


void hook_process_signals_handlers(Process *process, SignalHandlers *handlers) {
	// implement
}

void wait_on_process(Process *process) {
	// implement
}


ProcessList *create_process_list(void) {
	// implement
}


void add_process_to_list(ProcessList *list, Process *process) {
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

void set_job_stdin_redir(Job *job, FRedir *redir) {
	// implement
}

void set_job_stdout_redir(Job *job, FRedir *redir) {
	// implement
}

void set_job_stderr_redir(Job *job, FRedir *redir) {
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

JobList *create_job_list(void) {
    JobList *list = GC_ALLOC(sizeof(JobList));
    if (!list) {
        perror("create_job_list: allocation failed");
        exit(EXIT_FAILURE);
    }
    list->first_job = NULL;
    return list;
}

void add_job_to_list(JobList *list, Job *job) {
    if (!list || !job) return;
    if (list->first_job == NULL) {
        list->first_job = job;
    } else {
        Job *current = list->first_job;
        while (current->next) {
            current = current->next;
        }
        current->next = job;
    }
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
