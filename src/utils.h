#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum SymbolKind symkind_t;

typedef struct Arena Arena;
typedef struct Symtbl Symtbl;
typedef struct Symbol;

Arena *arena_init(size_t size);
void *arena_alloc(Arena *arena, size_t size);
void arena_reset(Arena *arena);
void arena_free(Arena *arena);


Symtbl *symtbl_init(size_t size);
void symtbl_insert(Symbol *sym, char *key, symkind_t kind);
Symbol *symtbl_retrieve(char *key, symkind_t kind);


#endif
