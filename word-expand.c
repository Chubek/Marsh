#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "marsh.h"

//=> alloc wordexp_heap, wordexp_alloc, wordexp_realloc, wordexp_dump
//=> hashfunc wordexp_heap_hashfunc

typedef struct Symdef {
  const char *name;
  enum Symkind {
    SYM_WORD,
    SYM_GLOB,
    SYM_COMMAND,
    SYM_STRING,
    SYM_CACHE,
  } kind;
  union {
    Word word;
    Glob glob;
    Command command;
    Param param;
    char *string;
    void *cache;
  };
  struct Symdef *next;
} Symdef;

typedef struct Symtbl {
  size_t num_defs;
  Symdef **defs;
};
