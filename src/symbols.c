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
    WORDSYM_UNKOWN,
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
    CMDSYM_UNKNOWN,
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
    IOSYM_UNKNOWN,
  } kind;
};

struct Symtbl {
  WordSym *word_stab[MAX_HASHBUCKET];
  CmdSym *cmd_stab[MAX_HASHBUCKET];
  IOSym *io_stab[MAX_HASHBUCKET];
  Arena *scratch;
};

static inline size_t hash_name(char *name) {
  size_t hash = 5381;
  int c;
  while ((c = *name++))
    hash = ((hash << 5) + hash) + c;
  return hash;
}

Symtbl *init_symtbl(Arena *scratch) {
  Symtbl *tbl = arena_alloc(scratch, sizeof(Symtbl));
  if (!tbl) {
    fprintf(stderr, "Memory allocation failed for symbol table\n");
    exit(EXIT_FAILURE);
  }
  memset(tbl->word_stab, 0, sizeof(tbl->word_stab));
  memset(tbl->cmd_stab, 0, sizeof(tbl->cmd_stab));
  memset(tbl->io_stab, 0, sizeof(tbl->io_stab));
  tbl->scratch = arena_init(INIT_SYMTBL_ARENA_SIZE);
  return tbl;
}

void free_symtbl(Symtbl *tbl) {
  if (!tbl)
    return;
  arena_free(tbl->scratch);
}

void insert_wordsym_literal(Symtbl *tbl, char *name, Literal *literal) {
  if (!tbl || !name || !literal)
    return;
  size_t index = hash_name(name) % MAX_BUCKET;
  WordSym *sym = arena_alloc(tbl->scratch, sizeof(WordSym));
  if (!sym) {
    fprintf(stderr, "Memory allocation failed for WordSym\n");
    exit(EXIT_FAILURE);
  }
  sym->name = arena_strdup(scratch, name);
  sym->kind = WORDSYM_LITERAL_CONST;
  sym->literal_const = literal;
  sym->next_s = tbl->word_stab[index];
  tbl->word_stab[index] = sym;
}

void insert_wordsym_arithexpn(Symtbl *tbl, char *name, ArithExpn *arith_expn) {
  if (!tbl || !name || !arith_expn)
    return;
  size_t index = hash_name(name) % MAX_BUCKET;
  WordSym *sym = arena_alloc(tbl->scratch, sizeof(WordSym));
  if (!sym) {
    fprintf(stderr, "Memory allocation failed for WordSym\n");
    exit(EXIT_FAILURE);
  }
  sym->name = arena_strdup(tbl->scratch, name);
  sym->kind = WORDSYM_ARITH_EXPN;
  sym->arith_expn = arith_expn;
  sym->next_s = tbl->word_stab[index];
  tbl->word_stab[index] = sym;
}

void insert_wordsym_cmdexpn(Symtbl *tbl, char *name, CmdExpn *cmd_expn) {
  if (!tbl || !name || !cmd_expn)
    return;
  size_t index = hash_name(name) % MAX_BUCKET;
  WordSym *sym = arena_alloc(tbl->scratch, sizeof(WordSym));
  if (!sym) {
    fprintf(stderr, "Memory allocation failed for WordSym\n");
    exit(EXIT_FAILURE);
  }
  sym->name = arena_strdup(tbl->scratch, name);
  sym->kind = WORDSYM_CMD_EXPN;
  sym->cmd_expn = cmd_expn;
  sym->next_s = tbl->word_stab[index];
  tbl->word_stab[index] = sym;
}

void insert_wordsym_globpattern(Symtbl *tbl, char *name,
                                GlobPattern *glob_pattern) {
  if (!tbl || !name || !glob_pattern)
    return;
  size_t index = hash_name(name) % MAX_BUCKET;
  WordSym *sym = arena_alloc(tbl->scratch, sizeof(WordSym));
  if (!sym) {
    fprintf(stderr, "Memory allocation failed for WordSym\n");
    exit(EXIT_FAILURE);
  }
  sym->name = arena_strdup(tbl->scratch, name);
  sym->kind = WORDSYM_GLOB_PATTERN;
  sym->glob_pattern = glob_pattern;
  sym->next_s = tbl->word_stab[index];
  tbl->word_stab[index] = sym;
}

Literal *get_wordsym_literal(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return NULL;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (WordSym *sym = tbl->word_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0 && sym->kind == WORDSYM_LITERAL_CONST) {
      return sym->literal_const;
    }
  }
  return NULL;
}

ArithExpn *get_wordsym_arith_expn(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return NULL;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (WordSym *sym = tbl->word_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0 && sym->kind == WORDSYM_ARITH_EXPN) {
      return sym->arith_expn;
    }
  }
  return NULL;
}

CmdExpn *get_wordsym_cmd_expn(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return NULL;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (WordSym *sym = tbl->word_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0 && sym->kind == WORDSYM_CMD_EXPN) {
      return sym->cmd_expn;
    }
  }
  return NULL;
}

GlobPattern *get_wordsym_glob_pattern(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return NULL;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (WordSym *sym = tbl->word_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0 && sym->kind == WORDSYM_GLOB_PATTERN) {
      return sym->glob_pattern;
    }
  }
  return NULL;
}

