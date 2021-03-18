#ifndef PICORBC_REGEX_H_
#define PICORBC_REGEX_H_

#include <stdint.h>
#include "regex_light/src/regex.h"

#define REGEX_MAX_RESULT_NUM 1
#define REGEX_MAX_RESULT_LENGTH 255

typedef struct regex_result
{
  char value[REGEX_MAX_RESULT_LENGTH];
} RegexResult;

bool Regex_match2(char *str, const char *pattern);

bool Regex_match3(char *str, const char *pattern, RegexResult result[REGEX_MAX_RESULT_NUM]);

void MyRegex_setup(bool use_global_preg_cache);

void MyRegexCache_free(void);

#endif
