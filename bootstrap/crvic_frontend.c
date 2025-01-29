#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lexel.h"

#include "region.h"
#include "ubiqs.h"

#include "crvic_ast.h"
#include "crvic_frontend.h"

#define NOT_SUPPORTED_YET(token)                                        \
    do {                                                                \
        fprintf(stderr, "%s token not supported yet\n", token_type_string(token)); \
        abort();                                                        \
    } while (0)

#define NOT_SUPPORTED_YET_PREVIOUS() NOT_SUPPORTED_YET(parser.previous_token)

struct parser {
    struct lxl_token current_token, previous_token;
    bool panic_mode;
} parser;

static void report_location(struct lxl_token token) {
    fprintf(stderr, "%d:%d: ", token.loc.line, token.loc.column);
}

static void parse_error_show_token_vargs(struct lxl_token token, const char *restrict fmt, va_list vargs) {
    if (parser.panic_mode) return;  // Avoid cascading errors.
    report_location(token);
    struct lxl_string_view token_value = lxl_token_value(token);
    fprintf(stderr, "Parse error in '"LXL_SV_FMT_SPEC"': ", LXL_SV_FMT_ARG(token_value));
    vfprintf(stderr, fmt, vargs);
    fprintf(stderr, ".\n");
    parser.panic_mode = true;
}

static void parse_error_current_show_token(const char *restrict fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(parser.current_token, fmt, vargs);
    va_end(vargs);
}

static void ensure_not_at_end(const char *fmt, ...) {
    if (!LXL_TOKEN_IS_END(parser.current_token)) return;
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(parser.current_token, fmt, vargs);
    va_end(vargs);
}

static struct lxl_token advance(void) {
    parser.previous_token = parser.current_token;
    parser.current_token = next_token();
    return parser.previous_token;
}

static bool check(enum token_type type) {
    return parser.current_token.token_type == (int)type;
}

static bool match(enum token_type type) {
    if (parser.current_token.token_type != (int)type) return false;
    advance();
    return true;
}

static struct lxl_token consume(enum token_type type, const char *fmt, ...) {
    if (match(type)) return parser.previous_token;
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(parser.current_token, fmt, vargs);
    va_end(vargs);
    return parser.current_token;
}

static void ensure_statement_end(void) {
    if (parser.current_token.token_type != LXL_TOKEN_LINE_ENDING) {
        consume(TOKEN_SEMICOLON, "Expect newline or ';' at end of statement");
    }
    if (parser.current_token.token_type == LXL_TOKEN_LINE_ENDING) {
        // We advance to the next line if we see "\n" OR ";\n".
        advance();
    }
}

static struct ast_expr *new_expr(struct region *region) {
    struct ast_expr *expr = region_allocate(sizeof *expr, region);
    assert(expr != NULL && "We're gonna need a bigger region!");
    return expr;
}

static struct ast_stmt *new_stmt(struct region *region) {
    struct ast_stmt *stmt = region_allocate(sizeof *stmt, region);
    assert(stmt != NULL && "We're gonna need a bigger region!");
    return stmt;
}

static struct ast_decl *new_decl(struct region *region) {
    struct ast_decl *decl = region_allocate(sizeof *decl, region);
    assert(decl != NULL && "We're gonna need a bigger region!");
    return decl;
}

static bool check_comparison(void) {
    return check(TOKEN_EQUALS_EQUALS)
        || check(TOKEN_BANG_EQUALS)
        || check(TOKEN_GREATER)
        || check(TOKEN_GREATER_EQUALS)
        || check(TOKEN_LESS)
        || check(TOKEN_LESS_EQUALS)
        ;
}

static bool check_assignment_target(struct ast_expr expr) {
    // For now, only variable names can be assignment tragets.
    return expr.kind == AST_EXPR_GET;
}

static TypeID parse_type(const char *fmt, ...) {
    switch (parser.current_token.token_type) {
    case TOKEN_KW_I8: return TYPE_I8;
    case TOKEN_KW_I16: return TYPE_I16;
    case TOKEN_KW_I32: return TYPE_I32;
    case TOKEN_KW_I64: return TYPE_I64;
    case TOKEN_KW_INT: return TYPE_INT;
    case TOKEN_KW_U8: return TYPE_U8;
    case TOKEN_KW_U16: return TYPE_U16;
    case TOKEN_KW_U32: return TYPE_U32;
    case TOKEN_KW_U64: return TYPE_U64;
    case TOKEN_KW_UINT: return TYPE_UINT;
    }
    // Not a type.
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(parser.current_token, fmt, vargs);
    va_end(vargs);
    return TYPE_NO_TYPE;
}

static bool is_digit(char c, int base) {
    assert(base == 10);  // We only support base 10 ATM.
    return '0' <= c && c <= '9';
}

