#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "picorbc.h"
#include "ruby-lemon-parse/parse.h"
#include "ruby-lemon-parse/keyword_helper.h"
#include "ruby-lemon-parse/token_helper.h"
#include "common.h"
#include "tokenizer.h"
#include "token.h"
#include "my_regex.h"

#include "token_data.h"

#define IS_ARG() (self->state == EXPR_ARG || self->state == EXPR_CMDARG)
#define IS_END() (self->state == EXPR_END || self->state == EXPR_ENDARG || self->state == EXPR_ENDFN)
#define IS_BEG() (self->state & EXPR_BEG || self->state == EXPR_MID || self->state == EXPR_VALUE || self->state == EXPR_CLASS)

#define IS_NUM(n) ('0' <= self->line[self->pos+(n)] && self->line[self->pos+(n)] <= '9')

static bool tokenizer_is_operator(char *word, size_t len)
{
  switch (len) {
    case 3:
      for (int i = 0; OPERATORS_3[i].string != NULL; i++){
        if ( !strncmp(word, OPERATORS_3[i].string, len) )
          return true;
      }
      break;
    case 2:
      for (int i = 0; OPERATORS_2[i].string != NULL; i++){
        if ( !strncmp(word, OPERATORS_2[i].string, len) )
          return true;
      }
      break;
    default:
      ERRORP("Out of range!");
      break;
  }
  return false;
}

static bool tokenizer_is_paren(int letter)
{
  for (int i = 0; PARENS[i].letter != 0; i++){
    if ( letter == PARENS[i].letter )
      return true;
  }
  return false;
}

static bool tokenizer_is_semicolon(int letter)
{
  for (int i = 0; SEMICOLONS[i].letter != 0; i++){
    if ( letter == SEMICOLONS[i].letter )
      return true;
  }
  return false;
}

static bool tokenizer_is_comma(int letter)
{
  for (int i = 0; COMMAS[i].letter != 0; i++){
    if ( letter == COMMAS[i].letter )
      return true;
  }
  return false;
}

static void tokenizer_paren_stack_pop(Tokenizer* const self)
{
  self->paren_stack_num--;
  DEBUGP("Paren popped stack_num: %d", self->paren_stack_num);
}

static void tokenizer_paren_stack_add(Tokenizer* const self, Paren paren)
{
  self->paren_stack_num++;
  self->paren_stack[self->paren_stack_num] = paren;
  DEBUGP("Paren added stack_num: %d, `%d`", self->paren_stack_num, paren);
}

Tokenizer* const Tokenizer_new(ParserState *p, StreamInterface *si)
{
  Tokenizer *self = (Tokenizer *)picorbc_alloc(sizeof(Tokenizer));
  self->p = p;
  { /* class vars in mmrbc.gem */
    self->currentToken = Token_new();
    self->paren_stack_num = -1;
    self->line = (char *)picorbc_alloc(sizeof(char) * (MAX_LINE_LENGTH));
    self->line[0] = '\0';
    self->line_num = 0;
    self->pos = 0;
  }
  tokenizer_paren_stack_add(self, PAREN_NONE);
  { /* instance vars in mmrbc.gem */
    self->si = si;
    self->mode = MODE_NONE;
    self->modeTerminater = '\0';
    self->state = EXPR_BEG;
    p->cmd_start = true;
  }
  return self;
}

void Tokenizer_free(Tokenizer *self)
{
  Token_free(self->currentToken);
  picorbc_free(self->line);
  picorbc_free(self);
}

void tokenizer_readLine(Tokenizer* const self)
{
  DEBUGP("readLine line_num: %d, pos: %d", self->line_num, self->pos);
  if (self->pos >= strlen(self->line)) {
    if (self->si->fgetsProc(self->line, MAX_LINE_LENGTH, self->si->stream) == NULL)
      self->line[0] = '\0';
    DEBUGP("line size: %ld", strlen(self->line));
    self->line_num++;
    self->pos = 0;
  }
}

bool Tokenizer_hasMoreTokens(Tokenizer* const self)
{
  if (self->si->stream == NULL) {
    return false;
  } else if ( self->si->feofProc(self->si->stream) == 0 || (self->line[0] != '\0') ) {
    return true;
  } else {
    return false;
  }
}

