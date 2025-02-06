#include <assert.h>
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
    struct lxl_lexer *lexer;
    bool panic_mode;
};

static struct parser parser = {0};

void init_parser(struct lxl_string_view source) {
    parser.lexer = init_lexer(source);
}

static void report_location(struct lxl_token token) {
    fprintf(stderr, "%d:%d: ", token.loc.line, token.loc.column);
}

static void print_line_with_token(struct lxl_token token) {
    const char *start = token.start;
    while (start >= parser.lexer->start && start[-1] != '\n') {
        --start;
    }
    const char *end = token.start;
    while (end < parser.lexer->end && end[0] != '\n') {
        ++end;
    }
    assert(start < end);
    struct lxl_string_view line = lxl_sv_from_startend(start, end);
    fprintf(stderr, ""LXL_SV_FMT_SPEC"\n", LXL_SV_FMT_ARG(line));
}

static void parse_error_show_token_vargs(struct lxl_token token, const char *restrict fmt, va_list vargs) {
    if (parser.panic_mode) return;  // Avoid cascading errors.
    report_location(token);
    struct lxl_string_view token_value = lxl_token_value(token);
    fprintf(stderr, "Parse error in '"LXL_SV_FMT_SPEC"': ", LXL_SV_FMT_ARG(token_value));
    vfprintf(stderr, fmt, vargs);
    fprintf(stderr, ".\n");
    fprintf(stderr, "On this line:\n");
    print_line_with_token(token);
    parser.panic_mode = true;
    exit(1);
}

static void parse_error_current_show_token(const char *restrict fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(parser.current_token, fmt, vargs);
    va_end(vargs);
}

static bool parser_is_finished(void) {
    return LXL_TOKEN_IS_END(parser.current_token);
}

