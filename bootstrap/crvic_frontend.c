#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "lexel.h"

#include "ubiqs.h"

#include "crvic_ast.h"
#include "crvic_frontend.h"
#include "crvic_resources.h"
#include "crvic_symbol_table.h"
#include "crvic_type.h"

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
    VIC_INT enum_counter;
    enum func_link_kind current_func_kind;
};

struct type_checker {
    bool had_error;
};

static struct parser parser = {0};

static struct symbol_table symbols = {0};

static struct type_checker type_checker = {0};

static const char *filename = NULL;

#define SYMBOL_TABLE_CAPACITY 128

void init_frontend(struct lxl_string_view source, const char *in_filename) {
    filename = in_filename;
    parser.lexer = init_lexer(source);
    parser.current_func_kind = FUNC_INTERNAL;
    symbols = (struct symbol_table) {
        .capacity = SYMBOL_TABLE_CAPACITY,
        .slots = ALLOCATE(perm, sizeof(struct st_slot[SYMBOL_TABLE_CAPACITY]))};
    insert_symbol(&symbols, st_key_of(LXL_SV_FROM_STRLIT("int")),
                  (struct symbol) {
                      .kind = SYMBOL_TYPE_ALIAS,
                      .type_alias = {.type = RESOLVED_TYPE(TYPE_INT)}});
    insert_symbol(&symbols, st_key_of(LXL_SV_FROM_STRLIT("uint")),
                  (struct symbol) {
                      .kind = SYMBOL_TYPE_ALIAS,
                      .type_alias = {.type = RESOLVED_TYPE(TYPE_UINT)}});
}

