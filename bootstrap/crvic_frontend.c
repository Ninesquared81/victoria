#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lexel.h"

#include "ubiqs.h"

#include "crvic_ast.h"
#include "crvic_frontend.h"
#include "crvic_function.h"
#include "crvic_resources.h"
#include "crvic_symbol_table.h"

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

struct type_checker {
    bool had_error;
};

static struct parser parser = {0};

static struct symbol_table symbols = {0};

static struct type_checker type_checker = {0};

#define SYMBOL_TABLE_CAPACITY 128

void init_frontend(struct lxl_string_view source) {
    parser.lexer = init_lexer(source);
    symbols = (struct symbol_table) {
        .capacity = SYMBOL_TABLE_CAPACITY,
        .slots = ALLOCATE(perm, sizeof(struct st_slot[SYMBOL_TABLE_CAPACITY]))};
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

static void parse_error_previous_show_token(const char *restrict fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(parser.previous_token, fmt, vargs);
    va_end(vargs);
}

static void parse_error_current_show_token(const char *restrict fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(parser.current_token, fmt, vargs);
    va_end(vargs);
}

void type_error(const char *msg, ...) {
    fprintf(stderr, "Type error: ");
    va_list vargs;
    va_start(vargs, msg);
    vfprintf(stderr, msg, vargs);
    va_end(vargs);
    fprintf(stderr, ".\n");
    type_checker.had_error = true;
}

void name_error(const char *msg, ...) {
    fprintf(stderr, "Name error: ");
    va_list vargs;
    va_start(vargs, msg);
    vfprintf(stderr, msg, vargs);
    va_end(vargs);
    fprintf(stderr, ".\n");
    parser.panic_mode = true;
    type_checker.had_error = true;
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

static struct ast_expr *new_expr(void) {
    struct ast_expr *expr = ALLOCATE(perm, sizeof *expr);
    assert(expr != NULL && "We're gonna need a bigger region!");
    return expr;
}

static struct ast_stmt *new_stmt(void) {
    struct ast_stmt *stmt = ALLOCATE(perm, sizeof *stmt);
    assert(stmt != NULL && "We're gonna need a bigger region!");
    return stmt;
}

static struct ast_decl *new_decl(void) {
    struct ast_decl *decl = ALLOCATE(perm, sizeof *decl);
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

static TypeID token_to_type(struct lxl_token token) {
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
    // Basic types.
    TypeID type = token_to_type(parser.current_token);
    if (type) {
        advance();
        return type;
    }
    // Type aliases.
    if (match(TOKEN_IDENTIFIER)) {
        struct lxl_string_view sv = lxl_token_value(parser.previous_token);
        struct symbol *symbol = lookup_symbol(&symbols, st_key_of(sv));
        if (!symbol) {
            name_error("Unknown type '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(sv));
            return TYPE_NO_TYPE;
        }
        if (symbol->kind != SYMBOL_TYPE_ALIAS) {
            parse_error_previous_show_token("Symbol '"LXL_SV_FMT_SPEC"' is not a type", LXL_SV_FMT_ARG(sv));
            return TYPE_NO_TYPE;
        }
        return symbol->type_alias.type;
    }
    // Record types.
    if (match(TOKEN_KW_RECORD)) {
        begin_temp();
        ignore_line_ending();
        consume(TOKEN_BKT_CURLY_LEFT, "Expect '{' after 'record'");
        struct type_decl_list fields = {.allocator = temp};
        while (!check(TOKEN_BKT_CURLY_RIGHT)) {
            ignore_line_ending();
            struct lxl_token field_name_token = consume(TOKEN_IDENTIFIER, "Expect field name");
            ignore_line_ending();
            consume(TOKEN_COLON, "Expect ':' after field name");
            ignore_line_ending();
            struct lxl_string_view field_name = lxl_token_value(field_name_token);
            TypeID field_type = parse_type("Expect field type after ':'");
            ignore_line_ending();
            struct type_decl field = {.name = field_name, .type = field_type};
            DA_APPEND(&fields, field);
            if (!match(TOKEN_COMMA)) break;
        }
        ignore_line_ending();
        consume(TOKEN_BKT_CURLY_RIGHT, "Expect '}' after record definition");
        TypeID record_type = find_record_type(fields);
        if (!record_type) {
            // Record not found; add it to the type table.
            struct type_decl_list new_fields =  PROMOTE_DA(&fields);
            record_type = add_type((struct type_info) {
                    .kind = KIND_RECORD,
                    .size = calculate_record_size(new_fields),
                    .record_type = {.fields = new_fields}});
        }
        return record_type;
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

static uint64_t parse_integer(struct lxl_token token) {
    begin_temp();
    struct lxl_string_view orig = lxl_token_value(token);
    char *start = ALLOCATE(temp, orig.length + 1);
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
    return value;
}

static uint64_t parse_previous_integer(void) {
    return parse_integer(parser.previous_token);
}

static struct ast_expr *parse_expr(void);
static struct ast_stmt *parse_stmt(void);
static struct ast_expr parse_assign(void);

static struct ast_list parse_comma_list(enum token_type terminator, const char *message) {
    struct ast_list args = {0};
    while (!match(terminator)) {
        struct ast_expr *arg = parse_expr();
        DA_APPEND(&args, EXPR_NODE(arg));
        if (!match(TOKEN_COMMA)) {
            consume(terminator, message);
            break;
        }
    }
    return args;
}

static struct ast_list parse_arg_list(void) {
    return parse_comma_list(TOKEN_BKT_ROUND_RIGHT, "Expect ')' after argument list");
}

static struct ast_expr parse_primary(void) {
    struct ast_expr expr = {0};
    if (match(TOKEN_LIT_INTEGER)) {
        expr = (struct ast_expr) {
            .kind = AST_EXPR_INTEGER,
            .integer = {.value = parse_previous_integer()}};
    }
    else if (match(TOKEN_IDENTIFIER)) {
        struct lxl_string_view name = lxl_token_value(parser.previous_token);
        if (match(TOKEN_BKT_CURLY_LEFT)) {
            struct ast_list inits = parse_comma_list(TOKEN_BKT_CURLY_RIGHT,
                                                     "Expect '}' after initialiser list");
            expr = (struct ast_expr) {
                .kind = AST_EXPR_CONSTRUCTOR,
                .constructor = {
                    .name = name,
                    .init_list = inits}};
        }
        else {
            expr = (struct ast_expr) {
                .kind = AST_EXPR_GET,
                .get = {.target = name}};
        }
    }
    else if (match(TOKEN_BKT_ROUND_LEFT)) {
        expr = parse_assign();
        consume(TOKEN_BKT_ROUND_RIGHT, "Expect ')' after grouped expression");
    }
    else if (match(TOKEN_KW_WHEN)) {
        struct ast_expr *cond = parse_expr();
        consume(TOKEN_KW_THEN, "Expect 'then' after condition in when expression");
        struct ast_expr *then_expr = parse_expr();
        consume(TOKEN_KW_ELSE, "Expect 'else' after then branch in when expression");
        struct ast_expr *else_expr = parse_expr();
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

static struct ast_expr parse_prefix(void) {
    struct ast_expr expr = {0};
    if (match(TOKEN_MINUS)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_PLUS)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else {
        expr = parse_primary();
    }
    return expr;
}

static struct ast_expr parse_suffix(void) {
    struct ast_expr expr = parse_prefix();
    if (match(TOKEN_CARET)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_DOT)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_BKT_ROUND_LEFT)) {
        struct ast_expr *callee = new_expr();
        *callee = expr;
        if (callee->kind != AST_EXPR_GET) {
            parse_error_previous_show_token("Callee must be an identifier");
        }
        struct ast_list args = parse_arg_list();
        expr = (struct ast_expr) {
            .kind = AST_EXPR_CALL,
            .call = {
                .callee_name = callee->get.target,
                .arity = args.count,
                .args = args}};
    }
    else if (match(TOKEN_BKT_SQUARE_LEFT)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_KW_AS) || match(TOKEN_KW_TO)) {
        struct lxl_token conversion = parser.previous_token;
        struct lxl_string_view conv_sv = lxl_token_value(conversion);
        TypeID target_type = parse_type("Expect target type for '"LXL_SV_FMT_SPEC"' conversion",
                                        LXL_SV_FMT_ARG(conv_sv));
        struct ast_expr *operand = new_expr();
        *operand = expr;
        expr = (struct ast_expr) {
            .kind = AST_EXPR_CONVERT,
            .convert = {
                .operand = operand,
                .target_type = target_type,
                .kind = (conversion.token_type == TOKEN_KW_AS) ? CONVERT_AS : CONVERT_TO}};
    }
    return expr;
}

static struct ast_expr parse_factor(void) {
    struct ast_expr expr = parse_suffix();
    while (match(TOKEN_STAR)) {
        struct ast_expr *lhs = new_expr();
        struct ast_expr *rhs = new_expr();
        *lhs = expr;
        *rhs = parse_suffix();
        expr = (struct ast_expr) {
            .kind = AST_EXPR_BINARY,
            .binary = {
                .lhs = lhs,
                .rhs = rhs,
                .op = AST_BIN_MUL}};
    }
    return expr;
}

static struct ast_expr parse_term(void) {
    struct ast_expr expr = parse_factor();
    while (match(TOKEN_PLUS)) {
        struct ast_expr *lhs = new_expr();
        struct ast_expr *rhs = new_expr();
        *lhs = expr;
        *rhs = parse_factor();
        expr = (struct ast_expr) {
            .kind = AST_EXPR_BINARY,
            .binary = {
                .lhs = lhs,
                .rhs = rhs,
                .op = AST_BIN_ADD}};
    }
    return expr;
}

static struct ast_expr parse_compare(void) {
    struct ast_expr expr = parse_term();
    if (check_comparison()) {
        struct lxl_token op = advance();
        // NOTE: I want to support multi-way comparisons.
        NOT_SUPPORTED_YET(op);
    }
    return expr;
}

static struct ast_expr parse_assign(void) {
    struct ast_expr expr = parse_compare();
    if (check_assignment_target(expr) && match(TOKEN_COLON_EQUALS)) {
        struct ast_expr *value = new_expr();
        *value = parse_compare();
        expr = (struct ast_expr) {
            .kind = AST_EXPR_ASSIGN,
            .assign = {
                .target = expr.get.target,
                .value = value}};
    }
    return expr;
}

struct ast_expr *parse_expr(void) {
    struct ast_expr *expr = new_expr();
    *expr = parse_assign();
    return expr;
}

struct ast_list parse_block(void) {
    ignore_line_ending();  // Ignore LF after '{'.
    struct ast_list stmts = {0};
    while (!match(TOKEN_BKT_CURLY_RIGHT)) {
        ensure_not_at_end("Unclosed block statement");
        struct ast_stmt *stmt = parse_stmt();
        DA_APPEND(&stmts, STMT_NODE(stmt));
    }
    ignore_line_ending();  // Ignore LF after '}'.
    return stmts;
}

static struct ast_decl *parse_func_decl(void) {
    struct ast_decl *decl = new_decl();
    *decl = (struct ast_decl) {0};
    struct lxl_token name_token = consume(TOKEN_IDENTIFIER, "Expect function name");
    struct func_sig *sig = ALLOCATE(perm, sizeof *sig);
    *sig = (struct func_sig) {
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
                .body = parse_block()}};
    }
    else if (match(TOKEN_COLON_EQUALS)) {
        consume(TOKEN_KW_EXTERNAL, "Expect 'external' after ':=' for function");
        *decl = (struct ast_decl) {
            .kind = AST_DECL_FUNC_DECL,
            .func_decl = {
                .sig = sig,
                .kind = FUNC_EXTERNAL}};
    }
    else {
        parse_error_current_show_token("Expect '{' or ':=' after function signature");
    }
    return decl;
}

struct ast_decl *parse_var_decl(void) {
    struct ast_decl *decl = new_decl();
    *decl = (struct ast_decl) {0};
    struct lxl_token name_token = consume(TOKEN_IDENTIFIER, "Expect variable name after 'var'");
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
        consume(TOKEN_COLON_EQUALS, "Expect ':=' or ': T =' after variable name");
    }
    struct ast_expr *value = parse_expr();
    end_statement();
    *decl = (struct ast_decl) {
        .kind = AST_DECL_VAR_DEFN,
        .var_defn = {
            .name = name,
            .type = type,
            .value = value}};
    return decl;
}

struct ast_decl *parse_type_defn(void) {
    struct lxl_token alias_token = consume(TOKEN_IDENTIFIER, "Expect alias after 'type'");
    struct lxl_string_view alias = lxl_token_value(alias_token);
    consume(TOKEN_COLON_EQUALS, "Expect ':=' after type alias");
    struct ast_decl *decl = new_decl();
    TypeID type = parse_type("Expect type after ':=' in type alias definition");
    end_statement();
    *decl = (struct ast_decl) {
        .kind = AST_DECL_TYPE_DEFN,
        .type_defn = {
            .alias = alias,
            .type = type}};
    bool insert_result =
        insert_symbol(&symbols, st_key_of(alias),
                      (struct symbol) {
                          .kind = SYMBOL_TYPE_ALIAS,
                          .type_alias = {.type = type}});
    if (!insert_result) {
        name_error("Cannot redefine symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(alias));
    }
    return decl;
}

struct ast_decl *try_parse_decl(void) {
    if (match(TOKEN_KW_FUNC)) {
        return parse_func_decl();
    }
    if (match(TOKEN_KW_VAR)) {
        return parse_var_decl();
    }
    if (match(TOKEN_KW_TYPE)) {
        return parse_type_defn();
    }
    return NULL;
}

struct ast_stmt parse_if(void) {
    ignore_line_ending();
    struct ast_expr *cond = parse_expr();
    ignore_line_ending();
    consume(TOKEN_BKT_CURLY_LEFT, "Expect '{' after 'if' condition");
    struct ast_list then_clause = parse_block();
    struct ast_list else_clause = {0};
    if (match(TOKEN_KW_ELSE)) {
        ignore_line_ending();
        if (match(TOKEN_BKT_CURLY_LEFT)) {
            else_clause = parse_block();
        }
        else {
            // Should we allow LF here? For now, we won't.
            consume(TOKEN_KW_IF, "Expect '{' or 'if' after 'else'");
            struct ast_stmt *elif_clause = new_stmt();
            *elif_clause = parse_if();
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

struct ast_stmt *parse_stmt(void) {
    struct ast_stmt *stmt = new_stmt();
    if (match(TOKEN_KW_IF)) {
        *stmt = parse_if();
    }
    else {
        struct ast_decl *decl = try_parse_decl();
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
                .expr = {.expr = parse_expr()}};
            end_statement();
        }
    }
    return stmt;
}

struct ast_list parse(void) {
    struct ast_list nodes = {0};
    advance();  // Prime parser with first token;
    for (; ignore_line_ending(), !parser_is_finished();) {
        struct ast_node current_node = {0};
        struct ast_decl *decl = try_parse_decl();
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

static bool expect_integer_type(TypeID type) {
    if (is_integer_type(type)) return true;
    struct lxl_string_view actual_type_sv = get_type_sv(type);
    type_error("Expect integer type, not '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(actual_type_sv));
    return false;
}

static TypeID convert_binary(TypeID lhs_type, TypeID rhs_type) {
    if (!expect_integer_type(lhs_type)) return TYPE_NO_TYPE;
    if (!expect_integer_type(rhs_type)) return TYPE_NO_TYPE;
    if (sign_of_type(lhs_type) == sign_of_type(rhs_type)) {
        return max_type_rank(lhs_type, rhs_type);
    }
    type_error("Cannot mix signed and unsigned here");
    return TYPE_NO_TYPE;
}

static TypeID resolve_type(struct ast_expr *expr) {
    TypeID result_type = TYPE_NO_TYPE;
    switch (expr->kind) {
    case AST_EXPR_ASSIGN: {
        struct lxl_string_view name = expr->assign.target;
        struct symbol *target_symbol = lookup_symbol(&symbols, st_key_of(name));
        if (!target_symbol) {
            name_error("Unknown symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        if (target_symbol->kind != SYMBOL_VAR) {
            name_error("Symbol '"LXL_SV_FMT_SPEC"' is not a variable", LXL_SV_FMT_ARG(name));
            break;
        }
        result_type = resolve_type(expr->assign.value);
        if (result_type != target_symbol->var.type) {
            struct lxl_string_view result_type_name = get_type_sv(result_type);
            struct lxl_string_view target_type_name = get_type_sv(target_symbol->var.type);
            type_error("Cannot assign value of type '"LXL_SV_FMT_SPEC"' to variable '"
                       LXL_SV_FMT_SPEC"' of type '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(result_type_name), LXL_SV_FMT_ARG(name),
                       LXL_SV_FMT_ARG(target_type_name));
        }
    } break;
    case AST_EXPR_BINARY: {
        TypeID lhs_type = resolve_type(expr->binary.lhs);
        TypeID rhs_type = resolve_type(expr->binary.rhs);
        result_type = convert_binary(lhs_type, rhs_type);
    } break;
    case AST_EXPR_CALL: {
        struct lxl_string_view callee_name = expr->call.callee_name;
        struct symbol *symbol = lookup_symbol(&symbols, st_key_of(callee_name));
        if (!symbol) {
            name_error("Unknown function '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(callee_name));
            break;
        }
        if (symbol->kind != SYMBOL_FUNC) {
            name_error("Symbol '"LXL_SV_FMT_SPEC"' is not a function", LXL_SV_FMT_ARG(callee_name));
            break;
        }
        assert(symbol && symbol->kind == SYMBOL_FUNC);
        struct symbol_func *callee = &symbol->func;
        // Set the result type here to avoid a cascade of type errors, even if the arguments are incorrect.
        result_type = callee->sig->ret_type;
        int expected_arity = callee->sig->arity;
        int actual_arity = expr->call.arity;
        if (actual_arity < expected_arity) {
            type_error("Not enough arguments passed to '"LXL_SV_FMT_SPEC"': expected %d, but got %d",
                       LXL_SV_FMT_ARG(callee_name), expected_arity, actual_arity);
            break;
        }
        if (actual_arity > expected_arity) {
            type_error("Too many arguments passed to '"LXL_SV_FMT_SPEC"': expected %d, but got %d",
                       LXL_SV_FMT_ARG(callee_name), expected_arity, actual_arity);
            break;
        }
        assert(actual_arity == expected_arity);
        assert(actual_arity == expr->call.args.count);
        assert(expected_arity == callee->sig->params.count);
        for (int i = 0; i < actual_arity; ++i) {
            // Parameter: expected, argument: actual.
            struct type_decl param = callee->sig->params.items[i];
            struct ast_node arg = expr->call.args.items[i];
            assert(arg.kind == AST_EXPR);
            TypeID arg_type = resolve_type(arg.expr);
            if (arg_type == TYPE_NO_TYPE) continue;  // Skip unknown type.
            if (arg_type != param.type) {
                struct lxl_string_view param_type_name = get_type_sv(param.type);
                struct lxl_string_view arg_type_name = get_type_sv(arg_type);
                type_error("Expected type '"LXL_SV_FMT_SPEC"' for parameter '"LXL_SV_FMT_SPEC"' "
                           "of function '"LXL_SV_FMT_SPEC"', but got type '"LXL_SV_FMT_SPEC"'",
                           LXL_SV_FMT_ARG(param_type_name), LXL_SV_FMT_ARG(param.name),
                           LXL_SV_FMT_ARG(callee_name), LXL_SV_FMT_ARG(arg_type_name));
            }
        }
    } break;
    case AST_EXPR_CONSTRUCTOR: {
        struct lxl_string_view name = expr->constructor.name;
        struct symbol *symbol = lookup_symbol(&symbols, st_key_of(name));
        if (!symbol) {
            name_error("Unknown symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        if (symbol->kind != SYMBOL_TYPE_ALIAS) {
            name_error("Symbol '"LXL_SV_FMT_SPEC"' is not not a type", LXL_SV_FMT_ARG(name));
            break;
        }
        TypeID type = symbol->type_alias.type;
        struct type_info *info = get_type(type);
        if (info->kind == KIND_RECORD) {
            if (info->record_type.fields.count != expr->constructor.init_list.count) {
                type_error("Incorrect number of fields in initialiser list for type '"
                           LXL_SV_FMT_SPEC"'; expected %d but got %d",
                           LXL_SV_FMT_ARG(name), info->record_type.fields.count,
                           expr->constructor.init_list.count);
                break;
            }
            for (int i = 0; i < info->record_type.fields.count; ++i) {
                TypeID expected_type = info->record_type.fields.items[i].type;
                struct ast_node node = expr->constructor.init_list.items[i];
                assert(node.kind == AST_EXPR);
                TypeID actual_type = resolve_type(node.expr);
                if (expected_type != actual_type) {
                    struct lxl_string_view expected_sv = get_type_sv(expected_type);
                    struct lxl_string_view actual_sv = get_type_sv(actual_type);
                    type_error("Expected type '"LXL_SV_FMT_SPEC "' for field %d of type '"
                               LXL_SV_FMT_SPEC"', but got type '"LXL_SV_FMT_SPEC"'",
                               LXL_SV_FMT_ARG(expected_sv), LXL_SV_FMT_ARG(name),
                               LXL_SV_FMT_ARG(actual_sv));
                    break;
                }
            }
        }
        else {
            type_error("Invalid type for constructor: '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        result_type = type;
    } break;
    case AST_EXPR_CONVERT: {
        if (expr->convert.kind == CONVERT_AS) {
            // TODO: ensure 'as' conversion is compatible.
        }
        else {
            // TODO: ensure 'to' conversion is compatible.
        }
        result_type = expr->convert.target_type;
    } break;
    case AST_EXPR_GET: {
        struct lxl_string_view name = expr->get.target;
        struct symbol *target_symbol = lookup_symbol(&symbols, st_key_of(name));
        if (!target_symbol) {
            name_error("Unknown symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        if (target_symbol->kind != SYMBOL_VAR) {
            name_error("Symbol '"LXL_SV_FMT_SPEC"' is not a variable", LXL_SV_FMT_ARG(name));
            break;
        }
        result_type = target_symbol->var.type;
    } break;
    case AST_EXPR_INTEGER:
        // TODO "untyped" literals... i.e. integer literals have an "INTEGER_LITERAL" type.
        result_type = TYPE_INT;
        break;
    case AST_EXPR_WHEN: {
        // Propogate errors.
        if (!resolve_type(expr->when.cond)) return TYPE_NO_TYPE;
        TypeID then_type = resolve_type(expr->when.then_expr);
        TypeID else_type = resolve_type(expr->when.else_expr);
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

static void type_check_function(struct func_sig *sig, struct ast_list *body) {
    struct st_key key = st_key_of(sig->name);
    struct symbol *symbol = lookup_symbol(&symbols, key);
    if (symbol == NULL) {
        // New definition.
        bool result = insert_symbol(&symbols, key, (struct symbol) {
                .kind = SYMBOL_FUNC,
                .func = {
                    .sig = sig,
                    .kind = FUNC_INTERNAL,
                    .body = body}});
        assert(result);
    }
    else {
        // Definition of exisiting function.
        if (symbol->func.body != NULL) {
            type_error("Redefinition of function '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(sig->name));
        }
        if (symbol->func.kind == FUNC_EXTERNAL) {
            type_error("Cannot define function '"LXL_SV_FMT_SPEC"' previously defined as 'external'",
                       LXL_SV_FMT_ARG(sig->name));
        }
    }
    // TODO: set up arguments as local variables.
    // TODO: local variables.
    for (int i = 0; i < body->count; ++i) {
        assert(body->items[i].kind == AST_STMT);
        struct ast_stmt *stmt = body->items[i].stmt;
        type_check_stmt(stmt, sig->ret_type);
    }
}

static bool compare_sigs(struct func_sig *sig1, struct func_sig *sig2) {
    assert(sig1 && sig2);
    assert(sig1->arity == sig1->params.count);
    assert(sig2->arity == sig2->params.count);
    if (sig1->arity != sig2->arity) return false;
    for (int i = 0; i < sig1->arity; ++i) {
        // NOTE: parameter names are allowed to differ.
        if (sig1->params.items[i].type != sig2->params.items[i].type) return false;
    }
    return true;
}

static void type_check_decl(struct ast_decl *decl) {
    switch (decl->kind) {
    case AST_DECL_VAR_DECL: {
        struct symbol symbol = {
            .kind = SYMBOL_VAR,
            .var = {.type = decl->var_decl.type}};
        if (!insert_symbol(&symbols, st_key_of(decl->var_decl.name), symbol)) {
            name_error("Redeclaration of symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(decl->var_decl.name));
        }
    } return;
    case AST_DECL_VAR_DEFN: {
        TypeID value_type = resolve_type(decl->var_defn.value);
        if (!value_type) return;
        if (!decl->var_defn.type) decl->var_defn.type = value_type;
        if (value_type != decl->var_defn.type) {
            type_error("Mismatched types in variable definition");
        }
        struct symbol symbol = {
            .kind = SYMBOL_VAR,
            .var = {.type = decl->var_defn.type}};
        if (!insert_symbol(&symbols, st_key_of(decl->var_decl.name), symbol)) {
            name_error("Redeclaration of symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(decl->var_decl.name));
        }
    } return;
    case AST_DECL_FUNC_DECL: {
        struct func_sig *sig = decl->func_decl.sig;
        struct st_key key = st_key_of(sig->name);
        struct symbol *symbol = lookup_symbol(&symbols, key);
        if (symbol == NULL) {
            // Unseen function; add it to the symbol table.
            bool result = insert_symbol(&symbols, key, (struct symbol) {
                    .kind = SYMBOL_FUNC,
                    .func = {
                        .sig = sig,
                        .kind = decl->func_decl.kind,
                        .body = NULL}});
            assert(result);  // This should be a new symbol in the table.
        }
        else {
            // Re-declaration.
            if (symbol->func.kind != decl->func_decl.kind) {
                // Cannot redeclare with different linkage.
                const char *prev_linkage = "external";
                const char *new_linkage = "internal";
                if (symbol->func.kind == FUNC_EXTERNAL) {
                    assert(decl->func_decl.kind == FUNC_INTERNAL);
                }
                else {
                    assert(decl->func_decl.kind == FUNC_EXTERNAL);
                    prev_linkage = "internal";
                    new_linkage = "external";
                }
                type_error("Cannot redeclare function '"LXL_SV_FMT_SPEC"' as '%s'"
                           " following previous declaration as '%s'",
                           LXL_SV_FMT_ARG(sig->name), prev_linkage, new_linkage);
            }
            if (!compare_sigs(sig, symbol->func.sig)) {
                type_error("Function '"LXL_SV_FMT_SPEC"' redeclared with different signature",
                           LXL_SV_FMT_ARG(sig->name));
            }
        }
    } return;
    case AST_DECL_FUNC_DEFN:
        type_check_function(decl->func_defn.sig, &decl->func_defn.body);
        return;
    case AST_DECL_TYPE_DEFN:
        /* No type checking. */
        return;
    }
    UNREACHABLE();
}

static void type_check_stmt_list(struct ast_list stmts, TypeID ret_type) {
    for (int i = 0; i < stmts.count; ++i) {
        struct ast_node node = stmts.items[i];
        assert(node.kind != AST_EXPR);
        if (node.kind == AST_STMT) {
            type_check_stmt(node.stmt, ret_type);
        }
        else if (node.kind == AST_DECL) {
            type_check_decl(node.decl);
        }
        else {
            UNREACHABLE();
        }
    }
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
        type_check_stmt_list(stmt->if_.then_clause, ret_type);
        type_check_stmt_list(stmt->if_.else_clause, ret_type);
        return;
    }
    UNREACHABLE();
}

bool type_check(struct ast_list *nodes) {
    for (int i = 0; i < nodes->count; ++i) {
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
