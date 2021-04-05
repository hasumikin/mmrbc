%fallback
  ON_NONE
  KW
  ON_SP
  SEMICOLON
  PERIOD
  ON_OP
  ON_TSTRING_SINGLE
  EMBDOC
  EMBDOC_BEG
  EMBDOC_END
  DSTRING_BEG
  DSTRING_END
  STRING_END
  IVAR
  GVAR
  CHAR
  TLAMBDA
  SYMBEG
  COMMENT
  LPAREN
  RPAREN
  LBRACKET
  RBRACKET
  LBRACE
  RBRACE
  WORDS_BEG
  WORDS_SEP
  SYMBOLS_BEG
  LABEL
  FLOAT
  .

%extra_context { ParserState *p }

%token_type { const char* }
//%token_destructor { LEMON_FREE($$); }

%default_type { Node* }
//%default_destructor { LEMON_FREE($$); }

%type call_op       { uint8_t }
%type cname         { const char* }
%type fname         { const char* }
%type sym           { const char* }
%type basic_symbol  { const char* }
%type operation2    { const char* }
%type f_norm_arg    { const char* }
%type f_block_arg   { const char* }

%include {
  #include <stdlib.h>
  #include <stdint.h>
  #include <string.h>
  #include "parse_header.h"
  #include "parse.h"
  #include "atom_helper.h"
  #include "../scope.h"
  #include "../node.h"
  #include "../token.h"
}

%ifdef LEMON_PICORBC
  %include {
    #ifdef MRBC_ALLOC_LIBC
      #define LEMON_ALLOC(size) malloc(size)
      #define LEMON_FREE(ptr)   free(ptr)
    #else
      void *picorbc_alloc(size_t size);
      void picorbc_free(void *ptr);
      #define LEMON_ALLOC(size) picorbc_alloc(size)
      #define LEMON_FREE(ptr)   picorbc_free(ptr)
    #endif /* MRBC_ALLOC_LIBC */
  }
%endif

%ifndef LEMON_PICORBC
  %include {
    #define LEMON_ALLOC(size) malloc(size)
    #define LEMON_FREE(ptr)   free(ptr)
  }
%endif

