#include <stdbool.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "../src/mrubyc/src/mrubyc.h"

#if defined(PICORBC_DEBUG) && !defined(MRBC_ALLOC_LIBC)
  #include "../src/mrubyc/src/alloc.c"
#endif

#include "../src/picorbc.h"

#include "heap.h"

int loglevel;

void vm_restart(struct VM *vm)
{
  vm->pc_irep = vm->irep;
  vm->inst = vm->pc_irep->code;
  vm->current_regs = vm->regs;
  vm->callinfo_tail = NULL;
  vm->target_class = mrbc_class_object;
  vm->exc = 0;
  vm->exception_tail = 0;
  vm->error_code = 0;
  vm->flag_preemption = 0;
}

#define NODE_BOX_SIZE 30

static uint8_t heap[HEAP_SIZE];

int
start_irb(void)
{
  bool firstRun = true;
  char script[1024];
  struct VM *c_vm;
  StreamInterface *si;

  mrbc_init_alloc(heap, HEAP_SIZE);
  mrbc_init_global();
  mrbc_init_class();
  c_vm = mrbc_vm_open(NULL);
  if(c_vm == NULL) {
    fprintf(stderr, "Error: Can't open VM.\n");
    return 1;
  }

  ParserState *p;// = Compiler_parseInitState(NODE_BOX_SIZE);

  Lvar *lvar;
  Symbol *symbol;
  Literal *literal;
  StringPool *string_pool;
  unsigned int sp;
  unsigned int max_sp;
  unsigned int nlocals;

  for (;;) {
    p = Compiler_parseInitState(NODE_BOX_SIZE);

    if (!firstRun) {
      p->scope->nlocals = nlocals;
      p->scope->lvar    = lvar;
      p->scope->symbol  = symbol;
      p->scope->literal = literal;
      p->current_string_pool = string_pool;
      p->scope->sp      = sp;
      p->scope->max_sp  = max_sp;
    }

    printf("picoirb> ");
    if (fgets(script, sizeof(script), stdin) == NULL) {
      fprintf(stderr, "Error: fgets() failed.\n");
      return 1;
    };
    script[strlen(script) - 1] = '\0';
    si = StreamInterface_new(script, STREAM_TYPE_MEMORY);

    if (Compiler_compile(p, si)) {
      if(mrbc_load_mrb(c_vm, p->scope->vm_code) != 0) {
        fprintf(stderr, "Error: Illegal bytecode.\n");
        return 1;
      }
      if (!firstRun) {
        vm_restart(c_vm);
      } else {
        mrbc_vm_begin(c_vm);
        firstRun = false;
      }
      mrbc_vm_run(c_vm);
      console_printf("=> ");
      mrbc_p_sub((const mrbc_value *)&c_vm->current_regs[p->scope->sp]);
#if defined(PICORBC_DEBUG)
      console_printf(" (reg_num: %d)", p->scope->sp);
#endif
      console_putchar('\n');

      lvar = p->scope->lvar;
      symbol = p->scope->symbol;
      literal = p->scope->literal;
      string_pool = p->current_string_pool;
      sp = p->scope->sp;
      max_sp = p->scope->max_sp;
      nlocals = p->scope->nlocals;
      p->current_string_pool = NULL;
      p->scope->lvar = NULL;
      p->scope->symbol = NULL;
      p->scope->literal = NULL;
    }
    StreamInterface_free(si);
    Compiler_parserStateFree(p);
  }
}

int
main(int argc, char *argv[])
{
  loglevel = LOGLEVEL_WARN;
  return start_irb();
}
