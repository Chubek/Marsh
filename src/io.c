#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "marsh.h"

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

void process_io_init(ProcessIO *pio, struct Arena *arena) {
  // TODO
}

void process_io_redirect_in(ProcessIO *pio, String *file_path) {
  // TODO
}

void process_io_redirect_out(ProcessIO *pio, String *file_path) {
  // TODO
}

void process_io_redirect_err(ProcessIO *pio, String *file_path) {
  // TODO
}

void process_io_cleanup(ProcessIO *pio) {
  // TODO
}
