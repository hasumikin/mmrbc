#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ruby-lemon-parse/parse.h"
#include "debug.h"
#include "common.h"
#include "token.h"

Token *Token_new(void)
{
  Token *self = (Token *)picorbc_alloc(sizeof(Token));
  self->value = NULL;
  self->type = ON_NONE;
  self->prev = NULL;
  self->next = NULL;
  self->refCount = 1;
  return self;
}

void Token_free(Token* self)
{
  if (self->value != NULL) {
    picorbc_free(self->value);
    DEBUGP("free Token->value: `%s`", self->value);
  }
  DEBUGP("free Token: %p", self);
  picorbc_free(self);
}

bool Token_exists(Token* const self)
{
  if (strlen(self->value) > 0)
    return true;
  return false;
}

void Token_GC(Token* token)
{
  if (token == NULL || token->refCount > 0) return;
  Token_GC(token->prev);
  token->next->prev = NULL;
  Token_free(token);
}
