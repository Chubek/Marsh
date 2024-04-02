#ifndef JOB_H
#define JOB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum IOFlags ioflags_t;
typedef enum SignalFlags sigflags_t;

typedef struct Process Process;
typedef struct Job Job;
typedef struct Environ Environ;

Process *get_process_by_pid(Job *job, pid_t pid);
Process *get_process_chain_by_pgid(Job *job, pid_t pgid);
Job *get_job_by_job_id(Environ *env, int job_id, bool bg);
Job *get_job_chain_by_jpgid(Environ *env, pid_t jpgid, bool bg);

Process *push_blank_process_to_job(Job *job, ioflags_t io_flags, char *io_word,
                                   int io_num, char *cmd_path, char **argv,
                                   size_t argc, bool is_async);

Job *push_blank_job_to_environ(Environ *env, int job_id);

Environ *init_environ(Environ *env, int tty_fdesc, char *working_dir,
                      char **env_vars, sigflags_t sigflags);

int handle_process_redirect(Process *process);

void execute_process(Process *process, int prev_out, char **env_vars);
void execute_job(Job *job, char **env_vars);

void wait_on_process(Process *process, bool force);
void set_process_state(Process *process);

void free_process(Process *process);
void free_job(Job *job);
void free_environ(Environ *env);

#endif /* job.h */