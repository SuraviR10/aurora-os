#include "auroralang.h"
#include "string.h"

/* ═══════════════════════════════════════════════════════════════════
   AURORALANG  PARSER  –  Recursive Descent
   ═══════════════════════════════════════════════════════════════════ */

/* ── AST helpers ───────────────────────────────────────────────────── */
int ast_alloc(ast_t *a, node_type_t t) {
    if (a->count >= AST_MAX_NODES) return -1;
    int idx = a->count++;
    kmemset(&a->nodes[idx], 0, sizeof(ast_node_t));
    a->nodes[idx].type = t;
    return idx;
}

void ast_add_child(ast_t *a, int parent, int child) {
    if (parent < 0 || child < 0) return;
    ast_node_t *p = &a->nodes[parent];
    if (p->child_count < MAX_CHILDREN)
        p->children[p->child_count++] = child;
}

/* ── Parser helpers ────────────────────────────────────────────────── */
static token_t p_peek(parser_t *p)  { return lexer_peek(p->lex); }
static token_t p_next(parser_t *p)  { return lexer_next(p->lex); }
static bool    p_check(parser_t *p, token_type_t t) { return p_peek(p).type == t; }

static token_t p_expect(parser_t *p, token_type_t t, const char *msg) {
    token_t tk = p_peek(p);
    if (tk.type != t) {
        ksnprintf(p->error, sizeof(p->error),
                  "Line %d: Expected %s, got '%s'", tk.line, msg, tk.lexeme);
        p->had_error = true;
    }
    return p_next(p);
}

static bool p_match(parser_t *p, token_type_t t) {
    if (p_check(p, t)) { p_next(p); return true; }
    return false;
}

/* Forward declarations */
static int parse_stmt(parser_t *p);
static int parse_expr(parser_t *p);
static int parse_block(parser_t *p);
static int parse_expr_prec(parser_t *p, int min_prec);

/* ── Block ─────────────────────────────────────────────────────────── */
static int parse_block(parser_t *p) {
    p_expect(p, TOK_LBRACE, "{");
    int blk = ast_alloc(p->ast, NODE_BLOCK);
    while (!p_check(p, TOK_RBRACE) && !p_check(p, TOK_EOF)) {
        int s = parse_stmt(p);
        if (s >= 0) ast_add_child(p->ast, blk, s);
        if (p->had_error) break;
    }
    p_expect(p, TOK_RBRACE, "}");
    return blk;
}

