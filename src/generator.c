#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "picorbc.h"
#include "common.h"
#include "debug.h"
#include "scope.h"
#include "node.h"
#include "generator.h"
#include "mrubyc/src/opcode.h"
#include "ruby-lemon-parse/parse_header.h"
#include "ruby-lemon-parse/crc.c"

#define END_SECTION_SIZE 8

bool hasCar(Node *n) {
  if (n->type != CONS)
    return false;
  if (n->cons.car) {
    return true;
  }
  return false;
}

bool hasCdr(Node *n) {
  if (n->type != CONS)
    return false;
  if (n->cons.cdr) {
    return true;
  }
  return false;
}

typedef enum misc {
  NUM_POS,
  NUM_NEG
} Misc;

void codegen(Scope *scope, Node *tree);

void gen_self(Scope *scope)
{
  Scope_pushCode(OP_LOADSELF);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
}

/*
 *  n == 0 -> Condition nest
 *  n == 1 -> Block nest
 */
void push_nest_stack(Scope *scope, uint8_t n)
{
  if (scope->nest_stack & 0x80000000) {
    FATALP("Nest stack too deep!");
    return;
  }
  scope->nest_stack = (scope->nest_stack << 1) | (n & 1);
}

void pop_nest_stack(Scope *scope)
{
  scope->nest_stack = (scope->nest_stack >> 1);
}

Scope *scope_nest(Scope *scope)
{
  uint32_t nest_stack = scope->nest_stack;;
  scope = scope->first_lower;
  for (uint16_t i = 0; i < scope->upper->next_lower_number; i++) {
    scope = scope->next;
  }
  scope->upper->next_lower_number++;
  scope->nest_stack = nest_stack;
  push_nest_stack(scope, 1); /* 1 represents BLOCK NEST */
  return scope;
}

Scope *scope_unnest(Scope *scope)
{
  pop_nest_stack(scope->upper);
  return scope->upper;
}

int gen_values(Scope *scope, Node *tree)
{
  int nargs = 0;
  Node *node = tree;
  while (node != NULL) {
    if (hasCdr(node) && hasCar(node->cons.cdr) && Node_atomType(node->cons.cdr->cons.car) == ATOM_args_add) {
      nargs++;
    }
    if (node->cons.cdr != NULL) {
      node = node->cons.cdr->cons.car;
    } else {
      node = NULL;
    }
  }
  codegen(scope, tree->cons.cdr->cons.car);
  return nargs;
}

void gen_str(Scope *scope, Node *node)
{
  Scope_pushCode(OP_STRING);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  int litIndex = Scope_newLit(scope, Node_literalName(node), STRING_LITERAL);
  Scope_pushCode(litIndex);
}

void gen_sym(Scope *scope, Node *node)
{
  Scope_pushCode(OP_LOADSYM);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  int litIndex = Scope_newSym(scope, Node_literalName(node));
  Scope_pushCode(litIndex);
}

void gen_literal_numeric(Scope *scope, const char *num, LiteralType type, Misc pos_neg)
{
  Scope_pushCode(OP_LOADL);
  Scope_pushCode(scope->sp);
  int litIndex;
  if (pos_neg == NUM_NEG) {
    char negnum[strlen(num) + 1];
    negnum[0] = '-';
    litIndex = Scope_newLit(scope, strsafecat(negnum, num, strlen(num) + 2), type);
  } else {
     litIndex = Scope_newLit(scope, num, type);
  }
  Scope_pushCode(litIndex);
}

void cleanup_numeric_literal(char *lit)
{
  char result[strlen(lit) + 1];
  int j = 0;
  for (int i = 0; i <= strlen(lit); i++) {
    if (lit[i] == '_') continue;
    result[j] = lit[i];
    j++;
  }
  result[j] = '\0';
  /* Rewrite StingPool */
  memcpy(lit, result, strlen(result));
  lit[strlen(result)] = '\0';
}

void gen_float(Scope *scope, Node *node, Misc pos_neg)
{
  char *value = (char *)Node_valueName(node->cons.car);
  cleanup_numeric_literal(value);
  gen_literal_numeric(scope, (const char *)value, FLOAT_LITERAL, pos_neg);
  Scope_push(scope);
}

