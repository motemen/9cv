#include "9cv.h"
#include <stdio.h>

void codegen_pop_t1() {
  printf("  # pop t1\n");
  printf("  lw t1, 0(sp)\n");
  printf("  addi sp, sp, 4\n");
}

void codegen_pop_t0() {
  printf("  # pop t0\n");
  printf("  lw t0, 0(sp)\n");
  printf("  addi sp, sp, 4\n");
}

void codegen_push_t0() {
  printf("  # push t0\n");
  printf("  sw t0, -4(sp)\n");
  printf("  addi sp, sp, -4\n");
}

void codegen_visit(Node *node) {
  switch (node->kind) {
  case ND_NUM:
    printf("  li t0, %d\n", node->val);

    // push
    codegen_push_t0();
    return;

  case ND_LT:
    codegen_visit(node->lhs);
    codegen_visit(node->rhs);

    // pop rhs -> t1
    codegen_pop_t1();
    // pop lhs -> t0
    codegen_pop_t0();

    printf("  slt t0, t0, t1\n");

    // push
    codegen_push_t0();
    return;

  case ND_GE:
    codegen_visit(node->lhs);
    codegen_visit(node->rhs);

    // pop rhs -> t1
    codegen_pop_t1();

    // pop lhs -> t0
    codegen_pop_t0();

    printf("  slt t0, t0, t1\n");
    printf("  xori t0, t0, 1\n");

    // push
    codegen_push_t0();
    return;

  case ND_ADD:
  case ND_SUB:
  case ND_MUL:
  case ND_DIV:
    codegen_visit(node->lhs);
    codegen_visit(node->rhs);

    // pop rhs -> t1
    codegen_pop_t1();
    // pop lhs -> t0
    codegen_pop_t0();

    if (node->kind == ND_ADD) {
      printf("  add t0, t0, t1\n");
    } else if (node->kind == ND_SUB) {
      printf("  sub t0, t0, t1\n");
    } else if (node->kind == ND_MUL) {
      printf("  mul t0, t0, t1\n");
    } else if (node->kind == ND_DIV) {
      printf("  div t0, t0, t1\n");
    } else {
      error("unreachable");
    }

    // push
    codegen_push_t0();
    return;

  case ND_EQ:
  case ND_NE:
    codegen_visit(node->lhs);
    codegen_visit(node->rhs);

    // pop rhs -> t1
    codegen_pop_t1();
    // pop lhs -> t0
    codegen_pop_t0();

    printf("  xor t0, t0, t1\n");
    if (node->kind == ND_EQ) {
      printf("  seqz t0, t0\n");
    } else {
      printf("  snez t0, t0\n");
    }

    // push
    codegen_push_t0();
    return;

  case ND_LVAR:
    printf("  lw t0, -%d(sp)\n", node->offset);
    codegen_push_t0();
    return;

  case ND_ASSIGN:
    if (node->lhs->kind != ND_LVAR) {
      error("not an lvalue");
    }

    codegen_visit(node->rhs);

    codegen_pop_t0();

    printf("  sw t0, -%d(sp)\n", node->lhs->offset);

    codegen_push_t0();

    return;
  }

  error("codegen not implemented: %s", node_kind_to_str(node->kind));
}
