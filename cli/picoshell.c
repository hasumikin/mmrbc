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
#include "picoshell_lib/shell.c"

int loglevel;

int
init_hal_fd(const char *pathname)
{
  int fd = hal_open(pathname, O_RDWR);
  //fd = open(pathname, O_RDWR|O_NONBLOCK);
  if (fd < 0) {
    FATALP("hal_open failed");
    return 1;
  }
  hal_set_fd(fd);
  return 0;
}

void
init_shell_interrupt(void (*handler)(int))
{
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGUSR1);
  struct sigaction act;
  memset(&act, 0, sizeof(struct sigaction));
  act.sa_handler = handler;
  act.sa_mask    = sigset;
  act.sa_flags  |= SA_RESTART;
  sigaction(SIGUSR1, &act, 0);
}

static mrbc_tcb *tcb_shell;

/*
 * int no; dummy to avoid warning. it's originally a signal number.
 */
static
void
resume_shell(int no) {
  if (tcb_shell == NULL) return;
  mrbc_resume_task(tcb_shell);
}

static void
c_is_fd_empty(mrbc_vm *vm, mrbc_value *v, int argc)
{
  fd_set readfds;
  FD_ZERO(&readfds);
  hal_FD_SET(1, &readfds);
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 1;
  if (hal_select(1, &readfds, NULL, NULL, &tv) == 0) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
}

static void
c_getc(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char c[1];
  int len;
  len = hal_read(1, c, 1);
  if (len < 0) {
    FATALP("read");
    SET_NIL_RETURN();
  } else if (len == 0) {
    SET_NIL_RETURN();
  } else {
    mrbc_value val = mrbc_string_new(vm, c, 1);
    SET_RETURN(val);
  }
}

#define BUFLEN 100

static void
c_gets(mrbc_vm *vm, mrbc_value *v, int argc)
{
  char *buf = picorbc_alloc(BUFLEN + 1);
  int len;
  len = hal_read(1, buf, BUFLEN);
  if (len == -1) {
    FATALP("read");
    SET_NIL_RETURN();
  } else if (len) {
    buf[len] = '\0'; // terminate;
    mrbc_value val = mrbc_string_new(vm, buf, len);
    SET_RETURN(val);
  } else {
    FATALP("This should not happen (c_gets)");
  }
  picorbc_free(buf);
}

static void
c_pid(mrbc_vm *vm, mrbc_value *v, int argc)
{
  SET_INT_RETURN(getpid());
}

static void
c_exit_shell(mrbc_vm *vm, mrbc_value *v, int argc)
{
  /*
   * PENDING
   *
   * you can not call q_delete_task() as it is static function in rrt0.c
   *
  q_delete_task(tcb_shell);
  */
}

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

static struct VM *c_vm;

static bool firstRun = true;

void vm_run(uint8_t *mrb)
{
  if (firstRun) {
    c_vm = mrbc_vm_open(NULL);
    if(c_vm == NULL) {
      hal_write(1, "Error: Can't open VM.\r\n", 23);
      return;
    }
  }
  if(mrbc_load_mrb(c_vm, mrb) != 0) {
    hal_write(1, "Error: Illegal bytecode.\r\n", 26);
    return;
  }
  if (firstRun) {
    mrbc_vm_begin(c_vm);
    firstRun = false;
  } else {
    vm_restart(c_vm);
  }
  mrbc_vm_run(c_vm);
}

static ParserState *p;

static Lvar *lvar = NULL;
static Symbol *symbol = NULL;
static Literal *literal = NULL;
static StringPool *string_pool = NULL;
static unsigned int sp = 1;
static unsigned int max_sp = 1;
static unsigned int nlocals = 0;

#define NODE_BOX_SIZE 30

static void
c_compile(mrbc_vm *vm, mrbc_value *v, int argc)
{
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
  StreamInterface *si = StreamInterface_new((char *)GET_STRING_ARG(1), STREAM_TYPE_MEMORY);
  if (Compiler_compile(p, si)) {
    SET_TRUE_RETURN();
  } else {
    SET_FALSE_RETURN();
  }
  StreamInterface_free(si);
}

static void
c_execute_vm(mrbc_vm *vm, mrbc_value *v, int argc)
{
  vm_run(p->scope->vm_code);
  SET_RETURN((mrbc_value)c_vm->current_regs[p->scope->sp]);
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
  Compiler_parserStateFree(p);
}

#define FREE_HEADER "          total       used       free       frag\r\n"
#define FREE_DOES_NOT_WORK "free() doesn't work on production build\r\n"
static void
c_free(mrbc_vm *vm, mrbc_value *v, int argc)
{
#if defined(PICORBC_DEBUG) && !defined(MRBC_ALLOC_LIBC)
  int total;
  int used;
  int free;
  int fragmentation;
  mrbc_alloc_statistics(&total, &used, &free, &fragmentation);
  char result[128];
  hal_write(1, FREE_HEADER, strlen(FREE_HEADER));
  snprintf(result, 128, "Mem: %10d %10d %10d %10d\r\n", total, used, free, fragmentation);
  hal_write(1, result, strlen(result));
//  mrbc_alloc_print_memory_pool();
#else
  hal_write(1, FREE_DOES_NOT_WORK, strlen(FREE_DOES_NOT_WORK));
#endif
  SET_NIL_RETURN();
}

void
process_parent(pid_t pid)
{
  WARNP("successfully forked. parent pid: %d", pid);
  fd_set readfds;
  FD_ZERO(&readfds);
  hal_FD_SET(1, &readfds);
  struct timespec ts;
  ts.tv_sec = 0;
  ts.tv_nsec = 1000;
  int ret;
  for (;;) {
    ret = hal_select(1, &readfds, NULL, NULL, NULL);
    if (ret == -1) {
      FATALP("select");
    } else if (ret == 0) {
      FATALP("This should not happen (1)");
    } else if (ret > 0) {
      INFOP("Input recieved. Issuing SIGUSR1 to pid %d", pid);
      kill(pid, SIGUSR1);
      ret = clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);
      if (ret) FATALP("clock_nanosleep");
    }
  }
}

static uint8_t heap[HEAP_SIZE];

void
process_child(void)
{
  WARNP("successfully forked. child");
  mrbc_init(heap, HEAP_SIZE);
  mrbc_define_method(0, mrbc_class_object, "free", c_free);
  mrbc_define_method(0, mrbc_class_object, "compile", c_compile);
  mrbc_define_method(0, mrbc_class_object, "execute_vm", c_execute_vm);
  mrbc_define_method(0, mrbc_class_object, "fd_empty?", c_is_fd_empty);
  mrbc_define_method(0, mrbc_class_object, "gets", c_gets);
  mrbc_define_method(0, mrbc_class_object, "getc", c_getc);
  mrbc_define_method(0, mrbc_class_object, "pid", c_pid);
  mrbc_define_method(0, mrbc_class_object, "exit_shell", c_exit_shell);
  //mrbc_define_method(0, mrbc_class_object, "xmodem", c_xmodem);
  tcb_shell = mrbc_create_task(shell, 0);
  mrbc_run();
  if (close(hal_fd) == -1) {
    FATALP("close");
    return;
  }
}

int
main(int argc, char *argv[])
{
  loglevel = LOGLEVEL_WARN;
  if (init_hal_fd(argv[1]) != 0) {
    return 1;
  }
  init_shell_interrupt(resume_shell);
  pid_t pid = fork();
  if (pid < 0) {
    FATALP("fork failed");
    return 1;
  } else if (pid == 0) {
    process_child();
  } else if (pid > 0) {
    process_parent(pid);
  }
  return 0;
}