%include {
  #include "parse_header.h"

  #define STRING_NULL (p->special_string_pool.null)
  #define STRING_NEG  (p->special_string_pool.neg)
  #define STRING_ARY  (p->special_string_pool.ary)

  static Node*
  cons_gen(ParserState *p, Node *car, Node *cdr)
  {
    Node *c;
    c = Node_new(p);
    c->type = CONS;
    c->cons.car = car;
    c->cons.cdr = cdr;
    return c;
  }
  #define cons(a,b) cons_gen(p,(a),(b))

  static Node*
  atom_gen(ParserState *p, int t)
  {
    Node* a;
    a = Node_new(p);
    a->type = ATOM;
    a->atom.type = t;
    return a;
  }
  #define atom(t) atom_gen(p, (t))

  static Node*
  literal_gen(ParserState *p, const char *s)
  {
    Node* l;
    l = Node_new(p);
    Node_setValue(l, s);
    return l;
  }
  #define literal(s) literal_gen(p, (s))

  static Node*
  list1_gen(ParserState *p, Node *a)
  {
    return cons(a, 0);
  }
  #define list1(a) list1_gen(p, (a))

  static Node*
  list2_gen(ParserState *p, Node *a, Node *b)
  {
    return cons(a, cons(b,0));
  }
  #define list2(a,b) list2_gen(p, (a),(b))

  static Node*
  list3_gen(ParserState *p, Node *a, Node *b, Node *c)
  {
    return cons(a, cons(b, cons(c,0)));
  }
  #define list3(a,b,c) list3_gen(p, (a),(b),(c))

  static Node*
  list4_gen(ParserState *p, Node *a, Node *b, Node *c, Node *d)
  {
    return cons(a, cons(b, cons(c, cons(d, 0))));
  }
  #define list4(a,b,c,d) list4_gen(p, (a),(b),(c),(d))

  static Node*
  list5_gen(ParserState *p, Node *a, Node *b, Node *c, Node *d, Node *e)
  {
    return cons(a, cons(b, cons(c, cons(d, cons(e, 0)))));
  }
  #define list5(a,b,c,d,e) list5_gen(p, (a),(b),(c),(d),(e))

//  static Node*
//  list6_gen(ParserState *p, Node *a, Node *b, Node *c, Node *d, Node *e, Node *f)
//  {
//    return cons(a, cons(b, cons(c, cons(d, cons(e, cons(f, 0))))));
//  }
//  #define list6(a,b,c,d,e,f) list6_gen(p, (a),(b),(c),(d),(e),(f))

  static Node*
  append_gen(ParserState *p, Node *a, Node *b)
  {
    Node *c = a;
    if (!a) return b;
    if (!b) return a;
    while (c->cons.cdr) {
      c = c->cons.cdr;
    }
    c->cons.cdr = b;
    return a;
  }
  #define append(a,b) append_gen(p,(a),(b))
  #define push(a,b) append_gen(p,(a),list1(b))

  #define nsym(x) ((Node*)(intptr_t)(x))
  #define nint(x) ((Node*)(intptr_t)(x))

  static Node*
  new_return(ParserState *p, Node *array)
  {
    return list2(atom(ATOM_kw_return), array);
  }

  /* (:sym) */
  static Node*
  new_sym(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_symbol_literal), literal(s));
  }

  static Node*
  new_dsym(ParserState *p, Node *n)
  {
    return list2(atom(ATOM_dsymbol), n);
  }

  /* (:call a b c) */
  static Node*
  new_call(ParserState *p, Node *a, const char* b, Node *c, int pass)
  {
    return list4(
      atom(pass ? ATOM_call : ATOM_scall),
      a,
      list2(atom(ATOM_at_ident), literal(b)),
      c
    );
  }

  /* (:begin prog...) */
  static Node*
  new_begin(ParserState *p, Node *body)
  {
    if (body) {
      return list3(atom(ATOM_stmts_add), list1(atom(ATOM_stmts_new)), body);
    }
    return cons(atom(ATOM_stmts_new), 0);//TODO ここおかしい
  }

  static Node*
  new_first_arg(ParserState *p, Node *arg)
  {
    return list3(atom(ATOM_args_add), list1(atom(ATOM_args_new)), arg);
  }

  static Node*
  call_bin_op_gen(ParserState *p, Node *recv, const char *op, Node *arg)
  {
    return new_call(p, recv, op, new_first_arg(p, arg), 1);
  }
  #define call_bin_op(a, m, b) call_bin_op_gen(p ,(a), (m), (b))

  static Node*
  call_uni_op(ParserState *p, Node *recv, const char *op)
  {
    return new_call(p, recv, op, 0, 1);
  }

  /* (:int . i) */
  static Node*
  new_lit(ParserState *p, const char *s, AtomType a)
  {
    Node* result = list2(atom(a), literal(s));
    return result;
  }

  const char *ParsePushStringPool(ParserState *p, char *s);

  /* nageted integer */
  static Node*
  new_neglit(ParserState *p, const char *s, AtomType a)
  {
    const char *const_neg = ParsePushStringPool(p, STRING_NEG);
    return list3(atom(ATOM_unary), literal(const_neg) ,list2(atom(a), literal(s)));
  }

  static Node*
  new_lvar(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_lvar), literal(s));
  }

  static Node*
  new_ivar(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_at_ivar), literal(s));
  }

  static Node*
  new_gvar(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_at_gvar), literal(s));
  }

  static Node*
  new_const(ParserState *p, const char *s)
  {
    return list2(atom(ATOM_at_const), literal(s));
  }

  static void
  generate_lvar(Scope *scope, Node *n)
  {
    if (Node_atomType(n) == ATOM_lvar) {
      LvarScopeReg lvar = Scope_lvar_findRegnum(scope, Node_literalName(n->cons.cdr));
      if (lvar.reg_num == 0) {
        Scope_newLvar(scope, Node_literalName(n->cons.cdr), scope->sp);
        Scope_push(scope);
      }
    }
  }

  static Node*
  new_asgn(ParserState *p, Node *lhs, Node *rhs)
  {
    generate_lvar(p->scope, lhs);
    return list3(atom(ATOM_assign), lhs, rhs);
  }

  static Node*
  new_op_asgn(ParserState *p, Node *lhs, const char *op, Node *rhs)
  {
    generate_lvar(p->scope, lhs);
    return list4(atom(ATOM_op_assign), lhs, list2(atom(ATOM_at_op), literal(op)), rhs);
  }

  /* (:self) */
  static Node*
  new_self(ParserState *p)
  {
    return list1(atom(ATOM_kw_self));
  }

  /* (:fcall self mid args) */
  static Node*
  new_fcall(ParserState *p, Node *b, Node *c)
  {
    //Node *n = new_self(p);
    return list3(atom(ATOM_fcall), b, c);
  }

  static Node*
  var_reference(ParserState *p, Node *lhs)
  {
    if (Node_atomType(lhs) == ATOM_lvar) {
      LvarScopeReg lvar = Scope_lvar_findRegnum(p->scope, lhs->cons.cdr->cons.car->value.name);
      if (lvar.reg_num == 0) {
        return new_fcall(p, lhs, 0);
      }
    }
    //Scope_newLvar(p->scope, lhs->cons.cdr->cons.car->value.name, p->scope->sp++);
    return lhs;
  }

  /* (:block_arg . a) */
  static Node*
  new_block_arg(ParserState *p, Node *a)
  {
    return list2(atom(ATOM_block_arg), a);
  }

