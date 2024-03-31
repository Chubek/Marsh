#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

#ifndef MAX_BUCKET
#define MAX_BUCKET (1 << 12)
#endif

typedef enum WordSymKind wordsymkind_t;
typedef enum CmSymKind cmdsymkind_t;
typedef enum IOSymKind iosymkind_t;

typedef struct WordSym WordSym;
typedef struct CmdSym CmdSym;
typedef struct IOSym IOSym;
typedef struct Symtbl Symtbl;

wordsymkind_t get_wordsym_kind(Symtbl *tbl, char *name);
cmdsymkind_t get_cmdsym_kind(Symtbl *tbl, char *name);
iosymkind_t get_iosym_kind(Symtbl *tbl, char *name);

void insert_wordsym_literal(Symtbl *tbl, char *name, Literal *literal);
void insert_wordsym_paramexpn(Symtbl *tbl, char *name, ParamExpn *param_expn);
void insert_wordsym_arithexpn(Symtbl *tbl, char *name, ArithExpn *arith_expn);
void insert_wordsym_cmdexpn(Symtbl *tbl, char *name, CmdExpn *cmd_expn);
void insert_wordsym_globpattern(Symtbl *tbl, char *name,
                                GlobPattern *glob_pattern);

Literal *get_wordsym_literal(Symtbl *tbl, char *name);
ParamExpn *get_wordsym_param_expn(Symtbl *tbl, char *name);
ArithExpn *get_wordsym_arith_expn(Symtbl *tbl, char *name);
CmdExpn *get_wordsym_cmd_expn(Symtbl *tbl, char *name);
GlobPattern *get_wordsym_glob_pattern(Symtbl *tbl, char *name);

void insert_cmdsym_reserved(Symtbl *tbl, char *reserved);
void insert_cmdsym_builtin(Symtbl *tbl, char *builtin,
                           BuiltinCallback *callback);
void insert_cmdsym_function(Symtbl *tbl, char *funcname, Function *defn);

bool is_word_reserved(Symtbl *tbl, char *name);
BuiltinCallback *get_builtin_callback(Symtbl *tbl, char *name);
Function *get_function(Symtbl *tbl, char *name);

void create_io_bindings(Symtbl *tbl, char *word, iosymkind_t kind);
int get_io_fdesc(Symtbl *tbl, char *word);
FILE *get_io_stream(Symtbl *tbl, char *word);
void destroy_io_stream(Symtbl *tbl, char *word);

Symtbl *init_symtbl(Arena *scratch);
void free_symtbl(Symtbl *tbl);

#endif
