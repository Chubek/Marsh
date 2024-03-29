#ifndef JOB_EXEC_H
#define JOB_EXEC_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Process Process;
typedef struct Job Job;
typedef struct Environ Environ;

Process *push_blank_process_to_job(Job *j);
Job *push_blank_job_to_environ(Environ *e);

void execute_process(Process *p);
void execute_job(Job *j);

void free_process(Process *p);
void free_job(Job *j);
void free_environ(Environ *e);

#endif