/* ── Primary expression ────────────────────────────────────────────── */
static int parse_primary(parser_t *p) {
    token_t tk = p_peek(p);

    /* Integer literal */
    if (tk.type == TOK_INT) {
        p_next(p);
        int n = ast_alloc(p->ast, NODE_INT_LIT);
        p->ast->nodes[n].ival = tk.ival;
        kstrcpy(p->ast->nodes[n].sval, tk.lexeme);
        return n;
    }
    /* Float literal */
    if (tk.type == TOK_FLOAT) {
        p_next(p);
        int n = ast_alloc(p->ast, NODE_FLOAT_LIT);
        p->ast->nodes[n].fval = tk.fval;
        kstrcpy(p->ast->nodes[n].sval, tk.lexeme);
        return n;
    }
    /* String literal */
    if (tk.type == TOK_STRING) {
        p_next(p);
        int n = ast_alloc(p->ast, NODE_STR_LIT);
        kstrcpy(p->ast->nodes[n].sval, tk.lexeme);
        return n;
    }
    /* Bool literal */
    if (tk.type == TOK_BOOL) {
        p_next(p);
        int n = ast_alloc(p->ast, NODE_BOOL_LIT);
        p->ast->nodes[n].ival = tk.ival;
        return n;
    }
    /* Null */
    if (tk.type == TOK_NULL) {
        p_next(p);
        return ast_alloc(p->ast, NODE_NULL_LIT);
    }
    /* str() cast */
    if (tk.type == TOK_STR) {
        p_next(p);
        p_expect(p, TOK_LPAREN, "(");
        int inner = parse_expr(p);
        p_expect(p, TOK_RPAREN, ")");
        int n = ast_alloc(p->ast, NODE_CAST);
        kstrcpy(p->ast->nodes[n].sval, "str");
        ast_add_child(p->ast, n, inner);
        return n;
    }
    /* Unary minus / not */
    if (tk.type == TOK_MINUS || tk.type == TOK_NOT) {
        p_next(p);
        int operand = parse_primary(p);
        int n = ast_alloc(p->ast, NODE_UNOP);
        kstrcpy(p->ast->nodes[n].sval, tk.lexeme);
        ast_add_child(p->ast, n, operand);
        return n;
    }
    /* Grouped expression */
    if (tk.type == TOK_LPAREN) {
        p_next(p);
        int inner = parse_expr(p);
        p_expect(p, TOK_RPAREN, ")");
        return inner;
    }
    /* Identifier or function call */
    if (tk.type == TOK_IDENT) {
        p_next(p);
        /* Function call */
        if (p_check(p, TOK_LPAREN)) {
            p_next(p);
            int call = ast_alloc(p->ast, NODE_CALL);
            kstrcpy(p->ast->nodes[call].sval, tk.lexeme);
            while (!p_check(p, TOK_RPAREN) && !p_check(p, TOK_EOF)) {
                int arg = parse_expr(p);
                ast_add_child(p->ast, call, arg);
                if (!p_match(p, TOK_COMMA)) break;
            }
            p_expect(p, TOK_RPAREN, ")");
            return call;
        }
        /* Index access: ident[expr] */
        if (p_check(p, TOK_LBRACKET)) {
            p_next(p);
            int idx_expr = parse_expr(p);
            p_expect(p, TOK_RBRACKET, "]");
            int n = ast_alloc(p->ast, NODE_INDEX);
            kstrcpy(p->ast->nodes[n].sval, tk.lexeme);
            ast_add_child(p->ast, n, idx_expr);
            return n;
        }
        int n = ast_alloc(p->ast, NODE_IDENT);
        kstrcpy(p->ast->nodes[n].sval, tk.lexeme);
        return n;
    }
    /* system.xxx() */
    if (tk.type == TOK_SYSTEM) {
        p_next(p);
        p_expect(p, TOK_DOT, ".");
        token_t method = p_next(p);
        p_expect(p, TOK_LPAREN, "(");
        p_expect(p, TOK_RPAREN, ")");
        int n = ast_alloc(p->ast, NODE_SYSTEM_CALL);
        kstrcpy(p->ast->nodes[n].sval, method.lexeme);
        return n;
    }

    /* Unknown — skip and return error node */
    p_next(p);
    int n = ast_alloc(p->ast, NODE_NULL_LIT);
    return n;
}

/* ── Binary operator precedence ────────────────────────────────────── */
static int op_prec(token_type_t t) {
    switch (t) {
    case TOK_OR:      return 1;
    case TOK_AND:     return 2;
    case TOK_EQ: case TOK_NEQ:
    case TOK_LT: case TOK_GT:
    case TOK_LTE: case TOK_GTE: return 3;
    case TOK_PLUS: case TOK_MINUS: return 4;
    case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return 5;
    default: return 0;
    }
}

static int parse_expr_prec(parser_t *p, int min_prec) {
    int left = parse_primary(p);
    while (true) {
        token_t op = p_peek(p);
        int prec = op_prec(op.type);
        if (prec <= min_prec) break;
        p_next(p);
        int right = parse_expr_prec(p, prec);
        int n = ast_alloc(p->ast, NODE_BINOP);
        kstrcpy(p->ast->nodes[n].sval, op.lexeme);
        ast_add_child(p->ast, n, left);
        ast_add_child(p->ast, n, right);
        left = n;
    }
    return left;
}

static int parse_expr(parser_t *p) { return parse_expr_prec(p, 0); }

/* ── Statements ────────────────────────────────────────────────────── */
static int parse_let(parser_t *p, bool is_const) {
    token_t name = p_expect(p, TOK_IDENT, "variable name");
    p_expect(p, TOK_ASSIGN, "=");
    int val = parse_expr(p);
    int n = ast_alloc(p->ast, is_const ? NODE_CONST_DECL : NODE_LET);
    kstrcpy(p->ast->nodes[n].sval, name.lexeme);
    ast_add_child(p->ast, n, val);
    return n;
}

