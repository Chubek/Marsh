#ifndef UTILS_H
#define UTILS_H


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct Arena Arena;

Arena *arena_init(size_t size);
void *arena_alloc(Arena *arena, size_t size);
void arena_reset(Arena *arena);
void arena_free(Arena *arena);







#endif