void gen_int(Scope *scope, Node *node, Misc pos_neg)
{
  char *value = (char *)Node_valueName(node->cons.car);
  cleanup_numeric_literal(value);
  unsigned long val;
  switch (value[1]) {
    case ('b'):
    case ('B'):
      val = strtol(value+2, NULL, 2);
      break;
    case ('o'):
    case ('O'):
      val = strtol(value+2, NULL, 8);
      break;
    case ('x'):
    case ('X'):
      val = strtol(value+2, NULL, 16);
      break;
    default:
      val = strtol(value, NULL, 10);
  }
  if (pos_neg == NUM_POS && 0 <= val && val <= 7) {
    Scope_pushCode(OP_LOADI_0 + val);
    Scope_pushCode(scope->sp);
  } else if (pos_neg == NUM_NEG && 0 <= val && val <= 1) {
    Scope_pushCode(OP_LOADI_0 - val);
    Scope_pushCode(scope->sp);
  } else if (val <= 0xff) {
    if (pos_neg == NUM_NEG) {
      Scope_pushCode(OP_LOADINEG);
    } else {
      Scope_pushCode(OP_LOADI);
    }
    Scope_pushCode(scope->sp);
    Scope_pushCode(val);
  } else if (val <= 0xffff) {
    Scope_pushCode(OP_EXT2);
    if (pos_neg == NUM_NEG) {
      Scope_pushCode(OP_LOADINEG);
    } else {
      Scope_pushCode(OP_LOADI);
    }
    Scope_pushCode(scope->sp);
    Scope_pushCode(val >> 8);
    Scope_pushCode(val & 0xff);
  } else {
    gen_literal_numeric(scope, (const char *)value, INTEGER_LITERAL, pos_neg);
  }
  Scope_push(scope);
}

void gen_call(Scope *scope, Node *node)
{
  int nargs = 0;
  int op = OP_SEND;
  if (node->cons.cdr->cons.car) {
    if (Node_atomType(node->cons.cdr->cons.car->cons.cdr->cons.car) == ATOM_block_arg) {
      /* .call(&:method) */
      codegen(scope, node->cons.cdr->cons.car->cons.cdr->cons.car->cons.cdr);
      Scope_pop(scope);
      op = OP_SENDB;
    } else {
      nargs = gen_values(scope, node);
      for (int i = 0; i < nargs; i++) {
        Scope_pop(scope);
      }
      if (nargs > 0 && Node_atomType(node->cons.cdr->cons.car->cons.cdr->cons.cdr->cons.car) == ATOM_block) {
        op = OP_SENDB;
        nargs--;
      }
    }
  }
  const char *method_name = Node_literalName(node->cons.car->cons.cdr);
  if (method_name[1] == '\0' && strchr("><+-*/", method_name[0]) != NULL) {
    switch (method_name[0]) {
      case '>': Scope_pushCode(OP_GT); break;
      case '<': Scope_pushCode(OP_LT); break;
      case '+': Scope_pushCode(OP_ADD); break;
      case '-': Scope_pushCode(OP_SUB); break;
      case '*': Scope_pushCode(OP_MUL); break;
      case '/': Scope_pushCode(OP_DIV); break;
      default: FATALP("This should not happen"); break;
    }
    Scope_pushCode(scope->sp - 1);
  } else if (strcmp(method_name, "==") == 0) {
    Scope_pushCode(OP_EQ);
    Scope_pushCode(scope->sp - 1);
  } else if (strcmp(method_name, ">=") == 0) {
    Scope_pushCode(OP_GE);
    Scope_pushCode(scope->sp - 1);
  } else if (strcmp(method_name, "<=") == 0) {
    Scope_pushCode(OP_LE);
    Scope_pushCode(scope->sp - 1);
  } else {
    Scope_pushCode(op);
    Scope_pushCode(scope->sp - 1);
    int symIndex = Scope_newSym(scope, method_name);
    Scope_pushCode(symIndex);
    Scope_pushCode(nargs);
  }
}

void gen_array(Scope *scope, Node *node)
{
  int nargs = 0;
  if (node->cons.cdr->cons.car) {
    nargs = gen_values(scope, node);
    for (int i = 0; i < nargs; i++) {
      Scope_pop(scope);
    }
  }
  Scope_pushCode(OP_ARRAY);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_pushCode(nargs);
}

void gen_hash(Scope *scope, Node *node)
{
  int nassocs = 0;
  Node *assoc = node;
  while (assoc) {
    codegen(scope, assoc->cons.car->cons.cdr);
    nassocs++;
    assoc = assoc->cons.cdr;
  }
  for (int i = 0; i < nassocs * 2; i++) {
    Scope_pop(scope);
  }
  Scope_pushCode(OP_HASH);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_pushCode(nassocs);
}

