i#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Arena {
  char *next;
  char *end;
  char buffer[];
};

Arena *arena_init(size_t size) {
  Arena *arena = malloc(sizeof(Arena) + size);
  if (arena) {
    arena->next = arena->buffer;
    arena->end = arena->buffer + size;
  }
  return arena;
}

void *arena_alloc(Arena *arena, size_t size) {
  if (arena->next + size > arena->end)
    return NULL;
  void *mem = arena->next;
  arena->next += size;
  return mem;
}

void arena_reset(Arena *arena) { arena->next = arena->buffer; }

void arena_free(Arena *arena) { free(arena); }
