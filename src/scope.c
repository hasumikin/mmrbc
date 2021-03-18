#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "scope.h"

#define IREP_HEADER_SIZE 14

void generateCodePool(Scope *self, uint16_t size)
{
  CodePool *pool = (CodePool *)picorbc_alloc(sizeof(CodePool) - IREP_HEADER_SIZE + size);
  pool->size = size;
  pool->index = 0;
  pool->next = NULL;
  if (self->current_code_pool)
    self->current_code_pool->next = pool;
  self->current_code_pool = pool;
}

Scope *Scope_new(Scope *upper, bool lvar_top)
{
  Scope *self = picorbc_alloc(sizeof(Scope));
  self->next_lower_number = 0;
  self->upper = upper;
  self->first_lower = NULL;
  self->lvar_top = lvar_top;
  self->nlowers = 0;
  self->next = NULL;
  if (upper != NULL) {
    if (upper->first_lower == NULL) {
      upper->first_lower = self;
    } else {
      Scope *prev = upper->first_lower;
      for (int i = 1; i < upper->nlowers; i++) prev = prev->next;
      prev->next = self;
    }
    upper->nlowers++;
  }
  self->current_code_pool = NULL;
  generateCodePool(self, IREP_HEADER_SIZE);
  self->first_code_pool = self->current_code_pool;
  self->first_code_pool->index = IREP_HEADER_SIZE;
  self->nlocals = 1;
  self->symbol = NULL;
  self->lvar = NULL;
  self->literal = NULL;
  self->sp = 1;
  self->max_sp = 1;
  self->vm_code = NULL;
  self->vm_code_size = 0;
  self->break_stack = NULL;
  self->last_assign_symbol = NULL;
  return self;
}

void freeLiteralRcsv(Literal *literal)
{
  if (literal == NULL) return;
  freeLiteralRcsv(literal->next);
  picorbc_free(literal);
}

void freeSymbolRcsv(Symbol *symbol)
{
  if (symbol == NULL) return;
  freeSymbolRcsv(symbol->next);
  picorbc_free(symbol);
}

void freeLvarRcsv(Lvar *lvar)
{
  if (lvar == NULL) return;
  freeLvarRcsv(lvar->next);
  picorbc_free(lvar);
}

void freeAssignSymbol(AssignSymbol *assign_symbol)
{
  AssignSymbol *prev;
  while (assign_symbol) {
    prev = assign_symbol->prev;
    picorbc_free((void *)assign_symbol->value);
    picorbc_free(assign_symbol);
    assign_symbol = prev;
  }
}

void Scope_free(Scope *self)
{
  if (self == NULL) return;
  Scope_free(self->next);
  Scope_free(self->first_lower);
  freeLiteralRcsv(self->literal);
  freeSymbolRcsv(self->symbol);
  freeLvarRcsv(self->lvar);
  freeAssignSymbol(self->last_assign_symbol);
  if (self->vm_code != NULL) {
    picorbc_free(self->vm_code);
  }
  picorbc_free(self);
}

void Scope_pushNCode_self(Scope *self, uint8_t *str, int size)
{
  CodePool *pool;
  if (size > CODE_POOL_SIZE)
    generateCodePool(self, size);
  if (self->current_code_pool->index + size > self->current_code_pool->size)
    generateCodePool(self, CODE_POOL_SIZE);
  pool = self->current_code_pool;
  memcpy(&pool->data[pool->index], str, size);
  pool->index += size;
  self->vm_code_size = self->vm_code_size + size;
}


void Scope_pushCode_self(Scope *self, int val)
{
  uint8_t str[1];
  str[0] = (uint8_t)val;
  Scope_pushNCode_self(self, str, 1);
}

Literal *literal_new(const char *value, LiteralType type)
{
  Literal *literal = picorbc_alloc(sizeof(Literal));
  literal->next = NULL;
  literal->type = type;
  literal->value = value;
  return literal;
}

/*
 * returns -1 if literal was not found
 */
int literal_findIndex(Literal *literal, const char *value, LiteralType type)
{
  int i = 0;
  while (literal != NULL) {
    if (literal->type == type && strcmp(literal->value, value) == 0) {
      return i;
    }
    literal = literal->next;
    i++;
  }
  return -1;
}

int Scope_newLit(Scope *self, const char *value, LiteralType type){
  int index = literal_findIndex(self->literal, value, type);
  if (index >= 0) return index;
  Literal *newLit = literal_new(value, type);
  Literal *lit = self->literal;
  if (lit == NULL) {
    self->literal = newLit;
    return 0;
  }
  for (index = 1; ; index++) {
    if (lit->next == NULL) break;
    lit = lit->next;
  }
  lit->next = newLit;
  return index;
}