static int parse_function(parser_t *p) {
    token_t name = p_expect(p, TOK_IDENT, "function name");
    p_expect(p, TOK_LPAREN, "(");
    int fn = ast_alloc(p->ast, NODE_FUNCTION);
    kstrcpy(p->ast->nodes[fn].sval, name.lexeme);
    /* Parameters */
    while (!p_check(p, TOK_RPAREN) && !p_check(p, TOK_EOF)) {
        token_t param = p_expect(p, TOK_IDENT, "parameter name");
        int pn = ast_alloc(p->ast, NODE_IDENT);
        kstrcpy(p->ast->nodes[pn].sval, param.lexeme);
        ast_add_child(p->ast, fn, pn);
        if (!p_match(p, TOK_COMMA)) break;
    }
    p_expect(p, TOK_RPAREN, ")");
    int body = parse_block(p);
    ast_add_child(p->ast, fn, body);
    return fn;
}

static int parse_if(parser_t *p) {
    int n = ast_alloc(p->ast, NODE_IF);
    int cond = parse_expr(p);
    ast_add_child(p->ast, n, cond);
    int then_blk = parse_block(p);
    ast_add_child(p->ast, n, then_blk);
    if (p_match(p, TOK_ELSE)) {
        int else_blk = p_check(p, TOK_IF)
            ? (p_next(p), parse_if(p))
            : parse_block(p);
        ast_add_child(p->ast, n, else_blk);
    }
    return n;
}

static int parse_while(parser_t *p) {
    int n = ast_alloc(p->ast, NODE_WHILE);
    int cond = parse_expr(p);
    ast_add_child(p->ast, n, cond);
    int body = parse_block(p);
    ast_add_child(p->ast, n, body);
    return n;
}

static int parse_loop(parser_t *p) {
    /* loop <var> from <start> to <end> { } */
    int n = ast_alloc(p->ast, NODE_LOOP);
    token_t var = p_expect(p, TOK_IDENT, "loop variable");
    kstrcpy(p->ast->nodes[n].sval, var.lexeme);
    p_expect(p, TOK_FROM, "from");
    int start = parse_expr(p);
    p_expect(p, TOK_TO, "to");
    int end = parse_expr(p);
    ast_add_child(p->ast, n, start);
    ast_add_child(p->ast, n, end);
    int body = parse_block(p);
    ast_add_child(p->ast, n, body);
    return n;
}

static int parse_print(parser_t *p) {
    int n = ast_alloc(p->ast, NODE_PRINT);
    /* print supports string concatenation with + */
    int val = parse_expr(p);
    ast_add_child(p->ast, n, val);
    return n;
}

static int parse_try(parser_t *p) {
    int n = ast_alloc(p->ast, NODE_TRY_CATCH);
    int try_blk = parse_block(p);
    ast_add_child(p->ast, n, try_blk);
    p_expect(p, TOK_CATCH, "catch");
    /* Optional catch variable: catch (e) */
    if (p_check(p, TOK_LPAREN)) {
        p_next(p);
        token_t evar = p_expect(p, TOK_IDENT, "error variable");
        kstrcpy(p->ast->nodes[n].sval, evar.lexeme);
        p_expect(p, TOK_RPAREN, ")");
    }
    int catch_blk = parse_block(p);
    ast_add_child(p->ast, n, catch_blk);
    return n;
}

static int parse_parallel(parser_t *p) {
    int n = ast_alloc(p->ast, NODE_PARALLEL);
    p_expect(p, TOK_LBRACE, "{");
    while (!p_check(p, TOK_RBRACE) && !p_check(p, TOK_EOF)) {
        /* Each line inside parallel is a task call */
        int s = parse_stmt(p);
        if (s >= 0) ast_add_child(p->ast, n, s);
    }
    p_expect(p, TOK_RBRACE, "}");
    return n;
}

static int parse_use(parser_t *p) {
    token_t pkg = p_next(p);
    int n = ast_alloc(p->ast, NODE_USE);
    kstrcpy(p->ast->nodes[n].sval, pkg.lexeme);
    return n;
}

static int parse_rewind(parser_t *p) {
    int n = ast_alloc(p->ast, NODE_REWIND);
    if (p_check(p, TOK_INT)) {
        token_t steps = p_next(p);
        p->ast->nodes[n].ival = steps.ival;
    } else {
        p->ast->nodes[n].ival = 1;
    }
    return n;
}

