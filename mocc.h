#ifdef __mocc_self__

typedef int bool;
typedef int size_t; // ホントは違うとおもうが
typedef void FILE;
typedef void *va_list;

extern void *stderr;
extern int errno;

#define __attribute__(x)
#define noreturn

#define NULL (0)
#define true (1)
#define false (0)
#define static

#else

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

#endif

extern char *user_input;
extern char *input_filename;

void codegen();

typedef struct NodeList NodeList;
typedef struct Node Node;
typedef struct Token Token;
typedef struct Type Type;
typedef struct Var Var;
typedef struct String String;

typedef enum {
  ND_ADD,    // +
  ND_SUB,    // -
  ND_MUL,    // *
  ND_DIV,    // /
  ND_EQ,     // ==
  ND_NE,     // !=
  ND_LT,     // <
  ND_GE,     // >=
  ND_LOGOR,  // ||
  ND_LOGAND, // &&
  ND_LVAR,
  ND_ASSIGN, // =
  ND_COND,   // ?:
  ND_NUM,    // numbers
  ND_RETURN,
  ND_IF,
  ND_SWITCH,
  ND_WHILE,
  ND_FOR,
  ND_BLOCK,
  ND_CALL,
  ND_FUNCDECL,
  ND_DEREF,   // unary *
  ND_ADDR,    // unary &
  ND_NOT,     // !
  ND_VARDECL, //
  ND_GVARDECL,
  ND_GVAR,
  ND_STRING,
  ND_BREAK,
  ND_CONTINUE,
  ND_CASE,
  ND_DEFAULT,
  ND_NOP,
  ND_MEMBER,
  ND_POSTINC,
} NodeKind;

struct Node {
  NodeKind kind;

  Node *lhs; // ND_IF, ND_WHILE のときは expr
  Node *rhs; // ND_IF, ND_WHILE のときは stmt
  Node *node3; // ND_IF のときは else, ND_FOR のときは i++ みたいなとこ
  Node *node4; // ND_FOR のときのみ stmt

  // ND_BLOCK のとき: stmt, ...
  // ND_CALL のとき: params = expr, ...
  // ND_FUNCDECL のとき: args = ident, ...
  // ND_GVARDECL のときは initializer = { expr, ... } （rhs == NULL
  // のときにかぎる）
  NodeList *nodes;

  // あとで locals とかにしたいかも？？
  // すくなくとも LVar *にしたい気がする
  NodeList *args;

  // ND_CALL, ND_FUNCDECL のときだけ。関数名
  // そのうち関数の使用に宣言が必要になったら
  // これも Func * みたいなものになりそう (cf. lvar, gvar)
  Token *ident;

  int val;   // used when kind == ND_NUM
             // あと ND_STRING のとき str_lits の index
  Var *lvar; // ND_LVAR || ND_VARDECL
  Var *gvar; // ND_VARDECL かつトップレベル
  Var *locals;

  int label_index; // ND_WHILE || ND_FOR

  char *source_pos; // デバッグ用
  int source_len;   // デバッグ用
};

// ND_FUNCDECL の仮引数
// ND_CALL の実引数
// ND_BLOCK の stmt*
struct NodeList {
  Node *node;
  NodeList *next;
};

typedef struct Scope Scope;

struct Scope {
  Scope *parent;
  Node *node;
  // ここにブロック中のローカル変数も出てくるかもしれない
};

extern Scope *curr_scope;

void scope_create(Node *node);
void scope_push(Node *node);
void scope_pop();

void parse_program();
extern Node *code[100];
char *node_kind_to_str(NodeKind kind);
Node *new_node(NodeKind kind, Node *lhs, Node *rhs);
char *type_to_str(Type *type);

typedef enum {
  TK_PUNCT,
  TK_RETURN,
  TK_IF,
  TK_ELSE,
  TK_SWITCH,
  TK_CASE,
  TK_DEFAULT,
  TK_WHILE,
  TK_FOR,
  TK_IDENT,
  TK_NUM,
  TK_EOF,
  TK_TYPE,
  TK_SIZEOF,
  TK_STRING,
  TK_BREAK,
  TK_CONTINUE,
  TK_STRUCT,
  TK_ENUM,
  TK_TYPEDEF,
  TK_EXTERN,
} TokenKind;

struct Token {
  TokenKind kind;
  Token *next;

  int val;

  // ソースコード上の位置
  char *str;
  int len;
};

extern Token *curr_token;
extern Token *prev_token;

void tokenize(char *p);

Token *token_consume(TokenKind kind);
bool token_consume_punct(char *op);
bool token_consume_type(char *type);
bool token_at_eof();
void token_expect_punct(char *op);
int token_expect_number();

typedef enum TypeKind {
  TY_CHAR,
  TY_INT,
  TY_PTR,
  TY_VOID,
  TY_ARRAY,
  TY_STRUCT,
  TY_ENUM,
  TY_TYPEDEF,
} TypeKind;

struct Type {
  TypeKind ty;
  Type *base;
  Type *next;

  size_t array_size; // ty == ARRAY
  Var *members;      // ty == STRUCT

  // 定義されたものはこれ
  char *name;
  int name_len; // name の長さ
};

extern Type defined_types;

// ローカル変数、構造体メンバ、グローバル変数
struct Var {
  Var *next;
  char *name;
  int len; // name の長さ
  Type *type;
  int offset; // メモリ上のオフセット。ローカル変数の場合
              // fp からの位置、構造体のメンバの場合は先頭からの位置
  Node *const_val; // 定数のときのみ。 なんなら enum のみ。
  bool is_struct_member;

  bool is_extern;
};

// 文字列リテラル!!!
struct String {
  String *next;
  char *str;
  int len;
  int index;
};

extern Var globals;
extern Var constants;
extern String strings;

int sizeof_type(Type *type);
Type *typeof_node(Node *node);

char *type_to_string(Type *type);

Var *add_var(Var *head, char *name, int len, Type *type, bool is_extern);
Var *find_var(Var *head, char *name, int len);
Var *find_var_in_curr_scope(char *name, int len);

String *add_string(char *str, int len);

Type *add_or_find_defined_type(Type *type);
Type *find_defined_type(char *name, int len);

noreturn void error(char *fmt, ...) __attribute__((format(printf, 1, 2)));
noreturn void error_at(char *loc, char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