Symbol *symbol_new(const char *value)
{
  Symbol *symbol = picorbc_alloc(sizeof(Symbol));
  symbol->next = NULL;
  symbol->value = value;
  return symbol;
}

Lvar *lvar_new(const char *name, int regnum)
{
  Lvar *lvar = picorbc_alloc(sizeof(Lvar));
  lvar->regnum = regnum;
  lvar->next = NULL;
  lvar->name = name;
  return lvar;
}

/*
 * returns -1 if symbol was not found
 */
int symbol_findIndex(Symbol *symbol, const char *value)
{
  int i = 0;
  while (symbol != NULL) {
    if (strcmp(symbol->value, value) == 0) {
      return i;
    }
    symbol = symbol->next;
    i++;
  }
  return -1;
}

int Scope_newSym(Scope *self, const char *value){
  int index = symbol_findIndex(self->symbol, value);
  if (index >= 0) return index;
  Symbol *newSym = symbol_new(value);
  Symbol *sym = self->symbol;
  if (sym == NULL) {
    self->symbol = newSym;
    return 0;
  }
  for (index = 1; ; index++) {
    if (sym->next == NULL) break;
    sym = sym->next;
  }
  sym->next = newSym;
  return index;
}

int Scope_assignSymIndex(Scope *self, const char *method_name)
{
  size_t length = strlen(method_name);
  char *assign_method_name = picorbc_alloc(length + 2);
  memcpy(assign_method_name, method_name, length);
  assign_method_name[length] = '=';
  assign_method_name[length + 1] = '\0';
  int symIndex = symbol_findIndex(self->symbol, assign_method_name);
  if (symIndex < 0) {
    symIndex = Scope_newSym(self, (const char *)assign_method_name);
    AssignSymbol *assign_symbol = picorbc_alloc(sizeof(AssignSymbol));
    assign_symbol->prev = NULL;
    assign_symbol->value = assign_method_name;
    assign_symbol->prev = self->last_assign_symbol;
    self->last_assign_symbol = assign_symbol;
  } else {
    picorbc_free(assign_method_name);
  }
  return symIndex;
}

/*
 * reg_num = 0 if lvar was not found
 */
LvarScopeReg Scope_lvar_findRegnum(Scope *self, const char *name)
{
  LvarScopeReg scopeReg = {0, 0};
  Scope *scope = self;
  Lvar *lvar;
  do {
    lvar = scope->lvar;
    while (lvar != NULL) {
      if (strcmp(lvar->name, name) == 0) {
        scopeReg.reg_num = lvar->regnum;
        return scopeReg;
      }
      lvar = lvar->next;
    }
    if (scope->upper == NULL) break;
    scopeReg.scope_num++;
    scope = scope->upper;
  } while (scope->lvar_top);
  return (LvarScopeReg){0, 0};
}

int Scope_newLvar(Scope *self, const char *name, int newRegnum){
  Lvar *newLvar = lvar_new(name, newRegnum);
  if (self->lvar == NULL) {
    self->lvar = newLvar;
  } else {
    Lvar *lvar = self->lvar;
    while (lvar->next) {
      lvar = lvar->next;
    }
    lvar->next = newLvar;
  }
  return newRegnum;
}

void Scope_push(Scope *self){
  self->sp++;
  if (self->max_sp < self->sp) self->max_sp = self->sp;
}

void Scope_pop(Scope *self){
  self->sp--;
}

int scope_codeSize(CodePool *code_pool)
{
  int size = 0;
  while (code_pool != NULL) {
    size += code_pool->index;
    code_pool = code_pool->next;
  }
  return size;
}

#define PICORUBYNULL "PICORUBYNULL\xF5"
/*
 * Replace back to null letter
 *  -> see REPLACE_NULL_PICORUBY, too
 */
size_t replace_picoruby_null(char *value)
{
  int i = 0;
  int j = 0;
  char j_value[strlen(value) + 1];
  while (value[i]) {
    if (value[i] != '\xF5') {
      j_value[j] = value[i];
    } else if (strncmp(value+i+1, PICORUBYNULL, sizeof(PICORUBYNULL)-1) == 0) {
      j_value[j] = '\0';
      i += sizeof(PICORUBYNULL) - 1;
    }
    i++;
    j++;
  }
  j_value[j] = '\0';
  if (j < i) memcpy(value, j_value, j);
  return j;
}