void gen_var(Scope *scope, Node *node)
{
  int num;
  LvarScopeReg lvar;
  switch(Node_atomType(node)) {
    case (ATOM_lvar):
      lvar = Scope_lvar_findRegnum(scope, Node_literalName(node->cons.cdr));
      if (lvar.scope_num > 0) {
        Scope_pushCode(OP_GETUPVAR);
        Scope_pushCode(scope->sp);
        Scope_push(scope);
        Scope_pushCode(lvar.reg_num);
        Scope_pushCode(lvar.scope_num - 1);
      } else {
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_push(scope);
        Scope_pushCode(lvar.reg_num);
      }
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      num = Scope_newSym(scope, Node_literalName(node->cons.cdr));
      switch (Node_atomType(node)) {
        case (ATOM_at_ivar):
          Scope_pushCode(OP_GETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_GETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_GETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      Scope_pushCode(num);
      break;
    default:
      break;
  }
}

void gen_assign(Scope *scope, Node *node)
{
  int num;
  LvarScopeReg lvar = {0, 0};
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      lvar = Scope_lvar_findRegnum(scope, Node_literalName(node->cons.car->cons.cdr));
      if (lvar.scope_num == 0) {
        num = lvar.reg_num;
        codegen(scope, node->cons.cdr);
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(num);
        Scope_pushCode(--scope->sp);
      } else {
        codegen(scope, node->cons.cdr);
        Scope_pushCode(OP_SETUPVAR);
        Scope_pushCode(--scope->sp);
        Scope_push(scope);
        Scope_pushCode(lvar.reg_num);
        Scope_pushCode(lvar.scope_num - 1);
      }
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      num = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr));
      codegen(scope, node->cons.cdr);
      switch(Node_atomType(node->cons.car)) {
        case (ATOM_at_ivar):
          Scope_pushCode(OP_SETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_SETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_SETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp - 1);
      Scope_pushCode(num);
      break;
    case ATOM_call:
      codegen(scope, node->cons.cdr->cons.car); /* right hand */
      codegen(scope, node->cons.car->cons.cdr->cons.car); /* left hand */
      Node *call_node = node->cons.car->cons.cdr->cons.cdr;
      int nargs = 0;
      if (call_node->cons.cdr->cons.car) {
        nargs = gen_values(scope, call_node);
      }
      Scope_pushCode(OP_MOVE);
      Scope_pushCode(scope->sp);
      for (int i = 0; i < nargs + 2; i++) {
        Scope_pop(scope);
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      Scope_pushCode(OP_SEND);
      Scope_pushCode(scope->sp);
      const char *method_name = Node_literalName(node->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr);
      int symIndex = Scope_assignSymIndex(scope, method_name);
      Scope_pushCode(symIndex);
      Scope_pushCode(nargs + 1);
      break;
    default:
      FATALP("error");
  }
}

void gen_op_assign(Scope *scope, Node *node)
{
  int num = 0;
  LvarScopeReg lvar = {0, 0};
  char *method_name = NULL;
  const char *call_name;
  { /* setup call_name here to avoid warning */
    if (Node_atomType(node->cons.car) == ATOM_call)
      call_name = Node_literalName(node->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr);
    else
      call_name = NULL;
  }
  bool is_call_name_at_ary = false;
  method_name = (char *)Node_literalName(node->cons.cdr->cons.car->cons.cdr);
  bool isANDOPorOROP = false; /* &&= or ||= */
  if (method_name[1] == '|' || method_name[1] == '&') {
    isANDOPorOROP = true;
  }
  JmpLabel *jmpLabel = NULL;
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      lvar = Scope_lvar_findRegnum(scope, Node_literalName(node->cons.car->cons.cdr));
      if (lvar.scope_num > 0) {
        Scope_push(scope);
        Scope_pushCode(OP_GETUPVAR);
        Scope_pushCode(scope->sp - 1);
        Scope_pushCode(lvar.reg_num);
        Scope_pushCode(lvar.scope_num - 1);
      } else {
        num = lvar.reg_num;
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_push(scope);
        Scope_pushCode(num);
      }
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      num = Scope_newSym(scope, Node_literalName(node->cons.car->cons.cdr));
      switch(Node_atomType(node->cons.car)) {
        case (ATOM_at_ivar):
          Scope_pushCode(OP_GETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_GETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_GETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      Scope_pushCode(num);
      break;
    case (ATOM_call):
      Scope_push(scope);
      if (!strcmp(call_name, "[]")) is_call_name_at_ary = true;
      codegen(scope, node->cons.car->cons.cdr->cons.car);
      node->cons.car->cons.cdr->cons.car = NULL;
      if (is_call_name_at_ary) {
        codegen(scope, node->cons.car->cons.cdr);
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_pushCode(scope->sp - 2);
        Scope_push(scope);
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_pushCode(scope->sp - 2);
        Scope_pushCode(OP_SEND);
        Scope_pushCode(scope->sp - 1);
        Scope_pushCode(Scope_newSym(scope, "[]"));
        Scope_pushCode(1);
      } else {
        Scope_pushCode(OP_MOVE);
        Scope_pushCode(scope->sp);
        Scope_pushCode(scope->sp - 1);
        Scope_push(scope);
        codegen(scope, node->cons.car);
      }
      break;
    default:
      FATALP("error");
  }
  /* right hand */
  if (!isANDOPorOROP) codegen(scope, node->cons.cdr->cons.cdr);
  switch (method_name[0]) {
    case '+':
      Scope_pushCode(OP_ADD);
      Scope_pop(scope);
      Scope_pop(scope);
      Scope_pushCode(scope->sp);
      break;
    case '-':
      Scope_pushCode(OP_SUB);
      Scope_pop(scope);
      Scope_pop(scope);
      Scope_pushCode(scope->sp);
      break;
    case '/':
      Scope_pushCode(OP_DIV);
      Scope_pop(scope);
      Scope_pop(scope);
      Scope_pushCode(scope->sp);
      break;
    case '%':
    case '^':
    case '&':
    case '|':
    case '<': /* <<= */
    case '>': /* >>= */
    case '*': /* *= and **= */
      if (!strcmp(method_name, "*=")) {
        Scope_pushCode(OP_MUL);
        Scope_pop(scope);
        Scope_pop(scope);
        Scope_pushCode(scope->sp);
      } else if (isANDOPorOROP) {
        switch (method_name[1]) {
          case '|':
            Scope_pushCode(OP_JMPIF);
            break;
          case '&':
            Scope_pushCode(OP_JMPNOT);
            break;
        }
        Scope_pushCode(--scope->sp);
        jmpLabel = Scope_reserveJmpLabel(scope);
        /* right condition of `___ &&= ___` */
        codegen(scope, node->cons.cdr);
        Scope_pop(scope);
      } else {
        Scope_pushCode(OP_SEND);
        Scope_pop(scope);
        Scope_pop(scope);
        Scope_pushCode(scope->sp);
        /*
         * method_name[strlen(method_name) - 1] = '\0';
         * 👆🐛
         * Because the same method_name may be reused
         */
        for (int i=1; i<strlen(method_name); i++) {
          if (method_name[i] == '=') method_name[i] = '\0';
        }
        Scope_pushCode(Scope_newSym(scope, (const char *)method_name));
        Scope_pushCode(1);
      }
      break;
    default:
      FATALP("error");
  }
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      if (lvar.scope_num > 0) {
        Scope_pushCode(OP_SETUPVAR);
        Scope_pushCode(scope->sp);
        Scope_pushCode(lvar.reg_num);
        Scope_pushCode(lvar.scope_num - 1);
      } else {
        Scope_pushCode(OP_MOVE);
      }
      break;
    case (ATOM_at_ivar):
      Scope_pushCode(OP_SETIV);
      break;
    case (ATOM_at_gvar):
      Scope_pushCode(OP_SETGV);
      break;
    case (ATOM_at_const):
      Scope_pushCode(OP_SETCONST);
      break;
    case (ATOM_call):
      /* right hand of assignment (mass-assign dosen't work) */
      Scope_pushCode(OP_MOVE);
      if (is_call_name_at_ary) {
        Scope_pushCode(scope->sp - 3);
      } else {
        Scope_pushCode(scope->sp - 2);
      }
      Scope_pushCode(scope->sp);
      if (!is_call_name_at_ary) gen_values(scope, node->cons.car->cons.cdr->cons.cdr);
      /* exec assignment .[]= or .attr= */
      Scope_pushCode(OP_SEND);
      Scope_pop(scope);
      if (is_call_name_at_ary) Scope_pop(scope);
      Scope_pushCode(scope->sp);
      Scope_pushCode(Scope_assignSymIndex(scope, call_name));
      /* count of args */
      if (is_call_name_at_ary) {
        Scope_pushCode(2); /* .[]= */
      } else {
        Scope_pushCode(1); /* .attr= */
      }
      break;
    default:
      FATALP("error");
  }
  switch(Node_atomType(node->cons.car)) {
    case (ATOM_lvar):
      if (lvar.scope_num > 0) break;
      Scope_pushCode(num);
      Scope_pushCode(scope->sp);
      break;
    case (ATOM_at_ivar):
    case (ATOM_at_gvar):
    case (ATOM_at_const):
      Scope_pushCode(scope->sp);
      Scope_pushCode(num);
      break;
    default:
      break;
  }
  /* goto label */
  if (isANDOPorOROP) Scope_backpatchJmpLabel(jmpLabel, scope->vm_code_size);
}

void gen_dstr(Scope *scope, Node *node)
{
  int num = 0; /* number of ATOM_dstr_add */
  Node *dstr = node->cons.cdr->cons.car;
  while (Node_atomType(dstr) != ATOM_dstr_new) {
    dstr = dstr->cons.cdr->cons.car;
    num++;
  }
  /* start from the bottom (== the first str) of the tree */
  int sp = scope->sp; /* copy */
  for (int count = num; 0 < count; count--) {
    dstr = node->cons.cdr->cons.car;
    for (int i = 1; i < count; i++) {
      dstr = dstr->cons.cdr->cons.car;
    }
    codegen(scope, dstr->cons.cdr->cons.cdr->cons.car);
    if (count != num) {
      /* TRICKY (I'm not sure if this is a correct way) */
// I don't remember why I wrote this :joy:
// TODO remove if it's OK
//      if (scope->sp > sp + 1) {
//        Scope_pushCode(OP_MOVE);
//        Scope_pushCode(sp + 1);
//        Scope_pushCode(scope->sp - 1);
//      }
      Scope_pushCode(OP_STRCAT);
      scope->sp = sp;
      Scope_pushCode(scope->sp);
      Scope_push(scope);
    }
  }
}

void gen_and_or(Scope *scope, Node *node, int opcode)
{
  /* left condition */
  codegen(scope, node->cons.car);
  Scope_pushCode(opcode); /* and->OP_JMPNOT, or->OP_JMPIF */
  Scope_pushCode(--scope->sp);
  JmpLabel *label = Scope_reserveJmpLabel(scope);
  /* right condition */
  codegen(scope, node->cons.cdr);
  /* goto label */
  Scope_backpatchJmpLabel(label, scope->vm_code_size);
}

void gen_case_when(Scope *scope, Node *node, int cond_reg, JmpLabel *label_true[])
{
  if (Node_atomType(node->cons.car) != ATOM_args_add) {
    return;
  } else {
    gen_case_when(scope, node->cons.car->cons.cdr, cond_reg, label_true + 1);
    scope->sp = cond_reg;
    codegen(scope, node->cons.car->cons.cdr->cons.cdr);
    Scope_pushCode(OP_MOVE);
    Scope_pushCode(scope->sp);
    Scope_pushCode(cond_reg - 1);
    Scope_pushCode(OP_SEND);
    Scope_pushCode(cond_reg);
    Scope_pushCode(Scope_newSym(scope, "==="));
    Scope_pushCode(1);
    /* when condition matched */
    Scope_pushCode(OP_JMPIF);
    Scope_pushCode(cond_reg);
    *label_true = Scope_reserveJmpLabel(scope);
    scope->sp = cond_reg;
  }
}

void gen_case(Scope *scope, Node *node)
{
Scope_pop(scope);
  /* count number of cases */
  Node *case_body = node->cons.cdr->cons.car;
  int i = 0;
  while (1) {
    if (case_body && case_body->cons.car) i++; else break;
    case_body = case_body->cons.cdr->cons.cdr->cons.car;
  }
  int when_count = i;
  JmpLabel *label_end_array[when_count];
  /* case expression */
  codegen(scope, node->cons.car);
  int cond_reg = scope->sp; /* cond_reg === when_expr */
  /* each case_body */
  case_body = node->cons.cdr->cons.car;
  i = 0;
  while (1) {
    /* when expression */
    int args_count = 0;
    {
      /* count number of args */
      Node *args_node = case_body;
      while (1) {
        if (Node_atomType(args_node->cons.car) != ATOM_args_add) break;
        args_count++;
        args_node = args_node->cons.car->cons.cdr;
      }
    }
    JmpLabel *label_true_array[args_count];
    gen_case_when(scope, case_body, cond_reg, label_true_array);
    /* when condition didn't match */
    Scope_pushCode(OP_JMP);
    JmpLabel *label_false = Scope_reserveJmpLabel(scope);
    /* content */
    for (int j = 0; j < args_count; j++)
      Scope_backpatchJmpLabel(label_true_array[j], scope->vm_code_size);
    { /* inside when */
      int32_t current_vm_code_size = scope->vm_code_size;
      codegen(scope, case_body->cons.cdr->cons.car);
      /* if code was empty */
      if (current_vm_code_size == scope->vm_code_size) {
        Scope_pushCode(OP_LOADNIL);
        Scope_pushCode(scope->sp);
        Scope_push(scope);
      }
    }
    Scope_pushCode(OP_JMP);
    label_end_array[i++] = Scope_reserveJmpLabel(scope);
    /* next case */
    Scope_backpatchJmpLabel(label_false, scope->vm_code_size);
    case_body = case_body->cons.cdr->cons.cdr->cons.car;
    Scope_pop(scope);
    if (case_body) {
      if (case_body->cons.car) {
        continue;
      } else {
        if (case_body->cons.cdr->cons.car) {
          /* else */
          codegen(scope, case_body->cons.cdr->cons.car);
          break;
        }
      }
    } else {
      /* no else clause */
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      break;
    }
  }
  for (i = 0; i < when_count; i++)
    Scope_backpatchJmpLabel(label_end_array[i], scope->vm_code_size);
}

void gen_if(Scope *scope, Node *node)
{
  int start_reg = scope->sp;
  /* assert condition */
  codegen(scope, node->cons.car);
  Scope_pushCode(OP_JMPNOT);
  Scope_pushCode(--scope->sp);
  JmpLabel *label_false = Scope_reserveJmpLabel(scope);
  /* condition true */
  codegen(scope, node->cons.cdr->cons.car);
  Scope_pop(scope);
//  Scope_pushCode(OP_MOVE);
//  Scope_pushCode(scope->sp);
//  Scope_pushCode(start_reg);
  Scope_pushCode(OP_JMP);
  JmpLabel *label_end = Scope_reserveJmpLabel(scope);
  /* condition false */
  Scope_backpatchJmpLabel(label_false, scope->vm_code_size);
  if (Node_atomType(node->cons.cdr->cons.cdr->cons.car) == ATOM_NONE) {
    /* right before KW_end */
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp);
    Scope_push(scope);
  } else {
    /* if_tail */
    codegen(scope, node->cons.cdr->cons.cdr->cons.car);
  }
  /* right after KW_end */
  Scope_backpatchJmpLabel(label_end, scope->vm_code_size);
  scope->sp = start_reg;
}

