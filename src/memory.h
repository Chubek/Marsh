#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum StrCompare strcmp_t;
typedef enum SymbolKind symkind_t;

typedef struct String String;
typedef struct Arena Arena;
typedef struct Symtbl Symtbl;
typedef struct Symbol;

Arena *arena_init(size_t size);
void *arena_alloc(Arena *arena, size_t size);
void arena_reset(Arena *arena);
void arena_free(Arena *arena);

String *create_string(uint8_t *source, size_t len, Arena *scratch);
uint16_t hash_string(String *string);
String *duplicate_string(String *string, Arena *scratch);
strcmp_t compare_strings(String *s1, String *s2);

Symtbl *symtbl_init(size_t size);
void symtbl_insert(Symbol *sym, char *key, symkind_t kind);
Symbol *symtbl_retrieve(char *key, symkind_t kind);
void symtbl_free(void);

#endif
