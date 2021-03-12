#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

#include "../src/mrubyc/src/alloc.h"

#include "../src/mmrbc.h"

#include "../src/ruby-lemon-parse/parse_header.h"

#include "heap.h"

int loglevel;

int handle_opt(int argc, char * const *argv)
{
  struct option longopts[] = {
    { "version",  no_argument,       NULL, 'v' },
    { "debug",    no_argument,       NULL, 'd' },
    { "verbose",  no_argument,       NULL, 'b' },
    { "loglevel", required_argument, NULL, 'l' },
    { 0,          0,                 0,     0  }
  };
  int opt;
  int longindex;
  loglevel = LOGLEVEL_INFO;
  while ((opt = getopt_long(argc, argv, "vdbl:", longopts, &longindex)) != -1) {
    switch (opt) {
      case 'v':
        fprintf(stdout, "mini mruby compiler %s\n", MMRBC_VERSION);
        return -1;
      case 'b': /* verbose */
        /* TODO */
        break;
      case 'd': /* debug */
        #ifndef MMRBC_DEBUG
          fprintf(stderr, "[ERROR] `--debug` option is only valid if you did `make` without CFLAGS=-DNDEBUG\n");
          return 1;
        #endif
        loglevel = LOGLEVEL_DEBUG;
        break;
      case 'l':
        #ifndef MMRBC_DEBUG
          fprintf(stderr, "[ERROR] `--loglevel=[level]` option is only valid if you made executable without -DNDEBUG\n");
          return 1;
        #endif
        if ( !strcmp(optarg, "debug") ) { loglevel = LOGLEVEL_DEBUG; } else
        if ( !strcmp(optarg, "info") )  { loglevel = LOGLEVEL_INFO; } else
        if ( !strcmp(optarg, "warn") )  { loglevel = LOGLEVEL_WARN; } else
        if ( !strcmp(optarg, "error") ) { loglevel = LOGLEVEL_ERROR; } else
        if ( !strcmp(optarg, "fatal") ) { loglevel = LOGLEVEL_FATAL; } else
        {
          fprintf(stderr, "Invalid loglevel option: %s\n", optarg);
          return 1;
        }
        break;
      default:
        fprintf(stderr, "error! \'%c\' \'%c\'\n", opt, optopt);
        return 1;
    }
  }
  return 0;
}

int output(Scope *scope, char *in)
{
  FILE *fp;
  char out[strlen(in) + 5];
  if (strcmp(&in[strlen(in) - 3], ".rb") == 0) {
    memcpy(out, in, strlen(in));
    memcpy(&out[strlen(in) - 3], ".mrb\0", 5);
  } else {
    memcpy(out, in, strlen(in));
    memcpy(&out[strlen(in)], ".mrb\0", 5);
  }
  if( (fp = fopen( out, "wb" ) ) == NULL ) {
    FATALP("mmrbc: cannot write a file. (%s)", out);
    return 1;
  } else {
    fwrite(scope->vm_code, scope->vm_code_size, 1, fp);
    fclose(fp);
  }
  return 0;
}

static uint8_t heap[HEAP_SIZE];

int main(int argc, char * const *argv)
{
  int ret = handle_opt(argc, argv);
  if (ret != 0) return ret;

  if ( !argv[optind] ) {
    ERRORP("mmrbc: no program file given");
    return 1;
  }

  char *in = argv[optind];

  mrbc_init_alloc(heap, HEAP_SIZE);

  StreamInterface *si = StreamInterface_new(in, STREAM_TYPE_FILE);
  if (si == NULL) return 1;
  ParserState *p = Compiler_parseInitState(si->node_box_size);
  if (Compiler_compile(p, si)) {
    ret = output(p->scope, in);
  } else {
    ret = 1;
  }
  StreamInterface_free(si);
  Compiler_parserStateFree(p);
  if (ret != 0) return ret;
#ifdef MMRBC_DEBUG
  memcheck();
#endif
  return ret;
}
