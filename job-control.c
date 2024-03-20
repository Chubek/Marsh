
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

typedef struct Process {
  pid_t pid;
  char **argv;
  int argc;
  int status;
  struct Process *next;
} Process;

typedef struct Job {
  pid_t pgid;
  Process *first_process;
  struct termios tmodes;
  int stdin, stdout, stderr;
  struct Job *next;
} Job;

static struct termios shell_tmodes;
static Job *jobs = NULL;

void initialize_job_control(void);
void handle_sigchld(int sig);
void launch_job(Job *job, int foreground);
void put_job_in_foreground(Job *job, int cont);
void put_job_in_background(Job *job, int cont);
void wait_for_job(Job *job);
void update_status(void);
void mark_process_status(pid_t pid, int status);
Job *create_job(void);
Process *create_process(char **argv, int argc);

void initialize_job_control(void) {
  pid_t pid = getpid();

  if (setpgid(pid, pid) < 0) {
    perror("Couldn't put the shell in its own process group");
    exit(1);
  }

  tcsetpgrp(STDIN_FILENO, pid);
  tcgetattr(STDIN_FILENO, &shell_tmodes);
  signal(SIGCHLD, handle_sigchld);
}