void gen_while(Scope *scope, Node *node, int op_jmp)
{
  push_nest_stack(scope, 0); /* 0 represents CONDITION NEST */
  Scope_pushBreakStack(scope);
  Scope_pushCode(OP_JMP);
  JmpLabel *label_cond = Scope_reserveJmpLabel(scope);
  scope->break_stack->redo_pos = scope->vm_code_size;
  /* inside while */
  uint32_t top = scope->vm_code_size;
  codegen(scope, node->cons.cdr);
  /* just before condition */
  Scope_backpatchJmpLabel(label_cond, scope->vm_code_size);
  /* condition */
  codegen(scope, node->cons.car);
  Scope_pushCode(op_jmp);
  Scope_pushCode(--scope->sp);
  JmpLabel *label_top = Scope_reserveJmpLabel(scope);
  Scope_backpatchJmpLabel(label_top, top);
  /* after while block */
  Scope_pushCode(OP_LOADNIL);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_popBreakStack(scope);
  pop_nest_stack(scope);
}

void gen_break(Scope *scope, Node *node)
{
  Scope_push(scope);
  codegen(scope, node);
  Scope_pop(scope);
  if (scope->nest_stack & 1) { /* BLOCK NEST */
    Scope_pushCode(OP_BREAK);
    Scope_pushCode(scope->sp);
  } else {                     /* CONDITION NEST */
    Scope_pushCode(OP_JMP);
    scope->break_stack->point = Scope_reserveJmpLabel(scope);
  }
}

