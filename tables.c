#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
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
  char fpath[FILENAME_MAX + 1];
  int fno;
  fdstate_t status;
  FDescTblNode *next;
};

struct FDescTbl {
  FDescTblNode *buckets[MAX_BUCKET];
  void *cache[MAX_CACHE];
};

typedef enum {
  NAME_STRING,
  NAME_PATTERN,
  NAME_PARAMETER,
  NAME_COMMAND,
  NAME_SPECIAL,
} namekind_t;

struct NameTblNode {
  char ident[UCHAR_MAX + 1];
  namekind_t kind;
  union {
    char *string;
    Pattern *pattern;
    Parameter *parameter;
    Command *command;
    Special *special;
  };
  NameTblNode *next;
};

struct NameTbl {
  NameTblNode *bucket[MAX_BUCKET];
  void *cache[MAX_CACHE];
};

struct FuncTblNode {
  char ident[UCHAR_MAX + 1];
  Function *fundef;
  Stdio *redir;
  FuncTblNode *next;
};

struct FuncTbl {
  FuncTblNode *buckets[MAX_BUCKET];
  void *cache[MAX_CACHE];
};
