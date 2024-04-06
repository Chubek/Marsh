#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "marsh.h"

//=> alloc ppio_bu, ppio_bu_alloc, ppio_bu_realloc, ppio_bu_dump
//=> hashfunc ppio_heap_hash

struct ProcessIO {
  int stdin_fd;
  int stdout_fd;
  int stderr_fd;
  bool redirect_in;
  bool redirect_out;
  bool redirect_err;
  String *in_file;
  String *out_file;
  String *err_file;
  Arena *scratch;
};

void process_io_init(ProcessIO *pio, Arena *arena) {
  if (!pio)
    return;

  pio->stdin_fd = STDIN_FILENO;
  pio->stdout_fd = STDOUT_FILENO;
  pio->stderr_fd = STDERR_FILENO;

  pio->redirect_in = false;
  pio->redirect_out = false;
  pio->redirect_err = false;

  pio->in_file = NULL;
  pio->out_file = NULL;
  pio->err_file = NULL;

  pio->scratch = arena_init(PIO_INIT_ARENA_SIZE);
}

void process_io_redirect_in(ProcessIO *pio, String *file_path) {
  if (!pio || !file_path)
    return;

  int fd = open(string_to_asciiz(filepath, pio->scratch), O_RDONLY);
  if (fd == -1) {
    perror("Failed to open file for input redirection");
    return;
  }
  pio->stdin_fd = fd;
  pio->redirect_in = true;
}

void process_io_redirect_out(ProcessIO *pio, String *file_path) {
  if (!pio || !file_path)
    return;

  int fd =
      open(string_to_asciiz(filepath, pio->scratch),
           O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd == -1) {
    perror("Failed to open file for output redirection");
    return;
  }
  pio->stdout_fd = fd;
  pio->redirect_out = true;
}

void process_io_redirect_err(ProcessIO *pio, String *file_path) {
  if (!pio || !file_path)
    return;

  int fd =
      open(string_to_asciiz(filepath, pio->scratch),
           O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  if (fd == -1) {
    perror("Failed to open file for error redirection");
    return;
  }
  pio->stderr_fd = fd;
  pio->redirect_err = true;
}

void process_io_cleanup(ProcessIO *pio) {
  if (!pio)
    return;

  if (pio->redirect_in && pio->stdin_fd != STDIN_FILENO)
    close(pio->stdin_fd);
  if (pio->redirect_out && pio->stdout_fd != STDOUT_FILENO)
    close(pio->stdout_fd);
  if (pio->redirect_err && pio->stderr_fd != STDERR_FILENO)
    close(pio->stderr_fd);
}
