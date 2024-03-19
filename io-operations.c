#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "marsh.h"

#ifndef FDTAB_SIZE
#define FDTAB_SIZE 1024
#endif

GCHeap *io_heap = NULL;

#define GC_INIT() io_heap = gc_heap_create()
#define GC_CALLOC(nmemb, size) gc_heap_alloc(io_heap, nmemb, size)
#define GC_ALLOC(size) GC_CALLOC(1, size)
#define GC_REALLOC(mem, new_size) gc_heap_realloc(io_heap, mem, new_size)
#define GC_FREE(mem) gc_heap_free(io_heap, mem)
#define GC_COLLECT() gc_heap_collect(io_heap)
#define GC_DUMP() gc_heap_dump(io_heap)
#define GC_MEMDUP(mem, size) gc_heap_memdup(io_heap, mem, size)
#define GC_VIZ(stream) gc_heap_visualize(io_heap, stream)

enum IOMode {
  IO_READ = 0,
  IO_WRITE = 1,
  IO_APPEND = 2,
};

enum RedirMode {
  REDIR_IN,
  REDIR_OUT,
  REDIR_APPEND,
  REDIR_RW,
};

struct FDesc {
  int fd;
  IOMode mode;
  FDesc *next;
};

struct FDescTable {
  FDesc *buckets[FDTAB_SIZE];
};

struct FRedir {
  RedirMode mode;
  char path[FILENAME_MAX + 1];
  FILE *stream;
  int fdesc;
  bool duplicate;
};

static inline uint16_t fdtab_hash(int fd) { return fd % FDTAB_SIZE; }

FDescTable *create_fdesc_table(void) {
  FDescTable *table = GC_ALLOC(sizeof(FDescTable));
  return table;
}

FDescTable *init_fdesc_table(FDesc *table) {
  for (size_t i = 0; i < FDTAB_SIZE; i++)
    table->buckets[i] = NULL;
}

FDesc *insert_fdesc_into_table(FDescTable *table, int fd, IOMode mode) {
  FDesc *node = GC_ALLOC(sizeof(FDesc));
  node->fd = fd;
  node->mode = mode;

  uint32_t hash = fdtab_hash(fd);
  if (table->buckets[hash] == NULL)
    table->buckets[hash] = node;
  else {
    node->next = table->buckets[hash];
    table->buckets[hash] = node;
  }

  return node;
}

FDesc *retrieve_fdesc_from_table(FDescTable *table, int fd) {
  uint32_t hash = fdtab_hash(fd);

  FDesc *node = table->bucket[hash];
  while (node != NULL) {
    if (node->fd == fd)
      return node;

    node = node->next;
  }

  return NULL;
}

void remove_fdesc_from_table(FDescTable *table, int fd) {
  FDesc *node = retrieve_fdesc_from_table(table, fd);
  GC_FREE(node);
  node = NULL;
}

FRedir *create_redir_from_path(RedirMode mode, char *path, bool duplicate) {
  FRedir *redir = GC_ALLOC(sizeof(FRedir));
  redir->mode = mode;
  memmove(&redir->path[0], path, FILENAME_MAX);
  redir->path[FILENAME_MAX] = 0;
  redir->fdesc = -1;
  redir->duplicate = duplicate;
  return redir;
}

FRedir *create_redir_from_fdesc(RedirMode mode, int fdesc, bool duplicate) {
  FRedir *redir = GC_ALLOC(sizeof(FRedir));
  redir->mode = mode;
  memset(&redir->path[0], 0, FILENAME_MAX);
  redir->fdesc = fdesc;
  redir->duplicate = duplicate;
  return redir;
}

void init_redir(FRedir *redir) {
  switch (redir->mode) {
  case REDIR_IN:
    redir->stream = 
	    redir->fdesc == -1 ? fopen(&redir->path[0], "r")
                                       : fdopen(redir->fdesc, "r");
    break;
  case REDIR_OUT:
    redir->stream = 
	    redir->fdesc == -1 ? fopen(&redir->path[0], "w")
                                       : fdopen(redir->fdesc, "w");
    break;
  case REDIR_APPEND:
    redir->stream = 
	    redir->fdesc == -1 ? fopen(&redir->path[0], "a")
                                       : fdopen(&redir->fdesc, "a");
    break;
  case REDIR_RW:
    redir->stream = 
	    redir->fdesc == -1 ? fopen(&redir->path[0], "w+")
                                       : fdopen(&redir->fdesc, "w+");
    break;
  default:
    break;
  }
}