void gen_next(Scope *scope, Node *node)
{
  Scope_push(scope);
  codegen(scope, node);
  Scope_pop(scope);
  if (scope->nest_stack & 1) { /* BLOCK NEST */
    Scope_pushCode(OP_RETURN);
    Scope_pushCode(scope->sp);
  } else {                     /* CONDITION NEST */
    Scope_pushCode(OP_JMP);
    JmpLabel *label = Scope_reserveJmpLabel(scope);
    Scope_backpatchJmpLabel(label, scope->break_stack->next_pos);
  }
}

void gen_redo(Scope *scope)
{
  Scope_push(scope);
  Scope_pushCode(OP_JMP);
  if (scope->nest_stack & 1) { /* BLOCK NEST */
    Scope_pushCode(0);
    Scope_pushCode(0);
  } else {                     /* CONDITION NEST */
    JmpLabel *label = Scope_reserveJmpLabel(scope);
    Scope_backpatchJmpLabel(label, scope->break_stack->redo_pos);
  }
}

uint32_t setup_parameters(Scope *scope, Node *node)
{
  if (Node_atomType(node) != ATOM_block_parameters) return 0;
  uint32_t bbb = 0;
  { /* mandatory args */
    uint8_t nmargs = gen_values(scope, node->cons.cdr->cons.car);
    nmargs <<= 2;
    bbb = (uint32_t)nmargs << 16;
  }
  /* TODO: rest, tail, etc. */
  Node *tailargs = node->cons.cdr->cons.cdr->cons.cdr->cons.cdr->cons.car;
  Node *args_tail = tailargs->cons.cdr->cons.car;
  if (Node_atomType(args_tail) != ATOM_args_tail) return bbb;
  { /* block */
    if (args_tail->cons.cdr->cons.cdr->cons.cdr->cons.car->value.name)
      bbb += 1;
  }
  return bbb;
}

