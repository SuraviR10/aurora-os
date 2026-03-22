#ifndef AURORALANG_H
#define AURORALANG_H

#include <stdint.h>
#include <stdbool.h>

#define AST_MAX_NODES 1024
#define MAX_CHILDREN 16

// --- Token Types ---
typedef enum {
    TOK_EOF, TOK_ERROR, TOK_NEWLINE,
    TOK_INT, TOK_FLOAT, TOK_STRING, TOK_BOOL, TOK_NULL,
    TOK_IDENT,
    TOK_LET, TOK_CONST, TOK_FUNCTION, TOK_RETURN,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_LOOP, TOK_FROM, TOK_TO,
    TOK_BREAK, TOK_CONTINUE,
    TOK_PRINT, TOK_INPUT,
    TOK_TRY, TOK_CATCH, TOK_THROW,
    TOK_PARALLEL, TOK_TASK,
    TOK_USE, TOK_SYSTEM,
    TOK_UI, TOK_WINDOW, TOK_BUTTON, TOK_LABEL,
    TOK_SERVER, TOK_ROUTE, TOK_START,
    TOK_CREATE, TOK_WRITE, TOK_READ, TOK_FILE,
    TOK_REWIND, TOK_STEP, TOK_REPLAY,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_ASSIGN, TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LTE, TOK_GTE,
    TOK_AND, TOK_OR, TOK_NOT,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_COMMA, TOK_DOT, TOK_COLON, TOK_SEMICOLON,
    TOK_STR
} token_type_t;

typedef struct {
    token_type_t type;
    char lexeme[64];
    int line;
    int ival;
    double fval;
} token_t;

// --- AST Node Types ---
typedef enum {
    NODE_PROGRAM,
    NODE_BLOCK,
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STR_LIT,
    NODE_BOOL_LIT,
    NODE_NULL_LIT,
    NODE_IDENT,
    NODE_BINOP,
    NODE_UNOP,
    NODE_CAST,
    NODE_CALL,
    NODE_LET,
    NODE_ASSIGN,
    NODE_FUNCTION,
    NODE_IF,
    NODE_WHILE,
    NODE_RETURN,
    NODE_PRINT,
    NODE_CONST_DECL, NODE_LOOP, NODE_BREAK, NODE_CONTINUE,
    NODE_TRY_CATCH, NODE_PARALLEL, NODE_USE, NODE_REWIND,
    NODE_STEP, NODE_INDEX, NODE_SYSTEM_CALL
} node_type_t;

// --- Value Types ---
typedef enum {
    VAL_NULL,
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_ERROR
} val_type_t;

// --- Structs ---

typedef struct {
    val_type_t type;
    int ival;
    double fval;
    char sval[256];
    bool bval;
} value_t;

typedef struct {
    node_type_t type;
    int ival;
    double fval;
    char sval[64];
    int children[16];
    int child_count;
} ast_node_t;

typedef struct {
    ast_node_t nodes[AST_MAX_NODES];
    int count;
} ast_t;

typedef struct {
    char name[64];
    int params[8];
    int param_count;
    int body;
    bool used;
} func_def_t;

typedef struct {
    char name[64];
    value_t val;
} env_var_t;

typedef struct {
    env_var_t vars[64];
    int count;
    int parent_idx; // For scope chain
} env_scope_t;

typedef struct {
    env_scope_t scopes[16];
    int top;
} env_stack_t;

#define MAX_FUNCS 64

typedef struct {
    ast_t *ast;
    env_stack_t env;
    func_def_t funcs[MAX_FUNCS];
    int func_count;
    
    // Control flow flags
    value_t return_val;
    bool returning;
    bool breaking;
    bool continuing;
} interp_t;

typedef struct {
    const char *source;
    int pos;
    int line;
    token_t tokens[AST_MAX_NODES]; // Simple fixed size buffer
    int tok_count;
    int tok_pos;
} lexer_t;

typedef struct {
    lexer_t *lex;
    ast_t *ast;
    bool had_error;
    char error[128];
} parser_t;

// --- Functions ---
void lexer_init(lexer_t *l, const char *source);
void lexer_tokenize(lexer_t *l);
token_t lexer_peek(lexer_t *l);
token_t lexer_next(lexer_t *l);
bool lexer_match(lexer_t *l, token_type_t t);
int parse_program(parser_t *p);

// Environment Helpers (Implemented in auroralang.c or env.c)
void env_push(env_stack_t *env);
void env_pop(env_stack_t *env);
void env_set(env_stack_t *env, const char *name, value_t val, bool is_new);
bool env_get(env_stack_t *env, const char *name, value_t *out);
bool env_assign(env_stack_t *env, const char *name, value_t val);

int aurora_run_string(const char *code);

#endif