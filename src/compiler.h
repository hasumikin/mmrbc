#ifndef PICORBC_COMPILE_H_
#define PICORBC_COMPILE_H_

#include <stdbool.h>

#include "scope.h"
#include "stream.h"
#include "ruby-lemon-parse/parse_header.h"

bool Compiler_compile(ParserState *p, StreamInterface *si);

ParserState *Compiler_parseInitState(uint8_t node_box_size);

void Compiler_parserStateFree(ParserState *p);

#endif
