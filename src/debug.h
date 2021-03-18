#ifndef PICORBC_DEBUG_H_
#define PICORBC_DEBUG_H_

#include <stdio.h>
#include <stdint.h>

#define LOGLEVEL_FATAL 0
#define LOGLEVEL_ERROR 1
#define LOGLEVEL_WARN  2
#define LOGLEVEL_INFO  3
#define LOGLEVEL_DEBUG 4

/* GLOBAL */
extern int loglevel;

#if !defined(NDEBUG)

  #define PICORBC_DEBUG

  #define DEBUGP(fmt, ...)                 \
    do {                                  \
      if (loglevel >= LOGLEVEL_DEBUG) {   \
        printf("\033[34;1m[DEBUG] ");     \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define INFOP(fmt, ...)                  \
    do {                                  \
      if (loglevel >= LOGLEVEL_INFO) {    \
        printf("[INFO]  ");      \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define WARNP(fmt, ...)                  \
    do {                                  \
      if (loglevel >= LOGLEVEL_WARN) {    \
        printf("\033[35;1m[WARN]  ");     \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define ERRORP(fmt, ...)                 \
    do {                                  \
      if (loglevel >= LOGLEVEL_ERROR) {   \
        printf("\033[31;1m[ERROR] ");     \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define FATALP(fmt, ...)                 \
    do {                                  \
      if (loglevel >= LOGLEVEL_FATAL) {   \
        printf("\033[37;41;1m[FATAL] ");     \
        DEBUG_PRINTF(fmt, ##__VA_ARGS__); \
        printf("\033[m\n");                 \
      }                                   \
    } while (0)
  #define DEBUG_PRINTF(fmt, ...)                  \
    do {                                          \
      printf("%s:%d %s() ",                       \
            __FILE__,  __LINE__,  __func__);      \
      printf(fmt, ##__VA_ARGS__);                 \
    } while (0)
#else
  #define DEBUGP(fmt, ...) /* omit */
  #define INFOP(fmt, ...)  /* omit */
  #define WARNP(fmt, ...)  (fprintf(stdout, "[WARN]  "),        \
                           fprintf(stdout, fmt, ##__VA_ARGS__),\
                           fprintf(stdout, "\n"))
  #define ERRORP(fmt, ...) (fprintf(stderr, "[ERROR] "),        \
                           fprintf(stderr, fmt, ##__VA_ARGS__),\
                           fprintf(stderr, "\n"))
  #define FATALP(fmt, ...) (fprintf(stderr, "[FATAL] "),        \
                           fprintf(stderr, fmt, ##__VA_ARGS__),\
                           fprintf(stderr, "\n"))
#endif /* !NDEBUG */

#endif /* PICORBC_DEBUG_H_ */