//  static Node*
//  setup_numparams(ParserState *p, Node *a)
//  {
//  }

  static Node*
  new_block(ParserState *p, Node *a, Node *b)
  {
    //a = setup_numparams(p, a);
    return list3(atom(ATOM_block), a, b);
  }

  static void
  call_with_block(ParserState *p, Node *a, Node *b)
  {
    Node *n;
    switch (Node_atomType(a)) {
    case ATOM_call:
    case ATOM_fcall:
      n = a->cons.cdr->cons.cdr->cons.cdr;
      if (!n->cons.car) {
        n->cons.car = new_first_arg(p, b);
      } else {
        n->cons.car = list3(atom(ATOM_args_add), n->cons.car, b);
      }
      break;
    default:
      break;
    }
  }

  static Node*
  new_class(ParserState *p, Node *c, Node *b)
  {
    return list4(atom(ATOM_class), c->cons.car, c->cons.cdr, b);
  }

  static Node*
  new_def(ParserState *p, const char* m, Node *a, Node *b)
  {
    return list5(atom(ATOM_def), literal(m), 0, a, b);
  }

  static Node*
  defn_setup(ParserState *p, Node *d, Node *a, Node *b)
  {
    /*
     * d->cons.cdr->cons.cdr->cons.cdr->cons.cdr
     *    ^^^^^^^^  ^^^^^^^^  ^^^^^^^^  ^^^^^^^^
     *  methodname  ???????   args      body
     */
    Node *n = d->cons.cdr->cons.cdr;
    //p->cmdarg_stack = intn(n->cons.cdr->cons.car);
    n->cons.cdr->cons.car = a;
    //local_resume(p, n->cons.cdr->cons.cdr->cons.car);
    n->cons.cdr->cons.cdr->cons.car = b;
    return d;
  }

  static void
  local_add_f(ParserState *p, const char *a)
  {
    // different way from mruby...
    Scope_newLvar(p->scope, a, p->scope->sp++);
  }

  static Node*
  new_arg(ParserState *p, const char* a)
  {
    Scope_newLvar(p->scope, a, p->scope->sp);
    return list2(atom(ATOM_arg), literal(a));
  }

  static Node*
  new_args(ParserState *p, Node *m, Node *opt, const char *rest, Node *m2, Node *tail)
  {
    return list5(atom(ATOM_block_parameters),
      list2(atom(ATOM_margs), m),
      list2(atom(ATOM_optargs),opt),
      list2(atom(ATOM_m2args), m2),
      list2(atom(ATOM_tailargs), tail)
    );
  }

  static void
  local_add_blk(ParserState *p, const char *blk)
  {
    local_add_f(p, blk ? blk : "&");
  }

  static Node*
  new_args_tail(ParserState *p, Node *kws, Node *kwrest, const char *blk)
  {
    local_add_blk(p, blk);
    return list4(atom(ATOM_args_tail), kws, kwrest, literal(blk));
  }

  /* (:dstr . a) */
  static Node*
  new_dstr(ParserState *p, Node *a)
  {
    return list2(atom(ATOM_dstr), a);
  }

  static Node*
  new_array(ParserState *p, Node *a)
  {
    return list2(atom(ATOM_array), a);
  }

  static Node*
  ret_args(ParserState *p, Node *a)
  {
    if (Node_atomType(a->cons.cdr->cons.car) == ATOM_args_add)
      /* multiple return values will be an array */
      return new_array(p, a);
    return a;
  }

  static Node*
  new_hash(ParserState *p, Node *a)
  {
    return list2(atom(ATOM_hash), a);
  }

  static Node*
  new_if(ParserState *p, Node *b, Node *c, Node *d)
  {
    return list4(atom(ATOM_if), b, c, d);
  }

  static Node*
  new_while(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_while), b, c);
  }

  static Node*
  new_until(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_until), b, c);
  }

  static Node*
  new_case(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_case), b, c);
  }

  static Node*
  new_break(ParserState *p, Node *b)
  {
    return list2(atom(ATOM_break), b);
  }

  static Node*
  new_next(ParserState *p, Node *b)
  {
    return list2(atom(ATOM_next), b);
  }

  static Node*
  new_and(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_and), b, c);
  }

  static Node*
  new_or(ParserState *p, Node *b, Node *c)
  {
    return list3(atom(ATOM_or), b, c);
  }

  static Node*
  new_str(ParserState *p, const char *a)
  {
    if (a == NULL || a[0] == '\0') {
      return list2(atom(ATOM_str), literal(STRING_NULL));
    } else {
      return list2(atom(ATOM_str), literal(a));
    }
  }

  static Node*
  unite_str(ParserState *p, Node *a, Node* b) {
    const char *a_value = Node_valueName(a);
    size_t a_len = strlen(a_value);
    const char *b_value = Node_valueName(b);
    size_t b_len = strlen(b_value);
    char new_value[a_len + b_len + 1];
    memcpy(new_value, a_value, a_len);
    memcpy(new_value + a_len, b_value, b_len);
    new_value[a_len + b_len] = '\0';
    const char *value = ParsePushStringPool(p, new_value);
    Node *n = literal(value);
    return n;
  }

  static Node*
  concat_string(ParserState *p, Node *a, Node* b)
  {
    Node *new_str_value;
    if (Node_atomType(a) == ATOM_str) {
      if (Node_atomType(b) == ATOM_str) { /* "str" "str" */
        new_str_value = unite_str(p, a->cons.cdr->cons.car, b->cons.cdr->cons.car);
        a->cons.cdr->cons.car = new_str_value;
        return a;
      } else { /* "str" "dstr" */
        Node *n = b->cons.cdr->cons.car;
        while (Node_atomType(n->cons.cdr->cons.car) != ATOM_dstr_new) {
          n = n->cons.cdr->cons.car;
        }
        new_str_value = unite_str(p, a->cons.cdr->cons.car, n->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car);
        n->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car = new_str_value;
        return b;
      }
    } else {
      if (Node_atomType(b) == ATOM_str) { /* "dstr" "str" */
        Node *a2 = a->cons.cdr->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car;
        new_str_value = unite_str(p, a2, b->cons.cdr->cons.car);
        a->cons.cdr->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car = new_str_value;
        return a;
      } else { /* "dstr_a" "dstr_b" ...ex) `"w#{1}x" "y#{2}z"`*/
        Node *a2, *b2;
        { /* unite the last str of dstr_a and the first str of dstr_b ...ex) "x" "y" */
          a2 = a->cons.cdr->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car;
          b2 = b->cons.cdr->cons.car;
          while (Node_atomType(b2->cons.cdr->cons.car) != ATOM_dstr_new) {
            b2 = b2->cons.cdr->cons.car;
          }
          new_str_value = unite_str(p, a2, b2->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car);
          a->cons.cdr->cons.car->cons.cdr->cons.cdr->cons.car->cons.cdr->cons.car = new_str_value;
          b2 = b2->cons.cdr->cons.cdr->cons.car;
        }
        Node *orig_a = a->cons.cdr->cons.car; /* copy to reuse it later */
        { /* replace dstr_a tree with dstr_b tree */
          a->cons.cdr->cons.car = b->cons.cdr->cons.car;
        }
        { /* insert original dstr_a tree into the bottom of new dstr tree*/
          a2 = a->cons.cdr->cons.car;
          while (Node_atomType(a2->cons.cdr->cons.car->cons.cdr->cons.car) != ATOM_dstr_new) {
            a2 = a2->cons.cdr->cons.car;
          }
          a2->cons.cdr->cons.car = orig_a;  /* insert original dstr_a */
        }
        return a;
      }
    }
    fprintf(stderr, "concat_string(); This should not happen\n");
  }

  static void
  scope_nest(ParserState *p, bool lvar_top)
  {
    p->scope = Scope_new(p->scope, lvar_top);
  }

  static void
  scope_unnest(ParserState *p)
  {
    p->scope = p->scope->upper;
  }
}

