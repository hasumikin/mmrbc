#include <string.h>
#include <stdlib.h>
#include "common.h"
#include "debug.h"

#include "mrubyc/src/alloc.h"

#ifdef PICORBC_DEBUG
  /* GLOBAL */
  int alloc_count = 0;
  int free_count = 0;
  AllocList *last_alloc;

  void print_memory(void)
  {
  #ifndef MRBC_ALLOC_LIBC
    int total, used, free, fragment;
    mrbc_alloc_statistics( &total, &used, &free, &fragment );
    DEBUGP("Memory total:%d, used:%d, free:%d, fragment:%d", total, used, free, fragment );
  #endif
  }
  void memcheck(void)
  {
    DEBUGP("---MEMCHECK---\nalloc_count: %d, free_count: %d", alloc_count, free_count);
    AllocList *ah = last_alloc;
    while (ah != NULL) {
      WARNP("MemoryLeakDetected! alloc_count: %d, ptr: %p", ah->count, ah->ptr);
      if (ah->prev != NULL) {
        ah = ah->prev;
        free(ah->next);
      } else {
        free(ah);
        break;
      }
    }
  }
#endif /* PICORBC_DEBUG */

void *picorbc_alloc(size_t size)
{
  void *ptr;
  ptr = PICORBC_ALLOC(size);
#ifdef PICORBC_DEBUG
  DEBUGP("alloc_count: %d, ptr: %p, size: %d", alloc_count, ptr, (int)size);
  print_memory();
  alloc_count++;
//  if (alloc_count == 2){
//    DEBUGP("sssss");
//  }
  AllocList *ah = malloc(sizeof(AllocList));
  ah->count = alloc_count;
  ah->ptr = ptr;
  if (last_alloc != NULL) {
    ah->prev = last_alloc;
    last_alloc->next = ah;
  } else {
    ah->prev = NULL;
  }
  ah->next = NULL;
  last_alloc = ah;
#endif
  return ptr;
}

void picorbc_free(void *ptr)
{
  if (ptr == NULL) return;
  DEBUGP("free: %p", ptr);
  PICORBC_FREE(ptr);
#ifdef PICORBC_DEBUG
  print_memory();
  free_count++;
  AllocList *ah = last_alloc;
  if (ah->ptr == ptr) {
    last_alloc = ah->prev;
    free(ah);
  } else {
    while (ah != NULL) {
      if (ah->ptr == ptr) {
        if (ah->prev != NULL) {
          ah->prev->next = ah->next;
        }
        if (ah->next != NULL) {
          ah->next->prev = ah->prev;
        }
        free(ah);
        break;
      }
      ah = ah->prev;
    }
  }
#endif
}

char *strsafencpy(char *s1, const char *s2, size_t n, size_t max)
{
  DEBUGP("s1: `%s`, s2: `%s`, n: %d, max: %d", s1, s2, (int)n, (int)max);
  if (n < max) {
    strncpy(s1, s2, n + 1);
    DEBUGP("Copied: %s", s1);
  } else {
    FATALP("Can't cpy string!");
  }
  return s1;
}

char *strsafecpy(char *s1, const char *s2, size_t max)
{
  DEBUGP("s1: `%s`, s2: `%s`, max: %d", s1, s2, (int)max);
  return strsafencpy(s1, s2, strlen(s2), max);
}

char *strsafecat(char *dst, const char *src, size_t max)
{
  size_t lensrc = strlen(src);
  if (strlen(dst) + lensrc < max) {
    strncat(dst, src, lensrc);
  } else {
    FATALP("Can't cat string!");
  }
  return dst;
}