void gen_irep(Scope *scope, Node *node)
{
  scope = scope_nest(scope);
  uint32_t bbb = setup_parameters(scope, node->cons.car);
  Scope_pushCode(OP_ENTER);
  Scope_pushCode((int)(bbb >> 16 & 0xFF));
  Scope_pushCode((int)(bbb >> 8 & 0xFF));
  Scope_pushCode((int)(bbb & 0xFF));
  { /* inside def */
    int32_t current_vm_code_size = scope->vm_code_size;
    codegen(scope, node->cons.cdr->cons.car);
    /* if code was empty */
    if (current_vm_code_size == scope->vm_code_size) {
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
    }
    Scope_pushCode(OP_RETURN);
    Scope_pushCode(--scope->sp);
  }
  Scope_finish(scope);
  scope = scope_unnest(scope);
}

void gen_block(Scope *scope, Node *node)
{
  Scope_pushCode(OP_BLOCK);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_pushCode(scope->next_lower_number);
  gen_irep(scope, node->cons.cdr);
}

void gen_def(Scope *scope, Node *node)
{
  Scope_pushCode(OP_TCLASS);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_pushCode(OP_METHOD);
  Scope_pushCode(scope->sp);
  Scope_pushCode(scope->next_lower_number);
  Scope_pushCode(OP_DEF);
  Scope_pushCode(--scope->sp);
  int litIndex = Scope_newSym(scope, Node_literalName(node));
  Scope_pushCode(litIndex);
  Scope_pushCode(OP_LOADSYM);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  Scope_pushCode(litIndex);

  gen_irep(scope, node->cons.cdr->cons.cdr);
}