%parse_accept {
}

%syntax_error {
  p->error_count++;
}

%parse_failure {
  p->error_count++;
}

%start_symbol program

%nonassoc LOWEST.
%nonassoc LBRACE_ARG.

%nonassoc  KW_modifier_if KW_modifier_unless KW_modifier_while KW_modifier_until.
%left KW_or KW_and.
%right KW_not.
%right E OP_ASGN.
%right QUESTION COLON.
%left OROP.
%left ANDOP.
%nonassoc EQ EQQ NEQ.
%left GT GEQ LT LEQ. // > >= < <=
%left OR XOR.
%left AND.
%left LSHIFT RSHIFT.
%left PLUS MINUS.
%left TIMES DIVIDE SURPLUS.
%right UMINUS_NUM UMINUS.
%right POW.
%right UNEG UNOT UPLUS.

program ::= top_compstmt(B).  {
                                yypParser->p->root_node_box->nodes = list2(atom(ATOM_program), B);
                              }

top_compstmt(A) ::= top_stmts(B) opt_terms. { A = B; }

top_stmts(A) ::= none. { A = new_begin(p, 0); }
top_stmts(A) ::= top_stmt(B). { A = new_begin(p, B); }
top_stmts(A) ::= top_stmts(B) terms top_stmt(C).
  { A = list3(atom(ATOM_stmts_add), B, C); }

top_stmt ::= stmt.

bodystmt ::= compstmt.

compstmt(A) ::= stmts(B) opt_terms. { A = B; }

stmts(A) ::= none. { A = new_begin(p, 0); }
stmts(A) ::= stmt(B). { A = new_begin(p, B); }
stmts(A) ::= stmts(B) terms stmt(C). { A = list3(atom(ATOM_stmts_add), B, C); }

stmt(A) ::= none. { A = new_begin(p, 0); }
//stmt ::= command_asgn.
//
//command_asgn(A) ::= lhs(B) E command_rhs(C). { A = new_asgn(p, B, C); }
//command_asgn(A) ::= var_lhs(B) OP_ASGN(C) command_rhs(D). { A = new_op_asgn(p, B, C, D); }
//command_asgn(A) ::= primary_value(B) LBRACKET opt_call_args(C) RBRACKET OP_ASGN(D) command_rhs(E).
//  { A = new_op_asgn(p, new_call(p, B, STRING_ARY, C, '.'), D, E); }
//
//command_rhs ::= command_call. [OP_ASGN]
//command_rhs ::= command_asgn.
stmt(A) ::= stmt(B) KW_modifier_if expr_value(C). {
              A = new_if(p, C, B, 0);
            }
stmt(A) ::= stmt(B) KW_modifier_unless expr_value(C). {
              A = new_if(p, C, 0, B);
            }
stmt(A) ::= stmt(B) KW_modifier_while expr_value(C). {
              A = new_while(p, C, B);
            }
stmt(A) ::= stmt(B) KW_modifier_until expr_value(C). {
              A = new_until(p, C, B);
            }

stmt ::= expr.

expr ::= command_call.
expr(A) ::= expr(B) KW_and expr(C). { A = new_and(p, B, C); }
expr(A) ::= expr(B) KW_or expr(C). { A = new_or(p, B, C); }
expr ::= arg.

defn_head(A) ::= KW_def fname(B). {
                  A = new_def(p, B, 0, 0); // nint(p->cmdarg_stack), local_switch(p)
                  // p->cmdarg_stackl = 0;
                  // p->in_def++;
                  // nvars_block(p);
                  scope_nest(p, true);
                }

expr_value(A) ::= expr(B). {
                   if (!B) A = list1(atom(ATOM_kw_nil));
                   else A = B;
                  }


command_call ::= command.
command_call ::= block_command.

block_command ::= block_call.

command(A) ::= operation(B) command_args(C). [LOWEST] { A = new_fcall(p, B, C); }
command(A) ::= primary_value(B) call_op(C) operation2(D) command_args(E).
  { A = new_call(p, B, D, E, C); }
command(A) ::= KW_return call_args(B). { A = new_return(p, ret_args(p, B)); }
command(A) ::= KW_break call_args(B). { A = new_break(p, ret_args(p, B)); }
command(A) ::= KW_next call_args(B). { A = new_next(p, ret_args(p, B)); }

command_args ::= call_args.

call_args(A) ::= args(B) opt_block_arg(C). { A = append(B, C); }
call_args(A) ::= block_arg(B). { A = list2(atom(ATOM_args_add), B); }

block_arg(A) ::= AMPER arg(B). { A = new_block_arg(p, B); }

opt_block_arg(A) ::= COMMA block_arg(B). { A = B; }
opt_block_arg(A) ::= none. { A = 0; }

args(A) ::= arg(B). { A = new_first_arg(p, B); }
args(A) ::= args(B) COMMA arg(C). { A = list3(atom(ATOM_args_add), B, C); }

arg(A) ::= lhs(B) E arg_rhs(C). { A = new_asgn(p, B, C); }
arg(A) ::= var_lhs(B) OP_ASGN(C) arg_rhs(D). { A = new_op_asgn(p, B, C, D); }
arg(A) ::= primary_value(B) LBRACKET opt_call_args(C) RBRACKET OP_ASGN(D) arg_rhs(E).
  { A = new_op_asgn(p, new_call(p, B, STRING_ARY, C, '.'), D, E); }
arg(A) ::= primary_value(B) call_op(C) IDENTIFIER(D) OP_ASGN(E) arg_rhs(F).
  { A = new_op_asgn(p, new_call(p, B, D, 0, C), E, F); }
