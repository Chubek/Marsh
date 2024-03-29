#ifndef MARSH_H
#define MARSH_H

// A. Global Symbolic Macro Definitions

#ifndef MAX_BUCKET
#define MAX_BUCKET 4096
#endif
  
#ifndef MAX_CACHE
#define MAX_CACHE 1024
#endif

#ifndef MAX_IDENT
#define MAX_IDENT 256
#endif

#ifndef FILEPATH_MAX
#define FILEPATH_MAX (FILENAME_MAX * 16)
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// B. Shell environment datatypes

typedef struct FDescState FDescState;
typedef struct Redirection Redirection;
typedef struct Command Command;
typedef struct Process Process;
typedef struct Job Job;
typedef struct Environ Environ;

// C. Shell tables datatyps

typedef struct FDescTblNode FDescTblNode;
typedef struct FDescTbl FDescTbl;
typedef struct SymTblNode SymTblNode;
typedef struct SymTbl SymTbl;
typedef struct FuncTblNode FuncTblNode;
typedef struct FuncTbl FuncTbl;

// D. Environment Definition Factory Function Declarations

/* Function Prototypes for  Redirection Factory Functions */
Redirection *create_redirection(redirflags_t flags, char *target,
                                bool target_fdesc, int fdesc);
Redirection *duplicate_redir(Redirection *redir);

/* Function Prototypes for Process & Command Factory Functions */
Command *create_command(char *util_name, char **arguments,
                        size_t num_arguments);
Command *duplicate_command(Command *command);
void set_command_redir(Command *command, Redirection *redir);
Process *create_process(Command *command);
Process *duplicate_process(Process *process);
void push_next_process(Process *head, Process *next);
void set_process_self_id(Process *process, pid_t self_id);
void set_process_group_id(Process *process, pid_t group_id);

/* Function Prototypes for Job & Environment Factory Functions */
Job *create_job(bool foreground, bool async);
void push_next_job(Job *job, Job *next);
void push_job_next_process(Job *job, Process *next_process);
void set_job_group_id(Job *job, pid_t group_id);
void set_job_terminal_state(Job *job, int shell_fd);

#endif
