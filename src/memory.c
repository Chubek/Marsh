#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistr.h>

#include "marsh.h"

#define DJB2_INIT 5381

extern Symtbl *symtbl;

//=> alloc region_heap, region_alloc, region_realloc, region_dump
//=> alloc stab_heap, stab_alloc, stab_realloc, stab_dump
//=> alloc str_backup_heap, str_backup_alloc, str_backup_realloc,
// str_backup_dump
//=> hashfunc stab_heap_hfunc

typedef enum {
  STRCMP_LEFT_SMALLER,
  STRCMP_RIGHT_SMALLER,
  STRCMP_EQUAL,
  STRCMP_UNEQUAL,
  STRCMP_NULL,
  STRCMP_UNKNOWN,
} strcmp_t;

struct Arena {
  uint8_t *next;
  uint8_t *end;
  uint8_t buffer[];
};

struct String {
  uint8_t *buf;
  size_t len;
  String *next;
};

struct Symbol {
  enum SymbolKind {
    SYM_FUNC,
    SYM_VAR,
    SYM_WORD,
  } kind;

  union {
    Function *func;
    Variable *var;
    Word *word;
  };
  String key;
  Symbol *next;
};

struct Symtbl {
  Symbol *buckets[NUM_BUCKETS];
  Arena *scratch;
};

Arena *arena_init(size_t size) {
  Arena *arena = region_alloc(sizeof(Arena) + size);
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

String *create_string(uint8_t *source, ssize_t len, Arena *scratch) {
  if (source == NULL)
    return NULL;

  len = len > 0 ? len : u8_strlen(source);

  String *string = (String *)arena_alloc(scratch, sizeof(String));
  string->buf = (uint8_t *)arena_alloc(scratch, len);

  if (buf == NULL) {
    buf = (uint8_t *)str_backup_alloc(len);
    if (buf == NULL)
      return NULL;
  }

  string->len = len;
  String->next = NULL;

  return string;
}

String *create_and_append_string(String **chain, uint8_t *source, ssize_t len,
                                 Arena *scratch) {
  if (chain == NULL)
    return NULL;

  String *s = create_string(source, len, scratch);

  if (next == NULL)
    return NULL;

  if (*chain == NULL) {
    *chain = s;
    return s;
  }

  for (String *chain_deref = *chain; chain_deref->next != NULL;
       chain_deref = chain_deref->next)
    ;
  chain_deref->next = s;

  return s;
}

void append_string(String **chain, String *s) {
  if (chain == NULL)
    return;

  if (*chain == NULL) {
    *chain = s;
    return;
  }

  for (String *chain_deref = *chain; chain_deref->next != NULL;
       chain_deref = chain_deref->next)
    ;
  chain_deref->next = s;
}

size_t get_num_strings(String *head) {
  size_t n = 0;
  while (head != NULL, n++)
    head = head->next;
  return n;
}

uint8_t *string_to_asciiz(String *s, Arena *scratch) {
  uint8_t *asciiz = (uint8_t *)arena_alloc(scratch, s->len + 1);

  if (asciiz == NULL) {
    asciiz = (uint8_t *)string_backup_alloc(s->len + 1);
    if (asciiz == NULL)
      return NULL;
  }

  memmove(&asciiz[0], &s->buf[0], s->len);
  return asciiz;
}

uint8_t **strings_to_asciiz_list(String *head, Arena *scratch) {
  size_t num_str = get_num_strings(head);
  uint8_t **asciiz_list =
      (uint8_t **)arena_alloc(scratch, (num_str + 1) * sizeof(uint8_t *));

  if (asciiz_list == NULL) {
    asciiz_list =
        (uint8_t **)str_backup_alloc((num_str + 1) * sizeof(uint8_t *));
    if (asciiz_list == NULL)
      return NULL;
  }

  asciiz_list[num_str] = NULL;

  for (size_t i = 0; i < num_str; i++) {
    asciiz_list[i] = string_to_asciiz(head, scratch);
    head = head->next;
  }

  return asciiz_list;
}

uint16_t hash_string(String *key) {
  uint16_t hash = DJB2_INIT;

  for (size_t i = 0; i < key->len; i++) {
    hash = ((hash << 5) + hash) + key->buf[i];
  }

  return hash;
}

String *duplicate_string(String *orig, Arena *scratch) {
  String *dup = arena_alloc(scratch, sizeof(String));
  dup->buf = (uint8_t *)arena_alloc(scratch, orig->len);

  if (dup->buf == NULL) {
    dup->buf = (uint8_t *)str_backup_alloc(orig->len);
    if (dup->buf == NULL)
      return NULL;
  }

  dup->len = orig->len;
  dup->next = NULL;
  return dup;
}

String *duplicate_string_list(String *chain, Arena *scratch) {
  String *dup_chain = NULL;
  while (chain != NULL) {
    append_string(&dup_chain, duplicate_string(chain));
    chain = chain->next;
  }
  return dup_chain;
}

strcmp_t compare_strings(String *s1, String *s2) {
  if (s1 == NULL || s2 == NULL)
    return STRCMP_NULL;

  if (s1->len < s2->len)
    return STRCMP_LEFT_SMALLER;
  else if (s1->len > s2->len)
    return STRCMP_RIGHT_SMALLER;
  else {
    for (size_t i = 0; i < s1->len; i++)
      if (s1->buf[i] != s2->buf[i])
        return STRCMP_UNEQUAL;
    return STRCMP_EQUAL;
  }
  return STRCMP_UNKNOWN;
}

void symtbl_init(size_t size) {
  for (size_t i = 0; i < NUM_BUCKETS; i++)
    symtbl->buckets[i] = NULL;
  symtbl->scratch = arena_init(INITIAL_SYMTAB_ARENA_SIZE);
}

void symtbl_insert(String key, symkind_t kind, void *value) {
  uint16_t idx = hash_string(key);

  Symbol *sym = (Symbol *)arena_alloc(symtbl->scratch, sizeof(Symbol));
  sym->key = duplicate_string(key, symtbl->scratch);
  sym->kind = kind;

  if (kind == SYM_FUNC)
    sym->func = (Function *)value;
  else if (kind == SYM_VAR)
    sym->var = (Variable *)value;
  else
    sym->word = (Word *)value;

  sym->next = symtbl->buckets[idx];
  symtbl->buckets[idx] = sym;
}

Symbol *symtbl_retrieve(String key, symkind_t kind) {
  uint16_t idx = hash_string(key);

  for (Symbol *s = symtbl->buckets[idx]; s != NULL; s = s->next)
    if (compare_strings(s->key, key) == STRCMP_EQUAL && s->kind == kind)
      return s;

  return NULL;
}

void symtbl_free(void) { free(symtbl->scratch); }