arg(A) ::= arg(B) PLUS arg(C).   { A = call_bin_op(B, "+" ,C); }
arg(A) ::= arg(B) MINUS arg(C).  { A = call_bin_op(B, "-", C); }
arg(A) ::= arg(B) TIMES arg(C).  { A = call_bin_op(B, "*", C); }
arg(A) ::= arg(B) DIVIDE arg(C). { A = call_bin_op(B, "/", C); }
arg(A) ::= arg(B) SURPLUS arg(C). { A = call_bin_op(B, "%", C); }
arg(A) ::= arg(B) POW arg(C). { A = call_bin_op(B, "**", C); }
arg(A) ::= UPLUS arg(B). { A = call_uni_op(p, B, "+@"); }
arg(A) ::= UMINUS arg(B). { A = call_uni_op(p, B, "-@"); }
arg(A) ::= arg(B) OR arg(C). { A = call_bin_op(B, "|", C); }
arg(A) ::= arg(B) XOR arg(C). { A = call_bin_op(B, "^", C); }
arg(A) ::= arg(B) AND arg(C). { A = call_bin_op(B, "&", C); }
arg(A) ::= arg(B) CMP arg(C). { A = call_bin_op(B, "<=>", C); }
arg(A) ::= arg(B) GT arg(C). { A = call_bin_op(B, ">", C); }
arg(A) ::= arg(B) GEQ arg(C). { A = call_bin_op(B, ">=", C); }
arg(A) ::= arg(B) LT arg(C). { A = call_bin_op(B, "<", C); }
arg(A) ::= arg(B) LEQ arg(C). { A = call_bin_op(B, "<=", C); }
arg(A) ::= arg(B) EQ arg(C). { A = call_bin_op(B, "==", C); }
arg(A) ::= arg(B) EQQ arg(C). { A = call_bin_op(B, "===", C); }
arg(A) ::= arg(B) NEQ arg(C). { A = call_bin_op(B, "!=", C); }
arg(A) ::= UNEG arg(B). { A = call_uni_op(p, B, "!"); }
arg(A) ::= UNOT arg(B). { A = call_uni_op(p, B, "~"); }
arg(A) ::= arg(B) LSHIFT arg(C). { A = call_bin_op(B, "<<", C); }
arg(A) ::= arg(B) RSHIFT arg(C). { A = call_bin_op(B, ">>", C); }
arg(A) ::= arg(B) ANDOP arg(C). { A = new_and(p, B, C); }
arg(A) ::= arg(B) OROP arg(C). { A = new_or(p, B, C); }
arg(A) ::= arg(B) QUESTION arg(C) COLON arg(D). { A = new_if(p, B, C, D); }
arg(A) ::= defn_head(B) f_arglist_paren(C) E arg(D). {
                  A = defn_setup(p, B, C, D);
                  scope_unnest(p);
                }

arg ::= primary.

arg_rhs ::= arg. [OP_ASGN]

lhs ::= variable.
lhs(A) ::= primary_value(B) LBRACKET opt_call_args(C) RBRACKET.
  { A = new_call(p, B, STRING_ARY, C, '.'); }
lhs(A) ::= primary_value(B) call_op(C) IDENTIFIER(D). { A = new_call(p, B, D, 0, C); }

cname ::= CONSTANT.

cpath(A) ::= cname(B).  {
                          A = literal(B);
                          //A = cons(nint(0), literal(B));
                        }

var_lhs ::= variable.

primary     ::= literal.
primary     ::= string.
primary     ::= var_ref.
primary(A)  ::= KW_begin
                bodystmt(B)
                KW_end. {
                  A = B;
                }
primary(A)  ::= LPAREN compstmt(B) RPAREN. {
                  A = B;
                }
primary(A)  ::= LBRACKET_ARRAY aref_args(B) RBRACKET. { A = new_array(p, B); }
primary(A)  ::= LBRACE assoc_list(B) RBRACE. { A = new_hash(p, B); }
primary(A)  ::= KW_return. { A = new_return(p, 0); }
primary(A)  ::= KW_not LPAREN_EXPR expr(B) RPAREN. { A = call_uni_op(p, B, "!"); }
primary(A)  ::= KW_not LPAREN_EXPR RPAREN. { A = call_uni_op(p, list1(atom(ATOM_kw_nil)), "!"); }
primary(A)  ::= operation(B) brace_block(C). {
                  A = new_fcall(p, B, list2(0, C));
                }
primary     ::= method_call.
primary(A)  ::= method_call(B) brace_block(C). {
                  call_with_block(p, B, C);
                  A = B;
                }
primary(A)  ::= KW_if expr_value(B) then
                compstmt(C)
                if_tail(D)
                KW_end. {
                  A = new_if(p, B, C, D);
                }
primary(A)  ::= KW_unless expr_value(B) then
                compstmt(C)
                opt_else(D)
                KW_end. {
                  A = new_if(p, B, D, C); /* NOTE: `D, C` in inverse order */
                }
cond_push_while ::= KW_while. { COND_PUSH(1); }
cond_push_until ::= KW_until. { COND_PUSH(1); }
do_pop ::= do. { COND_POP(); }
primary(A) ::=  cond_push_while expr_value(B) do_pop compstmt(C) KW_end. {
                  A = new_while(p, B, C);
                }
primary(A) ::=  cond_push_until expr_value(B) do_pop compstmt(C) KW_end. {
                  A = new_until(p, B, C);
                }
primary(A) ::=  KW_case expr_value(B) opt_terms
                case_body(C)
                KW_end. {
                  A = new_case(p, B, C);
                }
primary(A) ::=  KW_case opt_terms case_body(C) KW_end. {
                  A = new_case(p, 0, C);
                }
class_head(A) ::= KW_class
                  cpath(B) superclass(C). {
                    A = cons(B, C);
                    scope_nest(p, true);
                  }
