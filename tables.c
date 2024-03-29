#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <redirection.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAX_BUCKET
#define MAX_BUCKET 4096
#endif

#ifndef MAX_CACHE
#define MAX_CACHE 1024
#endif

#include "marsh.h"

typedef uint16_t hash_t;

typedef enum {
  FD_OPEN,
  FD_CLOSED,
  FD_ERROR,
} fstate_t;

struct FDescTblNode {
  char fpath[FILESYM_MAX + 1];
  int fno;
  fdstate_t status;
  FDescTblNode *next;
};

struct FDescTbl {
  FDescTblNode *buckets[MAX_BUCKET];
  void *cache[MAX_CACHE];
};

typedef enum {
  SYM_STRING,
  SYM_PATTERN,
  SYM_PARAMETER,
  SYM_COMMAND,
  SYM_SPECIAL,
} symkind_t;

struct SymTblNode {
  char ident[UCHAR_MAX + 1];
  symkind_t kind;
  union {
    char *string;
    Pattern *pattern;
    Parameter *parameter;
    Command *command;
    Special *special;
  };
  SymTblNode *next;
};

struct SymTbl {
  SymTblNode *bucket[MAX_BUCKET];
  void *cache[MAX_CACHE];
};



struct FuncTblNode {
  char ident[UCHAR_MAX + 1];
  Function *fundef;
  Redirection *redir;
  FuncTblNode *next;
};

struct FuncTbl {
  FuncTblNode *buckets[MAX_BUCKET];
  void *cache[MAX_CACHE];
};