void gen_class(Scope *scope, Node *node)
{
  int litIndex;
  Scope_pushCode(OP_LOADNIL);
  Scope_pushCode(scope->sp);
  Scope_push(scope);
  /*
   * TODO: `::Klass` `Klass::Glass`
   */
  if (Node_atomType(node->cons.cdr->cons.car) == ATOM_at_const) {
    Scope_pushCode(OP_GETCONST);
    Scope_pushCode(scope->sp--);
    litIndex = Scope_newSym(scope, Node_literalName(node->cons.cdr->cons.car->cons.cdr));
    Scope_pushCode(litIndex);
  } else {
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp--);
  }
  Scope_pushCode(OP_CLASS);
  Scope_pushCode(scope->sp);
  litIndex = Scope_newSym(scope, Node_literalName(node));
  Scope_pushCode(litIndex);

  if (node->cons.cdr->cons.cdr->cons.car->cons.cdr == NULL) {
    Scope_pushCode(OP_LOADNIL);
    Scope_pushCode(scope->sp);
    Scope_push(scope);
    scope->nlowers--;
  } else {
    node->cons.cdr->cons.car = NULL; /* Stop generating super class CONST */
    Scope_pushCode(OP_EXEC);
    Scope_pushCode(scope->sp);
    Scope_pushCode(scope->next_lower_number);

    scope = scope_nest(scope);
    codegen(scope, node->cons.cdr);
    Scope_pushCode(OP_RETURN);
    Scope_pushCode(--scope->sp);
    Scope_finish(scope);
    scope = scope_unnest(scope);
  }
}

void codegen(Scope *scope, Node *tree)
{
  int num;
  if (tree == NULL || Node_isAtom(tree)) return;
  switch (Node_atomType(tree)) {
    case ATOM_and:
      gen_and_or(scope, tree->cons.cdr, OP_JMPNOT);
      break;
    case ATOM_or:
      gen_and_or(scope, tree->cons.cdr, OP_JMPIF);
      break;
    case ATOM_case:
      gen_case(scope, tree->cons.cdr);
      break;
    case ATOM_if:
      gen_if(scope, tree->cons.cdr);
      break;
    case ATOM_while:
      gen_while(scope, tree->cons.cdr, OP_JMPIF);
      break;
    case ATOM_until:
      gen_while(scope, tree->cons.cdr, OP_JMPNOT);
      break;
    case ATOM_break:
      gen_break(scope, tree->cons.cdr);
      break;
    case ATOM_next:
      gen_next(scope, tree->cons.cdr);
      break;
    case ATOM_redo:
      gen_redo(scope);
      break;
    case ATOM_NONE:
      codegen(scope, tree->cons.car);
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_program:
      scope->nest_stack = 1; /* 00000000 00000000 00000000 00000001 */
      codegen(scope, tree->cons.cdr->cons.car);
      Scope_pop(scope);
      /* Prevent double return */
//      CodePool *pool = scope->current_code_pool;
      /* Prevention only works if `OP_RETURN Rn` aren't separated */
//      if (pool->index > 1)
//        if (pool->data[pool->index - 1] != scope->sp)
//          if (pool->data[pool->index - 2] != OP_RETURN) {
            Scope_pushCode(OP_RETURN);
            Scope_push(scope);
            Scope_pushCode(scope->sp);
//          }
      Scope_pushCode(OP_STOP);
      Scope_finish(scope);
      break;
    case ATOM_stmts_add:
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_stmts_new: // NEW_BEGIN
      break;
    case ATOM_assign:
      gen_assign(scope, tree->cons.cdr);
      break;
    case ATOM_op_assign:
      gen_op_assign(scope, tree->cons.cdr);
      break;
    case ATOM_command:
    case ATOM_fcall:
      gen_self(scope);
      gen_call(scope, tree->cons.cdr);
      break;
    case ATOM_call:
      codegen(scope, tree->cons.cdr->cons.car);
      gen_call(scope, tree->cons.cdr->cons.cdr);
      break;
    case ATOM_args_add:
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_args_new:
      codegen(scope, tree->cons.cdr);
      break;
    case ATOM_symbol_literal:
      gen_sym(scope, tree->cons.cdr);
      break;
    case ATOM_dsymbol:
      gen_dstr(scope, tree->cons.cdr->cons.car);
      Scope_pushCode(OP_INTERN);
      Scope_pushCode(scope->sp - 1);
      break;
    case ATOM_str:
      gen_str(scope, tree->cons.cdr);
      break;
    case ATOM_var_ref:
      gen_var(scope, tree->cons.cdr->cons.car);
      break;
    case ATOM_at_int:
      gen_int(scope, tree->cons.cdr, NUM_POS);
      break;
    case ATOM_at_float:
      gen_float(scope, tree->cons.cdr, NUM_POS);
      break;
    case ATOM_unary:
      switch (Node_atomType(tree->cons.cdr->cons.cdr->cons.car)) {
        case (ATOM_at_int):
          gen_int(scope, tree->cons.cdr->cons.cdr->cons.car->cons.cdr, NUM_NEG);
          break;
        case (ATOM_at_float):
          gen_float(scope, tree->cons.cdr->cons.cdr->cons.car->cons.cdr, NUM_NEG);
          break;
        default:
          break;
      }
      break;
    case ATOM_array:
      gen_array(scope, tree);
      break;
    case ATOM_hash:
      gen_hash(scope, tree->cons.cdr->cons.car);
      break;
    case ATOM_at_ivar:
    case ATOM_at_gvar:
    case ATOM_at_const:
      switch (Node_atomType(tree)) {
        case ATOM_at_ivar:
          Scope_pushCode(OP_GETIV);
          break;
        case (ATOM_at_gvar):
          Scope_pushCode(OP_GETGV);
          break;
        case (ATOM_at_const):
          Scope_pushCode(OP_GETCONST);
          break;
        default:
          FATALP("error");
      }
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      num = Scope_newSym(scope, Node_literalName(tree->cons.cdr));
      Scope_pushCode(num);
      break;
    case ATOM_lvar:
      gen_var(scope, tree);
      break;
    case ATOM_kw_nil:
      Scope_pushCode(OP_LOADNIL);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      break;
    case ATOM_kw_self:
      gen_self(scope);
      break;
    case ATOM_kw_true:
      Scope_pushCode(OP_LOADT);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      break;
    case ATOM_kw_false:
      Scope_pushCode(OP_LOADF);
      Scope_pushCode(scope->sp);
      Scope_push(scope);
      break;
    case ATOM_kw_return:
      if (tree->cons.cdr->cons.car) {
        codegen(scope, tree->cons.cdr->cons.car);
      } else {
        Scope_pushCode(OP_LOADNIL);
        Scope_pushCode(scope->sp);
        Scope_push(scope);
      }
      Scope_pushCode(OP_RETURN);
      Scope_pushCode(scope->sp - 1);
      break;
    case ATOM_dstr:
      gen_dstr(scope, tree);
      break;
    case ATOM_block:
      gen_block(scope, tree);
      break;
    case ATOM_arg:
      break;
    case ATOM_def:
      gen_def(scope, tree->cons.cdr);
      break;
    case ATOM_class:
      gen_class(scope, tree->cons.cdr);
      break;
    default:
//      FATALP("error");
      break;
  }
}