primary(A) ::=  class_head(B)
                bodystmt(C)
                KW_end. {
                  A = new_class(p, B, C);
                  scope_unnest(p);
                }
primary(A) ::=  defn_head(B) f_arglist(C)
                  bodystmt(D)
                KW_end. {
                  A = defn_setup(p, B, C, D);
                  // nvars_unnest(p);
                  // p->in_def--;
                  scope_unnest(p);
                }
primary(A) ::=  KW_break. {
                  A = new_break(p, 0);
                }
primary(A) ::=  KW_next. {
                  A = new_next(p, 0);
                }
primary(A) ::=  KW_redo. {
                  A = list1(atom(ATOM_redo));
                }

primary_value(A) ::= primary(B). { A = B; }

then ::= term.
then ::= KW_then.
then ::= term KW_then.

do ::= term.
do ::= KW_do_cond.
do ::= KW_do. // TODO remove after definingbrace_block

if_tail     ::= opt_else.
if_tail(A)  ::= KW_elsif expr_value(B) then
                compstmt(C)
                if_tail(D). {
                  A = new_if(p, B, C, D);
                }

opt_else    ::= none.
opt_else(A) ::= KW_else compstmt(B). { A = B; }

assoc_list ::= none.
assoc_list(A) ::= assocs(B) trailer. { A = B; }

assocs(A) ::= assoc(B). { A = list1(B); }
assocs(A) ::= assocs(B) COMMA assoc(C). { A = push(B, C); }

assoc(A) ::= arg(B) ASSOC arg(C). { A = list3(atom(ATOM_assoc_new), B, C); }
assoc(A) ::= LABEL(B) arg(C). { A = list3(atom(ATOM_assoc_new), new_sym(p, B), C); }


aref_args     ::= none.
aref_args(A)  ::= args(B) trailer. { A = B; }

opt_block_args_tail(A) ::= none. { A = new_args_tail(p, 0, 0, 0); }

block_param(A)  ::= f_arg(B) opt_block_args_tail(C). {
                      A = new_args(p, B, 0, 0, 0, C);
                    }

opt_block_param(A)  ::= none. {
                          local_add_blk(p, 0);
                          A = 0;
                        }
opt_block_param(A)  ::= block_param_def(B). {
                          p->cmd_start = true;
                          A = B;
                        }

block_param_def(A)  ::= OR opt_bv_decl OR. {
                          A = 0;
                        }
block_param_def(A)  ::= OR block_param(B) opt_bv_decl OR. {
                          A = B;
                        }

opt_bv_decl(A) ::= opt_nl. { A = 0; }
opt_bv_decl(A) ::= opt_nl SEMICOLON bv_decls opt_nl. { A = 0; }

bv_decls ::= bvar.
bv_decls ::= bv_decls COMMA bvar.

bvar ::= IDENTIFIER(B). {
           local_add_f(p, B);
           //new_bv(p, B);
         }

scope_nest_KW_do_block ::= KW_do_block. { scope_nest(p, false); }
do_block(A) ::= scope_nest_KW_do_block
                opt_block_param(B)
                bodystmt(C)
                KW_end. {
                  A = new_block(p, B, C);
                  scope_unnest(p);
                }

block_call(A) ::= command(B) do_block(C). {
                    call_with_block(p, B, C);
                    A = B;
                  }

method_call(A)  ::=  operation(B) paren_args(C). {
                       A = new_fcall(p, B, C);
                     }
method_call(A)  ::=  primary_value(B) call_op(C) operation2(D) opt_paren_args(E). {
                       A = new_call(p, B, D, E, C);
                     }
method_call(A)  ::=  primary_value(B) LBRACKET opt_call_args(C) RBRACKET. {
                       A = new_call(p, B, STRING_ARY, C, '.');
                     }

scope_nest_brace ::= LBRACE_BLOCK_PRIMARY. { scope_nest(p, false); }
scope_nest_KW_do ::= KW_do. { scope_nest(p, false); }
brace_block(A) ::= scope_nest_brace
                   opt_block_param(B)
                   bodystmt(C)
                   RBRACE. {
                     A = new_block(p, B, C);
                     scope_unnest(p);
                   }
brace_block(A) ::= scope_nest_KW_do
                   opt_block_param(B)
                   bodystmt(C)
                   KW_end. {
                     A = new_block(p, B, C);
                     scope_unnest(p);
                   }

call_op(A) ::= PERIOD. { A = '.'; }

opt_paren_args ::= none.
opt_paren_args ::= paren_args.

paren_args(A) ::= LPAREN_EXPR opt_call_args(B) RPAREN. { A = B; }

opt_call_args ::= none.
opt_call_args ::= call_args opt_terms.

case_body(A) ::= KW_when args(B) then
                 compstmt(C)
                 cases(D). {
                   A = list3(B, C, D);
                 }
cases(A) ::= opt_else(B). {
               if (B) {
                 A = list3(0, B, 0);
               } else {
                 A = 0;
               }
             }
cases ::= case_body.

f_norm_arg(A) ::= IDENTIFIER(B). {
                    local_add_f(p, B);
                    A = B;
                  }
f_arg_item(A) ::= f_norm_arg(B). {
                    A = new_arg(p, B);
                  }

f_arg(A) ::= f_arg_item(B). { A = new_first_arg(p, B); }
f_arg(A) ::= f_arg(B) COMMA f_arg_item(C). { A = list3(atom(ATOM_args_add), B, C); }

literal ::= numeric.
literal ::= symbol.
literal ::= words.
literal ::= symbols.

words(A) ::= WORDS_BEG opt_sep word_list(B) opt_sep. { A = new_array(p, B); }
word_list(A) ::= word(B). { A = new_first_arg(p, B); }
word_list(A) ::= word_list(B) opt_sep word(C). { A = list3(atom(ATOM_args_add), B, C); }
word(A) ::= STRING(B). { A = new_str(p, B); }