void tokenizer_pushToken(Tokenizer *self, int line_num, int pos, Type type, char *value, State state)
{
  DEBUGP("pushToken: `%s`", value);
  self->currentToken->pos = pos;
  self->currentToken->line_num = line_num;
  self->currentToken->type = type;
  self->currentToken->value = (char *)picorbc_alloc(sizeof(char) * (strlen(value) + 1));
  self->currentToken->value[0] = '\0';
  strsafecpy(self->currentToken->value, value, strlen(value) + 1);
  DEBUGP("Pushed token: `%s`", self->currentToken->value);
  self->currentToken->state = state;
  Token *newToken = Token_new();
  newToken->prev = self->currentToken;
  self->currentToken->next = newToken;
  self->currentToken = newToken;
}

/*
 * Replace null letter with "\xF5PICORUBYNULL\xF5"
 *  -> see also replace_picoruby_null();
 */
#define REPLACE_NULL_PICORUBY \
  do { \
    strsafecat(value, "\xF5PICORUBYNULL", MAX_TOKEN_LENGTH); \
    c[0] = '\xF5'; \
  } while (0)

int Tokenizer_advance(Tokenizer* const self, bool recursive)
{
  DEBUGP("Aadvance. mode: `%d`", self->mode);
  ParserState *p = self->p; /* to use macro in parse_header.h */
  Token_GC(self->currentToken->prev);
  Token *lazyToken = Token_new();
  char value[MAX_TOKEN_LENGTH];
  char c[3];
  int cmd_state;
  cmd_state = p->cmd_start;
  p->cmd_start = false;
retry:
  memset(value, '\0', MAX_TOKEN_LENGTH);
  Type type = ON_NONE;
  c[0] = '\0';
  c[1] = '\0';
  c[2] = '\0';

  RegexResult regexResult[REGEX_MAX_RESULT_NUM];

  tokenizer_readLine(self);
  if (self->line[0] == '\0') {
    Token_free(lazyToken);
    return -1;
  }
  if (self->mode == MODE_COMMENT) {
    if (Regex_match2(self->line, "^=end$") || Regex_match2(self->line, "^=end\\s")) {
      type = EMBDOC_END;
    } else {
      type = EMBDOC;
    }
    self->mode = MODE_NONE;
    strsafecpy(value, self->line, MAX_TOKEN_LENGTH);
  } else if (self->mode == MODE_WORDS
          || self->mode == MODE_SYMBOLS) {
    for (;;) {
      tokenizer_readLine(self);
      value[0] = '\0';
      type = ON_NONE;
      if (self->line[self->pos] == self->modeTerminater) {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = STRING_END;
        lazyToken->value = (char *)picorbc_alloc(sizeof(char) *(2));
        *(lazyToken->value) = self->modeTerminater;
        *(lazyToken->value + 1) = '\0';
        lazyToken->state = EXPR_END;
        self->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else if (self->line[self->pos] == ' ' || self->line[self->pos] == '\n') {
        Regex_match3(&(self->line[self->pos]), "^(\\s+)", regexResult);
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = WORDS_SEP;
      } else {
        int i = 0;
        for (;;) {
          c[0] = self->line[self->pos + i];
          c[1] = '\0';
          if (c[0] == '\0') break;
          if (c[0] != ' ' && c[0] != '\t' && c[0] != '\n' && c[0] != self->modeTerminater) {
            strsafecat(value, c, MAX_TOKEN_LENGTH);
            i++;
          } else {
            break;
          }
        }
        type = STRING;
      }
      if (strlen(value) > 0) {
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          type,
          value,
          EXPR_BEG);
        self->pos += strlen(value);
      }
    }
    self->pos--;
  } else if (self->mode == MODE_TSTRING_DOUBLE) {
    bool string_top = true;
    for (;;) {
      DEBUGP("modeTerminater: `%c`", self->modeTerminater);
      tokenizer_readLine(self);
      if (self->line[0] == '\0') {
        Token_free(lazyToken);
        return -1;
      }
      if (self->line[self->pos] == self->modeTerminater) {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = STRING_END;
        lazyToken->value = (char *)picorbc_alloc(sizeof(char) *(2));
        *(lazyToken->value) = self->modeTerminater;
        *(lazyToken->value + 1) = '\0';;
        lazyToken->state = EXPR_END;
        self->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else if (self->line[self->pos] == '#' && self->line[self->pos + 1] == '{') {
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          (string_top ? DSTRING_TOP : DSTRING_MID),
          value,
          EXPR_BEG);
        string_top = false;
        value[0] = '\0';
        c[0] = '\0';
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          DSTRING_BEG,
          "#{",
          EXPR_BEG);
        self->pos += 2;
        tokenizer_paren_stack_add(self, PAREN_BRACE);
        /**
         * Reserve some properties in order to revert them
         * after recursive calling
         */
        {
          Mode reserveMode = self->mode;
          State reserveState = self->state;
          char reserveModeTerminater = self->modeTerminater;
          bool reserveCmd_start = p->cmd_start;
          self->mode = MODE_NONE;
          self->state = EXPR_BEG;
          self->modeTerminater = '\0';
          p->cmd_start = true;
          while (Tokenizer_hasMoreTokens(self)) { /* recursive */
            if (Tokenizer_advance(self, false) == 1) break;
          }
          self->mode = reserveMode;
          self->state = reserveState;
          self->modeTerminater = reserveModeTerminater;
          p->cmd_start = reserveCmd_start;
        }
        tokenizer_pushToken(self,
          self->line_num,
          self->pos,
          DSTRING_END,
          "}",
          EXPR_END);
        self->pos++;
      } else if (self->line[self->pos] == '\\') {
        self->pos++;
        if (self->line[self->pos] == self->modeTerminater) {
          c[0] = self->modeTerminater;
        } else if (Regex_match3(&(self->line[self->pos]), "^([0-7]+)", regexResult)) {
          /*
           * An octal number
           */
          regexResult[0].value[3] = '\0'; /* maximum 3 digits */
          c[0] = strtol(regexResult[0].value, NULL, 8) % 0x100;
          if (c[0] == 0) REPLACE_NULL_PICORUBY;
          self->pos += strlen(regexResult[0].value) - 1;
        } else {
          switch (self->line[self->pos]) {
            case 'a':  c[0] =  7; break;
            case 'b':  c[0] =  8; break;
            case 't':  c[0] =  9; break;
            case 'n':  c[0] = 10; break;
            case 'v':  c[0] = 11; break;
            case 'f':  c[0] = 12; break;
            case 'r':  c[0] = 13; break;
            case 'e':  c[0] = 27; break;
            case '\\': c[0] = 92; break;
            case 'x':
              /*
               * Should ba a hex number
               */
              self->pos++;
              if (Regex_match3(&(self->line[self->pos]), "^([0-9a-fA-F]+)", regexResult)) {
                regexResult[0].value[2] = '\0'; /* maximum 2 digits */
                c[0] = strtol(regexResult[0].value, NULL, 16);
                if (c[0] == '\0') REPLACE_NULL_PICORUBY;
              } else {
                ERRORP("Invalid hex escape");
                self->p->error_count++;
              }
              break;
            default: c[0] = self->line[self->pos];
          }
        }
      } else {
        c[0] = self->line[self->pos];
      }
      self->pos += strlen(c);
      strsafecat(value, c, MAX_TOKEN_LENGTH);
    }
    self->pos--;
    if (strlen(value) > 0) {
      type = STRING;
    }
  } else if (self->mode == MODE_TSTRING_SINGLE) {
    for (;;) {
      tokenizer_readLine(self);
      if (self->line[0] == '\0') {
        Token_free(lazyToken);
        return -1;
      }
      if (self->line[self->pos] == self->modeTerminater) {
        lazyToken->line_num = self->line_num;
        lazyToken->pos = self->pos;
        lazyToken->type = STRING_END;
        lazyToken->value = (char *)picorbc_alloc(sizeof(char) *(2));
        *(lazyToken->value) = self->modeTerminater;
        *(lazyToken->value + 1) = '\0';
        lazyToken->state = EXPR_END;
        self->state = EXPR_END;
        self->pos++;
        self->mode = MODE_NONE;
        break;
      } else {
        c[0] = self->line[self->pos];
      }
      self->pos += strlen(c);
      strsafecat(value, c, MAX_TOKEN_LENGTH);
    }
    self->pos--;
    if (strlen(value) > 0) type = STRING;
  } else if (Regex_match2(self->line, "^=begin$") || Regex_match2(self->line, "^=begin\\s")) { // multi lines comment began
    self->mode = MODE_COMMENT;
    strsafecpy(value, strsafecat(self->line, "\n", MAX_TOKEN_LENGTH), MAX_TOKEN_LENGTH);
    type = EMBDOC_BEG;
  } else if (self->line[self->pos] == '\n') {
    value[0] = '\\';
    value[1] = 'n';
    value[2] = '\0';
    type = NL;
  } else if (self->line[self->pos] == '\r' && self->line[self->pos + 1] == '\n') {
    value[0] = '\\';
    value[1] = 'r';
    value[2] = '\\';
    value[3] = 'n';
    value[4] = '\0';
    type = NL;
  } else if (self->state != EXPR_DOT && self->state != EXPR_FNAME && tokenizer_is_operator(&(self->line[self->pos]), 3)) {
    value[0] = self->line[self->pos];
    value[1] = self->line[self->pos + 1];
    value[2] = self->line[self->pos + 2];
    value[3] = '\0';
    if (strcmp(value, "===") == 0) {
      type = EQQ;
    } else if (strcmp(value, "<=>") == 0) {
      type = CMP;
    } else if (value[2] == '=') {
      type = OP_ASGN;
    }
  } else if (self->state != EXPR_DOT && self->state != EXPR_FNAME && tokenizer_is_operator(&(self->line[self->pos]), 2)) {
    value[0] = self->line[self->pos];
    value[1] = self->line[self->pos + 1];
    value[2] = '\0';
    switch (value[0]) {
      case '=':
        switch (value[1]) {
          case '=': type = EQ; break;
          case '>': type = ASSOC; self->state = EXPR_BEG; break;
        }
        break;
      case '!':
        switch (value[1]) {
          case '=': type = NEQ; break;
        }
        break;
      case '*':
        switch (value[1]) {
          case '*': type = POW; break;
          case '=': type = OP_ASGN; break;
        }
        break;
      case '<':
        switch (value[1]) {
          case '<': type = LSHIFT; break;
          case '=': type = LEQ; break;
        }
        if (self->state == EXPR_FNAME || self->state == EXPR_DOT) {
          self->state = EXPR_ARG;
        } else {
          self->state = EXPR_BEG;
        }
        break;
      case '>':
        switch (value[1]) {
          case '>': type = RSHIFT; break;
          case '=': type = GEQ; break;
        }
        break;
      case '+':
      case '-':
      case '/':
      case '^':
      case '%':
        type = OP_ASGN;
        self->state = EXPR_BEG;
        break;
      case '&':
        if (value[1] == '&') {
          type = ANDOP;
        } else {
          type = OP_ASGN;
        }
        self->state = EXPR_BEG;
        break;
      case '|':
        if (value[1] == '|') {
          type = OROP;
        } else {
          type = OP_ASGN;
        }
        self->state = EXPR_BEG;
        break;
      default:
        FATALP("error");
        break;
    }
  } else if (Regex_match3(&(self->line[self->pos]), "^(@\\w+)", regexResult)) {
    strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
    type = IVAR;
    self->state = EXPR_END;
  } else if (Regex_match3(&(self->line[self->pos]), "^(\\$\\w+)", regexResult)) {
    strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
    type = GVAR;
    self->state = EXPR_END;
  } else if (Regex_match3(&(self->line[self->pos]), "^(\\?)", regexResult)) {
    strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
    type = QUESTION;
    self->state = EXPR_BEG;
  } else if (self->line[self->pos] == '-' && self->line[self->pos + 1] == '>') {
    value[0] = '-';
    value[1] = '>';
    value[2] = '\0';
    type = TLAMBDA;
  } else {
    if (self->line[self->pos] == '\\') {
      // ignore
      self->pos++;
    } else if (self->line[self->pos] == ':') {
      value[0] = ':';
      value[1] = '\0';
      if (Regex_match2(&(self->line[self->pos]), "^:[A-Za-z0-9'\"]")) {
        type = SYMBEG;
        if (self->line[self->pos + 1] == '\'' || self->line[self->pos + 1] == '"') {
          self->state = EXPR_CMDARG;
        } else {
          self->state = EXPR_FNAME;
        }
      } else {
        type = COLON;
        self->state = EXPR_BEG;
      }
    } else if (self->line[self->pos] == '#') {
      strsafecpy(value, &(self->line[self->pos]), MAX_TOKEN_LENGTH);
      if (value[strlen(value) - 1] == '\n') value[strlen(value) - 1] = '\0'; /* eliminate \n */
      type = COMMENT;
    } else if (self->line[self->pos] == ' ' || self->line[self->pos] == '\t') {
      Regex_match3(&(self->line[self->pos]), "^(\\s+)", regexResult);
      strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
      type = ON_SP;
    } else if (self->state == EXPR_FNAME || self->state == EXPR_DOT) {
      /* TODO: singleton method */
      if ( (Regex_match3(&(self->line[self->pos]), "^(\\w+[!?=]?)", regexResult))
            || (Regex_match3(&(self->line[self->pos]), "^(===?)", regexResult)) ) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = IDENTIFIER;
        self->state = EXPR_ENDFN;
      } else {
        ERRORP("Failed to tokenize!");
        Token_free(lazyToken);
        self->pos += 1; /* skip one */
        return 1;
      }
    } else if (tokenizer_is_paren(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      value[1] = '\0';
      switch (value[0]) {
        case '(':
        case '[':
        case '{':
          COND_PUSH(0);
          CMDARG_PUSH(0);
          break;
        default: /* must be ) ] } */
          COND_LEXPOP();
          CMDARG_LEXPOP();
          break;
      }
      switch (value[0]) {
        case '(':
          if (IS_BEG()) {
            type = LPAREN;
          } else {
            type = LPAREN_EXPR;
          }
          self->state = EXPR_BEG;
          tokenizer_paren_stack_add(self, PAREN_PAREN);
          break;
        case ')':
          type = RPAREN;
          self->state = EXPR_ENDFN;
          tokenizer_paren_stack_pop(self);
          break;
        case '[':
          if ( IS_BEG() || (IS_ARG() && self->line[self->pos - 1] == ' ') ) {
            type = LBRACKET_ARRAY;
          } else {
            type = LBRACKET;
          }
          self->state = EXPR_BEG|EXPR_LABEL;
          break;
        case ']':
          type = RBRACKET;
          self->state = EXPR_END;
          break;
        case '{':
          if (IS_ARG() || self->state == EXPR_END || self->state == EXPR_ENDFN) {
            type = LBRACE_BLOCK_PRIMARY; /* block (primary) */
          } else if (self->state == EXPR_ENDARG) {
            type = LBRACE_ARG;  /* block (expr) */
          } else {
            type = LBRACE;
          }
          self->state = EXPR_BEG;
          break;
        case '}':
          if (self->paren_stack[self->paren_stack_num] == PAREN_BRACE) {
            tokenizer_paren_stack_pop(self);
            Token_free(lazyToken);
            return 1;
          }
          type = RBRACE;
          self->state = EXPR_END;
          break;
        default:
          ERRORP("unknown paren error");
      }
    } else if (strchr("+-*/%&|^!~><=?:", self->line[self->pos]) != NULL) {
      if (Regex_match3(&(self->line[self->pos]), "^(%[iIwWqQ][]-~!@#$%^&*()_=+\[{};:'\"?/])", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        switch (value[1]) {
          case 'w':
          case 'W':
            type = WORDS_BEG;
            self->mode = MODE_WORDS;
            break;
          case 'q':
            type = STRING_BEG;
            self->mode = MODE_TSTRING_SINGLE;
            break;
          case 'Q':
            type = STRING_BEG;
            self->mode = MODE_TSTRING_DOUBLE;
            break;
          case 'i':
          case 'I':
            type = SYMBOLS_BEG;
            self->mode = MODE_SYMBOLS;
            break;
        }
        switch (value[2]) {
          case '[':
            self->modeTerminater = ']';
            break;
          case '{':
            self->modeTerminater = '}';
            break;
          case '(':
            self->modeTerminater = ')';
            break;
          default:
            self->modeTerminater = value[2];
        }
      } else {
        value[0] = self->line[self->pos];
        value[1] = '\0';
        if ( (IS_BEG() || (IS_ARG() && self->line[self->pos - 1] != ' ') ) && value[0] == '-') {
          if (IS_NUM(1)) {
            type = UMINUS_NUM;
          } else {
            type = UMINUS;
          }
        } else if (IS_BEG() && value[0] == '+') {
          type = UPLUS;
        } else {
          switch (value[0]) {
            case '=':
              type = E;
              break;
            case '>':
              type = GT;
              break;
            case '<':
              type = LT;
              break;
            case '+':
              type = PLUS;
              break;
            case '-':
              type = MINUS;
              break;
            case '*':
              type = TIMES;
              break;
            case '/':
              type = DIVIDE;
              break;
            case '%':
              type = SURPLUS;
              break;
            case '|':
              type = OR;
              break;
            case '^':
              type = XOR;
              break;
            case '&':
              if (IS_BEG()) {
                type = AMPER;
              } else {
                type = AND;
              }
              break;
            case '!':
              type = UNEG;
              break;
            case '~':
              type = UNOT;
              break;
            default:
              type = ON_OP;
          }
        }
        self->state = EXPR_BEG;
      }
    } else if (tokenizer_is_semicolon(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      type = SEMICOLON;
      self->state = EXPR_BEG;
    } else if (tokenizer_is_comma(self->line[self->pos])) {
      value[0] = self->line[self->pos];
      type = COMMA;
      self->state = EXPR_BEG|EXPR_LABEL;
    } else if (IS_NUM(0)) {
      self->state = EXPR_END;
      if (Regex_match3(&(self->line[self->pos]), "^(0[xX][0-9a-fA-F][0-9a-fA-F_]*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = INTEGER;
      } else if (Regex_match3(&(self->line[self->pos]), "^(0[oO][0-7][0-7_]*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = INTEGER;
      } else if (Regex_match3(&(self->line[self->pos]), "^(0[bB][01][01_]*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = INTEGER;
      } else if (Regex_match3(&(self->line[self->pos]), "^([0-9_]+\\.[0-9][0-9_]*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        if (strchr(value, (int)'.')[-1] == '_') {
          ERRORP("trailing `_' in number");
          return -1;
        }
        type = FLOAT;
        if (Regex_match3(&(self->line[self->pos + strlen(value)]), "^([eE][+\\-]?[0-9_]+)", regexResult)) {
          strsafecpy(value + strlen(value), regexResult[0].value, MAX_TOKEN_LENGTH);
        }
      } else if (Regex_match3(&(self->line[self->pos]), "^([0-9_]+)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        if (Regex_match3(&(self->line[self->pos + strlen(value)]), "^([eE][+\\-]?[0-9_]+)", regexResult)) {
          strsafecpy(value + strlen(value), regexResult[0].value, MAX_TOKEN_LENGTH);
          type = FLOAT;
        } else {
          type = INTEGER;
        }
      } else {
        ERRORP("Failed to tokenize a number");
        return -1;
      }
      if (value[strlen(value) - 1] == '_') {
        ERRORP("trailing `_' in number");
        return -1;
      }
    } else if (self->line[self->pos] == '.') {
      value[0] = '.';
      value[1] = '\0';
      type = PERIOD;
      self->state = EXPR_DOT;
    } else if (Regex_match2(&(self->line[self->pos]), "^\\w")) {
      if (Regex_match3(&(self->line[self->pos]), "^([A-Za-z0-9_?!]+:)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        value[strlen(value) - 1] = '\0';
        self->pos++;
        type = LABEL;
      } else if (Regex_match3(&(self->line[self->pos]), "^([A-Z]\\w*[!?])", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = IDENTIFIER;
      } else if (Regex_match3(&(self->line[self->pos]), "^([A-Z]\\w*)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = CONSTANT;
        self->state = EXPR_CMDARG;
      } else if (Regex_match3(&(self->line[self->pos]), "^(\\w+[!?]?)", regexResult)) {
        strsafecpy(value, regexResult[0].value, MAX_TOKEN_LENGTH);
        type = IDENTIFIER;
      } else {
        ERRORP("Failed to tokenize!");
        Token_free(lazyToken);
        return 1;
      }
    } else if (self->line[self->pos] == '"') {
      value[0] = '"';
      value[1] = '\0';
      self->mode = MODE_TSTRING_DOUBLE;
      self->modeTerminater = '"';
      type = STRING_BEG;
    } else if (self->line[self->pos] == '\'') {
      value[0] = '\'';
      value[1] = '\0';
      self->mode = MODE_TSTRING_SINGLE;
      self->modeTerminater = '\'';
      type = STRING_BEG;
    } else {
      ERRORP("Failed to tokenize!");
      Token_free(lazyToken);
      self->pos += 1; /* skip one */
      return 1;
    }
  }
  if (lazyToken->value == NULL) {
    self->pos += strlen(value);
  }
  if (type == NL) {
    switch ((int)self->state) {
      /*     ^^^
       * Casting to int can suprress `Warning: case not evaluated in enumerated type`
       */
      case EXPR_BEG:
      case EXPR_FNAME:
      case EXPR_DOT:
      case EXPR_CLASS:
      case EXPR_BEG|EXPR_LABEL:
        goto retry;
      default:
        break;
    }
    self->state = EXPR_BEG;
  }
  if (type != ON_NONE) {
    /* FIXME from here */
    int8_t kw_num = keyword(value);
    if ( kw_num > 0 ) {
      type = (uint8_t)kw_num;
      switch (type) {
        case KW_class:
          if (self->state == EXPR_BEG) {
            self->state = EXPR_CLASS;
          } else {
            type = IDENTIFIER;
            self->state = EXPR_ARG;
          }
          break;
        case KW_if:
          if (self->state != EXPR_BEG && self->state != EXPR_VALUE)
            type = KW_modifier_if;
          self->state = EXPR_VALUE;
          break;
        case KW_unless:
          if (self->state != EXPR_BEG && self->state != EXPR_VALUE)
            type = KW_modifier_unless;
          self->state = EXPR_VALUE;
          break;
        case KW_while:
          if (self->state != EXPR_BEG && self->state != EXPR_VALUE)
            type = KW_modifier_while;
          self->state = EXPR_VALUE;
          break;
        case KW_do:
          if (COND_P()) {
            type = KW_do_cond;
          } else if ((CMDARG_P() && self->state != EXPR_CMDARG)
                     || self->state == EXPR_ENDARG || self->state == EXPR_BEG){
            type = KW_do_block;
          }
        case KW_elsif:
        case KW_else:
        case KW_and:
        case KW_or:
        case KW_case:
        case KW_when:
          self->state = EXPR_BEG;
          break;
        case KW_return:
        case KW_break:
        case KW_next:
//        case KW_rescue:
          self->state = EXPR_MID;
          break;
        case KW_def:
//        case KW_alias:
//        case KW_undef:
          self->state = EXPR_FNAME;
          break;
        case KW_end:
        case KW_redo:
        default:
          self->state = EXPR_END;
      }
    } else if (type == IDENTIFIER) {
      switch (self->state) {
        case EXPR_CLASS:
          self->state = EXPR_ARG;
          break;
        case EXPR_FNAME:
          self->state = EXPR_ENDFN;
          break;
        default:
          if (IS_BEG() || self->state == EXPR_DOT || IS_ARG()) {
            if (cmd_state) {
              self->state = EXPR_CMDARG;
            }
            else {
              self->state = EXPR_ARG;
            }
          }
          else if (self->state == EXPR_FNAME) {
            self->state = EXPR_ENDFN;
          }
          else {
            self->state = EXPR_END;
          }
          break;
      }
    }
    DEBUGP("value len: %ld, `%s`", strlen(value), value);
    tokenizer_pushToken(self,
      self->line_num,
      self->pos - strlen(value),
        /* FIXME: ^^^^^^^^^^^^^ incorrect if value contains escape sequences */
      type,
      value,
      self->state);
  }
  if (lazyToken->value != NULL) {
    tokenizer_pushToken(self,
      lazyToken->line_num,
      lazyToken->pos,
      lazyToken->type,
      lazyToken->value,
      lazyToken->state);
    self->pos++;
  }
  Token_free(lazyToken);
  return 0;
}