static uint64_t parse_integer(struct lxl_token token, struct region *region) {
    struct lxl_string_view orig = lxl_token_value(token);
    char *start = region_allocate(orig.length + 1, region);
    size_t length = 0;
    size_t last_length = 0;
    int base = 10;  // Hard-coded for now.
    const char *const end = LXL_SV_END(orig);
    for (const char *p = orig.start; p < end; ++p) {
        if (is_digit(*p, base)) {
            ++length;
        }
        else {
            assert(length > last_length);
            memcpy(start + last_length, p, length - last_length);
            while (p < end && !is_digit(*++p, base)) {
                /* Do nothing. */
            }
            if (p >= end) break;
            last_length = length;
            ++length;  // We consumed a digit.
        }
    }
    assert(length <= orig.length);
    start[length] = 0;
    uint64_t value = strtoull(start, NULL, base);
    _Static_assert(sizeof value == sizeof 0ULL, "Unsupported integer size");
    region_deallocate(start, orig.length + 1, region);
    return value;
}

static uint64_t parse_previous_integer(struct region *region) {
    return parse_integer(parser.previous_token, region);
}

static struct ast_expr *parse_expr(struct region *region);
static struct ast_stmt *parse_stmt(struct region *region);
static struct ast_expr parse_assign(struct region *region);

static struct ast_expr parse_primary(struct region *region) {
    struct ast_expr expr = {0};
    if (match(TOKEN_LIT_INTEGER)) {
        expr = (struct ast_expr) {
            .kind = AST_EXPR_INTEGER,
            .integer = {.value = parse_previous_integer(region)}};
    }
    else if (match(TOKEN_IDENTIFIER)) {
        expr = (struct ast_expr) {
            .kind = AST_EXPR_GET,
            .get = {.target = lxl_token_value(parser.previous_token)}};
    }
    else if (match(TOKEN_BKT_ROUND_LEFT)) {
        expr = parse_assign(region);
        consume(TOKEN_BKT_ROUND_RIGHT, "Expect ')' after grouped expression");
    }
    else {
        parse_error_current_show_token("Expect expression");
    }
    return expr;
}

static struct ast_expr parse_prefix(struct region *region) {
    struct ast_expr expr = {0};
    if (match(TOKEN_MINUS)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_PLUS)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else {
        parse_primary(region);
    }
    return expr;
}

static struct ast_expr parse_suffix(struct region *region) {
    struct ast_expr expr = {0};
    if (match(TOKEN_CARET)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_DOT)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_BKT_ROUND_LEFT)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_BKT_SQUARE_LEFT)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else {
        expr = parse_prefix(region);
    }
    return expr;
}

static struct ast_expr parse_factor(struct region *region) {
    struct ast_expr expr = parse_suffix(region);
    while (match(TOKEN_STAR)) {
        struct ast_expr *lhs = new_expr(region);
        struct ast_expr *rhs = new_expr(region);
        *lhs = expr;
        *rhs = parse_suffix(region);
        expr = (struct ast_expr) {
            .kind = AST_EXPR_BINARY,
            .binary = {
                .lhs = lhs,
                .rhs = rhs,
                .op = AST_BIN_MUL}};
    }
    return expr;
}

static struct ast_expr parse_term(struct region *region) {
    struct ast_expr expr = parse_factor(region);
    while (match(TOKEN_PLUS)) {
        struct ast_expr *lhs = new_expr(region);
        struct ast_expr *rhs = new_expr(region);
        *lhs = expr;
        *rhs = parse_factor(region);
        expr = (struct ast_expr) {
            .kind = AST_EXPR_BINARY,
            .binary = {
                .lhs = lhs,
                .rhs = rhs,
                .op = AST_BIN_ADD}};
    }
    return expr;
}

static struct ast_expr parse_compare(struct region *region) {
    struct ast_expr expr = parse_term(region);
    if (check_comparison()) {
        struct lxl_token op = advance();
        // NOTE: I want to support multi-way comparisons.
        NOT_SUPPORTED_YET(op);
    }
    return expr;
}

static struct ast_expr parse_assign(struct region *region) {
    struct ast_expr expr = parse_compare(region);
    if (check_assignment_target(expr)) {
        struct ast_expr *value = new_expr(region);
        *value = parse_compare(region);
        expr = (struct ast_expr) {
            .kind = AST_EXPR_ASSIGN,
            .assign = {
                .target = expr.get.target,
                .value = value}};
    }
    return expr;
}

struct ast_expr *parse_expr(struct region *region) {
    struct ast_expr *expr = new_expr(region);
    *expr = parse_assign(region);
    return expr;
}

