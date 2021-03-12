#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "../src/mrubyc/src/mrubyc.h"

#include "../src/mmrbc.h"

#include "heap.h"

int loglevel;

void run(uint8_t *mrb)
{
  mrbc_init_global();
  mrbc_init_class();
  struct VM *vm = mrbc_vm_open(NULL);
  if( vm == 0 ) {
    fprintf(stderr, "Error: Can't open VM.\n");
    return;
  }

  if( mrbc_load_mrb(vm, mrb) != 0 ) {
    fprintf(stderr, "Error: Illegal bytecode.\n");
    return;
  }

  mrbc_vm_begin(vm);
  mrbc_vm_run(vm);
  mrbc_vm_end(vm);
  mrbc_vm_close(vm);
}

int handle_opt(int argc, char * const *argv, char **oneliner)
{
  struct option longopts[] = {
    { "version",  no_argument,       NULL, 'v' },
    { "oneline", required_argument,  NULL, 'e' },
    { "loglevel", required_argument, NULL, 'l' },
    { 0,          0,                 0,     0  }
  };
  int opt;
  int longindex;
  loglevel = LOGLEVEL_INFO;
  while ((opt = getopt_long(argc, argv, "ve:l:", longopts, &longindex)) != -1) {
    switch (opt) {
      case 'v':
        DEBUGP("v add: %p\n", optarg);
        fprintf(stdout, "mini mruby compiler %s\n", MMRBC_VERSION);
        return -1;
      case 'e':
        *oneliner = optarg;
        break;
      case 'l':
        DEBUGP("l add: %p\n", optarg);
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

static uint8_t heap[HEAP_SIZE];

int main(int argc, char *argv[])
{
  mrbc_init_alloc(heap, HEAP_SIZE);

  char *oneliner = NULL;
  int ret = handle_opt(argc, argv, &oneliner);
  if (ret != 0) return ret;

  StreamInterface *si;

  if (oneliner != NULL) {
    si = StreamInterface_new(oneliner, STREAM_TYPE_MEMORY);
  } else {
    if ( !argv[optind] ) {
      ERRORP("mmruby: no program file given");
      return 1;
    }
    si = StreamInterface_new(argv[optind], STREAM_TYPE_FILE);
    if (si == NULL) return 1;
  }
  ParserState *p = Compiler_parseInitState(si->node_box_size);
  if (Compiler_compile(p, si)) {
    run(p->scope->vm_code);
  } else {
    ret = 1;
  }

  StreamInterface_free(si);
  Compiler_parserStateFree(p);
#ifdef MMRBC_DEBUG
  memcheck();
#endif
  return ret;
}