symbols(A) ::= SYMBOLS_BEG opt_sep symbol_list(B) opt_sep. { A = new_array(p, B); }
symbol_list(A) ::= symbol_word(B). { A = new_first_arg(p, B); }
symbol_list(A) ::= symbol_list(B) opt_sep symbol_word(C). { A = list3(atom(ATOM_args_add), B, C); }
symbol_word(A) ::= STRING(B). { A = new_sym(p, B); }

opt_sep ::= none.
opt_sep ::= WORDS_SEP.
opt_sep ::= opt_sep WORDS_SEP.

superclass(A) ::= . { A = 0; }
superclass_head ::= LT. {
                  //p->state = EXPR_BEG;
                  p->cmd_start = true;
                }
superclass(A) ::= superclass_head expr_value(B) term. {
                    A = B;
                  }

numeric(A) ::= INTEGER(B). { A = new_lit(p, B, ATOM_at_int); }
numeric(A) ::= FLOAT(B).   { A = new_lit(p, B, ATOM_at_float); }
numeric(A) ::= UMINUS_NUM INTEGER(B). [LOWEST] { A = new_neglit(p, B, ATOM_at_int); }
numeric(A) ::= UMINUS_NUM FLOAT(B). [LOWEST]   { A = new_neglit(p, B, ATOM_at_float); }

variable(A) ::= IDENTIFIER(B). { A = new_lvar(p, B); }
variable(A) ::= IVAR(B).       { A = new_ivar(p, B); }
variable(A) ::= GVAR(B).       { A = new_gvar(p, B); }
variable(A) ::= CONSTANT(B).   { A = new_const(p, B); }

var_ref(A) ::= variable(B). { A = var_reference(p, B); }
var_ref(A) ::= KW_nil. { A = list1(atom(ATOM_kw_nil)); }
var_ref(A) ::= KW_self. { A = new_self(p); }
var_ref(A) ::= KW_true. { A = list1(atom(ATOM_kw_true)); }
var_ref(A) ::= KW_false. { A = list1(atom(ATOM_kw_false)); }

symbol(A) ::= basic_symbol(B). { A = new_sym(p, B); }
symbol(A) ::= SYMBEG STRING_BEG STRING(C). {
                A = new_sym(p, C);
              }
symbol(A) ::= SYMBEG STRING_BEG string_rep(B) STRING(C). {
                A = new_dsym(
                  p, new_dstr(p, list3(atom(ATOM_dstr_add), B, new_str(p, C)))
                );
              }

basic_symbol(A) ::= SYMBEG sym(B). { A = B; }

sym ::= fname.

fname ::= IDENTIFIER.
fname ::= CONSTANT.
fname ::= FID.

f_arglist_paren(A) ::= LPAREN_EXPR f_args(B) RPAREN. {
                         A = B;
//                         p->state = EXPR_BEG;
                         p->cmd_start = true;
                        }

f_arglist     ::= f_arglist_paren.
f_arglist(A)  ::= f_args(B) term. {
                    A = B;
                  }

f_args(A) ::= f_arg(B) opt_args_tail(C). {
                A = new_args(p, B, 0, 0, 0, C);
              }
f_args(A) ::= args_tail(B). {
                A = new_args(p, 0, 0, 0, 0, B);
              }
f_args(A) ::= . {
                // local_add_f(p, intern_op(and))
                A = new_args(p, 0, 0, 0, 0, 0);
              }

args_tail(A) ::= f_block_arg(B). {
                A = new_args_tail(p, 0, 0, B);
              }

blkarg_mark ::= AMPER.

f_block_arg(A) ::= blkarg_mark IDENTIFIER(B). {
                    A = B;
                  }

//opt_args_tail(A) ::= COMMA args_tail(B). {
//                      A = B;
//                    }
opt_args_tail(A) ::= . {
                      A = new_args_tail(p, 0, 0, 0);
                    }

string ::= string_fragment.
string(A) ::= string(B) string_fragment(C). { A = concat_string(p, B, C); }

string_fragment(A) ::= STRING(B). { A = new_str(p, B); }
string_fragment(A) ::= STRING_BEG STRING(B). { A = new_str(p, B); }
string_fragment(A) ::= STRING_BEG string_rep(B) STRING(C).
  { A = new_dstr(p, list3(atom(ATOM_dstr_add), B, new_str(p, C))); }

string_rep ::= string_interp.
string_rep(A) ::= string_rep(B) string_interp(C).
  { A = list3(atom(ATOM_dstr_add), B, C); }

string_interp(A) ::= DSTRING_TOP(B). { A = list3(atom(ATOM_dstr_add), list1(atom(ATOM_dstr_new)), new_str(p, B)); }
string_interp(A) ::= DSTRING_MID(B). { A = new_str(p, B); }
string_interp(A) ::= DSTRING_BEG compstmt(B) DSTRING_END.
  { A = B; }

operation(A) ::= IDENTIFIER(B). { A = list2(atom(ATOM_at_ident), literal(B)); }
operation(A) ::= CONSTANT(B). { A = list2(atom(ATOM_at_const), literal(B)); }
operation ::= FID.

operation2 ::= IDENTIFIER.
operation2 ::= CONSTANT.
operation2 ::= FID.

opt_nl ::= .
opt_nl ::= nl.

opt_terms ::= .
opt_terms ::= terms.

trailer ::= .
trailer ::= terms.
trailer ::= COMMA.

term ::= SEMICOLON.
term ::= nl.
term ::= COMMENT.

nl ::= NL.

terms ::= term.
terms ::= terms term.

none(A) ::= . { A = 0; }

