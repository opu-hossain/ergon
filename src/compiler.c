#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ergon/chunk.h"
#include "ergon/common.h"
#include "ergon/compiler.h"
#include "ergon/object.h"
#include "ergon/scanner.h"
#include "ergon/table.h"
#include "ergon/value.h"

#ifdef DEBUG_PRINT_CODE
#include "ergon/debug.h"
#endif

typedef struct {
  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT,
  PREC_OR,
  PREC_AND,
  PREC_EQUALITY,
  PREC_COMPARISON,
  PREC_TERM,
  PREC_FACTOR,
  PREC_UNARY,
  PREC_CALL,
  PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool can_assign);

typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;
} ParseRule;

typedef struct {
  Token name;
  int depth;
  bool is_const;
} Local;

typedef struct {
  Local locals[UINT8_COUNT];
  int local_count;
  int scope_depth;
} Compiler;

Parser parser;
Chunk *compiling_chunk;
Compiler *current = NULL;

static Table string_constants;

static Chunk *current_chunk() { return compiling_chunk; }

static void error_at(Token *token, const char *message) {
  if (parser.panic_mode)
    return;
  parser.panic_mode = true;
  fprintf(stderr, "[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }

  fprintf(stderr, ": %s\n", message);
  parser.had_error = true;
}

static void error(const char *message) { error_at(&parser.previous, message); }

static void error_at_current(const char *message) {
  error_at(&parser.current, message);
}

static void advance() {
  parser.previous = parser.current;
  for (;;) {
    parser.current = scan_token();
    if (parser.current.type != TOKEN_ERROR)
      break;

    error_at_current(parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }

  error_at_current(message);
}

static bool check(TokenType type) { return parser.current.type == type; }

static bool match(TokenType type) {
  if (!check(type))
    return false;
  advance();
  return true;
}

static void emit_byte(uint8_t byte) {
  write_chunk(current_chunk(), byte, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
  emit_byte(byte1);
  emit_byte(byte2);
}

static void emit_return() { emit_byte(OP_RETURN); }

static uint8_t make_constant(Value value) {
  int constant = add_constant(current_chunk(), value);
  if (constant > UINT8_MAX) {
    error("Too many constatns in one chunk.");
    return 0;
  }

  return (uint8_t)constant;
}

static void emit_constant(Value value) {
  emit_bytes(OP_CONSTANT, make_constant(value));
}

static void init_compiler(Compiler *compiler) {
  compiler->local_count = 0;
  compiler->scope_depth = 0;
  current = compiler;
}

static void end_compiler() {
  emit_return();
#ifdef DEBUG_PRINT_CODE
  if (!parser.had_error) {
    disassemble_chunk(current_chunk(), "code");
  }
#endif
}

static void begin_scope() { current->scope_depth++; }

static void end_scope() {
  current->scope_depth--;

  while (current->local_count > 0 &&
         current->locals[current->local_count - 1].depth >
             current->scope_depth) {
    emit_byte(OP_POP);
    current->local_count--;
  }
}

// some forward declaration
static void expression();
static ParseRule *get_rule(TokenType type);
static void parse_precedence(Precedence precedence);
static void statement();
static void declaration();
static uint8_t identifier_constant(Token *name);
static uint8_t identifier_global(Token *name);
static int resolve_local(Compiler *compiler, Token *name);
// end

static void binary(bool can_assign) {
  TokenType oparator_type = parser.previous.type;
  ParseRule *rule = get_rule(oparator_type);
  parse_precedence((Precedence)(rule->precedence + 1));

  switch (oparator_type) {
  case TOKEN_BANG_EQUAL:
    emit_byte(OP_NOT_EQUAL);
    break;
  case TOKEN_EQUAL_EQUAL:
    emit_byte(OP_EQUAL);
    break;
  case TOKEN_GREATER:
    emit_byte(OP_GREATER);
    break;
  case TOKEN_GREATER_EQUAL:
    emit_byte(OP_GREATER_EQUAL);
    break;
  case TOKEN_LESS:
    emit_byte(OP_LESS);
    break;
  case TOKEN_LESS_EQUAL:
    emit_byte(OP_LESS_EQUAL);
    break;
  case TOKEN_PLUS:
    emit_byte(OP_ADD);
    break;
  case TOKEN_MINUS:
    emit_byte(OP_SUBTRACT);
    break;
  case TOKEN_STAR:
    emit_byte(OP_MULTIPLY);
    break;
  case TOKEN_SLASH:
    emit_byte(OP_DEVIDE);
    break;
  default:
    return; // unreachable
  }
}

static void literal(bool can_assign) {
  switch (parser.previous.type) {
  case TOKEN_FALSE:
    emit_byte(OP_FALSE);
    break;
  case TOKEN_NIL:
    emit_byte(OP_NIL);
    break;

  case TOKEN_TRUE:
    emit_byte(OP_TRUE);
    break;
  default:
    return;
  }
}

static void grouping(bool can_assign) {
  expression();
  consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool can_assign) {
  double value = strtod(parser.previous.start, NULL);
  emit_constant(NUMBER_VAL(value));
}

static void string(bool can_assign) {
  emit_constant(OBJ_VAL(
      copy_string(parser.previous.start + 1, parser.previous.length - 2)));
}

static void named_variable(Token name, bool can_assign) {
  uint8_t get_op, set_op;
  int arg = resolve_local(current, &name);
  bool is_const;

  if (arg != -1) {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;
    is_const = current->locals[arg].is_const;
  } else {
    arg = identifier_global(&name);
    get_op = OP_GET_GLOBAL;
    set_op = OP_SET_GLOBAL;
    is_const = vm.global_values.globals[arg].is_const;
  }

  if (can_assign && match(TOKEN_EQUAL)) {
    if (is_const) {
      error("Cannot assign to a constant variable.");
    }
    expression();
    emit_bytes(set_op, (uint8_t)arg);
  } else {
    emit_bytes(get_op, (uint8_t)arg);
  }
}

static void variable(bool can_assign) {
  named_variable(parser.previous, can_assign);
}

static void unary(bool can_assign) {
  TokenType oparator_type = parser.previous.type;

  parse_precedence(PREC_UNARY);

  switch (oparator_type) {
  case TOKEN_MINUS:
    emit_byte(OP_NEGATE);
    break;
  case TOKEN_BANG:
    emit_byte(OP_NOT);
    break;
  default:
    return;
  }
}

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = {grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = {NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = {NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = {NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = {NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = {unary, binary, PREC_TERM},
    [TOKEN_PLUS] = {NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = {NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = {NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = {NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = {unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = {NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = {NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = {NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = {variable, NULL, PREC_NONE},
    [TOKEN_STRING] = {string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = {number, NULL, PREC_NONE},
    [TOKEN_AND] = {NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = {NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = {NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = {literal, NULL, PREC_NONE},
    [TOKEN_FOR] = {NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = {NULL, NULL, PREC_NONE},
    [TOKEN_IF] = {NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = {literal, NULL, PREC_NONE},
    [TOKEN_OR] = {NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = {NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = {NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = {NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = {NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = {literal, NULL, PREC_NONE},
    [TOKEN_VAR] = {NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = {NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = {NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = {NULL, NULL, PREC_NONE},
};

static void parse_precedence(Precedence precedence) {
  advance();
  ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
  if (prefix_rule == NULL) {
    error("Expect expression");
    return;
  }

  bool can_assign = precedence <= PREC_ASSIGNMENT;
  prefix_rule(can_assign);

  while (precedence <= get_rule(parser.current.type)->precedence) {
    advance();
    ParseFn infix_rule = get_rule(parser.previous.type)->infix;
    infix_rule(can_assign);
  }

  if (can_assign && match(TOKEN_EQUAL)) {
    error("Invalid assignment target.");
  }
}

static uint8_t identifier_constant(Token *name) {
  Obj_string *identifier = copy_string(name->start, name->length);

  Value index;
  if (table_get(&string_constants, identifier, &index)) {
    return (uint8_t)AS_NUMBER(index);
  }
  uint8_t constant = make_constant(OBJ_VAL(identifier));
  table_set(&string_constants, identifier, NUMBER_VAL((double)constant));
  return constant;
}

static uint8_t identifier_global(Token *name) {
  Obj_string *identifier = copy_string(name->start, name->length);

  Value index;
  if (table_get(&vm.global_names, identifier, &index)) {
    return (uint8_t)AS_NUMBER(index);
  }

  if (vm.global_values.count == UINT8_COUNT) {
    error("Too many global variables.");
    return 0;
  }

  Global global = {.name = identifier, .value = NIL_VAL, .is_defined = false};
  write_global_array(&vm.global_values, global);

  uint8_t slot = (uint8_t)(vm.global_values.count - 1);
  table_set(&vm.global_names, identifier, NUMBER_VAL((double)slot));
  return slot;
}

static bool identifiers_equal(Token *a, Token *b) {
  if (a->length != b->length)
    return false;
  return memcmp(a->start, b->start, a->length) == 0;
}

static int resolve_local(Compiler *compiler, Token *name) {
  for (int i = compiler->local_count - 1; i >= 0; i--) {
    Local *local = &compiler->locals[i];
    if (identifiers_equal(name, &local->name)) {
      if (local->depth == -1) {
        error("Can't read local variable in its own initializer.");
      }
      return i;
    }
  }
  return -1;
}

static void add_local(Token name) {
  if (current->local_count == UINT8_COUNT) {
    error("Too many local variables in function.");
    return;
  }
  Local *local = &current->locals[current->local_count++];
  local->name = name;
  local->depth = -1;
  local->is_const = false;
}

static void declare_varable() {
  if (current->scope_depth == 0)
    return;
  Token *name = &parser.previous;
  for (int i = current->local_count - 1; i >= 0; i--) {
    Local *local = &current->locals[i];
    if (local->depth != -1 && local->depth < current->scope_depth) {
      break;
    }
    if (identifiers_equal(name, &local->name)) {
      error("ALready a variable with the same name in this scope.");
    }
  }
  add_local(*name);
}

static uint8_t parse_variable(const char *error_message) {
  consume(TOKEN_IDENTIFIER, error_message);
  declare_varable();
  if (current->scope_depth > 0)
    return 0;
  return identifier_global(&parser.previous);
}

static void mark_initialized() {
  current->locals[current->local_count - 1].depth = current->scope_depth;
}

static void define_variable(uint8_t global) {
  if (current->scope_depth > 0) {
    mark_initialized();
    return;
  }
  emit_bytes(OP_DEFINE_GLOBAL, global);
}

static ParseRule *get_rule(TokenType type) { return &rules[type]; }

static void expression() { parse_precedence(PREC_ASSIGNMENT); }

static void block() {
  while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
    declaration();
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void const_declaration() {
  uint8_t global = parse_variable("Expect constant name.");

  consume(TOKEN_EQUAL, "Constant declaration requires an initializer.");
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after constant declaration.");

  if (current->scope_depth > 0) {
    current->locals[current->local_count - 1].is_const = true;
  } else {
    vm.global_values.globals[global].is_const = true;
  }
  define_variable(global);
}

static void var_declaration() {
  uint8_t global = parse_variable("Expect variable name.");

  if (match(TOKEN_EQUAL)) {
    expression();
  } else {
    emit_byte(OP_NIL);
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  define_variable(global);
}

static void expression_statement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  emit_byte(OP_POP);
}

static void print_statement() {
  expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  emit_byte(OP_PRINT);
}

static void synchoronize() {
  parser.panic_mode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON)
      return;
    switch (parser.current.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:; // Do nothing
    }
    advance();
  }
}

static void declaration() {
  if (match(TOKEN_VAR)) {
    var_declaration();
  } else if (match(TOKEN_CONST)) {
    const_declaration();
  } else {
    statement();
  }
  if (parser.panic_mode)
    synchoronize();
}

static void statement() {
  if (match(TOKEN_PRINT)) {
    print_statement();
  } else if (match(TOKEN_LEFT_BRACE)) {
    begin_scope();
    block();
    end_scope();
  } else {
    expression_statement();
  }
}

bool compile(const char *source, Chunk *chunk) {
  init_scanner(source);
  Compiler compiler;
  init_compiler(&compiler);
  init_table(&string_constants);

  compiling_chunk = chunk;
  parser.had_error = false;
  parser.panic_mode = false;

  advance();

  while (!match(TOKEN_EOF)) {
    declaration();
  }
  end_compiler();
  free_table(&string_constants);
  return !parser.had_error;
}
