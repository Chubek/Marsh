#ifndef SYMBOLS_H
#define SYMBOLS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef MAX_BUCKET
#define MAX_BUCKET (1 << 12)
#endif

typedef struct WordSym WordSym;
typedef struct CmdSym CmdSym;
typedef struct IOSym IOSym;
typedef struct FnSym FnSym;
typedef struct Symtbl Symtbl;

void insert_word_sym(Symtbl **tbl, char *name, /* TODO */);
void insert_cmd_sym(Symtbl **tbl, char *name, /* TODO */);
void insert_io_sym(Symtbl **tbl, char *name, /* TODO */);
void insert_fn_sym(Symtbl **tbl, char *name, /* TODO */);

// TODO: Exmaple:
FILE *get_iosym_file(Symtbl *tbl, char *name);

void free_symtbl(Symtbl *tbl);

#endif