static void ensure_not_at_end(const char *fmt, ...) {
    if (!parser_is_finished()) return;
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

static bool check_line_ending(void) {
    return parser.current_token.token_type == LXL_TOKEN_LINE_ENDING;
}

static void ignore_line_ending(void) {
    if (check_line_ending()) {
        advance();
    }
}

static void end_statement(void) {
    if (!check_line_ending()) {
        consume(TOKEN_SEMICOLON, "Expect newline or ';' at end of statement");
    }
    ignore_line_ending();
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

static TypeID get_type(struct lxl_token token) {
    switch (token.token_type) {
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
    return TYPE_NO_TYPE;
}

static TypeID parse_type(const char *fmt, ...) {
    static_assert(TYPE_NO_TYPE == 0, "TYPE_NO_TYPE should be 'falsy'");
    TypeID type = get_type(parser.current_token);
    if (type) {
        advance();
        return type;
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
            size_t run_length = length - last_length;
            memcpy(start + last_length, p - run_length, run_length);
            while (p < end && !is_digit(*++p, base)) {
                /* Do nothing. */
            }
            if (p >= end) break;
            last_length = length;
            ++length;  // We consumed a digit.
        }
    }
    if (last_length < length) {
        size_t run_length = length - last_length;
        memcpy(start + last_length, end - run_length, run_length);
    }
    assert(length <= orig.length);
    start[length] = 0;
    uint64_t value = strtoull(start, NULL, base);
    static_assert(sizeof value == sizeof 0ULL, "Unsupported integer size");
    region_deallocate(start, orig.length + 1, region);
    return value;
}

static uint64_t parse_previous_integer(struct region *region) {
    return parse_integer(parser.previous_token, region);
}

static struct ast_expr *parse_expr(struct region *region);
static struct ast_stmt *parse_stmt(struct region *region);
static struct ast_expr parse_assign(struct region *region);

static struct ast_list parse_arg_list(struct region *region) {
    struct ast_list args = {0};
    while (!match(TOKEN_BKT_ROUND_RIGHT)) {
        struct ast_expr *arg = parse_expr(region);
        DA_APPEND(&args, EXPR_NODE(arg));
        if (!match(TOKEN_COMMA)) {
            consume(TOKEN_BKT_ROUND_RIGHT, "Expect ')' after argument list");
            break;
        }
    }
    return args;
}

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
    else if (match(TOKEN_KW_WHEN)) {
        struct ast_expr *cond = parse_expr(region);
        consume(TOKEN_KW_THEN, "Expect 'then' after condition in when expression");
        struct ast_expr *then_expr = parse_expr(region);
        consume(TOKEN_KW_ELSE, "Expect 'else' after then branch in when expression");
        struct ast_expr *else_expr = parse_expr(region);
        expr = (struct ast_expr) {
            .kind = AST_EXPR_WHEN,
            .when = {
                .cond = cond,
                .then_expr = then_expr,
                .else_expr = else_expr}};
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
        expr = parse_primary(region);
    }
    return expr;
}

static struct ast_expr parse_suffix(struct region *region) {
    struct ast_expr expr = parse_prefix(region);
    if (match(TOKEN_CARET)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_DOT)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_BKT_ROUND_LEFT)) {
        struct ast_expr *callee = new_expr(region);
        *callee = expr;
        struct ast_list args = parse_arg_list(region);
        expr = (struct ast_expr) {
            .kind = AST_EXPR_CALL,
            .call = {
                .callee = callee,
                .args = args}};
    }
    else if (match(TOKEN_BKT_SQUARE_LEFT)) {
        NOT_SUPPORTED_YET_PREVIOUS();
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
    if (check_assignment_target(expr) && match(TOKEN_COLON_EQUALS)) {
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
    ignore_line_ending();  // Ignore LF after '{'.
    struct ast_list stmts = {0};
    while (!match(TOKEN_BKT_CURLY_RIGHT)) {
        ensure_not_at_end("Unclosed block statement");
        struct ast_stmt *stmt = parse_stmt(region);
        DA_APPEND(&stmts, STMT_NODE(stmt));
    }
    ignore_line_ending();  // Ignore LF after '}'.
    return stmts;
}

static struct ast_decl *parse_func_decl(struct region *region) {
    struct ast_decl *decl = new_decl(region);
    *decl = (struct ast_decl) {0};
    struct lxl_token name_token = consume(TOKEN_IDENTIFIER, "Expect function name");
    struct ast_func_sig *sig = region_allocate(sizeof *sig, region);
    *sig = (struct ast_func_sig) {
        .name = lxl_token_value(name_token),
        .params.allocator = STDLIB_ALLOCATOR_ARD,
    };
    consume(TOKEN_BKT_ROUND_LEFT, "Expect '(' after function name");
    while (!match(TOKEN_BKT_ROUND_RIGHT)) {
        ensure_not_at_end("Unclosed function parameter list");
        struct lxl_token param_token = consume(TOKEN_IDENTIFIER, "Expect parameter name");
        consume(TOKEN_COLON, "Expect ':' after parameter name");
        TypeID param_type = parse_type("Expect type after parameter name");
        struct lxl_string_view param_name = lxl_token_value(param_token);
        struct type_decl param = {
            .name = param_name,
            .type = param_type};
        DA_APPEND(&sig->params, param);
        if (!match(TOKEN_COMMA)) {
            // Allow no trailing comma.
            consume(TOKEN_BKT_ROUND_RIGHT, "Expect ',' after parameter or ')' at end of parameter list");
            break;
        }
    }
    sig->arity = sig->params.count;
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
        *decl = (struct ast_decl) {
            .kind = AST_DECL_FUNC_DECL,
            .func_decl = {
                .sig = sig,
                .kind = AST_FUNC_EXTERNAL}};
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
            end_statement();
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
    end_statement();
    *decl = (struct ast_decl) {
        .kind = AST_DECL_VAR_DEFN,
        .var_defn = {
            .name = name,
            .type = type,
            .value = value}};
    return decl;
}

struct ast_decl *try_parse_decl(struct region *region) {
    if (match(TOKEN_KW_FUNC)) {
        return parse_func_decl(region);
    }
    if (match(TOKEN_KW_VAR)) {
        return parse_var_decl(region);
    }
    return NULL;
}

struct ast_stmt parse_if(struct region *region) {
    ignore_line_ending();
    struct ast_expr *cond = parse_expr(region);
    ignore_line_ending();
    consume(TOKEN_BKT_CURLY_LEFT, "Expect '{' after 'if' condition");
    struct ast_list then_clause = parse_block(region);
    struct ast_list else_clause = {0};
    if (match(TOKEN_KW_ELSE)) {
        ignore_line_ending();
        if (match(TOKEN_BKT_CURLY_LEFT)) {
            else_clause = parse_block(region);
        }
        else {
            // Should we allow LF here? For now, we won't.
            consume(TOKEN_KW_IF, "Expect '{' or 'if' after 'else'");
            struct ast_stmt *elif_clause = new_stmt(region);
            *elif_clause = parse_if(region);
            DA_APPEND(&else_clause, STMT_NODE(elif_clause));
        }
    }
    return (struct ast_stmt) {
        .kind = AST_STMT_IF,
        .if_ = {
            .cond = cond,
            .then_clause = then_clause,
            .else_clause = else_clause}};
}

struct ast_stmt *parse_stmt(struct region *region) {
    struct ast_stmt *stmt = new_stmt(region);
    if (match(TOKEN_KW_IF)) {
        *stmt = parse_if(region);
    }
    else {
        struct ast_decl *decl = try_parse_decl(region);
        if (decl != NULL) {
            // Declaration statement.
            *stmt = (struct ast_stmt) {
                .kind = AST_STMT_DECL,
                .decl = {.decl = decl}};
        }
        else {
            // Expression statement.
            *stmt = (struct ast_stmt) {
                .kind = AST_STMT_EXPR,
                .expr = {.expr = parse_expr(region)}};
            end_statement();
        }
    }
    return stmt;
}

struct ast_list parse(struct region *region) {
    struct ast_list nodes = {0};
    advance();  // Prime parser with first token;
    for (; ignore_line_ending(), !parser_is_finished();) {
        struct ast_node current_node = {0};
        struct ast_decl *decl = try_parse_decl(region);
        if (decl != NULL) {
            current_node = (struct ast_node) {
                .kind = AST_DECL,
                .decl = decl};
        }
        else {
            parse_error_current_show_token("Unexpected token at module top level");
        }
        DA_APPEND(&nodes, current_node);
    }
    return nodes;
}

struct type_checker {
    bool had_error;
} type_checker = {0};

void type_error(const char *msg, ...) {
    fprintf(stderr, "Type error: ");
    va_list vargs;
    va_start(vargs, msg);
    vfprintf(stderr, msg, vargs);
    va_end(vargs);
    fprintf(stderr, ".\n");
    type_checker.had_error = true;
}

static TypeID resolve_type(struct ast_expr *expr) {
    TypeID result_type = TYPE_NO_TYPE;
    switch (expr->kind) {
    case AST_EXPR_ASSIGN:
        expr->type = resolve_type(expr->assign.value);
    case AST_EXPR_BINARY: {
        TypeID lhs_type = resolve_type(expr->binary.lhs);
        TypeID rhs_type = resolve_type(expr->binary.rhs);
        result_type = convert_binary(lhs_type, rhs_type);
    }
    case AST_EXPR_CALL:
        result_type = callee->sig.ret_type;
        break;
    case AST_EXPR_GET:
        assert(0 && "Not Implemented");
        break;
    case AST_EXPR_INTEGER:
        // TODO "untyped" literals... i.e. integer literals have an "INTEGER_LITERAL" type.
        result_type = TYPE_INT;
        break;
    case AST_EXPR_WHEN: {
        // Propogate errors.
        if (!resolve_type(expr->when.cond)) return TYPE_NO_TYPE;
        TypeId then_type = resolve_type(expr->when.then_expr);
        TypeId else_type = resolve_type(expr->when.else_expr);
        if (then_type == else_type) {
            result_type = then_type;
        }
        else {
            type_error("Types for 'then' branch and 'else' branch of 'when' expression must match");
        }
    } break;
    }
    expr->type = result_type;
    return result_type;
}

static void type_check_stmt(struct ast_stmt *stmt, TypeID ret_type);

static void type_check_function(struct ast_func_sig *sig, struct ast_list body) {
    // TODO: set up arguments as local variables.
    // TODO: local variables.
    for (size_t i = 0; i < body.count; ++i) {
        assert(body.items[i].kind == AST_STMT);
        struct ast_stmt *stmt = body.items[i].stmt;
        type_check_stmt(stmt, sig->ret_type);
    }
}

static void type_check_decl(struct ast_decl *decl) {
    switch (decl->kind) {
    case AST_DECL_VAR_DECL: return;
    case AST_DECL_VAR_DEFN: {
        TypeID value_type = resolve_type(decl->var_defn.value);
        if (!value_type) return;
        if (!decl->var_defn.type) decl->var_defn.type = value_type;
        if (value_type != decl->var_defn.type) {
            type_error("Mismatched types in variable definition");
        }
    } return;
    case AST_DECL_FUNC_DECL: return;
    case AST_DECL_FUNC_DEFN:
        type_check_function(decl->func_defn.sig, decl->func_defn.body);
        return;
    }
    UNREACHABLE();
}

static void type_check_stmt(struct ast_stmt *stmt, TypeID ret_type) {
    (void)ret_type;  // Needed for return statements.
    switch (stmt->kind) {
    case AST_STMT_DECL:
        type_check_decl(stmt->decl.decl);
        return;
    case AST_STMT_EXPR:
        resolve_type(stmt->expr.expr);
        return;
    case AST_STMT_IF:
        resolve_type(stmt->if_.cond);
        type_check_stmt(stmt->if_.then_clause);
        type_check_stmt(stmt->if_.else_clause);
        return;
    }
    UNREACHABLE();
}

bool type_check(struct ast_list *nodes) {
    for (size_t i = 0; i < nodes->count; ++i) {
        struct ast_node node = nodes->items[i];
        switch (node.kind) {
        case AST_EXPR:
            // Cannot have an expression here.
            UNREACHABLE();
            break;
        case AST_STMT:
            type_error("Only declarations and definitons are allowed at module scope");
            break;
        case AST_DECL:
            type_check_decl(node.decl);
            break;
        }
    }
    return !type_checker.had_error;
}
