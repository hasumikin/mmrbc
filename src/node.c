#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "node.h"
#include "ruby-lemon-parse/parse_header.h"

const char *Node_valueName(Node *self)
{
  return self->value.name;
}

void Node_setValue(Node *self, const char *s)
{
  self->type = LITERAL;
  self->value.name = s;
}

bool Node_isAtom(Node *self)
{
  if (self->type == ATOM) return true;
  return false;
}

bool Node_isCons(Node *self)
{
  if (self->type == CONS) return true;
  return false;
}

bool Node_isLiteral(Node *self)
{
  return (self->type == LITERAL);
}

AtomType Node_atomType(Node *self)
{
  if (self == NULL || self->cons.car == NULL || !Node_isAtom(self->cons.car)) {
    return ATOM_NONE;
  } else {
    if (self->cons.car->type != ATOM) {
      return ATOM_NONE;
    } else {
      return self->cons.car->atom.type;
    }
  }
}

const char *Node_literalName(Node *self)
{
  if (self->cons.car == NULL || !Node_isLiteral(self->cons.car)) {
    return NULL;
  } else {
    return Node_valueName(self->cons.car);
  }
}

NodeBox *Node_newBox(ParserState *p)
{
  int size = sizeof(NodeBox) + sizeof(Node) * p->node_box_size;
  NodeBox *node_box = (NodeBox *)picorbc_alloc(size);
  memset(node_box, 0, size);
  if (p->current_node_box) p->current_node_box->next = node_box;
  p->current_node_box = node_box;
  node_box->next = NULL;
  node_box->size = p->node_box_size;
  node_box->index = 0;
  return node_box;
}

Node *Node_new(ParserState *p)
{
  NodeBox *box = p->current_node_box;
  box->index++;
  if (box->index == box->size) {
    box = Node_newBox(p);
  }
  return (Node *)((Node *)(&box->nodes) + box->index);
}

void Node_freeAllNode(NodeBox *box)
{
  NodeBox *next;
  while (box) {
    next = box->next;
    picorbc_free(box);
    box = next;
  }
}
