#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
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
  REDIR_OUT_NOCLOBBER,
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

typedef struct IORedir {
  struct IORedir *next_p;
  IORedirKind redir_kind;

  enum SourceKind {
    SOURCE_STDIN = 0,
    SOURCE_STDOUT = 1,
    SOURCE_STDERR = 2,
    SOURCE_FILEPATH,
    SOURCE_TEXT,
    SOURCE_FDESC,
  } source_kind;

  union {
    char source_filepath[FILENAME_MAX + 1];
    char *source_text;
    int source_fdesc;
  };

  enum TargetKind {
    TARGET_STDIN = 0,
    TARGET_STDOUT = 1,
    TARGET_STDERR = 2,
    TARGET_FILEPATH,
    TARGET_FILEDESC,
  } target_kind;

  union {
    char target_filepath[FILENAME_MAX + 1];
    int target_fdesc;
  };

  enum DuplicateKind {
    DUPLICATE_STDIN = 0,
    DUPLICATE_STDOUT = 1,
    DUPLICATE_STDERR = 2,
    DUPLCIATE_FILEPATH,
    DUPLICATE_FDESCi,
  } duplicate_kind;

  union {
    char duplicate_path[FILENAME_MAX + 1];
    int duplicate_fdesc;
  };

} IORedir;

IORedir *create_io_redir(IORedirKind redir_kind, SourceKind source_kind,
                         void *source, TargetKind target_kind, void *target,
                         DuplicateKind duplicate_kind, void *duplicate) {

  IORedir *redir = (IORedir *)GC_ALLOC(sizeof(IORedir));
  redir->next_p = NULL;
  redir->redir_kind = redir_kind;
  redir->source_kind = source_kind;
  redir->target_kind = target_kind;

  if (source_kind == SOUCRE_FILEPATH)
    memmove(&redir->source_filepath[0], (char *)source, FILENAME_MAX);
  else if (source_kind == SOURCE_TEXT)
    redir->source_text =
        (char *)GC_MEMDUP((char *)source, MASH_BOUNDS_HERE_IO_MAX + 1);
  else if (source_kind == SOURCE_FDESC)
    redir->source_fdesc = *((int *)source);
  else
    redir->source_fdesc = source_kind;

  if (target_kind == TARGET_FILEPATH)
    memmove(&redir->target_filepath[0], (char *)target,
            FILENAME_MAX) else if (target_kind == TARGET_FDESC)
        redir->target_fdesc = *((int *)target);
  else
    redir->target_fdesc = target_kind;

  if (duplicate_kind == DUPLICATE_FILEPATH)
    memmove(&redir->duplicate_filepath[0], (char *)duplicate, FILENAME_MAX);
  else if (duplicate_kind == DUPLICATE_FDESC)
    redir->duplicate_fdesc = *(*(int)duplicate);
  else
    redir->duplicate_fdesc = duplicate_kind;

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
