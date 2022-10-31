#include "codegen.h"
#include "9cv.h"
#include "util.h"
#include <assert.h>
#include <stdio.h>

// ターゲットの ISA は RISCV64GC ということにする
// https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf
// int のサイズは 4 bytes
// void * のサイズは 8 bytes

// これ addi 4 にしたら死んだ
void codegen_pop_t0() {
  printf("  # pop t0\n");
  printf("  ld t0, 0(sp)\n");
  printf("  addi sp, sp, 8\n");
}

void codegen_pop_t1() {
  printf("  # pop t1\n");
  printf("  ld t1, 0(sp)\n");
  printf("  addi sp, sp, 8\n");
}

void codegen_pop_discard() {
  printf("  # pop\n");
  printf("  addi sp, sp, 8\n");
}

void codegen_push_t0() {
  printf("  # push t0\n");
  printf("  sd t0, -8(sp)\n");
  printf("  addi sp, sp, -8\n");
}

// FIXME: これの存在おかしい気がする。pop しすぎでは？
// codegen を codegen_expr と codegen_stmt にわけるのがよさそう
void codegen_push_dummy() {
  printf("  # push dummy\n");
  printf("  addi sp, sp, -8\n");
}

// TODO: ローカル変数をなんとかしないといけないのだが…
void codegen_prologue() {
  int num_locals = 0;
  LVar *l = locals;
  while (l) {
    num_locals++;
    l = l->next;
  }

  printf("  # Prologue\n");
  printf("  sd ra, 0(sp)\n");  // ra を保存
  printf("  sd fp, -8(sp)\n"); // fp を保存
  printf("  addi fp, sp, -8\n");
  // スタックポインタを移動。関数を抜けるまで動かない
  printf("  addi sp, sp, -%d\n", 8 * num_locals + 8 /* for saved fp */);
  printf("\n");
}

// a0 に返り値を設定してから呼ぶこと
void codegen_epilogue() {
  int num_locals = 0;
  LVar *l = locals;
  while (l) {
    num_locals++;
    l = l->next;
  }

  printf("\n");
  printf("  # Epilogue\n");
  // sp を戻す
  printf("  addi sp, sp, %d\n", 8 * num_locals + 8);
  // fp も戻す
  // ここ lw にしたらおかしくなった
  printf("  ld fp, -8(sp)\n");
  // ra も戻す
  printf("  ld ra, 0(sp)\n");

  printf("  ret\n");
}

int label_index = 0;

// LVar である node のアドレスを push する
// または、ポインタの deref を計算してアドレスを push する
void codegen_push_lvalue(Node *node) {
  if (node->kind == ND_LVAR) {
    printf("  # address for '%.*s'\n", node->source_len, node->source_pos);
    printf("  addi t0, fp, -%d\n", node->lvar->offset);
    codegen_push_t0();
    return;
  }

  // *y -> y の値をアドレスとして push する
  // **z -> (*z) の値をアドレスとして push する
  if (node->kind == ND_DEREF) {
    printf("  # deref\n");
    codegen_visit(node->lhs);
    return;
  }

  error_at(node->source_pos, "not an lvalue: %s", node_kind_to_str(node->kind));
}

