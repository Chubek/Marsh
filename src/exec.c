#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "marsh.h"

struct Command {
  String *path;
  String *argv_list;
  Arena *scratch;
};

Command *new_command(char *asciiz, char **asciiz_nullterm, Arena *scratch) {
  Command *cmd = (Command *)arena_alloc(scrath, sizeof(Command));

  if (cmd == NULL) {
    cmd = (Command *)cbu_alloc(sizeof(Command));
    if (cmd == NULL)
      return NULL;
  }

  cmd->path = create_string(asciiz);
  cmd->argv_list = create_string_list(asciiz_nullterm);

  return cmd;
}
