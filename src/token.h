#ifndef PICORBC_TOKEN_H_
#define PICORBC_TOKEN_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum state
{
  EXPR_NONE    = 0b00000000000000,
  EXPR_BEG     = 0b00000000000001, /* beginning of an expr. right after \n ( { [ ! ? : , op= etc. ignore newline, +/- is a sign. */
  EXPR_VALUE   = 0b00000000000010,
  EXPR_END     = 0b00000000000100, /* able to be terminal of a statement --eg: after literal or )-- except when EXPR_ENDARG. newline significant, +/- is a operator. */
  EXPR_ENDARG  = 0b00000000001000, /* special case of EXPR_END. right after RPAREN which corresponds to tLPAREN_ARG. newline significant, +/- is a operator. */
  EXPR_ENDFN   = 0b00000000010000,
  EXPR_END_ANY = 0b00000000011100,
  EXPR_ARG     = 0b00000000100000, /* possibly right after method name of right after [ excpept when EXPR_CMDARG. newline significant, +/- is a operator. */
  EXPR_CMDARG  = 0b00000001000000, /* expecting the first argument of a normal method. newline significant, +/- is a operator. */
  EXPR_ARG_ANY = 0b00000001100000,
  EXPR_MID     = 0b00000010000000, /* right after return break rescue. binary op like * & will be invalid. newline significant, +/- is a operator. */
  EXPR_FNAME   = 0b00000100000000, /* right after def, alias, undef, : of a symbol. ` becomes a name. ignore newline, no reserved words. */
  EXPR_DOT     = 0b00001000000000, /* right after `.' or `::', no reserved words. */
  EXPR_CLASS   = 0b00010000000000, /* immediate after `class', no here document. */
  EXPR_BEG_ANY = 0b00010010000010,
  EXPR_LABEL   = 0b00100000000000,
  EXPR_LABELED = 0b01000000000000,
  EXPR_FITEM   = 0b10000000000000
} State;

typedef uint8_t Type; //defined in parse.h

typedef struct token
{
  int line_num;
  int pos;
  Type type;
  State state;
  struct token *prev;
  struct token *next;
  char *value;
  int refCount;
} Token;

Token *Token_new(void);

void Token_free(Token *self);

void Token_GC(Token *currentToken);

bool Token_exists(Token* const self);

#endif
