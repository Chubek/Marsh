#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symbols.h"
#include "utils.h"

struct WordSym {
  char *name;
  WordSym *next_s;

  enum WordSymKind {
    WORDSYM_LITERAL_CONST,
    WORDSYM_PARAM_EXPN,
    WORDSYM_CMD_EXPN,
    WORDSYM_ARITH_EXPN,
    WORDSYM_GLOB_PATTERN,
  } kind;

  union {
    Literal *literal_const;
    ParamExpn *param_expn;
    CmdExpn *cmd_expn;
    ArithExpn *arith_expn;
    GlobPattern *glob_pattern;
  };
};

struct CmdSym {
  char *name;
  CmdSym *next_s;

  enum CmdSymKind {
    CMDSYM_RESERVED,
    CMDSYM_BUILTIN,
    CMDSYM_FUNCTION,
  } kind;

  union {
    BuiltinCallback *builtin_impl;
    ShellFunction *function_impl;
  };
};

struct IOSym {
  char *name;
  IOSym *next_s;

  int fdesc;
  FILE *stream;

  enum IOSymKind {
    IOSYM_OPEN_R,
    IOSYM_OPEN_W,
    IOSYM_OPEN_A,
    IOSYM_OPEN_RW,
    IOSYM_CLOSED,
    IOSYM_ERR,
  } kind;
};


struct Symtbl {
   WordSym *word_stab[MAX_HASHBUCKET];
   CmdSym *cmd_stab[MAX_HASHBUCKET];
   IOSym *io_stab[MAX_HASHBUCKET];
   Arena *scratch;
};














