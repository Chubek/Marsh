#ifndef MARSH_H
#define MARSH_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Process Process;
typedef struct Job Job;

void initialize_job_control(void);
void sigchld_handler(int sig);
void sigint_handler(int sig);
void sigstp_handler(int sig);
void sigcont_handler(int sig);
void setup_signal_handlers(void);
void launch_job(Job *job, int foreground);
void put_job_in_foreground(Job *job, int cont);
void put_job_in_background(Job *job, int cont);
void wait_for_job(Job *job);
bool job_is_stopped(Job *job);
bool job_is_completed(Job *job);
void update_status(void);
void mark_process_status(pid_t pid, int status);
Job *create_job(void);
Process *create_process(char **argv, int argc);
void append_process_to_job(Job *job, Process *process);
void append_job_to_list(Job *new_job);

typedef struct Symdef Symdef;
typedef struct Symtbl Symtbl;

#endif