struct ast_list parse_block(struct region *region) {
    struct ast_list  = {.allocator = STDLIB_ALLOCATOR_ARD};
    while (!match(TOKEN_BKT_CURLY_RIGHT)) {
        ensure_not_at_end("Unclosed block statement");
        struct ast_stmt *stmt = parse_stmt(region);
        DA_APPEND(&stmts, STMT_NODE(stmt));
    }
    return stmts;
}

struct ast_stmt *parse_stmt(struct region *region) {
    struct ast_stmt *stmt = new_stmt(region);
    *stmt = (struct ast_stmt) {
        .kind = AST_STMT_EXPR,
        .expr = {.expr = parse_expr(region)}};
    return stmt;
}

static struct ast_decl *parse_func_decl(struct region *region) {
    struct ast_decl *decl = new_decl(region);
    *decl = (struct ast_decl) {0};
    struct lxl_token name_token = consume(TOKEN_IDENTIFIER, "Expect function name");
    struct ast_func_sig *sig = region_allocate(sizeof *sig, region);
    *sig = (struct ast_func_sig) {.name = lxl_token_value(name_token)};
    consume(TOKEN_BKT_ROUND_LEFT, "Expect '(' after function name");
    struct sv_list param_names = {0};
    while (!match(TOKEN_BKT_ROUND_RIGHT)) {
        ensure_not_at_end("Unclosed function parameter list");
        struct lxl_token param_token = consume(TOKEN_IDENTIFIER, "Expect parameter name");
        consume(TOKEN_COLON, "Expect ':' after parameter name");
        TypeID param_type = parse_type("Expect type after parameter name");
        struct lxl_string_view param_name = lxl_token_value(param_token);
        DA_APPEND(&param_names, param_name);
        DA_APPEND(&sig->param_types, param_type);
        if (!match(TOKEN_COMMA)) {
            // Allow no trailing comma.
            consume(TOKEN_BKT_ROUND_RIGHT, "Expect ')' at end of parameter list");
            break;
        }
    }
    sig->arity = sig->param_types.count;
    if (match(TOKEN_ARROW_RIGHT)) {
        TypeID ret_type = parse_type("Expect return type after '->'");
        sig->ret_type = ret_type;
    }
    else {
        // No return type (i.e. unit type).
        sig->ret_type = TYPE_UNIT;
    }
    if (match(TOKEN_BKT_CURLY_LEFT)) {
        *decl = (struct ast_decl) {
            .kind = AST_DECL_FUNC_DEFN,
            .func_defn = {
                .sig = sig,
                .body = parse_block(region)}};
    }
    else if (match(TOKEN_COLON_EQUALS)) {
        consume(TOKEN_KW_EXTERNAL, "Expect 'external' after ':=' for function");
    }
    else {
        parse_error_current_show_token("Expect '{' or ':=' after function signature");
    }
    return decl;
}

struct ast_decl *parse_var_decl(struct region *region) {
    struct ast_decl *decl = new_decl(region);
    *decl = (struct ast_decl) {0};
    struct lxl_token name_token = consume(TOKEN_IDENTIFIER, "Expect variable name after `var`");
    struct lxl_string_view name = lxl_token_value(name_token);
    TypeID type = TYPE_NO_TYPE;
    if (match(TOKEN_COLON)) {
        type = parse_type("Expect type after ':'");
        if (!match(TOKEN_EQUALS)) {
            ensure_statement_end();
            *decl = (struct ast_decl) {
                .kind = AST_DECL_VAR_DECL,
                .var_decl = {
                    .name = name,
                    .type = type}};
            return decl;
        }
    }
    else {
        consume(TOKEN_COLON_EQUALS, "Expect `:=` or `: T =` after variable name");
    }
    struct ast_expr *value = parse_expr(region);
    ensure_statement_end();
    *decl = (struct ast_decl) {
        .kind = AST_DECL_VAR_DEFN,
        .var_defn = {
            .name = name,
            .type = type,
            .value = value}};
    return decl;
}

struct ast_list parse(struct region *region) {
    struct ast_list nodes = {.allocator = STDLIB_ALLOCATOR_ARD};
    advance();  // Prime parser with first token;
    for (;;) {
        struct ast_node current_node = {0};
        if (match(TOKEN_KW_FUNC)) {
            current_node = (struct ast_node) {
                .kind = AST_DECL,
                .decl = parse_func_decl(region)};
        }
        else if (match(TOKEN_KW_VAR)) {
            current_node = (struct ast_node) {
                .kind = AST_DECL,
                .decl = parse_var_decl(region)};
        }
        else {
            parse_error_current_show_token("Unexpected token at module top level");
        }
        DA_APPEND(&nodes, current_node);
    }
    return nodes;
}