void Scope_finish(Scope *scope)
{
  int op_size = scope->vm_code_size;
  int count;
  int len;
  uint8_t *data = scope->first_code_pool->data;
  // literal
  Literal *lit;
  count = 0;
  lit = (Literal *)scope->literal;
  while (lit != NULL) {
    count++;
    lit = lit->next;
  }
  Scope_pushCode((count >> 24) & 0xff);
  Scope_pushCode((count >> 16) & 0xff);
  Scope_pushCode((count >> 8) & 0xff);
  Scope_pushCode(count & 0xff);
  lit = scope->literal;
  while (lit != NULL) {
    Scope_pushCode(lit->type);
    len = replace_picoruby_null((char *)lit->value);
    Scope_pushCode((len >>8) & 0xff);
    Scope_pushCode(len & 0xff);
    Scope_pushNCode((uint8_t *)lit->value, len);
    lit = lit->next;
  }
  // symbol
  Symbol *sym;
  count = 0;
  sym = scope->symbol;
  while (sym != NULL) {
    count++;
    sym = sym->next;
  }
  Scope_pushCode((count >> 24) & 0xff);
  Scope_pushCode((count >> 16) & 0xff);
  Scope_pushCode((count >> 8) & 0xff);
  Scope_pushCode(count & 0xff);
  sym = scope->symbol;
  while (sym != NULL) {
    len = strlen(sym->value);
    Scope_pushCode((len >>8) & 0xff);
    Scope_pushCode(len & 0xff);
    Scope_pushNCode((uint8_t *)sym->value, len);
    Scope_pushCode(0); // NULL terminate? FIXME
    sym = sym->next;
  }
  // irep header
  // record length. but whatever it works because of mruby's bug
  //memcpy(&data[0], "\0\0\0\0", 4);
  { // monkey patch for migrating mruby3. see mrubyc/src/load.c
    data[0] = 0xff;
    data[1] = 0xff;
    data[2] = 0xff;
    data[3] = 0xff;
  }
  int l = scope->nlocals;
  data[4] = (l >> 8) & 0xff;
  data[5] = l & 0xff;
  l = scope->max_sp + 1; // RIGHT? FIXME
  data[6] = (l >> 8) & 0xff;
  data[7] = l & 0xff;
  l = scope->nlowers;
  data[8] = (l >> 8) & 0xff;
  data[9] = l & 0xff;
  data[10] = (op_size >> 24) & 0xff;
  data[11] = (op_size >> 16) & 0xff;
  data[12] = (op_size >> 8) & 0xff;
  data[13] = op_size & 0xff;
}

void freeCodePool(CodePool *pool)
{
  CodePool *next ;
  while (1) {
    next = pool->next;
    picorbc_free(pool);
    if (next == NULL) break;
    pool = next;
  }
}

void Scope_freeCodePool(Scope *self)
{
  if (self == NULL) return;
  Scope_freeCodePool(self->next);
  Scope_freeCodePool(self->first_lower);
  freeCodePool(self->first_code_pool);
}

JmpLabel *Scope_reserveJmpLabel(Scope *scope)
{
  Scope_pushNCode((uint8_t *)"\0\0", 2);
  return (void *)&scope->current_code_pool->data[scope->current_code_pool->index - 2];
}

void Scope_backpatchJmpLabel(void *label, int32_t position)
{
  uint8_t *data = (uint8_t *)label;
  data[0] = (position >> 8) & 0xff;
  data[1] = position & 0xff;
}

void Scope_pushBreakStack(Scope *self)
{
  BreakStack *break_stack = picorbc_alloc(sizeof(BreakStack));
  break_stack->point = NULL;
  break_stack->next_pos = self->vm_code_size;
  if (self->break_stack) {
    break_stack->prev = self->break_stack;
  } else {
    break_stack->prev = NULL;
  }
  self->break_stack = break_stack;
}

void Scope_popBreakStack(Scope *self)
{
  BreakStack *memo = self->break_stack;
  if (self->break_stack->point)
    Scope_backpatchJmpLabel(self->break_stack->point, self->vm_code_size);
  self->break_stack = self->break_stack->prev;
  picorbc_free(memo);
}

int Scope_updateVmCodeSizeThenReturnTotalSize(Scope *self)
{
  int totalSize = 0;
  if (self == NULL) return 0;
  /* check if it's an empty child scope like `class A; #HERE; end` */
  if (self->first_code_pool->next == NULL &&
      self->first_code_pool->index == IREP_HEADER_SIZE) return 0;
  totalSize += Scope_updateVmCodeSizeThenReturnTotalSize(self->first_lower);
  totalSize += Scope_updateVmCodeSizeThenReturnTotalSize(self->next);
  self->vm_code_size = scope_codeSize(self->first_code_pool);
  totalSize += self->vm_code_size;
  return totalSize;
}

