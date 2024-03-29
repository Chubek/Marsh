#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <redirection.h>
#include <stdlib.h>
#include <string.h>

#include "marsh.h"

typedef uint16_t hash_t;

typedef enum {
  FD_OPEN,
  FD_CLOSED,
  FD_ERROR,
} fstate_t;

struct FDescTblNode {
  char fpath[FILEPATH_MAX + 1];
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
  char ident[MAX_IDENT + 1];
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
  char ident[MAX_IDENT + 1];
  Function *fundef;
  Redirection *redir;
  FuncTblNode *next;
};

struct FuncTbl {
  FuncTblNode *buckets[MAX_BUCKET];
  void *cache[MAX_CACHE];
};