void memcpyFlattenCode(uint8_t *body, CodePool *code_pool)
{
  if (code_pool->next == NULL && code_pool->index == code_pool->size) return;
  int pos = 0;
  while (code_pool) {
    memcpy(&body[pos], code_pool->data, code_pool->index);
    pos += code_pool->index;
    code_pool = code_pool->next;
  }
}

#ifdef PICORBC_DEBUG
#include "dump.h"
#endif

uint8_t *writeCode(Scope *scope, uint8_t *pos)
{
  if (scope == NULL) return pos;
  memcpyFlattenCode(pos, scope->first_code_pool);
#ifdef PICORBC_DEBUG
  Dump_codeDump(pos);
#endif
  pos += scope->vm_code_size;
  pos = writeCode(scope->first_lower, pos);
  pos = writeCode(scope->next, pos);
  return pos;
}

void Generator_generate(Scope *scope, Node *root)
{
  codegen(scope, root);
  int irepSize = Scope_updateVmCodeSizeThenReturnTotalSize(scope);
  int32_t codeSize = HEADER_SIZE + irepSize + END_SECTION_SIZE;
  uint8_t *vmCode = picorbc_alloc(codeSize);
  memcpy(&vmCode[0], "RITE0006", 8);
  vmCode[10] = (codeSize >> 24) & 0xff;
  vmCode[11] = (codeSize >> 16) & 0xff;
  vmCode[12] = (codeSize >> 8) & 0xff;
  vmCode[13] = codeSize & 0xff;
  memcpy(&vmCode[14], "MATZ0000", 8);
  memcpy(&vmCode[22], "IREP", 4);
  int sectionSize = irepSize + END_SECTION_SIZE + 4;
  vmCode[26] = (sectionSize >> 24) & 0xff; // size of the section
  vmCode[27] = (sectionSize >> 16) & 0xff;
  vmCode[28] = (sectionSize >> 8) & 0xff;
  vmCode[29] = sectionSize & 0xff;
  memcpy(&vmCode[30], "0002", 4); // instruction version
  writeCode(scope, &vmCode[HEADER_SIZE]);
  memcpy(&vmCode[HEADER_SIZE + irepSize], "END\0\0\0\0", 7);
  vmCode[codeSize - 1] = 0x08;
  uint16_t crc = calc_crc_16_ccitt(&vmCode[10], codeSize - 10, 0);
  vmCode[8] = (crc >> 8) & 0xff;
  vmCode[9] = crc & 0xff;
  scope->vm_code = vmCode;
  scope->vm_code_size = codeSize;
}