static void report_location(struct lxl_token token) {
    fprintf(stderr, "%s:%d:%d: ", filename, token.loc.line, token.loc.column);
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

static void parse_error_show_token(struct lxl_token token, const char *restrict fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(token, fmt, vargs);
    va_end(vargs);
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

void type_error_vargs(const char *fmt, va_list vargs) {
    fprintf(stderr, "Type error: ");
    vfprintf(stderr, fmt, vargs);
    fprintf(stderr, ".\n");
    type_checker.had_error = true;
}

void type_error(const char *fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    type_error_vargs(fmt, vargs);
    va_end(vargs);
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

static bool check_line_ending(void) {
    return parser.current_token.token_type == LXL_TOKEN_LINE_ENDING;
}

static void ignore_line_ending(void) {
    if (check_line_ending()) {
        advance();
    }
}

static bool check(enum token_type type, bool strict) {
    if (!strict) {
        ignore_line_ending();
    }
    return parser.current_token.token_type == (int)type;
}

static bool match(enum token_type type, bool strict) {
    if (!check(type, strict)) return false;
    advance();
    return true;
}

static struct lxl_token consume(enum token_type type, bool strict, const char *fmt, ...) {
    if (match(type, strict)) return parser.previous_token;
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(parser.current_token, fmt, vargs);
    va_end(vargs);
    return parser.current_token;
}

static void end_statement(void) {
    // Allow the case where we've already consumed the line ending.
    if (parser.previous_token.token_type == LXL_TOKEN_LINE_ENDING) return;
    if (!check_line_ending()) {
        consume(TOKEN_SEMICOLON, true, "Expect newline or ';' at end of statement");
    }
    ignore_line_ending();
}

static void set_enum_counter(VIC_INT value) {
    parser.enum_counter = value;
}

static VIC_INT next_enum_value(void) {
    return parser.enum_counter++;
}

static struct ast_expr *new_expr(void) {
    struct ast_expr *expr = ALLOCATE(perm, sizeof *expr);
    assert(expr != NULL && "We're gonna need a bigger region!");
    return expr;
}

/*
static struct ast_stmt *new_stmt(void) {
    struct ast_stmt *stmt = ALLOCATE(perm, sizeof *stmt);
    assert(stmt != NULL && "We're gonna need a bigger region!");
    return stmt;
}
//*/

static struct ast_decl *new_decl(void) {
    struct ast_decl *decl = ALLOCATE(perm, sizeof *decl);
    assert(decl != NULL && "We're gonna need a bigger region!");
    return decl;
}

static struct ast_type *new_type(void) {
    struct ast_type *type = ALLOCATE(perm, sizeof *type);
    assert(type != NULL && "We're gonna need a bigger region!");
    return type;
}

static struct ast_node *new_node(void) {
    struct ast_node *node = ALLOCATE(perm, sizeof *node);
    assert(node != NULL && "We're gonna need a bugger region!");
    return node;
}

static struct ast_expr *copy_expr(struct ast_expr expr) {
    struct ast_expr *new = new_expr();
    *new = expr;
    return new;
}

/*
static struct ast_stmt *copy_stmt(struct ast_stmt stmt) {
    struct ast_stmt *new = new_stmt();
    *new = stmt;
    return new;
}
//*/

static struct ast_decl *copy_decl(struct ast_decl decl) {
    struct ast_decl *new = new_decl();
    *new = decl;
    return new;
}

static struct ast_type *copy_type(struct ast_type type) {
    struct ast_type *new = new_type();
    *new = type;
    return new;
}

static struct ast_node *copy_node(struct ast_node node) {
    struct ast_node *new = new_node();
    *new = node;
    return new;
}

static bool check_comparison(bool strict) {
    return check(TOKEN_EQUALS_EQUALS, strict)
        || check(TOKEN_BANG_EQUALS, strict)
        || check(TOKEN_GREATER, strict)
        || check(TOKEN_GREATER_EQUALS, strict)
        || check(TOKEN_LESS, strict)
        || check(TOKEN_LESS_EQUALS, strict)
        ;
}

static bool check_assignment_target(struct ast_expr expr) {
    // For now, only variable names can be assignment targets.
    return expr.kind == AST_EXPR_DEREF
        || expr.kind == AST_EXPR_IDENTIFIER
        || expr.kind == AST_EXPR_INDEX
        ;
}

static bool is_digit(char c, int base) {
    assert(base == 10);  // We only support base 10 ATM.
    return '0' <= c && c <= '9';
}

static uint64_t parse_integer(struct lxl_token token) {
    AUTO_BEGIN_TEMP();
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
    AUTO_END_TEMP();
    return value;
}

static uint64_t parse_previous_integer(void) {
    return parse_integer(parser.previous_token);
}

static struct lxl_string_view parse_string(struct lxl_token token) {
    AUTO_BEGIN_TEMP();
    struct sb_head sb = {.allocator = ALLOCATOR_ARD2AD(temp)};
    const char *start = token.start;
    assert(*start == 'c');
    ++start;
    assert(*start == '"');
    ++start;
    for (const char *p = start; p < token.end - 1; ++p) {
        if (*p == '\\') {
            sb_append(&sb, lxl_sv_from_startend(start, p));
            switch (*++p) {
                /* Single digits. */
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                sb_append_char(&sb, *p - '0'); break;
                /* Special characters. */
            case 'a': sb_append_char(&sb, '\a'); break;
            case 'b': sb_append_char(&sb, '\b'); break;
            case 'e': sb_append_char(&sb, '\x1B'); break;
            case 'n': sb_append_char(&sb, '\n'); break;
            case 'r': sb_append_char(&sb, '\r'); break;
            case 't': sb_append_char(&sb, '\t'); break;
            case 'v': sb_append_char(&sb, '\v'); break;
                /* Escaped characters. */
            case '"': case '\'': case '\\':
                sb_append_char(&sb, *p); break;
                /* Numeric escape sequences. */
            case 'd':
                TODO("Decimal string escape"); break;
            case 'o':
                TODO("Octal string escape"); break;
            case 'x':
                TODO("Hex string escape"); break;
                /* Unknown. */
            default:
                parse_error_previous_show_token("Unknown escape sequence '\\%c'", *p);
                break;
            }
            start = p + 1;
        }
    }
    sb_append(&sb, lxl_sv_from_startend(start, token.end - 1));
    struct lxl_string_view sv = sb_to_sv(&sb, ALLOCATOR_ARD2AD(perm));
    AUTO_END_TEMP();
    return sv;
}

static struct lxl_string_view parse_previous_string(void) {
    return parse_string(parser.previous_token);
}

static TypeID token_to_type(struct lxl_token token) {
    switch (token.token_type) {
        /* Symbolic types */
    case TOKEN_BANG: return TYPE_ABSURD;
        /* Keyword type names */
    case TOKEN_KW_C_STRING: return TYPE_C_STRING;
    case TOKEN_KW_I8: return TYPE_I8;
    case TOKEN_KW_I16: return TYPE_I16;
    case TOKEN_KW_I32: return TYPE_I32;
    case TOKEN_KW_I64: return TYPE_I64;
    case TOKEN_KW_STRING: TODO("string keyword"); break;
    case TOKEN_KW_U8: return TYPE_U8;
    case TOKEN_KW_U16: return TYPE_U16;
    case TOKEN_KW_U32: return TYPE_U32;
    case TOKEN_KW_U64: return TYPE_U64;
        /* Keyword type aliases (predefined) */
    case TOKEN_KW_INT: return TYPE_INT;
    case TOKEN_KW_UINT: return TYPE_UINT;
    }
    /* Unresolved type aliases */
    return TYPE_NO_TYPE;
}

static struct ast_expr parse_expr(void);
static struct ast_stmt parse_stmt(void);
static struct ast_sig parse_func_sig(struct lxl_string_view name);

static enum rw_access parse_rw_access(void) {
    if (match(TOKEN_KW_MUT, false)) return RW_READ_WRITE;
    if (match(TOKEN_KW_OUT, false)) return RW_WRITE_BEFORE_READ;
    return RW_READ_ONLY;
}

static struct ast_type parse_type(const char *fmt, ...) {
    static_assert(TYPE_NO_TYPE == 0, "TYPE_NO_TYPE should be 'falsy'");
    // Basic types.
    TypeID primitive_type = token_to_type(parser.current_token);
    if (primitive_type) {
        advance();
        return (struct ast_type) {
            .kind = AST_TYPE_PRIMITIVE,
            .resolved_type = primitive_type};
    }
    // Type aliases.
    if (match(TOKEN_IDENTIFIER, false)) {
        return (struct ast_type) {
            .kind = AST_TYPE_ALIAS,
            .alias = {
                .name = lxl_token_value(parser.previous_token)}};
    }
    // Record types.
    if (match(TOKEN_KW_RECORD, false)) {
        // begin_temp();
        consume(TOKEN_BKT_CURLY_LEFT, false, "Expect '{' after 'record'");
        struct ast_type_decl_list fields = AST_TYPE_DECL_LIST(perm);
        while (!check(TOKEN_BKT_CURLY_RIGHT, false)) {
            struct lxl_token field_name_token = consume(TOKEN_IDENTIFIER, false, "Expect field name");
            consume(TOKEN_COLON, false, "Expect ':' after field name");
            ignore_line_ending();
            struct lxl_string_view field_name = lxl_token_value(field_name_token);
            struct ast_type field_type = parse_type("Expect field type after ':'");
            struct ast_type_decl field = {.name = field_name, .type = copy_type(field_type)};
            DA_APPEND(&fields, field);
            if (!match(TOKEN_COMMA, false)) break;
        }
        consume(TOKEN_BKT_CURLY_RIGHT, false, "Expect '}' after record definition");
        return (struct ast_type) {
            .kind = AST_TYPE_RECORD,
            .record_lit = {
                .fields = fields}};
    }
    // Enum types.
    if (match(TOKEN_KW_ENUM, false)) {
        // begin_temp();
        struct ast_type underlying_type = {
            .kind = AST_TYPE_PRIMITIVE,
            .resolved_type = TYPE_INT};
        if (match(TOKEN_COLON, false)) {
            // Underlying type.
            ignore_line_ending();
            underlying_type = parse_type("Expect underlying type for enum after ':'");
        }
        consume(TOKEN_BKT_CURLY_LEFT, false, "Expect '{' after 'enum'");
        struct ast_enum_field_list fields = AST_ENUM_FIELD_LIST(perm);
        while (!check(TOKEN_BKT_CURLY_RIGHT, false)) {
            struct lxl_token field_name_token = consume(TOKEN_IDENTIFIER, false, "Expect field name");
            struct lxl_string_view field_name = lxl_token_value(field_name_token);
            struct ast_expr *value = NULL;
            if (match(TOKEN_COLON_EQUALS, false)) {
                // Specified value.
                // Parsing as expr allows for constant expressions like `1+2`.
                value = copy_expr(parse_expr());
            }
            struct ast_enum_field field = {.name = field_name, .value = value};
            DA_APPEND(&fields, field);
            if (!match(TOKEN_COMMA, false)) break;
        }
        consume(TOKEN_BKT_CURLY_RIGHT, false, "Expect '}' after enum field list");
        return (struct ast_type) {
            .kind = AST_TYPE_ENUM,
            .enum_lit = {
                .underlying_type = copy_type(underlying_type),
                .fields = fields}};
    }
    // Pointer types.
    if (match(TOKEN_CARET, false)) {
        enum rw_access rw = parse_rw_access();
        struct ast_type dest_type = parse_type("Expect type after '^'");
        return (struct ast_type) {
            .kind = AST_TYPE_POINTER,
            .pointer = {
                .kind = POINTER_PROPER,
                .rw = rw,
                .dest_type = copy_type(dest_type)}};
    }
    // Array, array-like pointer, slice.
    if (match(TOKEN_BKT_SQUARE_LEFT, false)) {
        // Array-like pointer.
        if (match(TOKEN_CARET, false)) {
            consume(TOKEN_BKT_SQUARE_RIGHT, false, "Expect ']' after '[^' for array-like pointer type");
            enum rw_access rw = parse_rw_access();
            struct ast_type dest_type = parse_type("Expect type after '[^]'");
            return (struct ast_type) {
                .kind = AST_TYPE_POINTER,
                .pointer = {
                    .kind = POINTER_ARRAY_LIKE,
                    .rw = rw,
                    .dest_type = copy_type(dest_type)}};
        }
        // Slice.
        if (match(TOKEN_BKT_SQUARE_RIGHT, false)) {
            TODO("slices");  // Do we even need them in rVic?
        }
        // Array (fixed size).
        struct ast_expr count = parse_expr();
        // TODO: magic '_' for size. Idea: NULL means inferred size.
        consume(TOKEN_BKT_SQUARE_RIGHT, false, "Expect ']' after array size");
        enum rw_access rw = RW_READ_ONLY;
        if (match(TOKEN_KW_MUT, false)) {
            rw = RW_READ_WRITE;
        }
        else if (match(TOKEN_KW_OUT, false)) {
            rw = RW_WRITE_BEFORE_READ;
        }
        struct ast_type dest_type = parse_type("Expect array element type");
        return (struct ast_type) {
            .kind = AST_TYPE_ARRAY,
            .array = {
                .count = copy_expr(count),
                .rw = rw,
                .dest_type = copy_type(dest_type)}};
    }
    // Function types.
    if (match(TOKEN_KW_FUNC, false)) {
        consume(TOKEN_BKT_ROUND_LEFT, false,
                "Expect '(' after 'func' in fucntion type (note the name is omitted)");
        struct ast_sig *sig = ALLOCATE(perm, sizeof *sig);
        *sig  = parse_func_sig(LXL_SV_EMPTY());
        return (struct ast_type) {
            .kind = AST_TYPE_FUNCTION,
            .function = {.sig = sig}};
    }
    // Not a type.
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_show_token_vargs(parser.current_token, fmt, vargs);
    va_end(vargs);
    return (struct ast_type) {
        .kind = AST_TYPE_PRIMITIVE,
        .resolved_type = TYPE_NO_TYPE};
}

static struct ast_expr parse_assign(void);

static struct ast_expr parse_compare(void);

static struct ast_list parse_comma_list_no_assign(enum token_type terminator, const char *message) {
    struct ast_list args = {0};
    while (!match(terminator, false)) {
        struct ast_node *arg = copy_node(EXPR_NODE(parse_compare()));  // Do not allow assign expressions.
        DLLIST_APPEND(&args, arg);
        if (!match(TOKEN_COMMA, true)) {
            consume(terminator, false, message);
            break;
        }
    }
    return args;
}

static struct ast_list parse_arg_list(void) {
    return parse_comma_list_no_assign(TOKEN_BKT_ROUND_RIGHT, "Expect ')' after argument list");
}

static struct ast_expr parse_primary(void) {
    struct ast_expr expr = {0};
    if (match(TOKEN_LIT_INTEGER, true)) {
        expr = (struct ast_expr) {
            .kind = AST_EXPR_INTEGER,
            .integer = {.value = parse_previous_integer()}};
    }
    else if (match(TOKEN_LIT_STRING, true)) {
        expr = (struct ast_expr) {
            .kind = AST_EXPR_STRING,
            .string = {.value = parse_previous_string()}};
    }
    else if (match(TOKEN_IDENTIFIER, true)) {
        // Why is this handled here? Surely we can handle constructors as a suffix operator.
        struct lxl_string_view name = lxl_token_value(parser.previous_token);
        if (match(TOKEN_BKT_CURLY_LEFT, true)) {
            struct ast_list inits = parse_comma_list_no_assign(TOKEN_BKT_CURLY_RIGHT,
                                                     "Expect '}' after initialiser list");
            expr = (struct ast_expr) {
                .kind = AST_EXPR_CONSTRUCTOR,
                .constructor = {
                    .name = name,
                    .init_list = inits}};
        }
        else {
            expr = (struct ast_expr) {
                .kind = AST_EXPR_IDENTIFIER,
                .identifier = {.name = name}};
        }
    }
    else if (match(TOKEN_BKT_ROUND_LEFT, true)) {
        ignore_line_ending();
        expr = parse_assign();
        consume(TOKEN_BKT_ROUND_RIGHT, false, "Expect ')' after grouped expression");
    }
    else if (match(TOKEN_KW_NULL, true)) {
        expr = (struct ast_expr) {
            .kind = AST_EXPR_NULL};
    }
    else if (match(TOKEN_KW_WHEN, true)) {
        ignore_line_ending();
        struct ast_expr *cond = new_expr();
        struct ast_expr *then_expr = new_expr();
        struct ast_expr *else_expr = new_expr();
        *cond = parse_expr();
        consume(TOKEN_KW_THEN, false, "Expect 'then' after condition in when expression");
        ignore_line_ending();
        *then_expr = parse_expr();
        consume(TOKEN_KW_ELSE, false, "Expect 'else' after then branch in when expression");
        *else_expr = parse_expr();
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

static struct ast_expr parse_suffix(void) {
    struct ast_expr expr = parse_primary();
    for (;;) {
        // Parse multiple suffixes.
        if (match(TOKEN_CARET, true)) {
            expr = (struct ast_expr) {
                .kind = AST_EXPR_DEREF,
                .deref = {.pointer = copy_expr(expr)}};
        }
        else if (match(TOKEN_DOT, true)) {
            struct lxl_token name = consume(TOKEN_IDENTIFIER, false, "Expect field name after '.'");
            expr = (struct ast_expr) {
                .kind = AST_EXPR_FIELD,
                .field = {
                    .target = copy_expr(expr),
                    .name = lxl_token_value(name)}};
        }
        else if (match(TOKEN_BKT_ROUND_LEFT, true)) {
            struct ast_expr *callee = new_expr();
            *callee = expr;
            if (callee->kind != AST_EXPR_IDENTIFIER) {
                parse_error_previous_show_token("Callee must be an identifier");
            }
            struct ast_list args = parse_arg_list();
            expr = (struct ast_expr) {
                .kind = AST_EXPR_CALL,
                .call = {
                    .callee_name = callee->identifier.name,
                    .arity = args.count,
                    .args = args}};
        }
        else if (match(TOKEN_BKT_SQUARE_LEFT, true)) {
            ignore_line_ending();
            struct ast_expr index = parse_expr();
            consume(TOKEN_BKT_SQUARE_RIGHT, false, "Expect ']' after array index");
            expr = (struct ast_expr) {
                .kind = AST_EXPR_INDEX,
                .index = {
                    .array = copy_expr(expr),
                    .index = copy_expr(index)}};
        }
        else if (match(TOKEN_KW_AS, true) || match(TOKEN_KW_TO, true)) {
            struct lxl_token conversion = parser.previous_token;
            struct lxl_string_view conv_sv = lxl_token_value(conversion);
            struct ast_type *target_type = new_type();
            *target_type = parse_type("Expect target type for '"LXL_SV_FMT_SPEC"' conversion",
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
        else {
            // No more suffixes.
            break;
        }
    }
    return expr;
}

static struct ast_expr parse_prefix(void) {
    struct ast_expr expr = {0};
    if (match(TOKEN_AMPERSAND, true)) {
        enum rw_access rw = RW_READ_ONLY;
        if (match(TOKEN_KW_MUT, false)) {
            rw = RW_READ_WRITE;
        }
        else if (match(TOKEN_KW_OUT, false)) {
            rw = RW_WRITE_BEFORE_READ;
        }
        ignore_line_ending();
        struct ast_expr target = parse_prefix();
        expr = (struct ast_expr) {
            .kind = AST_EXPR_ADDRESS_OF,
            .address_of = {
                .target = copy_expr(target),
                .rw = rw}};
    }
    else if (match(TOKEN_MINUS, true)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else if (match(TOKEN_PLUS, true)) {
        NOT_SUPPORTED_YET_PREVIOUS();
    }
    else {
        expr = parse_suffix();
    }
    return expr;
}

static struct ast_expr parse_factor(void) {
    struct ast_expr expr = parse_prefix();
    while (match(TOKEN_STAR, true)) {
        struct ast_expr *lhs = new_expr();
        struct ast_expr *rhs = new_expr();
        *lhs = expr;
        *rhs = parse_factor();
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
    while (match(TOKEN_PLUS, true)) {
        struct ast_expr *lhs = new_expr();
        struct ast_expr *rhs = new_expr();
        *lhs = expr;
        *rhs = parse_term();
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
    if (check_comparison(true)) {
        struct lxl_token op = advance();
        // NOTE: I want to support multi-way comparisons.
        NOT_SUPPORTED_YET(op);
    }
    return expr;
}

static struct ast_expr parse_assign(void) {
    struct ast_expr expr = parse_compare();
    if (!match(TOKEN_COLON_EQUALS, true)) return expr;
    if (!check_assignment_target(expr)) {
        parse_error_previous_show_token("Expression on left hand side of ':=' is not assignable");
    }
    struct ast_expr *value = new_expr();
    *value = parse_compare();
    expr = (struct ast_expr) {
        .kind = AST_EXPR_ASSIGN,
        .assign = {
            .target = copy_expr(expr),
            .value = value}};
    return expr;
}

static struct ast_expr parse_expr(void) {
    return parse_assign();
}

static struct ast_list parse_block(void) {
    ignore_line_ending();  // Ignore LF after '{'.
    struct ast_list stmts = {0};
    while (!match(TOKEN_BKT_CURLY_RIGHT, false)) {
        ensure_not_at_end("Unclosed block statement");
        struct ast_stmt stmt = parse_stmt();
        struct ast_node *node = copy_node(STMT_NODE(stmt));
        DLLIST_APPEND(&stmts, node);
    }
    ignore_line_ending();  // Ignore LF after '}'.
    return stmts;
}

static struct ast_sig parse_func_sig(struct lxl_string_view name) {
    struct ast_sig sig = {
        .resolved_sig = {
            .name = name,
            .params = TYPE_DECL_LIST(perm)},
        .params = AST_TYPE_DECL_LIST(perm)};
    while (!match(TOKEN_BKT_ROUND_RIGHT, false)) {
        ensure_not_at_end("Unclosed function parameter list");
        if (match(TOKEN_DOT_DOT_BANG, false)) {
            // Variadic (C-style) parameter(s).
            sig.c_variadic = true;
            if (parser.current_func_kind != FUNC_EXTERNAL) {
                parse_error_previous_show_token(
                    "Only external functions can have C-style variadic parameters");
            }
            // NOTE: '..!' must be the final parameter, although a trailing comma is still allowed.
            (void)match(TOKEN_COMMA, true);
            consume(TOKEN_BKT_ROUND_RIGHT, false,
                    "Expect end of parameter list after C-style variadic parameter declaration '..!'");
            break;
        }
        struct lxl_token param_token = consume(TOKEN_IDENTIFIER, false, "Expect parameter name");
        consume(TOKEN_COLON, false, "Expect ':' after parameter name");
        struct ast_type *param_type = new_type();
        *param_type = parse_type("Expect type after parameter name");
        struct lxl_string_view param_name = lxl_token_value(param_token);
        struct ast_type_decl param = {
            .name = param_name,
            .type = param_type};
        DA_APPEND(&sig.params, param);
        if (!match(TOKEN_COMMA, true)) {
            // Allow no trailing comma.
            consume(TOKEN_BKT_ROUND_RIGHT, false,
                    "Expect ',' after parameter or ')' at end of parameter list");
            break;
        }
    }
    sig.ret_type = new_type();
    *sig.ret_type = (match(TOKEN_ARROW_RIGHT, true))
        ? parse_type("Expect return type after '->'")
        : (struct ast_type) {
            .kind = AST_TYPE_PRIMITIVE,
            .resolved_type = TYPE_UNIT};
    return sig;
}

static struct ast_decl parse_func_decl(void) {
    struct ast_decl decl = {0};
    struct lxl_token name_token = consume(TOKEN_IDENTIFIER, false, "Expect function name");
    struct lxl_string_view name = lxl_token_value(name_token);
    consume(TOKEN_BKT_ROUND_LEFT, false, "Expect '(' after function name");
    struct ast_sig *sig = ALLOCATE(perm, sizeof *sig);
    *sig = parse_func_sig(name);
    bool had_line_ending = check_line_ending();
    if (match(TOKEN_BKT_CURLY_LEFT, false)) {
        if (parser.current_func_kind == FUNC_EXTERNAL) {
            parse_error_previous_show_token("External functions can only be declared, not defined");
        }
        else {
            assert(parser.current_func_kind == FUNC_INTERNAL);
        }
        decl = (struct ast_decl) {
            .kind = AST_DECL_FUNC,
            .func = {
                .sig = sig,
                .link_kind = parser.current_func_kind,
                .decl_kind = AST_FUNC_DEFN,
                .body = parse_block()}};
    }
    else if (had_line_ending || match(TOKEN_SEMICOLON, true)) {
        decl = (struct ast_decl) {
            .kind = AST_DECL_FUNC,
            .func = {
                .sig = sig,
                .link_kind = parser.current_func_kind,
                .decl_kind = AST_FUNC_DECL}};
    }
    else {
        parse_error_current_show_token("Expect '{' or end of statement after function signature");
    }
    struct st_key key = st_key_of(name);
    struct symbol *symbol = lookup_symbol(&symbols, key);
    if (!symbol) {
        // New function.
        symbol = insert_symbol(&symbols, key, (struct symbol) {
                .kind = SYMBOL_FUNC,
                .func = {
                    .func_decl = decl,
                    .sig = &decl.func.sig->resolved_sig}});
        assert(symbol);
        assert(symbol->kind == SYMBOL_FUNC);
        add_function(&symbol->func);
    }
    else if (symbol->kind == SYMBOL_FUNC) {
        // Function previously declared/defined.
        struct ast_decl **prev_decl_ptr = &decl.func.prev_decl;
        // TODO: use address of decl node for `.func_decl`
        *prev_decl_ptr = copy_decl(symbol->func.func_decl);
        symbol->func.func_decl = decl;
    }
    else {
        name_error("Previously defined symbol '"LXL_SV_FMT_SPEC"' redeclared as function", name);
    }
    return decl;
}

struct ast_decl parse_var_decl(enum ast_var_kind kind) {
    struct lxl_token var_token = parser.previous_token;
    // Sanity check:
    assert((var_token.token_type == TOKEN_KW_VAR && kind == AST_VAR_VAR) ||
           (var_token.token_type == TOKEN_KW_VAL && kind == AST_VAR_VAL));
    struct lxl_token name_token = consume(TOKEN_IDENTIFIER, false, "Expect variable name after 'var'");
    struct lxl_string_view name = lxl_token_value(name_token);
    struct ast_type *type = NULL;
    struct ast_expr *value = NULL;
    if (match(TOKEN_COLON, false)) {
        type = copy_type(parse_type("Expect type after ':'"));
        if (!match(TOKEN_EQUALS, false)) {
            if (kind == AST_VAR_VAL) {
                parse_error_show_token(var_token, "Immutable value '"LXL_SV_FMT_SPEC"' must be initialised",
                                       LXL_SV_FMT_ARG(name));
            }
        }
        else {
            ignore_line_ending();
            value = copy_expr(parse_expr());
        }
    }
    else {
        consume(TOKEN_COLON_EQUALS, false, "Expect ':=' or ': T =' after variable name");
        ignore_line_ending();
        value = copy_expr(parse_expr());
    }
    end_statement();
    return (struct ast_decl) {
        .kind = AST_DECL_VAR,
        .var = {
            .name = name,
            .type = type,
            .kind = kind,
            .value = value}};
}

struct ast_decl parse_type_defn(void) {
    struct lxl_token alias_token = consume(TOKEN_IDENTIFIER, false, "Expect alias after 'type'");
    struct lxl_string_view alias = lxl_token_value(alias_token);
    consume(TOKEN_COLON_EQUALS, false, "Expect ':=' after type alias");
    struct ast_decl decl = {0};
    struct ast_type *type = new_type();
    *type = parse_type("Expect type after ':=' in type alias definition");
    end_statement();
    decl = (struct ast_decl) {
        .kind = AST_DECL_TYPE_DEFN,
        .type_defn = {
            .alias = alias,
            .type = type}};
    bool insert_result =
        insert_symbol(&symbols, st_key_of(alias),
                      (struct symbol) {
                          .kind = SYMBOL_TYPE_ALIAS,
                          .type_alias = {.type = *type}});
    if (!insert_result) {
        name_error("Cannot redefine symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(alias));
    }
    return decl;
}

bool try_parse_decl(struct ast_decl *OUT_decl) {
    if (match(TOKEN_KW_EXTERNAL, true)) {
        enum func_link_kind prev_kind = parser.current_func_kind;
        parser.current_func_kind = FUNC_EXTERNAL;
        if (match(TOKEN_KW_FUNC, false)) {
            *OUT_decl = parse_func_decl();
        }
        else {
            consume(TOKEN_BKT_CURLY_LEFT, false, "Expect 'func' or '{' after 'external'");
            *OUT_decl = (struct ast_decl) {
                .kind = AST_DECL_EXTERNAL_BLOCK,
                .external_block = {
                    .decls = {0}}};
            while (!match(TOKEN_BKT_CURLY_RIGHT, false)) {
                struct lxl_token func_token = consume(TOKEN_KW_FUNC, false, "Expect function declaration");
                struct ast_node *node = new_node();
                *node = DECL_NODE(parse_func_decl());
                struct ast_decl *func = &node->decl;
                assert(func->kind == AST_DECL_FUNC);
                if (func->func.decl_kind != AST_FUNC_DECL) {
                    parse_error_show_token(func_token, "Expect function delcaration in 'external' block");
                }
                else {
                    assert(func->func.link_kind == FUNC_EXTERNAL &&
                           "Function should be external inside 'external' block");
                    DLLIST_APPEND(&OUT_decl->external_block.decls, node);
                }
            }
            ignore_line_ending();

        }
        parser.current_func_kind = prev_kind;
    }
    else if (match(TOKEN_KW_FUNC, true)) {
        *OUT_decl = parse_func_decl();
    }
    else if (match(TOKEN_KW_TYPE, true)) {
        *OUT_decl = parse_type_defn();
    }
    else if (match(TOKEN_KW_VAR, true)) {
        *OUT_decl = parse_var_decl(AST_VAR_VAR);
    }
    else if (match(TOKEN_KW_VAL, true)) {
        *OUT_decl = parse_var_decl(AST_VAR_VAL);
    }
    else {
        return false;
    }
    return true;
}

struct ast_stmt parse_if(void) {
    ignore_line_ending();
    struct ast_expr *cond = new_expr();
    *cond = parse_expr();
    consume(TOKEN_BKT_CURLY_LEFT, false, "Expect '{' after 'if' condition");
    struct ast_list then_clause = parse_block();
    struct ast_list else_clause = {0};
    if (match(TOKEN_KW_ELSE, true)) {
        ignore_line_ending();
        if (match(TOKEN_BKT_CURLY_LEFT, false)) {
            else_clause = parse_block();
        }
        else {
            consume(TOKEN_KW_IF, false, "Expect '{' or 'if' after 'else'");
            struct ast_node *elif_clause = new_node();
            *elif_clause = STMT_NODE(parse_if());
            DLLIST_APPEND(&else_clause, elif_clause);
        }
    }
    return (struct ast_stmt) {
        .kind = AST_STMT_IF,
        .if_ = {
            .cond = cond,
            .then_clause = then_clause,
            .else_clause = else_clause}};
}

struct ast_stmt parse_return(void) {
    struct ast_expr *expr = NULL;
    if (check_line_ending() || check(TOKEN_SEMICOLON, true)) {
        end_statement();
    }
    else {
        // Return with expression.
        expr = new_expr();
        *expr = parse_expr();
    }
    return (struct ast_stmt) {
        .kind = AST_STMT_RETURN,
        .return_ = {.expr = expr}};
}

struct ast_stmt parse_stmt(void) {
    if (match(TOKEN_KW_IF, true)) {
        return parse_if();
    }
    if (match(TOKEN_KW_RETURN, true)) {
        return parse_return();
    }
    struct ast_decl decl = {0};
    if (try_parse_decl(&decl)) {
        // Declaration statement.
        return (struct ast_stmt) {
            .kind = AST_STMT_DECL,
            .decl = {.decl = copy_decl(decl)}};
    }
    // Expression statement.
    struct ast_expr expr = parse_expr();
    end_statement();
    return (struct ast_stmt) {
        .kind = AST_STMT_EXPR,
        .expr = {.expr = copy_expr(expr)}};
}

struct ast_list parse(void) {
    struct ast_list nodes = {0};
    advance();  // Prime parser with first token;
    for (; ignore_line_ending(), !parser_is_finished();) {
        struct ast_decl decl = {0};
        if (try_parse_decl(&decl)) {
            struct ast_node *node = copy_node(DECL_NODE(decl));
            DLLIST_APPEND(&nodes, node);
        }
        else {
            parse_error_current_show_token("Unexpected token at module top level");
        }
    }
    return nodes;
}

VIC_INT fold_integer_constant(struct ast_expr *expr, const char *fmt, ...) {
    if (expr->kind == AST_EXPR_INTEGER) return expr->integer.value;
    TODO("Other constant expressions");
    // Not an integer constant.
    va_list vargs;
    va_start(vargs, fmt);
    type_error_vargs(fmt, vargs);
    va_end(vargs);
    return 0;
}

static TypeID type_check_expr(struct ast_expr *expr);
static TypeID resolve_type(struct ast_type *type);

static TypeID resolve_array(struct ast_type *type) {
    assert(type->kind == AST_TYPE_ARRAY);
    type_check_expr(type->array.count);
    VIC_INT count = fold_integer_constant(type->array.count, "Array size must be an integer constant");
    TypeID dest_type = resolve_type(type->array.dest_type);
    struct type_info array_info = {
        .kind = KIND_ARRAY,
        .array_type = {
            .count = count,
            .rw = type->array.rw,
            .dest_type = dest_type}};
    type->resolved_type = get_or_add_type(array_info);
    return type->resolved_type;
}

static struct type_decl resolve_type_decl(struct ast_type_decl *type_decl) {
    return (struct type_decl) {
        .name = type_decl->name,
        .type = resolve_type(type_decl->type)};
}

static struct func_sig *resolve_func_sig(struct ast_sig *sig) {
    if (sig->resolved) return &sig->resolved_sig;;
    struct type_decl_list *resolved_params = &sig->resolved_sig.params;
    DA_RESERVE(resolved_params, sig->params.count);
    for (int i = 0; i < sig->params.count; ++i) {
        resolved_params->items[resolved_params->count++] = resolve_type_decl(&sig->params.items[i]);
    }
    sig->resolved_sig.ret_type = resolve_type(sig->ret_type);
    sig->resolved_sig.arity = sig->resolved_sig.params.count;
    sig->resolved_sig.c_variadic = sig->c_variadic;
    sig->resolved = true;
    return &sig->resolved_sig;
}

static TypeID resolve_record(struct ast_type *type) {
    assert(type->kind == AST_TYPE_RECORD);
    AUTO_BEGIN_TEMP();
    struct type_decl_list fields = TYPE_DECL_LIST(temp);
    DA_RESERVE(&fields, type->record_lit.fields.count);
    for (int i = 0; i < type->record_lit.fields.count; ++i) {
        fields.items[fields.count++] = resolve_type_decl(&type->record_lit.fields.items[i]);
    }
    struct type_info record_info = {
        .kind = KIND_RECORD,
        .record_type = {
            .fields = PROMOTE_DA(&fields)}};
    AUTO_END_TEMP();
    type->resolved_type = get_or_add_type(record_info);
    return type->resolved_type;
}

static struct enum_field resolve_enum_field(struct ast_enum_field *field) {
    if (field->value) {
        set_enum_counter(fold_integer_constant(field->value, "Expect integer constant for enum field"));
    }
    return (struct enum_field) {
        .name = field->name,
        .value = next_enum_value()};
}

static TypeID resolve_enum(struct ast_type *type) {
    assert(type->kind == AST_TYPE_ENUM);
    AUTO_BEGIN_TEMP();
    struct enum_field_list fields = ENUM_FIELD_LIST(temp);
    set_enum_counter(0);
    DA_RESERVE(&fields, type->enum_lit.fields.count);
    for (int i = 0; i < type->enum_lit.fields.count; ++i) {
        fields.items[fields.count++] = resolve_enum_field(&type->enum_lit.fields.items[i]);
    }
    TypeID underlying_type = resolve_type(type->enum_lit.underlying_type);
    struct type_info enum_info = {
        .kind = KIND_ENUM,
        .enum_type = {
            .underlying_type = underlying_type,
            .fields = PROMOTE_DA(&fields)}};
    AUTO_END_TEMP();
    type->resolved_type = get_or_add_type(enum_info);
    return type->resolved_type;
}

static TypeID resolve_pointer(struct ast_type *type) {
    assert(type->kind == AST_TYPE_POINTER);
    enum pointer_kind pointer_kind = POINTER_PROPER;
    TypeID dest_type = resolve_type(type->pointer.dest_type);
    assert(dest_type != TYPE_NO_TYPE);
    struct type_info pointer_info = {
        .kind = KIND_POINTER,
        .pointer_type = {
            .kind = pointer_kind,
            .rw = type->pointer.rw,
            .dest_type = dest_type}};
    type->resolved_type = get_or_add_type(pointer_info);
    return type->resolved_type;
}

static TypeID resolve_function(struct ast_type *type) {
    assert(type->kind == AST_TYPE_FUNCTION);
    struct func_sig *sig = resolve_func_sig(type->function.sig);
    struct type_info function_info = {
        .kind = KIND_FUNCTION,
        .function_type = {.sig = sig}};
    type->resolved_type = get_or_add_type(function_info);
    return type->resolved_type;
}

static TypeID resolve_type(struct ast_type *type) {
    assert(type);
    if (type->resolved_type) return type->resolved_type;
    switch (type->kind) {
    case AST_TYPE_PRIMITIVE:
        UNREACHABLE();  // Should have been caught above.
        break;
    case AST_TYPE_ALIAS: {
        struct symbol *symbol = lookup_symbol(&symbols, st_key_of(type->alias.name));
        if (!symbol) {
            name_error("Unknown symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(type->alias.name));
            break;
        }
        if (symbol->kind != SYMBOL_TYPE_ALIAS) {
            name_error("Symbol '"LXL_SV_FMT_SPEC"' is not a type alias", LXL_SV_FMT_ARG(type->alias.name));
            break;
        }
        return (type->resolved_type = resolve_type(&symbol->type_alias.type));
    }
    case AST_TYPE_ARRAY:
        return resolve_array(type);
    case AST_TYPE_ENUM:
        return resolve_enum(type);
    case AST_TYPE_FUNCTION:
        return resolve_function(type);
    case AST_TYPE_POINTER:
        return resolve_pointer(type);
    case AST_TYPE_RECORD:
        return resolve_record(type);
    }
    UNREACHABLE();
    return TYPE_NO_TYPE;
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

static bool check_assignable(TypeID ltype, TypeID rtype) {
    if (ltype == rtype) return true;
    struct type_info *linfo = get_type(ltype);
    struct type_info *rinfo = get_type(rtype);
    assert(linfo && rinfo);
    if (linfo->kind == KIND_POINTER) {
        // Pointers can be assigned `null`.
        if (rtype == TYPE_NULLPTR_TYPE) return true;
        // Pointers to `!` can be obtained from any pointer.
        if (linfo->pointer_type.dest_type == TYPE_ABSURD && rinfo->kind == KIND_POINTER) return true;
    }
    return false;
}

static TypeID type_check_assignment_target(struct ast_expr *target) {
    switch (target->kind) {
    case AST_EXPR_IDENTIFIER: {
        struct lxl_string_view name = target->identifier.name;
        struct symbol *target_symbol = lookup_symbol(&symbols, st_key_of(name));
        if (!target_symbol) {
            name_error("Unknown symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        if (target_symbol->kind == SYMBOL_VAL) {
            type_error("Cannot reassign immutable value '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        else if (target_symbol->kind != SYMBOL_VAR) {
            name_error("Symbol '"LXL_SV_FMT_SPEC"' is not a variable", LXL_SV_FMT_ARG(name));
            break;
        }
        target->type = target_symbol->var.type;
    } break;
    case AST_EXPR_DEREF: {
        TypeID pointer_type = type_check_expr(target->deref.pointer);
        struct type_info *pointer_info = get_type(pointer_type);
        assert(pointer_info && pointer_info->kind == KIND_POINTER);
        switch (pointer_info->pointer_type.rw) {
        case RW_READ_ONLY:
            type_error("Attempt to write to read-only pointer");
            break;
        case RW_READ_WRITE:
            // Do nothing.
            break;
        case RW_WRITE_BEFORE_READ:
            TODO("Check 'out' pointers");
            break;
        }
        target->type = pointer_info->pointer_type.dest_type;
        if (target->type == TYPE_ABSURD) {
            type_error("Cannot dereference pointer to absurd type '!'");
        }
    } break;
    case AST_EXPR_INDEX: {
        TypeID array_type = type_check_expr(target->index.array);
        struct type_info *array_info = get_type(array_type);
        assert(array_info && array_info->kind == KIND_ARRAY);
        switch (array_info->array_type.rw) {
        case RW_READ_ONLY:
            type_error("Attempt to write to read-only array element");
            break;
        case RW_READ_WRITE:
            // Do nothing.
            break;
        case RW_WRITE_BEFORE_READ:
            // Should we even allow 'out' elements?
            TODO("Check 'out' array elements");
            break;
        }
        target->type = array_info->array_type.dest_type;
        assert(target->type != TYPE_ABSURD);  // You cannot have an array of absurd.
    } break;
    default:
        UNREACHABLE();
    }
    return target->type;
}

static TypeID type_check_expr(struct ast_expr *expr) {
    TypeID result_type = TYPE_NO_TYPE;
    switch (expr->kind) {
    case AST_EXPR_ADDRESS_OF: {
        TypeID target_type = type_check_expr(expr->address_of.target);
        // TODO: check if `target_type` is addressable.
        enum pointer_kind pointer_kind = POINTER_PROPER;
        struct type_info pointer_info = {
            .kind = KIND_POINTER,
            .pointer_type = {
                .kind = pointer_kind,
                .rw = expr->address_of.rw,
                .dest_type = target_type}};
        result_type = get_or_add_type(pointer_info);
    } break;
    case AST_EXPR_ASSIGN: {
        struct ast_expr *target = expr->assign.target;
        TypeID target_type = type_check_assignment_target(target);
        result_type = type_check_expr(expr->assign.value);
        if (!check_assignable(target_type, result_type)) {
            struct lxl_string_view result_type_name = get_type_sv(result_type);
            struct lxl_string_view target_type_name = get_type_sv(target_type);
            type_error("Cannot assign value of type '"LXL_SV_FMT_SPEC"'"
                       " to target of type '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(result_type_name),
                       LXL_SV_FMT_ARG(target_type_name));
        }
    } break;
    case AST_EXPR_BINARY: {
        TypeID lhs_type = type_check_expr(expr->binary.lhs);
        TypeID rhs_type = type_check_expr(expr->binary.rhs);
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
        if (!symbol->resolved) {
            assert(!callee->func_decl.func.sig->resolved);
            struct func_sig *sig = resolve_func_sig(callee->func_decl.func.sig);
            assert(sig == callee->sig);
            symbol->resolved = true;
        }
        assert(callee->func_decl.func.sig->resolved);
        assert(symbol->resolved);
        // Set the result type here to avoid a cascade of type errors, even if the arguments are incorrect.
        result_type = callee->sig->ret_type;
        int expected_arity = callee->sig->arity;
        int actual_arity = expr->call.arity;
        if (actual_arity < expected_arity) {
            type_error("Not enough arguments passed to '"LXL_SV_FMT_SPEC"': expected %s%d, but got %d",
                       LXL_SV_FMT_ARG(callee_name),
                       ((callee->sig->c_variadic) ? "at least " : ""),
                       expected_arity, actual_arity);
            break;
        }
        if (actual_arity > expected_arity && !callee->sig->c_variadic) {
            type_error("Too many arguments passed to '"LXL_SV_FMT_SPEC"': expected %d, but got %d",
                       LXL_SV_FMT_ARG(callee_name), expected_arity, actual_arity);
            break;
        }
        assert(actual_arity == expected_arity || (actual_arity > expected_arity && callee->sig->c_variadic));
        assert(actual_arity == expr->call.args.count);
        assert(expected_arity == callee->sig->params.count);
        struct ast_node *arg = expr->call.args.head;
        for (int i = 0; i < expected_arity; ++i, arg = arg->next) {
            // Parameter: expected, argument: actual.
            assert(arg != NULL);
            struct type_decl *param = &callee->sig->params.items[i];
            assert(arg->kind == AST_EXPR);
            TypeID arg_type = type_check_expr(&arg->expr);
            if (arg_type == TYPE_NO_TYPE) continue;  // Skip unknown type.
            if (arg_type != param->type) {
                struct lxl_string_view param_type_name = get_type_sv(param->type);
                struct lxl_string_view arg_type_name = get_type_sv(arg_type);
                type_error("Expected type '"LXL_SV_FMT_SPEC"' for parameter '"LXL_SV_FMT_SPEC"' "
                           "of function '"LXL_SV_FMT_SPEC"', but got type '"LXL_SV_FMT_SPEC"'",
                           LXL_SV_FMT_ARG(param_type_name), LXL_SV_FMT_ARG(param->name),
                           LXL_SV_FMT_ARG(callee_name), LXL_SV_FMT_ARG(arg_type_name));
            }
        }
        for (; arg != NULL; arg = arg->next) {
            // Type check variadic arguments.
            // NOTE: variadic arguments can have any type; we don't verify varidic argument types.
            type_check_expr(&arg->expr);
        }
        assert(arg == NULL || callee->sig->c_variadic);
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
        TypeID type = resolve_type(&symbol->type_alias.type);
        struct type_info *info = get_type(type);
        if (info->kind == KIND_RECORD) {
            if (info->record_type.fields.count != expr->constructor.init_list.count) {
                type_error("Incorrect number of fields in initialiser list for type '"
                           LXL_SV_FMT_SPEC"'; expected %d but got %d",
                           LXL_SV_FMT_ARG(name), info->record_type.fields.count,
                           expr->constructor.init_list.count);
                break;
            }
            struct ast_node *node = expr->constructor.init_list.head;
            for (int i = 0; i < info->record_type.fields.count; ++i, node = node->next) {
                assert(node != NULL);
                TypeID expected_type = info->record_type.fields.items[i].type;
                assert(node->kind == AST_EXPR);
                TypeID actual_type = type_check_expr(&node->expr);
                if (expected_type != actual_type) {
                    struct lxl_string_view expected_sv = get_type_sv(expected_type);
                    struct lxl_string_view actual_sv = get_type_sv(actual_type);
                    type_error("Expected type '"LXL_SV_FMT_SPEC "' for field %d of type '"
                               LXL_SV_FMT_SPEC"', but got type '"LXL_SV_FMT_SPEC"'",
                               LXL_SV_FMT_ARG(expected_sv),
                               i, LXL_SV_FMT_ARG(name),
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
        TypeID operand_type = type_check_expr(expr->convert.operand);
        (void)operand_type;
        if (expr->convert.kind == CONVERT_AS) {
            // TODO: ensure 'as' conversion is compatible.
        }
        else {
            // TODO: ensure 'to' conversion is compatible.
        }
        result_type = resolve_type(expr->convert.target_type);
    } break;
    case AST_EXPR_DEREF: {
        TypeID pointer = type_check_expr(expr->deref.pointer);
        struct type_info *info = get_type(pointer);
        assert(info);
        if (info->kind != KIND_POINTER) {
            struct lxl_string_view type_name = get_type_sv(pointer);
            type_error("Cannot dereference type '"LXL_SV_FMT_SPEC"'; expect pointer",
                       LXL_SV_FMT_ARG(type_name));
            break;
        }
        if (info->pointer_type.dest_type == TYPE_ABSURD) {
            type_error("Cannot dereference pointer to absurd type '!'");
        }
        result_type = info->pointer_type.dest_type;
    } break;
    case AST_EXPR_FIELD: {
        TypeID target_type = type_check_expr(expr->field.target);
        struct type_info *target_info = get_type(target_type);
        assert(target_info);
        if (target_info->kind == KIND_RECORD) {
            TypeID field_type = get_record_field_type(target_info->record_type, expr->field.name);
            if (!field_type) {
                name_error("Unknown field '"LXL_SV_FMT_SPEC"' in type '"LXL_SV_FMT_SPEC"'",
                           LXL_SV_FMT_ARG(expr->field.name), LXL_SV_FMT_ARG(target_info->repr));
                break;
            }
            result_type = field_type;
        }
        else if (target_type == TYPE_TYPE_EXPR) {
            // Do we need the type TYPE_TYPE_EXPR?
            assert(expr->field.target->kind == AST_EXPR_TYPE_EXPR);
            TypeID inner_type = resolve_type(expr->field.target->type_expr.type);
            struct type_info *inner_info = get_type(inner_type);
            if (inner_info->kind == KIND_ENUM) {
                VIC_INT value = 0;
                if (!get_enum_field_value(inner_info->enum_type, expr->field.name, &value)) {
                    name_error("Unknown variant '"LXL_SV_FMT_SPEC"' for type '"LXL_SV_FMT_SPEC"'.",
                               LXL_SV_FMT_ARG(expr->field.name), LXL_SV_FMT_ARG(inner_info->repr));
                    break;
                }
                *expr = (struct ast_expr) {
                    .kind = AST_EXPR_INTEGER,
                    .type = inner_info->enum_type.underlying_type,
                    .integer = {.value = value}};
            }
            else {
                type_error("Expect enum, not '"LXL_SV_FMT_SPEC"'.", LXL_SV_FMT_ARG(inner_info->repr));
                break;
            }
            result_type = inner_type;
        }
        else {
            type_error("Cannot get field of type '"LXL_SV_FMT_SPEC"'.",
                       LXL_SV_FMT_ARG(target_info->repr));
            break;
        }
    } break;
    case AST_EXPR_IDENTIFIER: {
        struct lxl_string_view name = expr->identifier.name;
        struct symbol *symbol = lookup_symbol(&symbols, st_key_of(name));
        if (!symbol) {
            name_error("Unknown symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        if (symbol->kind == SYMBOL_VAR) {
            result_type = symbol->var.type;
        }
        else if (symbol->kind == SYMBOL_VAL) {
            result_type = symbol->val.type;
        }
        else if (symbol->kind == SYMBOL_TYPE_ALIAS) {
            struct ast_type *aliased_type = &symbol->type_alias.type;
            *expr = (struct ast_expr) {
                .kind = AST_EXPR_TYPE_EXPR,
                .type = resolve_type(aliased_type),
                .type_expr = {
                    .type = aliased_type}};
            result_type = TYPE_TYPE_EXPR;
        }
        else {
            name_error("Symbol '"LXL_SV_FMT_SPEC"' is not a variable or type", LXL_SV_FMT_ARG(name));
        }
    } break;
    case AST_EXPR_INDEX: {
        TypeID array_type = type_check_expr(expr->index.array);
        struct type_info *array_info = get_type(array_type);
        assert(array_info);
        if (array_info->kind != KIND_ARRAY) {
            struct lxl_string_view array_type_name = get_type_sv(array_type);
            type_error("Only array types can be indexed, not '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(array_type_name));
            break;
        }
        TypeID index_type = type_check_expr(expr->index.index);
        if (!is_integer_type(index_type)) {
            struct lxl_string_view index_type_name = get_type_sv(index_type);
            type_error("Expect integer type for array index, not '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(index_type_name));
            break;
        }
        result_type = array_info->array_type.dest_type;
    } break;
    case AST_EXPR_INTEGER:
        // TODO "untyped" literals... i.e. integer literals have an "INTEGER_LITERAL" type.
        result_type = TYPE_INT;
        break;
    case AST_EXPR_NULL:
        result_type = TYPE_NULLPTR_TYPE;
        break;
    case AST_EXPR_STRING: {
        // TODO: proper (not C-style) strings.
        result_type = TYPE_C_STRING;
        break;
    }
    case AST_EXPR_TYPE_EXPR:
        TODO("type check type expressions");
        break;
    case AST_EXPR_WHEN: {
        // Propagate errors.
        if (!type_check_expr(expr->when.cond)) return TYPE_NO_TYPE;
        TypeID then_type = type_check_expr(expr->when.then_expr);
        TypeID else_type = type_check_expr(expr->when.else_expr);
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

static void type_check_function(struct ast_decl_func *func) {
    struct func_sig *sig = resolve_func_sig(func->sig);
    struct st_key key = st_key_of(sig->name);
    struct symbol *symbol = lookup_symbol(&symbols, key);
    // We insert the function symbol during parsing.
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_FUNC);
    if (!symbol->resolved) {
        for (struct ast_decl *decl = &symbol->func.func_decl; decl != NULL; decl = decl->func.prev_decl) {
            if (!compare_sigs(resolve_func_sig(decl->func.sig), sig)) {
                type_error("Redeclaration of function '"LXL_SV_FMT_SPEC"' with a different signature",
                           LXL_SV_FMT_ARG(sig->name));
            }
        }
        if (symbol->func.func_decl.func.link_kind != func->link_kind) {
            // Cannot redeclare with different linkage.
            const char *prev_linkage = "external";
            const char *new_linkage = "internal";
            if (symbol->func.func_decl.func.link_kind == FUNC_EXTERNAL) {
                assert(func->link_kind == FUNC_INTERNAL);
            }
            else {
                assert(func->link_kind == FUNC_EXTERNAL);
                prev_linkage = "internal";
                new_linkage = "external";
            }
            type_error("Cannot redeclare function '"LXL_SV_FMT_SPEC"' as '%s'"
                       " following previous declaration as '%s'",
                       LXL_SV_FMT_ARG(sig->name), prev_linkage, new_linkage);
        }
        symbol->resolved = true;
    }
    if (func->decl_kind == AST_FUNC_DECL) return;
    // TODO: set up arguments as local variables.
    // TODO: local variables.
    FOR_DLLIST (struct ast_node *, node, &func->body) {
        assert(node->kind == AST_STMT);
        struct ast_stmt *stmt = &node->stmt;
        type_check_stmt(stmt, sig->ret_type);
    }
}

static void type_check_decl(struct ast_decl *decl) {
    switch (decl->kind) {
    case AST_DECL_VAR: {
        assert(decl->var.value != NULL || decl->var.type != NULL);
        TypeID value_type = (decl->var.value) ? type_check_expr(decl->var.value) : TYPE_NO_TYPE;
        TypeID var_type   = (decl->var.type)  ? resolve_type(decl->var.type)     : value_type;
        assert(var_type);
        if (var_type == TYPE_ABSURD) {
            type_error("Cannot create variable of absurd type '!'");
        }
        if (value_type && !check_assignable(var_type, value_type)) {
            type_error("Mismatched types in variable definition");
        }
        struct symbol symbol = (decl->var.kind == AST_VAR_VAR)
            ? (struct symbol) {
                .kind = SYMBOL_VAR,
                .var = {.type = var_type}}
            : (struct symbol) {
                .kind = SYMBOL_VAL,
                .val = {.type = var_type}};
        if (!insert_symbol(&symbols, st_key_of(decl->var.name), symbol)) {
            name_error("Redeclaration of symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(decl->var.name));
        }
    } return;
    case AST_DECL_FUNC:
        type_check_function(&decl->func);
        return;
    case AST_DECL_EXTERNAL_BLOCK:
        FOR_DLLIST (struct ast_node *, node, &decl->external_block.decls) {
            assert(node->kind == AST_DECL);
            type_check_decl(&node->decl);
        }
        return;
    case AST_DECL_TYPE_DEFN:
        /* No type checking. */
        return;
    }
    UNREACHABLE();
}

static void type_check_stmt_list(struct ast_list stmts, TypeID ret_type) {
    FOR_DLLIST (struct ast_node *, node, &stmts) {
        switch (node->kind) {
        case AST_EXPR: case AST_TYPE: UNREACHABLE(); break;
        case AST_STMT: type_check_stmt(&node->stmt, ret_type); break;
        case AST_DECL: type_check_decl(&node->decl); break;
        }
    }
}

static void type_check_stmt(struct ast_stmt *stmt, TypeID ret_type) {
    switch (stmt->kind) {
    case AST_STMT_DECL:
        type_check_decl(stmt->decl.decl);
        return;
    case AST_STMT_EXPR:
        type_check_expr(stmt->expr.expr);
        return;
    case AST_STMT_IF:
        type_check_expr(stmt->if_.cond);
        type_check_stmt_list(stmt->if_.then_clause, ret_type);
        type_check_stmt_list(stmt->if_.else_clause, ret_type);
        return;
    case AST_STMT_RETURN:
        if (stmt->return_.expr) {
            TypeID return_expr_type = type_check_expr(stmt->return_.expr);
            if (return_expr_type != ret_type) {
                struct lxl_string_view expected_sv = get_type_sv(ret_type);
                struct lxl_string_view actual_sv = get_type_sv(return_expr_type);
                type_error("Expected return type '"LXL_SV_FMT_SPEC"' but got '"LXL_SV_FMT_SPEC"'",
                           LXL_SV_FMT_ARG(expected_sv), LXL_SV_FMT_ARG(actual_sv));
            }
        }
        else if (ret_type == TYPE_ABSURD) {
            type_error("Cannot return from non-returning function");
        }
        else if (ret_type != TYPE_UNIT) {
            struct lxl_string_view expected_sv = get_type_sv(ret_type);
            type_error("Expression-less return from function with non-unit return type '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(expected_sv));
        }
        return;
    }
    UNREACHABLE();
}

bool type_check(struct ast_list *nodes) {
    FOR_DLLIST (struct ast_node *, node, nodes) {
        switch (node->kind) {
        case AST_EXPR:
        case AST_TYPE:
            // Cannot have an expression here.
            UNREACHABLE();
            break;
        case AST_STMT:
            type_error("Only declarations and definitons are allowed at module scope");
            break;
        case AST_DECL:
            type_check_decl(&node->decl);
            break;
        }
    }
    return !type_checker.had_error;
}
