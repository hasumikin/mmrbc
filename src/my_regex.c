#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "picorbc.h"
#include "common.h"
#include "my_regex.h"

typedef struct preg_cache {
  const char *pattern;
  regex_t preg;
} PregCache;

#define PREG_CACHE_SIZE 23

PregCache *global_preg_cache;

void MyRegex_setup(bool use_global_preg_cache)
{
#ifndef MRBC_ALLOC_LIBC
  RegexSetAllocProcs(picorbc_alloc, picorbc_free);
#endif
  if (use_global_preg_cache) {
    global_preg_cache = picorbc_alloc(sizeof(PregCache) * PREG_CACHE_SIZE);
    memset(global_preg_cache, 0, sizeof(PregCache) * PREG_CACHE_SIZE);
  } else {
    global_preg_cache = NULL;
  }
}

void regex_cache_free(void)
{
  int i = 0;
  PregCache *cache = global_preg_cache;
  while (cache->preg.atoms && i < PREG_CACHE_SIZE) {
    regfree(&cache->preg);
    cache++;
    i++;
  }
}

void MyRegexCache_free(void)
{
  if (global_preg_cache == NULL) return;
  regex_cache_free();
  picorbc_free(global_preg_cache);
}

bool regex_match(char *str, const char *pattern, bool resultRequired, RegexResult *result)
{
  int i = 0;
  int size;
  bool status = false;
  PregCache *cache;

  if (global_preg_cache) {
    cache = global_preg_cache;
    while (cache->preg.atoms && i < PREG_CACHE_SIZE) {
      if (cache->pattern == pattern) break;
      cache++;
      i++;
    }
    if (i == PREG_CACHE_SIZE) {
      regex_cache_free();
      memset(global_preg_cache, 0, sizeof(PregCache) * PREG_CACHE_SIZE);
      cache = global_preg_cache;
    }
  } else {
    cache = picorbc_alloc(sizeof(PregCache));
    cache->preg.atoms = NULL;
  }

  if (cache->preg.atoms == NULL) {
    cache->pattern = pattern;
    if (regcomp(&cache->preg, pattern, REG_EXTENDED|REG_NEWLINE) != 0){
      FATALP("regcomp failed: /%s/", pattern);
    }
  }

  size = (&cache->preg)->re_nsub + 1;
  regmatch_t pmatch[size]; // number of subexpression + 1

  if (regexec(&cache->preg, str, size, pmatch, 0) != 0){
    DEBUGP("no match: %s", pattern);
  } else {
    DEBUGP("match!: %s", pattern);
    status = true;
  }

  if (status && resultRequired) {
    for (i = 0; i < size; i++){
      int startIndex = pmatch[i].rm_so;
      int endIndex = pmatch[i].rm_eo;
      if (startIndex == -1 || endIndex == -1) {
        continue;
      }
      DEBUGP("match[%d] index [start, end] = %d, %d", i, startIndex, endIndex);
      strsafencpy((result + i)->value, str + startIndex, endIndex - startIndex, MAX_TOKEN_LENGTH);
      (result + i)->value[endIndex - startIndex] = '\0';
      DEBUGP("match result: %s", (result + i)->value);
    }
  }

  if (global_preg_cache == NULL) {
    regfree(&cache->preg);
    picorbc_free(cache);
  }

  return status;
}

bool Regex_match2(char *str, const char *pattern)
{
  return regex_match(str, pattern, false, NULL);
}

bool Regex_match3(char *str, const char *pattern, RegexResult result[REGEX_MAX_RESULT_NUM])
{
  return regex_match(str, pattern, true, result);
}
