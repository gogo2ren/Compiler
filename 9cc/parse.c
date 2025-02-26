#include "9cc.h"


static Node *compound_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

static Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_unary(NodeKind kind, Node *expr) {
  Node *node = new_node(kind);
  node->lhs = expr;
  return node;
}

static Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  return node;
}

static Node *new_var_node(char name) {
  Node *node = new_node(ND_VAR);
  node->name = name;
  return node;
}

// stmt    = expr ";" | "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
static Node *stmt(Token **rest, Token *tok) {

  Node *node;
  if (tok->kind == TK_KEYWORD) {
    if(equal(tok,"return"))
    {
      Node *node = new_unary(ND_RETURN, expr(&tok, tok->next));
      *rest = skip(tok, ";");
      return node;
    }
    if (equal(tok, "if")) {
      Node *node = new_node(ND_IF);
      tok = skip(tok->next, "(");
      node->cond = expr(&tok, tok);
      tok = skip(tok, ")");
      node->then = stmt(&tok, tok);
      if (equal(tok, "else"))
        node->els = stmt(&tok, tok->next);
      *rest = tok;
      return node;
    }
    if (equal(tok, "while")) {
      Node *node = new_node(ND_LOOP);
      tok = skip(tok->next, "(");
      node->cond = expr(&tok, tok);
      tok = skip(tok, ")");
      node->then = stmt(rest, tok);
      return node;
    }
    if (equal(tok, "for")) {
      Node *node = new_node(ND_LOOP);
      tok = skip(tok->next, "(");
  
      node->init = expr_stmt(&tok, tok);
  
      if (!equal(tok, ";"))
        node->cond = expr(&tok, tok);
      tok = skip(tok, ";");
  
      if (!equal(tok, ")"))
        node->inc = expr(&tok, tok);
      tok = skip(tok, ")");
  
      node->then = stmt(rest, tok);
      return node;
    }
  }
  if (equal(tok, "{"))
  return compound_stmt(rest, tok->next);

  return expr_stmt(rest, tok);
}

// compound-stmt = stmt* "}"
static Node *compound_stmt(Token **rest, Token *tok) {
  Node head = {};
  Node *cur = &head;
  while (!equal(tok, "}"))
    cur = cur->next = stmt(&tok, tok);

  Node *node = new_node(ND_BLOCK);
  node->body = head.next;
  *rest = tok->next;
  return node;
}

// expr-stmt = expr ";"
static Node *expr_stmt(Token **rest, Token *tok) {
  if (equal(tok, ";")) {
    *rest = tok->next;
    return new_node(ND_BLOCK);
  }

  Node *node = new_unary(ND_EXPR_STMT, expr(&tok, tok));
  *rest = skip(tok, ";");
  return node;
}

// expr = assign
static Node *expr(Token **rest, Token *tok) {
  return assign(rest, tok);
}

// assign = equality ("=" assign)?
static Node *assign(Token **rest, Token *tok) {
  Node *node = equality(&tok, tok);
  if (equal(tok, "="))
    node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next));
  *rest = tok;
  return node;
}

// equality = relational ("==" relational | "!=" relational)*
static Node *equality(Token **rest, Token *tok) {
  Node *node = relational(&tok, tok);

  for (;;) {
    if (equal(tok, "==")) {
      node = new_binary(ND_EQ, node, relational(&tok, tok->next));
      continue;
    }

    if (equal(tok, "!=")) {
      node = new_binary(ND_NE, node, relational(&tok, tok->next));
      continue;
    }

    *rest = tok;
    return node;
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(Token **rest, Token *tok) {
  Node *node = add(&tok, tok);

  for (;;) {
    if (equal(tok, "<")) {
      node = new_binary(ND_LT, node, add(&tok, tok->next));
      continue;
    }

    if (equal(tok, "<=")) {
      node = new_binary(ND_LE, node, add(&tok, tok->next));
      continue;
    }

    if (equal(tok, ">")) {
      node = new_binary(ND_LT, add(&tok, tok->next), node);
      continue;
    }

    if (equal(tok, ">=")) {
      node = new_binary(ND_LE, add(&tok, tok->next), node);
      continue;
    }

    *rest = tok;
    return node;
  }
}

// add = mul ("+" mul | "-" mul)*
static Node *add(Token **rest, Token *tok) {
  Node *node = mul(&tok, tok);

  for (;;) {
    if (equal(tok, "+")) {
      node = new_binary(ND_ADD, node, mul(&tok, tok->next));
      continue;
    }

    if (equal(tok, "-")) {
      node = new_binary(ND_SUB, node, mul(&tok, tok->next));
      continue;
    }

    *rest = tok;
    return node;
  }
}

// mul = unary ("*" unary | "/" unary)*
static Node *mul(Token **rest, Token *tok) {
  Node *node = unary(&tok, tok);

  for (;;) {
    if (equal(tok, "*")) {
      node = new_binary(ND_MUL, node, unary(&tok, tok->next));
      continue;
    }

    if (equal(tok, "/")) {
      node = new_binary(ND_DIV, node, unary(&tok, tok->next));
      continue;
    }

    *rest = tok;
    return node;
  }
}

// unary = ("+" | "-") unary
//       | primary
static Node *unary(Token **rest, Token *tok) {
  if (equal(tok, "+"))
    return unary(rest, tok->next);

  if (equal(tok, "-"))
    return new_unary(ND_NEG, unary(rest, tok->next));

  return primary(rest, tok);
}

// primary = "(" expr ")" | ident | num
static Node *primary(Token **rest, Token *tok) {
  if (equal(tok, "(")) {
    Node *node = expr(&tok, tok->next);
    *rest = skip(tok, ")");
    return node;
  }

  if (tok->kind == TK_IDENT) {
    Node *node = new_var_node(*tok->loc);
    *rest = tok->next;
    return node;
  }

  if (tok->kind == TK_NUM) {
    Node *node = new_num(tok->val);
    *rest = tok->next;
    return node;
  }

  error_tok(tok, "expected an expression");
}

// program = stmt*
Node *parse(Token *tok) {
  Node head = {};
  Node *cur = &head;
  if(!equal(tok, "{"))
  {
    cur = cur->next = stmt(&tok, tok);
}
  else{
    tok = skip(tok, "{");
    cur = cur->next = compound_stmt(&tok, tok);
}
  return head.next;
}