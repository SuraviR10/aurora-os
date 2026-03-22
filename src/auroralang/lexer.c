#include "auroralang.h"
#include "string.h"

/* ═══════════════════════════════════════════════════════════════════
   AURORALANG  LEXER
   ═══════════════════════════════════════════════════════════════════ */

static bool is_alpha(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
static bool is_digit(char c) { return c>='0'&&c<='9'; }
static bool is_alnum(char c) { return is_alpha(c)||is_digit(c); }
static bool is_space(char c) { return c==' '||c=='\t'||c=='\r'; }

typedef struct { const char *word; token_type_t type; } keyword_t;

static const keyword_t keywords[] = {
    {"let",      TOK_LET},      {"const",    TOK_CONST},
    {"function", TOK_FUNCTION}, {"return",   TOK_RETURN},
    {"if",       TOK_IF},       {"else",     TOK_ELSE},
    {"loop",     TOK_LOOP},     {"from",     TOK_FROM},
    {"to",       TOK_TO},       {"while",    TOK_WHILE},
    {"break",    TOK_BREAK},    {"continue", TOK_CONTINUE},
    {"print",    TOK_PRINT},    {"input",    TOK_INPUT},
    {"try",      TOK_TRY},      {"catch",    TOK_CATCH},
    {"throw",    TOK_THROW},
    {"parallel", TOK_PARALLEL}, {"task",     TOK_TASK},
    {"use",      TOK_USE},
    {"system",   TOK_SYSTEM},
    {"ui",       TOK_UI},       {"window",   TOK_WINDOW},
    {"button",   TOK_BUTTON},   {"label",    TOK_LABEL},
    {"server",   TOK_SERVER},   {"route",    TOK_ROUTE},
    {"start",    TOK_START},
    {"create",   TOK_CREATE},   {"write",    TOK_WRITE},
    {"read",     TOK_READ},     {"file",     TOK_FILE},
    {"rewind",   TOK_REWIND},   {"step",     TOK_STEP},
    {"replay",   TOK_REPLAY},
    {"true",     TOK_BOOL},     {"false",    TOK_BOOL},
    {"null",     TOK_NULL},
    {"and",      TOK_AND},      {"or",       TOK_OR},
    {"not",      TOK_NOT},
    {"str",      TOK_STR},
    {0, 0}
};

static token_type_t lookup_keyword(const char *s) {
    for (int i = 0; keywords[i].word; i++)
        if (kstrcmp(keywords[i].word, s) == 0) return keywords[i].type;
    return TOK_IDENT;
}

static void emit(lexer_t *l, token_type_t t, const char *lex, int ival, double fval) {
    if (l->tok_count >= AST_MAX_NODES) return;
    token_t *tk = &l->tokens[l->tok_count++];
    tk->type = t;
    tk->line = l->line;
    tk->ival = ival;
    tk->fval = fval;
    kstrncpy(tk->lexeme, lex, 127);
}

void lexer_init(lexer_t *l, const char *src) {
    l->source    = src;
    l->pos       = 0;
    l->line      = 1;
    l->tok_count = 0;
    l->tok_pos   = 0;
}

void lexer_tokenize(lexer_t *l) {
    const char *s = l->source;
    int i = 0;
    while (s[i]) {
        /* Skip spaces */
        if (is_space(s[i])) { i++; continue; }
        /* Newline */
        if (s[i] == '\n') { emit(l, TOK_NEWLINE, "\\n", 0, 0); l->line++; i++; continue; }
        /* Comment # or // */
        if (s[i] == '#' || (s[i] == '/' && s[i+1] == '/')) {
            while (s[i] && s[i] != '\n') i++;
            continue;
        }
        /* String literal */
        if (s[i] == '"') {
            i++;
            char buf[128]; int bi = 0;
            while (s[i] && s[i] != '"') {
                if (s[i] == '\\') {
                    i++;
                    switch (s[i]) {
                    case 'n': buf[bi++] = '\n'; break;
                    case 't': buf[bi++] = '\t'; break;
                    case '"': buf[bi++] = '"';  break;
                    case '\\':buf[bi++] = '\\'; break;
                    default:  buf[bi++] = s[i]; break;
                    }
                } else buf[bi++] = s[i];
                i++;
                if (bi >= 127) break;
            }
            buf[bi] = '\0';
            if (s[i] == '"') i++;
            emit(l, TOK_STRING, buf, 0, 0);
            continue;
        }
        /* Number */
        if (is_digit(s[i]) || (s[i] == '-' && is_digit(s[i+1]))) {
            char buf[32]; int bi = 0; bool is_float = false;
            if (s[i] == '-') buf[bi++] = s[i++];
            while (is_digit(s[i])) buf[bi++] = s[i++];
            if (s[i] == '.' && is_digit(s[i+1])) {
                is_float = true;
                buf[bi++] = s[i++];
                while (is_digit(s[i])) buf[bi++] = s[i++];
            }
            buf[bi] = '\0';
            if (is_float) emit(l, TOK_FLOAT, buf, 0, 0.0); /* simplified */
            else          emit(l, TOK_INT,   buf, katoi(buf), 0);
            continue;
        }
        /* Identifier / keyword */
        if (is_alpha(s[i])) {
            char buf[64]; int bi = 0;
            while (is_alnum(s[i])) buf[bi++] = s[i++];
            buf[bi] = '\0';
            token_type_t t = lookup_keyword(buf);
            int iv = 0;
            if (t == TOK_BOOL) iv = (kstrcmp(buf, "true") == 0) ? 1 : 0;
            emit(l, t, buf, iv, 0);
            continue;
        }
        /* Two-char operators */
        char c = s[i], n = s[i+1];
        if (c=='='&&n=='=') { emit(l,TOK_EQ,  "==",0,0); i+=2; continue; }
        if (c=='!'&&n=='=') { emit(l,TOK_NEQ, "!=",0,0); i+=2; continue; }
        if (c=='<'&&n=='=') { emit(l,TOK_LTE, "<=",0,0); i+=2; continue; }
        if (c=='>'&&n=='=') { emit(l,TOK_GTE, ">=",0,0); i+=2; continue; }
        if (c=='+'&&n=='=') { emit(l,TOK_PLUS_ASSIGN, "+=",0,0); i+=2; continue; }
        if (c=='-'&&n=='=') { emit(l,TOK_MINUS_ASSIGN,"-=",0,0); i+=2; continue; }
        if (c=='&'&&n=='&') { emit(l,TOK_AND, "&&",0,0); i+=2; continue; }
        if (c=='|'&&n=='|') { emit(l,TOK_OR,  "||",0,0); i+=2; continue; }
        /* Single-char */
        token_type_t st = TOK_ERROR;
        char lex[2] = {c, '\0'};
        switch (c) {
        case '+': st=TOK_PLUS;     break; case '-': st=TOK_MINUS;    break;
        case '*': st=TOK_STAR;     break; case '/': st=TOK_SLASH;    break;
        case '%': st=TOK_PERCENT;  break; case '=': st=TOK_ASSIGN;   break;
        case '<': st=TOK_LT;       break; case '>': st=TOK_GT;       break;
        case '!': st=TOK_NOT;      break;
        case '(': st=TOK_LPAREN;   break; case ')': st=TOK_RPAREN;   break;
        case '{': st=TOK_LBRACE;   break; case '}': st=TOK_RBRACE;   break;
        case '[': st=TOK_LBRACKET; break; case ']': st=TOK_RBRACKET; break;
        case ',': st=TOK_COMMA;    break; case '.': st=TOK_DOT;      break;
        case ':': st=TOK_COLON;    break; case ';': st=TOK_SEMICOLON;break;
        }
        emit(l, st, lex, 0, 0);
        i++;
    }
    emit(l, TOK_EOF, "", 0, 0);
}

token_t lexer_peek(lexer_t *l) {
    int p = l->tok_pos;
    /* Skip newlines for peek */
    while (p < l->tok_count && l->tokens[p].type == TOK_NEWLINE) p++;
    return p < l->tok_count ? l->tokens[p] : l->tokens[l->tok_count-1];
}

token_t lexer_next(lexer_t *l) {
    while (l->tok_pos < l->tok_count && l->tokens[l->tok_pos].type == TOK_NEWLINE)
        l->tok_pos++;
    if (l->tok_pos >= l->tok_count) return l->tokens[l->tok_count-1];
    return l->tokens[l->tok_pos++];
}

bool lexer_match(lexer_t *l, token_type_t t) {
    if (lexer_peek(l).type == t) { lexer_next(l); return true; }
    return false;
}
