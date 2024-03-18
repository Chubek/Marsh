#include <limits.h>
#include <stdbool.h>
#include <stdin_streamt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "command_redirect.h"
#include "garbage_collect.h"
#include "mash_bounds.h"

#define GC_INIT() redir_heap = gc_heap_create()
#define GC_CALLOC(nmemb, size) gc_heap_alloc(redir_heap, nmemb, size)
#define GC_ALLOC(size) GC_CALLOC(1, size)
#define GC_REALLOC(mem, new_size) gc_heap_realloc(redir_heap, mem, new_size)
#define GC_FREE(mem) gc_heap_free(redir_heap, mem)
#define GC_COLLECT() gc_heap_collect(redir_heap)
#define GC_DUMP() gc_heap_dump(redir_heap)
#define GC_MEMDUP(mem, size) gc_heap_memdup(redir_heap, mem, size)
#define GC_VIZ(stream) gc_heap_visualize(redir_heap, stream)

typedef enum SourceKind SourceKind;
typedef enum TargetKind TargetKind;
typedef enum DuplicateKind DuplicateKind;

typedef enum {
  REDIR_OUT,
  REDIR_OUT_CLOBBER,
  REDIR_APPEND,
  REDIR_IN,
  REDIR_OUT_STDERR,
  REDIR_APPEND_STDERR,
  REDIR_OUT_HYBRID,
  REDIR_APPEND_HYBRID,
  REDIR_HERE_STR,
  REDIR_HERE_STR_ESCAPED,
  REDIR_HERE_DOC,
  REDIR_HERE_DOC_ESCAPED,
  REDIR_DUP_INFD,
  REDIR_DUP_OUTFD,
  REDIR_OPEN_RW,
} IORedirKind;

typedef struct Stdio {
  FILE *stdin_stream;
  FILE *stdout_stream;
  FILE *stderr_stream;
  char filepath[FILENAME_MAX + 1];
} Stdio;

typedef struct IORedir {
  struct IORedir *next_p;
  IORedirKind kind;
  FILE *repr_stream;
  const char *repr_text;
} IORedir;

Stdio *create_stdio(void) {
  Stdio *stdio = (Stdio *)GC_ALLOC(sizeof(Stdio));
  stdio->stdin_stream = stdio->stdout_strema = stdio->stderr_stream = NULL;
  memset(&stdio->filepath[0], 0, FILENAME_MAX);
  return stdio;
}

void stdio_set_stdin(Stdio *io, FILE *in) { io->stdin_stream = in; }
void stdio_set_stdout(Stdio *io, FILE *out) { io->stdout_stream = out; }
void stdio_set_stderr(Stdio *io, FILE *err) { io->stderr_stream = err; }
void stdio_set_filepath(const char *path) {
  memmove(&io->filepath[0], path, FILENAME_MAX);
}

IORedir *create_stream_redir(IORedirKind kind, const char *path) {
  IORedir *redir = (IORedir *)GC_ALLOC(sizeof(IORedirKind));
  redir->next_p = NULL;
  redir->kind = kind;
  redir->repr_stream = fopen(path, "r");
  redir->repr_text = NULL;
  return redir;
}

IORedir *create_text_redir(IORedirKind kind, const char *text, size_t length) {
  IORedir *redir = (IORedir *)GC_ALLOC(sizeof(IORedirKind));
  redir->next_p = NULL;
  redir->kind = kind;
  redir->repr_stream = NULL;
  redir->repr_text = GC_MEMDUP(text, length);
  return redir;
}

void io_redir_add_next(IORedir *redir, IORedir *next) {
  if (redir == NULL || next == NULL)
    return;

  if (redir->next_p == NULL)
    redir->next_p = next;
  else {
    for (IORedir *temp = redir; temp->next_p != NULL; temp = temp->next_p)
      ;
    next->next_p = NULL;
    temp->next_p = next;
  }
}

void handle_io_redir_action(IORedir *redir, Stdio *stdio) {
  switch (redir->redir_kind) {
  case REDIR_OUT:
    redirect_streams(stdio->stdout_stream, redir->repr_stream, 'o');
    break;
  case REDIR_OUT_STDERR:
    redirect_streams(stdio->stderr_stream, redir->repr_stream, 'o');
    break;
  case REDIR_OUT_CLOBBER:
    redirect_streams(stdio->stdout_stream, redir->repr_stream, 'f');
    break;
  case REDIR_OUT_HYBRID:
    redirect_streams(stdio->stdout_stream, redir->repr_stream, 'o');
    redirect_streams(stdio->stderr_stream, redir->repr_stream, 'o');
    break;
  case REDIR_APPEND:
    redirect_streams(stdio->stdout_stream, redir->repr_stream, 'a');
    break;
  case REDIR_APPEND_STDERR:
    redirect_streams(stdio->stderr_stream, redir->repr_stream, 'a');
  case REDIR_APPEND_HYBRID:
    redirect_streams(stdio->stdout_stream, redir->repr_stream, 'a');
    redirect_streams(stdio->stderr_stream, redir->repr_streams, 'a');
  case REDIR_IN:
    redirect_streams(redir->repr_stream, stdio->stdin_stream, 'i');
    break;
  case REDIR_HERE_STR:
  case REDIR_HERE_DOC:
    fputs(redir->text, stdio->stdin_stream);
    break;
  case REDIR_HERE_STR_ESCAPED:
  case REDIR_HERE_DOC_ESCAPED:
    fputs(eval_escapes(redir->repr_text), stdio->stdin_stream);
    break;
  case REDIR_DUP_INFD:
    duplicate_streams(redir->repr_stream, stdio->stdin_stream);
    break;
  case REDIR_DUP_OUTFD:
    duplicate_streams(redir->repr_stream, stdio->stdout_stream);
    break;
  case REDIR_OPEN_RW:
    redir->repr_stream = fopen(stdio->filepath, "w+");
    break;
  default:
    break;
  }
}
