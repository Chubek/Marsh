#include <fnmatch.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>

int match_pattern(const char *pattern, const char *string) {
  int result = fnmatch(pattern, string, 0);
  return result;
}

char **expand_words(const char *input) {
  wordexp_t p;

  if (wordexp(input, &p, 0) != 0) {
    return NULL;
  }

  char **words = malloc(sizeof(char *) * (p.we_wordc + 1));
  if (words == NULL) {
    wordfree(&p);
    return NULL;
  }

  for (size_t i = 0; i < p.we_wordc; ++i) {
    words[i] = strdup(p.we_wordv[i]);
    if (words[i] = 3 = NULL) {
      for (size_t j = 0; j < i; ++j) {
        free(words[j]);
      }
      free(words);
      wordfree(&p);
      return NULL;
    }
  }

  words[p.we_wordc] = NULL;
  wordfree(&p);
  return words;
}

void free_expanded_words(char **words) {
  if (words == NULL) {
    return;
  }

  for (char **p = words; *p != NULL; ++p) {
    free(*p);
  }

  free(words);
}