static int parse_assign_or_call(parser_t *p) {
    token_t name = p_next(p); /* already consumed IDENT */
    /* Assignment: name = expr  or  name += expr */
    if (p_check(p, TOK_ASSIGN)) {
        p_next(p);
        int val = parse_expr(p);
        int n = ast_alloc(p->ast, NODE_ASSIGN);
        kstrcpy(p->ast->nodes[n].sval, name.lexeme);
        ast_add_child(p->ast, n, val);
        return n;
    }
    if (p_check(p, TOK_PLUS_ASSIGN) || p_check(p, TOK_MINUS_ASSIGN)) {
        token_t op = p_next(p);
        int rhs = parse_expr(p);
        /* Desugar: name += rhs  →  name = name + rhs */
        int lhs_node = ast_alloc(p->ast, NODE_IDENT);
        kstrcpy(p->ast->nodes[lhs_node].sval, name.lexeme);
        int binop = ast_alloc(p->ast, NODE_BINOP);
        kstrcpy(p->ast->nodes[binop].sval, op.type == TOK_PLUS_ASSIGN ? "+" : "-");
        ast_add_child(p->ast, binop, lhs_node);
        ast_add_child(p->ast, binop, rhs);
        int n = ast_alloc(p->ast, NODE_ASSIGN);
        kstrcpy(p->ast->nodes[n].sval, name.lexeme);
        ast_add_child(p->ast, n, binop);
        return n;
    }
    /* Function call as statement */
    if (p_check(p, TOK_LPAREN)) {
        p_next(p);
        int call = ast_alloc(p->ast, NODE_CALL);
        kstrcpy(p->ast->nodes[call].sval, name.lexeme);
        while (!p_check(p, TOK_RPAREN) && !p_check(p, TOK_EOF)) {
            int arg = parse_expr(p);
            ast_add_child(p->ast, call, arg);
            if (!p_match(p, TOK_COMMA)) break;
        }
        p_expect(p, TOK_RPAREN, ")");
        return call;
    }
    /* Bare identifier — treat as expression statement */
    int n = ast_alloc(p->ast, NODE_IDENT);
    kstrcpy(p->ast->nodes[n].sval, name.lexeme);
    return n;
}

/* ── Statement dispatcher ──────────────────────────────────────────── */
static int parse_stmt(parser_t *p) {
    if (p->had_error) return -1;
    token_t tk = p_peek(p);

    switch (tk.type) {
    case TOK_LET:      p_next(p); return parse_let(p, false);
    case TOK_CONST:    p_next(p); return parse_let(p, true);
    case TOK_FUNCTION: p_next(p); return parse_function(p);
    case TOK_RETURN: {
        p_next(p);
        int n = ast_alloc(p->ast, NODE_RETURN);
        if (!p_check(p, TOK_RBRACE) && !p_check(p, TOK_EOF))
            ast_add_child(p->ast, n, parse_expr(p));
        return n;
    }
    case TOK_IF:       p_next(p); return parse_if(p);
    case TOK_WHILE:    p_next(p); return parse_while(p);
    case TOK_LOOP:     p_next(p); return parse_loop(p);
    case TOK_BREAK:    p_next(p); return ast_alloc(p->ast, NODE_BREAK);
    case TOK_CONTINUE: p_next(p); return ast_alloc(p->ast, NODE_CONTINUE);
    case TOK_PRINT:    p_next(p); return parse_print(p);
    case TOK_TRY:      p_next(p); return parse_try(p);
    case TOK_PARALLEL: p_next(p); return parse_parallel(p);
    case TOK_USE:      p_next(p); return parse_use(p);
    case TOK_REWIND:   p_next(p); return parse_rewind(p);
    case TOK_STEP:     p_next(p); return ast_alloc(p->ast, NODE_STEP);
    case TOK_IDENT:    return parse_assign_or_call(p);
    case TOK_LBRACE:   return parse_block(p);
    case TOK_SEMICOLON: p_next(p); return -1;
    default:
        /* Skip unknown token */
        p_next(p);
        return -1;
    }
}

/* ── Top-level program ─────────────────────────────────────────────── */
int parse_program(parser_t *p) {
    int prog = ast_alloc(p->ast, NODE_PROGRAM);
    while (!p_check(p, TOK_EOF) && !p->had_error) {
        int s = parse_stmt(p);
        if (s >= 0) ast_add_child(p->ast, prog, s);
    }
    return prog;
}