void codegen_visit(Node *node) {
  int lindex;

  switch (node->kind) {
  case ND_NUM:
    printf("  # constant '%d'\n", node->val);
    printf("  li t0, %d\n", node->val);

    // push
    codegen_push_t0();
    return;

  case ND_LT:
    codegen_visit(node->lhs); // -> t0
    codegen_visit(node->rhs); // -> t1

    codegen_pop_t1();
    codegen_pop_t0();

    printf("  slt t0, t0, t1\n");

    codegen_push_t0();
    return;

  case ND_GE:
    codegen_visit(node->lhs); // -> t0
    codegen_visit(node->rhs); // -> t1

    codegen_pop_t1();
    codegen_pop_t0();

    printf("  slt t0, t0, t1\n");
    printf("  xori t0, t0, 1\n");

    codegen_push_t0();
    return;

  case ND_ADD:
  case ND_SUB: {
    int ptr_size = 0;
    if (node->lhs->kind == ND_LVAR && node->lhs->lvar->type->ty == PTR) {
      if (node->lhs->lvar->type->ptr_to->ty == INT) {
        ptr_size = 4;
      } else {
        ptr_size = 8;
      }
    }

    codegen_visit(node->lhs); // -> t0
    codegen_visit(node->rhs); // -> t1

    codegen_pop_t1();
    if (ptr_size > 0) {
      printf("  # do pointer arithmetic\n");
      printf("  li t3, %d\n", ptr_size);
      printf("  mul t1, t1, t3\n");
    }

    codegen_pop_t0();

    if (node->kind == ND_ADD) {
      printf("  add t0, t0, t1\n");
    } else if (node->kind == ND_SUB) {
      printf("  sub t0, t0, t1\n");
    } else {
      assert(0);
    }

    codegen_push_t0();
    return;
  }

  case ND_MUL:
  case ND_DIV:
    codegen_visit(node->lhs); // -> t0
    codegen_visit(node->rhs); // -> t1

    codegen_pop_t1();
    codegen_pop_t0();

    if (node->kind == ND_MUL) {
      printf("  mul t0, t0, t1\n");
    } else if (node->kind == ND_DIV) {
      printf("  div t0, t0, t1\n");
    } else {
      assert(0);
    }

    codegen_push_t0();
    return;

  case ND_EQ:
  case ND_NE:
    codegen_visit(node->lhs); // -> t0
    codegen_visit(node->rhs); // -> t1

    codegen_pop_t1();
    codegen_pop_t0();

    printf("  xor t0, t0, t1\n");
    if (node->kind == ND_EQ) {
      printf("  seqz t0, t0\n");
    } else {
      printf("  snez t0, t0\n");
    }

    codegen_push_t0();
    return;

  case ND_LVAR:
    codegen_push_lvalue(node);
    codegen_pop_t0();
    printf("  ld t0, 0(t0)\n");
    codegen_push_t0();
    return;

  case ND_ASSIGN:
    printf("  # ND_ASSIGN {\n");
    // -> t0
    codegen_push_lvalue(node->lhs);
    // -> t1
    codegen_visit(node->rhs);

    codegen_pop_t1();
    codegen_pop_t0();

    printf("  # assign to variable '%.*s'\n", node->lhs->source_len,
           node->lhs->source_pos);
    printf("  sd t1, 0(t0)\n");
    printf("  mv t0, t1\n");

    codegen_push_t0();
    printf("  # ND_ASSIGN }\n");
    return;

  case ND_RETURN:
    printf("  # ND_RETURN LHS {\n");
    codegen_visit(node->lhs);
    printf("  # ND_RETURN LHS }\n");
    codegen_pop_t0();

    printf("  mv a0, t0\n");
    codegen_epilogue();

    return;

  case ND_IF:
    lindex = ++label_index;

    printf("  # if {\n");

    codegen_visit(node->lhs);
    codegen_pop_t0();

    printf("  beqz t0, .Lelse%03d\n", lindex);

    // if {
    codegen_visit(node->rhs);
    codegen_pop_discard();
    printf("  j .Lend%03d\n", lindex);
    // }

    // else {
    printf(".Lelse%03d:\n", lindex);
    if (node->node3) {
      codegen_visit(node->node3);
      codegen_pop_discard();
    }
    // }

    printf(".Lend%03d:\n", lindex);

    codegen_push_dummy();

    printf("  # if }\n");
    printf("\n");

    return;

  case ND_WHILE:
    lindex = ++label_index;

    printf(".Lbegin%03d:\n", lindex);

    codegen_visit(node->lhs);
    codegen_pop_t0();

    printf("  beqz t0, .Lend%03d\n", lindex);

    codegen_visit(node->rhs); // { ... }
    codegen_pop_discard();

    printf("  j .Lbegin%03d\n", lindex);

    printf(".Lend%03d:\n", lindex);

    codegen_push_dummy();

    return;

  case ND_FOR:
    lindex = ++label_index;

    if (node->lhs) {
      codegen_visit(node->lhs);
      codegen_pop_discard();
    }

    printf(".Lbegin%03d:\n", lindex);

    if (node->rhs) {
      codegen_visit(node->rhs);
      codegen_pop_t0();
      printf("  beqz t0, .Lend%03d\n", lindex);
    }

    if (node->node4) {
      codegen_visit(node->node4); // { ... }
      codegen_pop_discard();
    }

    // i++ みたいなとこ
    if (node->node3) {
      codegen_visit(node->node3);
      codegen_pop_discard();
    }

    printf("j .Lbegin%03d\n", lindex);

    printf(".Lend%03d:\n", lindex);

    codegen_push_dummy();

    return;

  case ND_BLOCK:
    for (NodeList *n = node->nodes; n; n = n->next) {
      codegen_visit(n->node);
      codegen_pop_discard();
    }

    codegen_push_dummy();

    return;

  case ND_CALL: {
    int arg_count = 0;
    for (NodeList *n = node->nodes; n; n = n->next) {
      codegen_visit(n->node);
      codegen_pop_t0();
      printf("  mv a%d, t0\n", arg_count++);
    }

    // これいるか？？ sp はどういう状態で引き渡せばいいんだ
    printf("  addi sp, sp, -8\n");
    printf("  call %.*s\n", node->name_len, node->name);
    printf("  addi sp, sp, 8\n");

    // 結果は a0 に入っているよな
    printf("  # push a0\n");
    printf("  sd a0, -8(sp)\n");
    printf("  addi sp, sp, -8\n");
    return;
  }

  case ND_FUNCDECL:
    printf("\n");
    printf("%.*s:\n", node->name_len, node->name);

    codegen_prologue();

    int arg_count = 0;
    for (NodeList *a = node->args; a; a = a->next) {
      // a0 を lvar xyz に代入するみたいなことをする
      printf("  # assign to argument '%.*s'\n", a->node->source_len,
             a->node->source_pos);
      printf("  sd a%d, -%d(fp)\n", arg_count, a->node->lvar->offset);

      arg_count++;
    }

    for (NodeList *n = node->nodes; n; n = n->next) {
      codegen_visit(n->node);
      codegen_pop_t0();
    }

    printf("  mv a0, zero\n");
    codegen_epilogue();

    return;

  case ND_DEREF:
    printf("  # ND_DEREF {\n");
    // codegen_push_lvalue(node->lhs);
    codegen_visit(node->lhs);
    codegen_pop_t0();
    printf("  ld t0, 0(t0)\n");
    codegen_push_t0();
    printf("  # ND_DEREF }\n");

    return;

  case ND_ADDR:
    codegen_push_lvalue(node->lhs);

    return;

  case ND_VARDECL:
    // なにもしない
    printf("  # vardecl '%.*s' offset=%d\n", node->lvar->len, node->lvar->name,
           node->lvar->offset);
    codegen_push_dummy();
    return;
  }

  error_at(node->source_pos, "codegen not implemented: %s",
           node_kind_to_str(node->kind));
}
