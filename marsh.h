#ifndef MARSH_H
#define MARSH_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct GCHeap GCHeap;

typedef enum IOMode IOMode;
typedef enum RedirMode RedirMode;

typedef struct FDesc FDesc;
typedef struct FDescTable FDescTable;
typedef struct FRedir FRedir;

GCHeap *gc_heap_create(void);
void *gc_heap_alloc(GCHeap *, size_t, size_t);
void *gc_heap_realloc(GCHeap *, void *, size_t);
void *gc_heap_free(GCHeap *, void *);
void *gc_heap_dump(GCHeap *);
void *gc_heap_memdup(GCHeap *, void *, size_t);
void gc_heap_visualize(GCHeap *, FILE *);

FDescTable *create_fdesc_table(void);
FDesc *insert_fdesc_into_table(FDescTable *table, int fd, IOMode mode);
FDesc *retrieve_fdesc_from_table(FDescTable *table, int fd);
void remove_fdesc_from_table(FDescTable *table, int fd);
FRedir *create_redir_from_path(RedirMode mode, char *path, bool duplicate);
FRedir *create_redir_from_fdesc(RedirMode mode, int fdesc, bool duplicate);
void init_redir_and_insert(FDescTable *table, FRedir *redir);

#endif
