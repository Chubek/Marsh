#ifndef JOB_EXEC_H
#define JOB_EXEC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum IOFlags ioflags_t;
typedef enum SignalFlags sigflags_t;

typedef struct Process Process;
typedef struct Job Job;
typedef struct Environ Environ;

Process *push_blank_process_to_job(Job *job, ioflags_t io_flags, char *io_word,
                                   int io_num, char *cmd_path, char **argv,
                                   size_t argc, bool is_async);
Job *push_blank_job_to_environ(Environ *env, int job_id);
Environ *init_environ(Environ *env, int tty_fdesc, char *working_dir,
                      char **env_vars, sigflags_t sigflags);

void execute_process(Process *p);
void execute_job(Job *j);

void free_process(Process *p);
void free_job(Job *j);
void free_environ(Environ *e);

#endif