void insert_cmdsym_reserved(Symtbl *tbl, char *reserved) {
  if (!tbl || !reserved)
    return;
  size_t index = hash_name(reserved) % MAX_BUCKET;
  CmdSym *sym = arena_alloc(tbl->scratch, sizeof(CmdSym));
  if (!sym) {
    fprintf(stderr, "Memory allocation failed for CmdSym\n");
    exit(EXIT_FAILURE);
  }
  sym->name = arena_strdup(tbl->scratch, reserved);
  sym->kind = CMDSYM_RESERVED;
  sym->next_s = tbl->cmd_stab[index];
  tbl->cmd_stab[index] = sym;
}

void insert_cmdsym_builtin(Symtbl *tbl, char *builtin,
                           BuiltinCallback *callback) {
  if (!tbl || !builtin || !callback)
    return;
  size_t index = hash_name(builtin) % MAX_BUCKET;
  CmdSym *sym = arena_alloc(tbl->scratch, sizeof(CmdSym));
  if (!sym) {
    fprintf(stderr, "Memory allocation failed for CmdSym\n");
    exit(EXIT_FAILURE);
  }
  sym->name = arena_strdup(tbl->scratch, builtin);
  sym->kind = CMDSYM_BUILTIN;
  sym->builtin_impl = callback;
  sym->next_s = tbl->cmd_stab[index];
  tbl->cmd_stab[index] = sym;
}

void insert_cmdsym_function(Symtbl *tbl, char *funcname, ShellFunction *defn) {
  if (!tbl || !funcname || !defn)
    return;
  size_t index = hash_name(funcname) % MAX_BUCKET;
  CmdSym *sym = arena_alloc(tbl->scratch, sizeof(CmdSym));
  if (!sym) {
    fprintf(stderr, "Memory allocation failed for CmdSym\n");
    exit(EXIT_FAILURE);
  }
  sym->name = arena_strdup(tbl->scratch, funcname);
  sym->kind = CMDSYM_FUNCTION;
  sym->function_impl = defn;
  sym->next_s = tbl->cmd_stab[index];
  tbl->cmd_stab[index] = sym;
}

bool is_word_reserved(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return false;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (CmdSym *sym = tbl->cmd_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0 && sym->kind == CMDSYM_RESERVED) {
      return true;
    }
  }
  return false;
}

BuiltinCallback *get_builtin_callback(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return NULL;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (CmdSym *sym = tbl->cmd_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0 && sym->kind == CMDSYM_BUILTIN) {
      return sym->builtin_impl;
    }
  }
  return NULL;
}

ShellFunction *get_function(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return NULL;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (CmdSym *sym = tbl->cmd_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0 && sym->kind == CMDSYM_FUNCTION) {
      return sym->function_impl;
    }
  }
  return NULL;
}

void create_io_bindings(Symtbl *tbl, char *word, iosymkind_t kind) {
  if (!tbl || !word)
    return;
  size_t index = hash_name(word) % MAX_BUCKET;
  IOSym *sym = arena_alloc(tbl->scratch, sizeof(IOSym));
  if (!sym) {
    fprintf(stderr, "Memory allocation failed for IOSym\n");
    exit(EXIT_FAILURE);
  }
  sym->name = arena_strdup(tbl->scratch, word);
  sym->kind = kind;
  sym->fdesc = -1;
  sym->stream = NULL;
  sym->next_s = tbl->io_stab[index];
  tbl->io_stab[index] = sym;
}

int get_io_fdesc(Symtbl *tbl, char *word) {
  if (!tbl || !word)
    return -1;
  size_t index = hash_name(word) % MAX_BUCKET;
  for (IOSym *sym = tbl->io_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, word) == 0) {
      return sym->fdesc;
    }
  }
  return -1;
}

FILE *get_io_stream(Symtbl *tbl, char *word) {
  if (!tbl || !word)
    return NULL;
  size_t index = hash_name(word) % MAX_BUCKET;
  for (IOSym *sym = tbl->io_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, word) == 0) {
      return sym->stream;
    }
  }
  return NULL;
}

void destroy_io_stream(Symtbl *tbl, char *word) {
  if (!tbl || !word)
    return;
  size_t index = hash_name(word) % MAX_BUCKET;
  IOSym **sym_ptr = &tbl->io_stab[index];
  while (*sym_ptr != NULL) {
    IOSym *sym = *sym_ptr;
    if (strcmp(sym->name, word) == 0) {
      if (sym->stream != NULL) {
        fclose(sym->stream);
        sym->stream = NULL;
      }
      *sym_ptr = sym->next_s;
      break;
    }
    sym_ptr = &sym->next_s;
  }
}

wordsymkind_t get_wordsym_kind(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return WORDSYM_UNKNOWN;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (WordSym *sym = tbl->word_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0) {
      return sym->kind;
    }
  }
  return WORDSYM_UNKNOWN;
}

cmdsymkind_t get_cmdsym_kind(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return CMDSYM_UNKNOWN;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (CmdSym *sym = tbl->cmd_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0) {
      return sym->kind;
    }
  }
  return CMDSYM_UNKNOWN;
}

iosymkind_t get_iosym_kind(Symtbl *tbl, char *name) {
  if (!tbl || !name)
    return IOSYM_UNKNOWN;
  size_t index = hash_name(name) % MAX_BUCKET;
  for (IOSym *sym = tbl->io_stab[index]; sym != NULL; sym = sym->next_s) {
    if (strcmp(sym->name, name) == 0) {
      return sym->kind;
    }
  }
  return IOSYM_UNKNOWN;
}
