#ifndef PICORBC_COMMON_H_
#define PICORBC_COMMON_H_

#include <stdint.h>

#include "debug.h"

#ifdef MRBC_ALLOC_LIBC
  #include <stdlib.h>
  #define PICORBC_ALLOC(size) malloc(size)
  #define PICORBC_FREE(ptr)   free(ptr)
#else
  #define PICORBC_ALLOC(size) mrbc_raw_alloc(size)
  #define PICORBC_FREE(ptr)   mrbc_raw_free(ptr)
#endif /* MRBC_ALLOC_LIBC */

#define MAX_LINE_LENGTH 256

#define MAX_TOKEN_LENGTH 256

#ifdef PICORBC_DEBUG
typedef struct alloc_list
{
  int count;
  void *ptr;
  struct alloc_list *prev;
  struct alloc_list *next;
} AllocList;

void print_memory(void);

void memcheck(void);
#endif /* !PICORBC_DEBUG */

void *picorbc_alloc(size_t size);

void picorbc_free(void *ptr);

char *strsafecpy(char *str1, const char *str2, size_t max);

char *strsafencpy(char *s1, const char *s2, size_t n, size_t max);

char *strsafecat(char *dst, const char *src, size_t max);

#endif /* PICORBC_COMMON_H_ */