%code {

  StringPool *stringPool_new(StringPool *prev, uint16_t size)
  {
    StringPool *pool = (StringPool *)LEMON_ALLOC(STRING_POOL_HEADER_SIZE + size);
    pool->prev = prev;
    pool->size = size;
    pool->index = 0;
    return pool;
  }

  const char *findFromStringPool(ParserState *p, char *s)
  {
    if (s[0] == '\0') return STRING_NULL;
    if (s[1] == '-' && s[0] == '\0') return STRING_NEG;
    if (strcmp(s, "[]") == 0) return STRING_ARY;
    StringPool *pool = p->current_string_pool;
    uint16_t index;
    while (pool) {
      index = 0;
      for (;;) {
        if (strcmp(s, &pool->strings[index]) == 0) return &pool->strings[index];
        while (index < pool->index) {
          if (pool->strings[index] == '\0') break;
          index++;
        }
        index++;
        if (index >= pool->index) break;
      }
      pool = pool->prev;
    }
    return NULL;
  }

  const char *ParsePushStringPool(ParserState *p, char *s)
  {
    const char *result = findFromStringPool(p, s);
    if (result) return result;
    size_t length = strlen(s) + 1; /* including \0 */
    StringPool *pool = p->current_string_pool;
    if (pool->size < pool->index + length) {
      if (length > STRING_POOL_POOL_SIZE) {
        p->current_string_pool = stringPool_new(pool, length);
      } else {
        p->current_string_pool = stringPool_new(pool, STRING_POOL_POOL_SIZE);
      }
      pool = p->current_string_pool;
    }
    uint16_t index = pool->index;
    strcpy((char *)&pool->strings[index], s);
    pool->index += length; /* position of the next insertion */
    return (const char *)&pool->strings[index];
  }

  ParserState *ParseInitState(uint8_t node_box_size)
  {
    ParserState *p = LEMON_ALLOC(sizeof(ParserState));
    p->node_box_size = node_box_size;
    p->scope = Scope_new(NULL, true);
    p->current_node_box = NULL;
    p->root_node_box = Node_newBox(p);
    p->current_node_box = p->root_node_box;
    p->current_string_pool = stringPool_new(NULL, STRING_POOL_POOL_SIZE);
    strcpy(STRING_NULL, "");
    strcpy(STRING_NEG,  "-");
    strcpy(STRING_ARY, "[]");
    p->error_count = 0;
    p->cond_stack = 0;
    p->cmdarg_stack = 0;
    return p;
  }

  void ParserStateFree(ParserState *p) {
    Scope *scope = p->scope;
    /* trace back to the up most scope in case of syntax error */
    while (1) {
      if (scope->upper == NULL) break;
      scope = scope->upper;
    }
    Scope_freeCodePool(scope);
    StringPool *prev_pool;
    while (p->current_string_pool) {
      prev_pool = p->current_string_pool->prev;
      LEMON_FREE(p->current_string_pool);
      p->current_string_pool = prev_pool;
    }
    Scope_free(scope);
    LEMON_FREE(p);
  }

  void freeNode(Node *n) {
    //printf("before free cons: %p\n", n);
    if (n == NULL) return;
    if (n->type == CONS) {
      freeNode(n->cons.car);
      freeNode(n->cons.cdr);
    } else if (n->type == LITERAL) {
      LEMON_FREE((char *)n->value.name);
    }
    //printf("after free cons: %p\n", n);
    LEMON_FREE(n);
  }

  void ParseFreeAllNode(yyParser *yyp) {
    Node_freeAllNode(yyp->p->root_node_box);
  }

#ifndef NDEBUG
  void showNode1(Node *n, bool isCar, int indent, bool isRightMost) {
    if (n == NULL) return;
    switch (n->type) {
      case CONS:
        if (isCar) {
          printf("\n");
          for (int i=0; i<indent; i++) {
            printf("  ");
          }
          printf("[");
        } else {
          printf(", ");
        }
        if (n->cons.car && n->cons.car->type != CONS && n->cons.cdr == NULL) {
          isRightMost = true;
        }
        break;
      case ATOM:
        printf("%s", atom_name(n->atom.type));
        if (isRightMost) {
          printf("]");
        }
        break;
      case LITERAL:
        printf("\e[31;1m\"%s\"\e[m", Node_valueName(n));
        if (isRightMost) {
          printf("]");
        }
        break;
    }
    if (n->type == CONS) {
      showNode1(n->cons.car, true, indent+1, isRightMost);
      showNode1(n->cons.cdr, false, indent, isRightMost);
    }
  }

  void showNode2(Node *n) {
    if (n == NULL) return;
    switch (n->type) {
      case ATOM:
        printf("    atom:%p", n);
        printf("  value:%d\n", n->atom.type);
        break;
      case LITERAL:
        printf("    literal:%p", n);
        printf("  name:\"%s\"\n", Node_valueName(n));
        break;
      case CONS:
        printf("cons:%p\n", n);
        printf(" car:%p\n", n->cons.car);
        printf(" cdr:%p\n", n->cons.cdr);
        showNode2(n->cons.car);
        showNode2(n->cons.cdr);
    }
  }

  void ParseShowAllNode(yyParser *yyp, int way) {
    if (way == 1) {
      showNode1(yyp->p->root_node_box->nodes, true, 0, false);
    } else if (way == 2) {
      showNode2(yyp->p->root_node_box->nodes);
    }
    printf("\n");
  }
#endif /* !NDEBUG */

  char *kind(Node *n){
    char *type = NULL;
    switch (n->type) {
      case ATOM:
        type = "a";
        break;
      case LITERAL:
        type = "l";
        break;
      case CONS:
        type = "c";
        break;
    }
    return type;
  }

  int atom_type(Node *n) {
    if (n->type != ATOM) {
      return ATOM_NONE;
    }
    return n->atom.type;
  }

}
