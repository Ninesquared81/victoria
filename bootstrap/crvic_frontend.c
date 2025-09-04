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
#include "crvic_package.h"
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
    struct module *module;
    bool panic_mode;
    bool had_error;
    VIC_INT enum_counter;
    enum func_link_kind current_func_kind;
};

struct type_checker {
    bool had_error;
};

static struct parser parser = {0};

static struct package package = {0};

static struct symbol_table *symbols = NULL;

static struct type_checker type_checker = {0};

static const char *filename = NULL;

#define GLOBALS_CAPACITY 256
#define LOCALS_CAPACITY 128

struct package *get_package(void) {
    return &package;
}

struct symbol_table *new_globals(void) {
    return new_symbol_table(ALLOCATOR_ARD2AD(perm), GLOBALS_CAPACITY);
}

struct symbol_table *new_locals(struct symbol_table *parent) {
    struct symbol_table *locals = new_symbol_table(ALLOCATOR_ARD2AD(perm), LOCALS_CAPACITY);
    locals->parent = parent;
    return locals;
}

static bool check_or_add_package(struct lxl_string_view name) {
    if (package.name.start == NULL) {
        package.name = name;
        return true;
    }
    return lxl_sv_equal(name, package.name);
}

static struct symbol_table *enter_function(struct symbol_table *locals) {
    struct symbol_table *old_symbols = symbols;
    symbols = locals;
    return old_symbols;
}

static void leave_function(struct symbol_table *old_symbols) {
    symbols = old_symbols;
}

void init_frontend(struct lxl_string_view source, const char *in_filepath) {
    filename = in_filepath;
    parser.lexer = init_lexer(source);
    parser.current_func_kind = FUNC_INTERNAL;
    parser.module = add_new_module(&package, lxl_sv_from_string(filename));
    symbols = parser.module->globals;
    insert_symbol(symbols, st_key_of(LXL_SV_FROM_STRLIT("int")),
                  (struct symbol) {
                      .kind = SYMBOL_TYPE_ALIAS,
                      .type_alias = {.type = RESOLVED_TYPE(TYPE_INT)}});
    insert_symbol(symbols, st_key_of(LXL_SV_FROM_STRLIT("uint")),
                  (struct symbol) {
                      .kind = SYMBOL_TYPE_ALIAS,
                      .type_alias = {.type = RESOLVED_TYPE(TYPE_UINT)}});
    insert_symbol(symbols, st_key_of(LXL_SV_FROM_STRLIT("count_of")),
                  (struct symbol) {.kind = SYMBOL_MAGIC_FUNC});
    insert_symbol(symbols, st_key_of(LXL_SV_FROM_STRLIT("size_of")),
                  (struct symbol) {.kind = SYMBOL_MAGIC_FUNC});
    insert_symbol(symbols, st_key_of(LXL_SV_FROM_STRLIT("type_of")),
                  (struct symbol) {.kind = SYMBOL_MAGIC_FUNC});
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

static void error_at_token_vargs(struct lxl_token token, const char *restrict error_kind,
                                 const char *restrict fmt, va_list vargs) {
    report_location(token);
    struct lxl_string_view token_value = lxl_token_value(token);
    fprintf(stderr, "%s error in '"LXL_SV_FMT_SPEC"': ",
            error_kind, LXL_SV_FMT_ARG(token_value));
    vfprintf(stderr, fmt, vargs);
    fprintf(stderr, ".\n");
    fprintf(stderr, "On this line:\n");
    print_line_with_token(token);
}

static void parse_error_vargs(struct lxl_token token, const char *restrict fmt, va_list vargs) {
    if (parser.panic_mode) return;  // Avoid cascading errors.
    error_at_token_vargs(token, "Parse", fmt, vargs);
    parser.panic_mode = true;
    parser.had_error = true;
    exit(1);  // TODO: remove this
}

static void parse_error(struct lxl_token token, const char *restrict fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_vargs(token, fmt, vargs);
    va_end(vargs);
}

static void parse_error_previous(const char *restrict fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_vargs(parser.previous_token, fmt, vargs);
    va_end(vargs);
}

static void parse_error_current(const char *restrict fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_vargs(parser.current_token, fmt, vargs);
    va_end(vargs);
}

void type_error_vargs(struct lxl_token token, const char *fmt, va_list vargs) {
    error_at_token_vargs(token, "Type", fmt, vargs);
    type_checker.had_error = true;
}

void type_error(struct lxl_token token, const char *fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    type_error_vargs(token, fmt, vargs);
    va_end(vargs);
}

void name_error_vargs(struct lxl_token token, const char *msg, va_list vargs) {
    error_at_token_vargs(token, "Name", msg, vargs);
    parser.panic_mode = true;
    parser.had_error = true;
    type_checker.had_error = true;
}

void name_error(struct lxl_token token, const char *msg, ...) {
    va_list vargs;
    va_start(vargs, msg);
    name_error_vargs(token, msg, vargs);
    va_end(vargs);
}


static bool parser_is_finished(void) {
    return LXL_TOKEN_IS_END(parser.current_token);
}

static void ensure_not_at_end(const char *fmt, ...) {
    if (!parser_is_finished()) return;
    va_list vargs;
    va_start(vargs, fmt);
    parse_error_vargs(parser.current_token, fmt, vargs);
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
    parse_error_vargs(parser.current_token, fmt, vargs);
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

static struct ast_stmt *copy_stmt(struct ast_stmt stmt) {
    struct ast_stmt *new = new_stmt();
    *new = stmt;
    return new;
}

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

static bool match_comparison(enum ast_cmp_op_kind *OUT_op, bool strict) {
    enum ast_cmp_op_kind dummy;
    if (!OUT_op) OUT_op = &dummy;
    if (match(TOKEN_EQUALS_EQUALS, strict)) {
        *OUT_op = AST_CMP_EQ;
        return true;
    }
    if (match(TOKEN_BANG_EQUALS, strict)) {
        *OUT_op = AST_CMP_NEQ;
        return true;
    }
    if (match(TOKEN_GREATER, strict)) {
        *OUT_op = AST_CMP_GT;
        return true;
    }
    if (match(TOKEN_GREATER_EQUALS, strict)) {
        *OUT_op = AST_CMP_GE;
        return true;
    }
    if (match(TOKEN_LESS, strict)) {
        *OUT_op = AST_CMP_LT;
        return true;
    }
    if (match(TOKEN_LESS_EQUALS, strict)){
        *OUT_op = AST_CMP_LE;
        return true;
    }
    return false;
}

static bool check_assignment_target(struct ast_expr expr) {
    // For now, only variable names can be assignment targets.
    return expr.kind == AST_EXPR_DEREF
        || expr.kind == AST_EXPR_FIELD
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
    if (*start == 'c') {
        ++start;
    }
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
                parse_error_previous("Unknown escape sequence '\\%c'", *p);
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

static TypeID token_to_type(struct lxl_token token) {
    switch (token.token_type) {
        /* Symbolic types */
    case TOKEN_BANG: return TYPE_ABSURD;
        /* Keyword type names */
    case TOKEN_KW_BOOL: return TYPE_BOOL;
    case TOKEN_KW_C_STRING: return TYPE_C_STRING;
    case TOKEN_KW_I8: return TYPE_I8;
    case TOKEN_KW_I16: return TYPE_I16;
    case TOKEN_KW_I32: return TYPE_I32;
    case TOKEN_KW_I64: return TYPE_I64;
    case TOKEN_KW_STRING: return TYPE_STRING;
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
static struct ast_type parse_type(const char *fmt, ...);

bool try_parse_type(struct ast_type *OUT_type) {
    struct lxl_token anchor = parser.current_token;
    // Basic types.
    TypeID primitive_type = token_to_type(parser.current_token);
    if (primitive_type) {
        advance();
        *OUT_type =  (struct ast_type) {
            .anchor = anchor,
            .kind = AST_TYPE_PRIMITIVE,
            .resolved_type = primitive_type};
        return true;
    }
    // Type aliases.
    if (match(TOKEN_IDENTIFIER, false)) {
        *OUT_type =  (struct ast_type) {
            .anchor = anchor,
            .kind = AST_TYPE_ALIAS,
            .alias = {
                .name = lxl_token_value(parser.previous_token)}};
        return true;
    }
    // Type alias in different module.
    if (match(TOKEN_DOT, false)) {
        struct lxl_token module_name_token = consume(TOKEN_IDENTIFIER, false, "Expect module name after '.'");
        consume(TOKEN_DOT, false, "Expect '.' after module name");
        struct lxl_token alias_name_token = consume(TOKEN_IDENTIFIER, false, "Expect type alias after '.'");
        *OUT_type = (struct ast_type) {
            .anchor = anchor,
            .kind = AST_TYPE_MODULE_ALIAS,
            .module_alias = {
                .module_name = lxl_token_value(module_name_token),
                .alias_name = lxl_token_value(alias_name_token)}};
        return true;
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
        *OUT_type =  (struct ast_type) {
            .anchor = anchor,
            .kind = AST_TYPE_RECORD,
            .record_lit = {
                .fields = fields}};
        return true;
    }
    // Enum types.
    if (match(TOKEN_KW_ENUM, false)) {
        // begin_temp();
        struct ast_type underlying_type = {
            .anchor = anchor,
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
        *OUT_type =  (struct ast_type) {
            .anchor = anchor,
            .kind = AST_TYPE_ENUM,
            .enum_lit = {
                .underlying_type = copy_type(underlying_type),
                .fields = fields}};
        return true;
    }
    // Union types.
    if (match(TOKEN_KW_UNION, false)) {
        consume(TOKEN_BKT_CURLY_LEFT, false, "Expect '{' after 'union'");
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
        consume(TOKEN_BKT_CURLY_RIGHT, false, "Expect '}' after union fields");
        *OUT_type = (struct ast_type) {
            .anchor = anchor,
            .kind = AST_TYPE_UNION,
            .union_lit = {
                .fields = fields}};
        return true;
    }
    // Pointer types.
    if (match(TOKEN_CARET, false)) {
        enum rw_access rw = parse_rw_access();
        struct ast_type dest_type = parse_type("Expect type after '^'");
        *OUT_type =  (struct ast_type) {
            .anchor = anchor,
            .kind = AST_TYPE_POINTER,
            .pointer = {
                .kind = POINTER_PROPER,
                .rw = rw,
                .dest_type = copy_type(dest_type)}};
        return true;
    }
    // Array, array-like pointer, slice.
    if (match(TOKEN_BKT_SQUARE_LEFT, false)) {
        // Array-like pointer.
        if (match(TOKEN_CARET, false)) {
            consume(TOKEN_BKT_SQUARE_RIGHT, false, "Expect ']' after '[^' for array-like pointer type");
            enum rw_access rw = parse_rw_access();
            struct ast_type dest_type = parse_type("Expect type after '[^]'");
            *OUT_type =  (struct ast_type) {
                .anchor = anchor,
                .kind = AST_TYPE_POINTER,
                .pointer = {
                    .kind = POINTER_ARRAY_LIKE,
                    .rw = rw,
                    .dest_type = copy_type(dest_type)}};
            return true;
        }
        // Slice.
        if (match(TOKEN_BKT_SQUARE_RIGHT, false)) {
            enum rw_access rw = parse_rw_access();
            struct ast_type dest_type = parse_type("Expect type after '[]'");
            *OUT_type = (struct ast_type) {
                .anchor = anchor,
                .kind = AST_TYPE_SLICE,
                .slice = {
                    .rw = rw,
                    .dest_type = copy_type(dest_type)}};
            return true;
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
        *OUT_type =  (struct ast_type) {
            .anchor = anchor,
            .kind = AST_TYPE_ARRAY,
            .array = {
                .count = copy_expr(count),
                .rw = rw,
                .dest_type = copy_type(dest_type)}};
        return true;
    }
    // Function types.
    if (match(TOKEN_KW_FUNC, false)) {
        consume(TOKEN_BKT_ROUND_LEFT, false,
                "Expect '(' after 'func' in fucntion type (note the name is omitted)");
        struct ast_sig *sig = ALLOCATE(perm, sizeof *sig);
        *sig  = parse_func_sig(LXL_SV_EMPTY());
        *OUT_type =  (struct ast_type) {
            .anchor = anchor,
            .kind = AST_TYPE_FUNCTION,
            .function = {.sig = sig}};
        return true;
    }
    // Not a type.
    return false;
}

static struct ast_type parse_type(const char *fmt, ...) {
    static_assert(TYPE_NO_TYPE == 0, "TYPE_NO_TYPE should be 'falsy'");
    struct ast_type type = {
        .anchor = parser.current_token,
        .kind = AST_TYPE_PRIMITIVE,
        .resolved_type = TYPE_NO_TYPE};
    if (!try_parse_type(&type)) {
        // Not a type.
        va_list vargs;
        va_start(vargs, fmt);
        parse_error_vargs(parser.current_token, fmt, vargs);
        va_end(vargs);
    }
    return type;
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
    struct ast_type type = {0};
    if (match(TOKEN_LIT_INTEGER, true)) {
        expr = (struct ast_expr) {
            .anchor = parser.previous_token,
            .kind = AST_EXPR_INTEGER,
            .integer = {.value = parse_previous_integer()}};
    }
    else if (match(TOKEN_LIT_STRING, true)) {
        struct lxl_token token = parser.previous_token;
        expr = (struct ast_expr) {
            .anchor = token,
            .kind = AST_EXPR_STRING,
            .string = {.value = parse_string(token)}};
        if (token.start[0] == 'c') {
            // C-style strings.
            expr = (struct ast_expr) {
                .anchor = token,
                .kind = AST_EXPR_CONVERT,
                .convert = {
                    .operand = copy_expr(expr),
                    .target_type = copy_type(RESOLVED_TYPE(TYPE_C_STRING)),
                    .kind = CONVERT_TO}};
        }
    }
    else if (match(TOKEN_IDENTIFIER, true)) {
        struct lxl_string_view name = lxl_token_value(parser.previous_token);
        expr = (struct ast_expr) {
            .anchor = parser.previous_token,
            .kind = AST_EXPR_IDENTIFIER,
            .identifier = {.name = name}};
    }
    else if (match(TOKEN_BKT_ROUND_LEFT, true)) {
        ignore_line_ending();
        expr = parse_assign();
        consume(TOKEN_BKT_ROUND_RIGHT, false, "Expect ')' after grouped expression");
    }
    else if (match(TOKEN_DOT, true)) {
        struct lxl_token module_name = consume(TOKEN_IDENTIFIER, false, "Expect module name after '.'");
        consume(TOKEN_DOT, false, "Expect '.' after module name");
        struct ast_expr identifier = parse_primary();
        if (identifier.kind != AST_EXPR_IDENTIFIER) {
            parse_error(identifier.anchor, "Expect module member after '.'");
        }
        expr = (struct ast_expr) {
            .anchor = module_name,
            .kind = AST_EXPR_MODULE_IDENTIFIER,
            .module_identifier = {
                .module_name = lxl_token_value(module_name),
                .identifier = copy_expr(identifier)}};
    }
    else if (match(TOKEN_KW_TRUE, true) || match(TOKEN_KW_FALSE, true)) {
        bool value = parser.previous_token.token_type == TOKEN_KW_TRUE;
        assert(value || parser.previous_token.token_type == TOKEN_KW_FALSE);
        expr = (struct ast_expr) {
            .anchor = parser.previous_token,
            .kind = AST_EXPR_BOOLEAN,
            .boolean = {.value = value}};
    }
    else if (match(TOKEN_KW_NULL, true)) {
        expr = (struct ast_expr) {
            .anchor = parser.previous_token,
            .kind = AST_EXPR_NULL};
    }
    else if (match(TOKEN_KW_WHEN, true)) {
        ignore_line_ending();
        struct lxl_token when_token = parser.previous_token;
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
            .anchor = when_token,
            .kind = AST_EXPR_WHEN,
            .when = {
                .cond = cond,
                .then_expr = then_expr,
                .else_expr = else_expr}};
    }
    else if (try_parse_type(&type)) {
        expr = (struct ast_expr) {
            .anchor = type.anchor,
            .kind = AST_EXPR_TYPE_EXPR,
            .type_expr = {.type = copy_type(type)}};
    }
    else {
        parse_error_current("Expect expression");
    }
    return expr;
}

static struct ast_expr parse_suffix(void) {
    struct ast_expr expr = parse_primary();
    for (;;) {
        // Parse multiple suffixes.
        if (match(TOKEN_CARET, true)) {
            expr = (struct ast_expr) {
                .anchor = parser.previous_token,
                .kind = AST_EXPR_DEREF,
                .deref = {.pointer = copy_expr(expr)}};
        }
        else if (match(TOKEN_DOT, true)) {
            struct lxl_token name = consume(TOKEN_IDENTIFIER, false, "Expect field name after '.'");
            expr = (struct ast_expr) {
                .anchor = name,
                .kind = AST_EXPR_FIELD,
                .field = {
                    .target = copy_expr(expr),
                    .name = lxl_token_value(name)}};
        }
        else if (match(TOKEN_BKT_ROUND_LEFT, true)) {
            struct lxl_token anchor = parser.previous_token;
            struct ast_expr *callee = copy_expr(expr);
            struct ast_list args = parse_arg_list();
            expr = (struct ast_expr) {
                .anchor = anchor,
                .kind = AST_EXPR_CALL,
                .call = {
                    .callee = callee,
                    .arity = args.count,
                    .args = args}};
        }
        else if (match(TOKEN_BKT_SQUARE_LEFT, true)) {
            struct lxl_token anchor = parser.previous_token;
            ignore_line_ending();
            struct ast_expr index = parse_expr();
            consume(TOKEN_BKT_SQUARE_RIGHT, false, "Expect ']' after array index");
            expr = (struct ast_expr) {
                .anchor = anchor,
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
                .anchor = conversion,
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
    struct lxl_token anchor = parser.current_token;  // This is before any calls to match()!!!
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
            .anchor = anchor,
            .kind = AST_EXPR_ADDRESS_OF,
            .address_of = {
                .target = copy_expr(target),
                .rw = rw}};
    }
    else if (match(TOKEN_BANG, true)) {
        struct ast_expr operand = parse_suffix();
        expr = (struct ast_expr) {
            .anchor = anchor,
            .kind = AST_EXPR_NOT,
            .not = {.operand = copy_expr(operand)}};
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
        struct lxl_token anchor = parser.previous_token;
        struct ast_expr *lhs = new_expr();
        struct ast_expr *rhs = new_expr();
        *lhs = expr;
        *rhs = parse_factor();
        expr = (struct ast_expr) {
            .anchor = anchor,
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
    for (;;) {
        enum ast_bin_op_kind op;
        if (match(TOKEN_PLUS, true)) {
            op = AST_BIN_ADD;
        }
        else if (match(TOKEN_MINUS, true)) {
            op = AST_BIN_SUB;
        }
        else {
            break;
        }
        struct lxl_token anchor = parser.previous_token;
        struct ast_expr *lhs = copy_expr(expr);
        struct ast_expr *rhs = copy_expr(parse_term());
        expr = (struct ast_expr) {
            .anchor = anchor,
            .kind = AST_EXPR_BINARY,
            .binary = {
                .lhs = lhs,
                .rhs = rhs,
                .op = op}};
    }
    return expr;
}

static struct ast_expr parse_compare(void) {
    struct ast_expr expr = parse_term();
    enum ast_cmp_op_kind op;
    if (match_comparison(&op, true)) {
        struct lxl_token anchor = parser.previous_token;
        struct ast_expr rhs = parse_term();
        expr = (struct ast_expr) {
            .anchor = anchor,
            .kind = AST_EXPR_COMPARE,
            .compare = {
                .lhs = copy_expr(expr),
                .rhs = copy_expr(rhs),
                .op = op}};
        if (match_comparison(NULL, true)) {
            parse_error_previous("Chained comparisons are not allowed in rVic");
        }
    }
    return expr;
}

static struct ast_expr parse_and(void) {
    struct ast_expr expr = parse_compare();
    while (match(TOKEN_KW_AND, true)) {
        struct lxl_token anchor = parser.previous_token;
        struct ast_expr rhs = parse_compare();
        expr = (struct ast_expr) {
            .anchor = anchor,
            .kind = AST_EXPR_LOGICAL,
            .logical = {
                .lhs = copy_expr(expr),
                .rhs = copy_expr(rhs),
                .op = AST_LOG_AND}};
    }
    return expr;
}

static struct ast_expr parse_or(void) {
    struct ast_expr expr = parse_and();
    while (match(TOKEN_KW_OR, true)) {
        struct lxl_token anchor = parser.previous_token;
        struct ast_expr rhs = parse_and();
        expr = (struct ast_expr) {
            .anchor = anchor,
            .kind = AST_EXPR_LOGICAL,
            .logical = {
                .lhs = copy_expr(expr),
                .rhs = copy_expr(rhs),
                .op = AST_LOG_OR}};
    }
    return expr;
}

static struct ast_expr parse_assign(void) {
    struct ast_expr expr = parse_or();
    if (!match(TOKEN_COLON_EQUALS, true)) return expr;
    struct lxl_token anchor = parser.previous_token;
    if (!check_assignment_target(expr)) {
        parse_error_previous("Expression on left hand side of ':=' is not assignable");
    }
    struct ast_expr *value = new_expr();
    *value = parse_compare();
    expr = (struct ast_expr) {
        .anchor = anchor,
        .kind = AST_EXPR_ASSIGN,
        .assign = {
            .target = copy_expr(expr),
            .value = value}};
    return expr;
}

static struct ast_expr parse_expr(void) {
    return parse_assign();
}

static struct ast_stmt_block parse_block(void) {
    struct ast_stmt_block block = {
        .stmts = {0},
        .symbols = new_locals(symbols)};
    // The name "enter_function" is a misnomer.
    struct symbol_table *old_symbols = enter_function(block.symbols);
    ignore_line_ending();  // Ignore LF after '{'.
    while (!match(TOKEN_BKT_CURLY_RIGHT, false)) {
        ensure_not_at_end("Unclosed block statement");
        struct ast_stmt stmt = parse_stmt();
        struct ast_node *node = copy_node(STMT_NODE(stmt));
        DLLIST_APPEND(&block.stmts, node);
    }
    ignore_line_ending();  // Ignore LF after '}'.
    leave_function(old_symbols);
    return block;
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
                parse_error_previous(
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
            .anchor = parser.previous_token,
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
            parse_error_previous("External functions can only be declared, not defined");
        }
        else {
            assert(parser.current_func_kind == FUNC_INTERNAL);
        }
        struct symbol_table *locals = new_locals(symbols);
        struct symbol_table *old_symbols = enter_function(locals);
        struct ast_stmt_block body = parse_block();
        leave_function(old_symbols);
        decl = (struct ast_decl) {
            .anchor = name_token,
            .kind = AST_DECL_FUNC,
            .func = {
                .sig = sig,
                .link_kind = parser.current_func_kind,
                .decl_kind = AST_FUNC_DEFN,
                .body = body,
                .symbols = locals}};
    }
    else if (had_line_ending || match(TOKEN_SEMICOLON, true)) {
        decl = (struct ast_decl) {
            .anchor = name_token,
            .kind = AST_DECL_FUNC,
            .func = {
                .sig = sig,
                .link_kind = parser.current_func_kind,
                .decl_kind = AST_FUNC_DECL}};
    }
    else {
        parse_error_current("Expect '{' or end of statement after function signature");
    }
    struct st_key key = st_key_of(name);
    struct symbol *symbol = lookup_symbol(symbols, key);
    if (!symbol) {
        // New function.
        symbol = insert_symbol(symbols, key, (struct symbol) {
                .kind = SYMBOL_FUNC,
                .func = {
                    .decl = decl.func,
                    .sig = &decl.func.sig->resolved_sig}});
        assert(symbol);
        assert(symbol->kind == SYMBOL_FUNC);
        add_new_function(parser.module, decl.func);
    }
    else if (symbol->kind == SYMBOL_FUNC) {
        // Function previously declared/defined.
        struct ast_decl_func **prev_decl_ptr = &decl.func.prev_decl;
        *prev_decl_ptr = ALLOCATE(perm, sizeof **prev_decl_ptr);
        **prev_decl_ptr = symbol->func.decl;
        symbol->func.decl = decl.func;
    }
    else {
        name_error(name_token,
                   "Previously defined symbol '"LXL_SV_FMT_SPEC"' redeclared as function", name);
    }
    return decl;
}

static struct ast_decl parse_var_decl(enum ast_var_kind kind) {
    struct lxl_token var_token = parser.previous_token;
    // Sanity check:
    assert((var_token.token_type == TOKEN_KW_VAR && kind == AST_VAR_VAR) ||
           (var_token.token_type == TOKEN_KW_VAL && kind == AST_VAR_VAL) ||
           (var_token.token_type == TOKEN_KW_CONST && kind == AST_VAR_CONST));
    struct lxl_token name_token = consume(TOKEN_IDENTIFIER, false, "Expect variable name after 'var'");
    struct lxl_string_view name = lxl_token_value(name_token);
    struct ast_type *type = NULL;
    struct ast_expr *value = NULL;
    if (match(TOKEN_COLON, false)) {
        type = copy_type(parse_type("Expect type after ':'"));
        if (!match(TOKEN_EQUALS, false)) {
            if (kind == AST_VAR_VAL) {
                parse_error(var_token, "Immutable value '"LXL_SV_FMT_SPEC"' must be initialised",
                            LXL_SV_FMT_ARG(name));
            }
            else if (kind == AST_VAR_CONST) {
                parse_error(var_token, "Constant value '"LXL_SV_FMT_SPEC"' must be initialised",
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
        .anchor = var_token,
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
        .anchor = alias_token,  // Should this be the preceding 'type' token?
        .kind = AST_DECL_TYPE_DEFN,
        .type_defn = {
            .alias = alias,
            .type = type}};
    bool insert_result =
        insert_symbol(symbols, st_key_of(alias),
                      (struct symbol) {
                          .kind = SYMBOL_TYPE_ALIAS,
                          .type_alias = {.type = *type}});
    if (!insert_result) {
        name_error(alias_token, "Cannot redefine symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(alias));
    }
    return decl;
}

bool try_parse_decl(struct ast_decl *OUT_decl) {
    if (match(TOKEN_KW_CONST, true)) {
        *OUT_decl = parse_var_decl(AST_VAR_CONST);
    }
    else if (match(TOKEN_KW_EXTERNAL, true)) {
        enum func_link_kind prev_kind = parser.current_func_kind;
        parser.current_func_kind = FUNC_EXTERNAL;
        if (match(TOKEN_KW_FUNC, false)) {
            *OUT_decl = parse_func_decl();
        }
        else {
            consume(TOKEN_BKT_CURLY_LEFT, false, "Expect 'func' or '{' after 'external'");
            *OUT_decl = (struct ast_decl) {
                .anchor = parser.previous_token,
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
                    parse_error(func_token, "Expect function delcaration in 'external' block");
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
    else if (match(TOKEN_KW_PACKAGE, true)) {
        struct lxl_token name_token = consume(TOKEN_IDENTIFIER, false, "Expect identifier after 'package'");
        *OUT_decl = (struct ast_decl) {
            .anchor = name_token,  // Should this be the preceding 'package' token?
            .kind = AST_DECL_PACKAGE,
            .package = {.name = lxl_token_value(name_token)}};
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
    struct lxl_token if_token = parser.previous_token;
    assert(if_token.token_type == TOKEN_KW_IF);
    ignore_line_ending();
    struct ast_expr *cond = new_expr();
    *cond = parse_expr();
    consume(TOKEN_BKT_CURLY_LEFT, false, "Expect '{' after 'if' condition");
    struct ast_stmt_block then_clause = parse_block();
    struct ast_stmt *else_clause = NULL;
    if (match(TOKEN_KW_ELSE, true)) {
        ignore_line_ending();
        if (match(TOKEN_BKT_CURLY_LEFT, false)) {
            struct lxl_token anchor = parser.previous_token;
            else_clause = copy_stmt(BLOCK_STMT(anchor, parse_block()));
        }
        else {
            consume(TOKEN_KW_IF, false, "Expect '{' or 'if' after 'else'");
            else_clause = copy_stmt(parse_if());
        }
    }
    return (struct ast_stmt) {
        .anchor = if_token,
        .kind = AST_STMT_IF,
        .if_ = {
            .cond = cond,
            .then_clause = then_clause,
            .else_clause = else_clause}};
}

struct ast_stmt parse_return(void) {
    struct lxl_token return_token = parser.previous_token;
    assert(return_token.token_type == TOKEN_KW_RETURN);
    struct ast_expr *expr = NULL;
    if (!check_line_ending() && !check(TOKEN_SEMICOLON, true)) {
        // Return with expression.
        expr = new_expr();
        *expr = parse_expr();
    }
    end_statement();
    return (struct ast_stmt) {
        .anchor = return_token,
        .kind = AST_STMT_RETURN,
        .return_ = {.expr = expr}};
}

struct ast_stmt parse_while(void) {
    struct lxl_token while_token = parser.previous_token;
    assert(while_token.token_type == TOKEN_KW_WHILE);
    ignore_line_ending();
    struct ast_expr *cond = copy_expr(parse_expr());
    consume(TOKEN_BKT_CURLY_LEFT, false, "Expect '{' after condition in 'while' loop");
    struct ast_stmt_block body = parse_block();
    return (struct ast_stmt) {
        .anchor = while_token,
        .kind = AST_STMT_WHILE,
        .while_ = {
            .cond = cond,
            .body = body}};
}

struct ast_stmt parse_stmt(void) {
    if (match(TOKEN_KW_IF, true)) {
        return parse_if();
    }
    if (match(TOKEN_KW_RETURN, true)) {
        return parse_return();
    }
    if (match(TOKEN_KW_WHILE, true)) {
        return parse_while();
    }
    struct ast_decl decl = {0};
    if (try_parse_decl(&decl)) {
        // Declaration statement.
        return (struct ast_stmt) {
            .anchor = decl.anchor,
            .kind = AST_STMT_DECL,
            .decl = {.decl = copy_decl(decl)}};
    }
    // Expression statement.
    struct ast_expr expr = parse_expr();
    end_statement();
    return (struct ast_stmt) {
        .anchor = expr.anchor,
        .kind = AST_STMT_EXPR,
        .expr = {.expr = copy_expr(expr)}};
}

bool parse(void) {
    assert(parser.module->decls.count == 0);
    advance();  // Prime parser with first token;
    for (; ignore_line_ending(), !parser_is_finished();) {
        struct ast_decl decl = {0};
        if (try_parse_decl(&decl)) {
            struct ast_node *node = copy_node(DECL_NODE(decl));
            DLLIST_APPEND(&parser.module->decls, node);
        }
        else {
            parse_error_current("Unexpected token at module top level");
        }
    }
    return !parser.had_error;
}

static TypeID type_check_expr(struct ast_expr *expr);
static TypeID resolve_type(struct ast_type *type);

VIC_INT fold_integer_constant(struct ast_expr *expr, const char *fmt, ...) {
    if (expr->kind == AST_EXPR_INTEGER) return expr->integer.value;
    TODO("Other constant expressions");
    // Not an integer constant.
    va_list vargs;
    va_start(vargs, fmt);
    type_error_vargs(expr->anchor, fmt, vargs);
    va_end(vargs);
    return 0;
}

static TypeID resolve_array(struct ast_type *type) {
    assert(type->kind == AST_TYPE_ARRAY);
    type_check_expr(type->array.count);
    VIC_INT count = fold_integer_constant(type->array.count, "Array size must be an integer constant");
    TypeID dest_type = resolve_type(type->array.dest_type);
    struct type_info array_info = {
        .kind = KIND_ARRAY,
        .array = {
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

static TypeID resolve_record_or_union(struct ast_type *type) {
    assert(type->kind == AST_TYPE_RECORD || type->kind == AST_TYPE_UNION);
    struct ast_type_decl_list *ast_fields = (type->kind == AST_TYPE_RECORD)
        ? &type->record_lit.fields
        : &type->union_lit.fields;
    AUTO_BEGIN_TEMP();
    struct type_decl_list fields = TYPE_DECL_LIST(temp);
    DA_RESERVE(&fields, ast_fields->count);
    for (int i = 0; i < ast_fields->count; ++i) {
        fields.items[fields.count++] = resolve_type_decl(&ast_fields->items[i]);
    }
    struct type_info info = {
        .kind = KIND_RECORD,
        .record = {
            .fields = PROMOTE_DA(&fields)}};
    AUTO_END_TEMP();
    if (type->kind == AST_TYPE_UNION) {
        info.kind = KIND_UNION;
        info.union_.fields = info.record.fields;  // Probably not needed.
    }
    type->resolved_type = get_or_add_type(info);
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
        .enum_ = {
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
    if (!dest_type) return TYPE_NO_TYPE;
    struct type_info pointer_info = {
        .kind = KIND_POINTER,
        .pointer = {
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
        .function = {.sig = sig}};
    type->resolved_type = get_or_add_type(function_info);
    return type->resolved_type;
}

static TypeID resolve_slice(struct ast_type *type) {
    assert(type->kind == AST_TYPE_SLICE);
    TypeID dest_type = resolve_type(type->slice.dest_type);
    struct type_info slice_info = {
        .kind = KIND_SLICE,
        .slice = {
            .rw = type->slice.rw,
            .dest_type = dest_type}};
    type->resolved_type = get_or_add_type(slice_info);
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
        struct symbol *symbol = lookup_symbol(symbols, st_key_of(type->alias.name));
        if (!symbol) {
            name_error(type->anchor,
                       "Unknown symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(type->alias.name));
            return TYPE_NO_TYPE;
        }
        if (symbol->kind != SYMBOL_TYPE_ALIAS) {
            name_error(type->anchor,
                       "Symbol '"LXL_SV_FMT_SPEC"' is not a type alias", LXL_SV_FMT_ARG(type->alias.name));
            return TYPE_NO_TYPE;
        }
        return (type->resolved_type = resolve_type(&symbol->type_alias.type));
    }
    case AST_TYPE_ARRAY:
        return resolve_array(type);
    case AST_TYPE_ENUM:
        return resolve_enum(type);
    case AST_TYPE_FUNCTION:
        return resolve_function(type);
    case AST_TYPE_MODULE_ALIAS: {
        struct module *module = find_module(&package, type->module_alias.module_name);
        type->module_alias.module = module;
        if (!module) {
            name_error(type->anchor,
                       "Unknown module '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(type->module_alias.module_name));
            return TYPE_NO_TYPE;
        }
        struct symbol *symbol = lookup_symbol(module->globals, st_key_of(type->module_alias.alias_name));
        if (!symbol) {
            name_error(type->anchor,
                       "Unknown symbol '"LXL_SV_FMT_SPEC"' in module '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(type->module_alias.alias_name),
                       LXL_SV_FMT_ARG(type->module_alias.module_name));
            return TYPE_NO_TYPE;
        }
        if (symbol->kind != SYMBOL_TYPE_ALIAS) {
            name_error(type->anchor,
                       "Symbol '"LXL_SV_FMT_SPEC"' is not a type alias",
                       LXL_SV_FMT_ARG(type->module_alias.alias_name));
            return TYPE_NO_TYPE;
        }
        return (type->resolved_type = resolve_type(&symbol->type_alias.type));
    }
    case AST_TYPE_POINTER:
        return resolve_pointer(type);
    case AST_TYPE_RECORD:
        return resolve_record_or_union(type);
    case AST_TYPE_SLICE:
        return resolve_slice(type);
    case AST_TYPE_UNION:
        return resolve_record_or_union(type);
    }
    UNREACHABLE();
    return TYPE_NO_TYPE;
}

static bool expect_integer_type(struct ast_expr *expr) {
    if (is_integer_type(expr->type)) return true;
    struct lxl_string_view actual_type_sv = get_type_sv(expr->type);
    type_error(expr->anchor,
               "Expect integer type, not '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(actual_type_sv));
    return false;
}

static bool check_signs(TypeID ltype, TypeID rtype) {
    if (!is_integer_type(ltype) || !is_integer_type(rtype)) return false;
    if (sign_of_type(ltype) == sign_of_type(rtype)) return true;
    return ltype == TYPE_CONST_INT || rtype == TYPE_CONST_INT;
}

static TypeID convert_binary(struct ast_expr *lhs, struct ast_expr *rhs) {
    TypeID lhs_type = lhs->type;
    TypeID rhs_type = rhs->type;
    if (!expect_integer_type(lhs)) return TYPE_NO_TYPE;
    if (!expect_integer_type(rhs)) return TYPE_NO_TYPE;
    if (check_signs(lhs_type, rhs_type)) {
        return max_type_rank(lhs_type, rhs_type);
    }
    type_error(lhs->anchor, "Cannot mix signed and unsigned here");
    return TYPE_NO_TYPE;
}

static bool check_assignable_rw_access(enum rw_access lrw, enum rw_access rrw) {
    // ^mut T := ^mut T  // ok
    // ^out T := ^out T  // ok
    // ^out T := ^mut T  // ok
    // ^T     := ^T      // ok
    // ^T     := ^mut T  // ok
    // anything else disallowed
    return lrw == rrw || rrw == RW_READ_WRITE;
}

static bool check_assignable(TypeID ltype, TypeID rtype) {
    if (ltype == rtype) return true;
    if (is_integer_type(ltype) && rtype == TYPE_CONST_INT) return true;
    if (ltype == TYPE_BOOL && rtype == TYPE_CONST_BOOL) return true;
    struct type_info *linfo = get_type(ltype);
    struct type_info *rinfo = get_type(rtype);
    assert(linfo && rinfo);
    if (linfo->kind == KIND_POINTER) {
        // Pointers can be assigned `null`.
        if (rtype == TYPE_NULLPTR_TYPE) return true;
        // Pointers to `!` can be obtained from any pointer.
        if (rinfo->kind == KIND_POINTER) {
            return (linfo->pointer.dest_type == rinfo->pointer.dest_type
                    || linfo->pointer.dest_type == TYPE_ABSURD)
                && linfo->pointer.kind == rinfo->pointer.kind
                && check_assignable_rw_access(linfo->pointer.rw, rinfo->pointer.rw);
        }
    }
    return false;
}

static TypeID check_comparable(TypeID ltype, TypeID rtype) {
    // We allow the same conversions as in assignment, but in both directions.
    return check_assignable(ltype, rtype) || check_assignable(rtype, ltype);
}

static TypeID type_check_assignment_target(struct ast_expr *target) {
    switch (target->kind) {
    case AST_EXPR_IDENTIFIER: {
        struct lxl_string_view name = target->identifier.name;
        struct symbol *target_symbol = lookup_symbol(symbols, st_key_of(name));
        target->identifier.symbol = target_symbol;
        if (!target_symbol) {
            name_error(target->anchor, "Unknown symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        if (target_symbol->kind == SYMBOL_VAL || target_symbol->kind == SYMBOL_PARAM) {
            type_error(target->anchor, "Cannot reassign immutable value '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(name));
            break;
        }
        else if (target_symbol->kind != SYMBOL_VAR) {
            name_error(target->anchor, "Symbol '"LXL_SV_FMT_SPEC"' is not a variable",
                       LXL_SV_FMT_ARG(name));
            break;
        }
        target->type = target_symbol->var.type;
    } break;
    case AST_EXPR_DEREF: {
        TypeID pointer_type = type_check_expr(target->deref.pointer);
        if (!pointer_type) return TYPE_NO_TYPE;
        struct type_info *pointer_info = get_type(pointer_type);
        if (pointer_info->kind != KIND_POINTER) {
            type_error(target->anchor, "Only pointers can be dereferenced, not '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(pointer_info->repr));
            break;
        }
        switch (pointer_info->pointer.rw) {
        case RW_READ_ONLY:
            type_error(target->anchor, "Attempt to write to read-only pointer");
            break;
        case RW_READ_WRITE:
            // Do nothing.
            break;
        case RW_WRITE_BEFORE_READ:
            // To simplify things, `out` pointers are treated as write-only.
            // Do nothing.
            break;
        }
        target->type = pointer_info->pointer.dest_type;
        if (target->type == TYPE_ABSURD) {
            type_error(target->anchor, "Cannot dereference pointer to absurd type '!'");
        }
    } break;
    case AST_EXPR_FIELD: {
        TypeID object_type = (target->field.target->kind == AST_EXPR_DEREF)
            ? type_check_assignment_target(target->field.target)
            : type_check_expr(target->field.target);
        if (!object_type) break;
        struct type_info *object_info = get_type(object_type);
        if (object_info->kind != KIND_RECORD) {
            type_error(target->anchor, "Invalid type for '.': '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(object_info->repr));
            break;
        }
        TypeID field_type = get_record_field_type(object_info->record, target->field.name);
        if (!field_type) {
            name_error(target->anchor, "Unknown field '"LXL_SV_FMT_SPEC"' for '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(target->field.name), LXL_SV_FMT_ARG(object_info->repr));
            break;
        }
        target->type = field_type;
    } break;
    case AST_EXPR_INDEX: {
        TypeID array_type = type_check_expr(target->index.array);
        struct type_info *array_info = get_type(array_type);
        if (!is_array_like_type(array_type)) {
            type_error(target->anchor, "Only array-like types can be indexed");
            break;
        }
        TypeID dest_type =
            (array_info->kind  == KIND_ARRAY)
            ?   array_info->array.dest_type
            : (array_info->kind == KIND_SLICE)
            ?   array_info->slice.dest_type
            :   array_info->pointer.dest_type
            ;
        enum rw_access rw =
            (array_info->kind  == KIND_ARRAY)
            ?   array_info->array.rw
            : (array_info->kind == KIND_SLICE)
            ?   array_info->slice.rw
            :   array_info->pointer.dest_type
            ;
        switch (rw) {
        case RW_READ_ONLY:
            type_error(target->anchor, "Attempt to write to read-only array element");
            break;
        case RW_READ_WRITE:
            // Do nothing.
            break;
        case RW_WRITE_BEFORE_READ:
            // Should we even allow 'out' elements?
            TODO("Check 'out' array elements");
            break;
        }
        target->type = dest_type;
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
            .pointer = {
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
            type_error(expr->anchor,
                       "Cannot assign value of type '"LXL_SV_FMT_SPEC"'"
                       " to target of type '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(result_type_name),
                       LXL_SV_FMT_ARG(target_type_name));
        }
    } break;
    case AST_EXPR_BINARY: {
        TypeID lhs_type = type_check_expr(expr->binary.lhs);
        TypeID rhs_type = type_check_expr(expr->binary.rhs);
        if (!check_assignable(lhs_type, rhs_type) && !check_assignable(rhs_type, lhs_type)) {
            struct lxl_string_view lname = get_type_sv(lhs_type);
            struct lxl_string_view rname = get_type_sv(rhs_type);
            type_error(expr->anchor,
                       "Incopatible types for binary operator: '"LXL_SV_FMT_SPEC"'"
                       " and '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(lname), LXL_SV_FMT_ARG(rname));
        }
        result_type = convert_binary(expr->binary.lhs, expr->binary.rhs);
    } break;
    case AST_EXPR_BOOLEAN: {
        result_type = TYPE_CONST_BOOL;
    } break;
    case AST_EXPR_CALL: {
        TypeID callee_type = type_check_expr(expr->call.callee);
        if (expr->call.callee->kind == AST_EXPR_TYPE_EXPR) {
            result_type = resolve_type(expr->call.callee->type_expr.type);
            *expr = (struct ast_expr) {
                .anchor = expr->anchor,
                .kind = AST_EXPR_CONSTRUCTOR,
                .constructor = {
                    .type = copy_type(RESOLVED_TYPE(result_type)),
                    .init_list = expr->call.args}};
            struct type_info *info = get_type(result_type);
            struct lxl_string_view name = info->repr;
            if (info->kind == KIND_RECORD) {
                if (info->record.fields.count != expr->constructor.init_list.count) {
                    type_error(expr->anchor,
                               "Incorrect number of fields in initialiser list for type '"
                               LXL_SV_FMT_SPEC"'; expected %d but got %d",
                               LXL_SV_FMT_ARG(name), info->record.fields.count,
                               expr->constructor.init_list.count);
                    break;
                }
                struct ast_node *node = expr->constructor.init_list.head;
                for (int i = 0; i < info->record.fields.count; ++i, node = node->next) {
                    assert(node != NULL);
                    TypeID expected_type = info->record.fields.items[i].type;
                    assert(node->kind == AST_EXPR);
                    TypeID actual_type = type_check_expr(&node->expr);
                    if (!check_assignable(expected_type, actual_type)) {
                        struct lxl_string_view expected_sv = get_type_sv(expected_type);
                        struct lxl_string_view actual_sv = get_type_sv(actual_type);
                        type_error(expr->anchor,
                                   "Expected type '"LXL_SV_FMT_SPEC "' for field %d of type '"
                                   LXL_SV_FMT_SPEC"', but got type '"LXL_SV_FMT_SPEC"'",
                                   LXL_SV_FMT_ARG(expected_sv),
                                   i, LXL_SV_FMT_ARG(name),
                                   LXL_SV_FMT_ARG(actual_sv));
                        break;
                    }
                }
            }
            else {
                type_error(expr->anchor,
                           "Invalid type for constructor: '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
                break;
            }
            break;
        }
        else if (expr->call.callee->kind == AST_EXPR_MAGIC_FUNC) {
            if (!lxl_sv_equal(expr->call.callee->magic_func.name, LXL_SV_FROM_STRLIT("count_of"))) {
                TODO("`size_of()` and `type_of()`");
            }
            if (expr->call.arity != 1) {
                type_error(expr->anchor,
                           "Expect exactly one argument to `count_of()`, not %d", expr->call.arity);
                break;
            }
            struct ast_expr *operand = &expr->call.args.head->expr;
            TypeID operand_type = type_check_expr(operand);
            if (operand->kind == AST_EXPR_TYPE_EXPR) {
                operand_type = resolve_type(operand->type_expr.type);
            }
            struct type_info *info = get_type(operand_type);
            VIC_INT known_count = -1;  // -1 is a sentinel meaning "unknown".
            switch (info->kind) {
            case KIND_NO_KIND:
                UNREACHABLE();
                break;
            case KIND_ENUM:
                known_count = info->enum_.fields.count;
                break;
            case KIND_RECORD:
                known_count = info->record.fields.count;
                break;
            case KIND_ARRAY:
                known_count = info->array.count;
                break;
            case KIND_FUNCTION:
                known_count = info->function.sig->arity;
                break;
            case KIND_SLICE:
                break;  // Count is only known at runtime.
            case KIND_UNION:
                known_count = info->union_.fields.count;
                break;
            case KIND_PRIMITIVE:
                assert(operand_type != TYPE_TYPE_EXPR);
                if (operand_type == TYPE_STRING) break;
                FALLTHROUGH;
            case KIND_POINTER:
                type_error(expr->anchor,
                           "Invalid operand type for 'count_of()': '"LXL_SV_FMT_SPEC"'", info->repr);
                break;
            }
            if (known_count >= 0) {
                *expr = (struct ast_expr) {
                    .anchor = expr->anchor,
                    .kind = AST_EXPR_INTEGER,
                    .integer = {.value = known_count}};
            }
            else {
                *expr = (struct ast_expr) {
                    .anchor = expr->anchor,
                    .kind = AST_EXPR_COUNT_OF,
                    .count_of = {.operand = operand}};
            }
            result_type = TYPE_INT;
            break;
        }
        struct type_info *callee_info = get_type(callee_type);
        assert(callee_info);
        if (callee_info->kind != KIND_FUNCTION) {
            type_error(expr->anchor,
                       "Only functions are callable, not '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(callee_info->repr));
            break;
        }
        struct function_info *callee = &callee_info->function;
        // Set the result type here to avoid a cascade of type errors, even if the arguments are incorrect.
        result_type = callee->sig->ret_type;
        struct lxl_string_view callee_name = (expr->call.callee->kind == AST_EXPR_FUNC_EXPR)
            ? expr->call.callee->func_expr.name
            : LXL_SV_FROM_STRLIT("<Indirect function>");
        int expected_arity = callee->sig->arity;
        int actual_arity = expr->call.arity;
        if (actual_arity < expected_arity) {
            type_error(expr->anchor,
                       "Not enough arguments passed to '"LXL_SV_FMT_SPEC"': expected %s%d, but got %d",
                       LXL_SV_FMT_ARG(callee_name),
                       ((callee->sig->c_variadic) ? "at least " : ""),
                       expected_arity, actual_arity);
            break;
        }
        if (actual_arity > expected_arity && !callee->sig->c_variadic) {
            type_error(expr->anchor,
                       "Too many arguments passed to '"LXL_SV_FMT_SPEC"': expected %d, but got %d",
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
            if (!check_assignable(param->type, arg_type)) {
                struct lxl_string_view param_type_name = get_type_sv(param->type);
                struct lxl_string_view arg_type_name = get_type_sv(arg_type);
                type_error(expr->anchor,
                           "Expected type '"LXL_SV_FMT_SPEC"' for parameter '"LXL_SV_FMT_SPEC"' "
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
    case AST_EXPR_COMPARE: {
        TypeID ltype = type_check_expr(expr->compare.lhs);
        TypeID rtype = type_check_expr(expr->compare.rhs);
        result_type = TYPE_BOOL;
        if (!check_comparable(ltype, rtype)) {
            struct lxl_string_view lname = get_type_sv(ltype);
            struct lxl_string_view rname = get_type_sv(rtype);
            type_error(expr->anchor,
                       "Cannot compare types '"LXL_SV_FMT_SPEC"' and '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(lname), LXL_SV_FMT_ARG(rname));
            break;
        }
        if (expr->compare.op == AST_CMP_EQ || expr->compare.op == AST_CMP_NEQ) break;
        if (!is_ordered_type(ltype)) {
            struct lxl_string_view lname = get_type_sv(ltype);
            type_error(expr->anchor,
                       "Cannot use ordered comparison on value of type '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(lname));
            break;
        }
    } break;
    case AST_EXPR_CONSTRUCTOR:
    case AST_EXPR_COUNT_OF:
        UNREACHABLE();
        break;
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
            type_error(expr->anchor,
                       "Cannot dereference type '"LXL_SV_FMT_SPEC"'; expect pointer",
                       LXL_SV_FMT_ARG(type_name));
            break;
        }
        if (info->pointer.dest_type == TYPE_ABSURD) {
            type_error(expr->anchor, "Cannot dereference pointer to absurd type '!'");
        }
        if (info->pointer.rw == RW_WRITE_BEFORE_READ) {
            type_error(expr->anchor, "Cannot read from 'out' pointer in rVic");
        }
        result_type = info->pointer.dest_type;
    } break;
    case AST_EXPR_FIELD: {
        TypeID target_type = type_check_expr(expr->field.target);
        struct type_info *target_info = get_type(target_type);
        assert(target_info);
        if (target_info->kind == KIND_RECORD) {
            TypeID field_type = get_record_field_type(target_info->record, expr->field.name);
            if (!field_type) {
                name_error(expr->anchor,
                           "Unknown field '"LXL_SV_FMT_SPEC"' in type '"LXL_SV_FMT_SPEC"'",
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
                if (!get_enum_field_value(inner_info->enum_, expr->field.name, &value)) {
                    name_error(expr->anchor,
                               "Unknown variant '"LXL_SV_FMT_SPEC"' for type '"LXL_SV_FMT_SPEC"'.",
                               LXL_SV_FMT_ARG(expr->field.name), LXL_SV_FMT_ARG(inner_info->repr));
                    break;
                }
                *expr = (struct ast_expr) {
                    .anchor = expr->anchor,
                    .kind = AST_EXPR_INTEGER,
                    .type = inner_info->enum_.underlying_type,
                    .integer = {.value = value}};
            }
            else {
                type_error(expr->anchor,
                           "Expect enum, not '"LXL_SV_FMT_SPEC"'.", LXL_SV_FMT_ARG(inner_info->repr));
                break;
            }
            result_type = inner_type;
        }
        else {
            type_error(expr->anchor,
                       "Cannot get field of type '"LXL_SV_FMT_SPEC"'.",
                       LXL_SV_FMT_ARG(target_info->repr));
            break;
        }
    } break;
    case AST_EXPR_FUNC_EXPR:
        // These nodes are produced BY the type checker and thus should never end up being type checked.
        UNREACHABLE();
        break;
    case AST_EXPR_IDENTIFIER: {
        struct lxl_string_view name = expr->identifier.name;
        struct symbol *symbol = lookup_symbol(symbols, st_key_of(name));
        expr->identifier.symbol = symbol;
        if (!symbol) {
            name_error(expr->anchor, "Unknown symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        if (symbol->kind == SYMBOL_PARAM) {
            result_type = symbol->param.type;
        }
        else if (symbol->kind == SYMBOL_VAR) {
            result_type = symbol->var.type;
        }
        else if (symbol->kind == SYMBOL_VAL) {
            result_type = symbol->val.type;
        }
        else if (symbol->kind == SYMBOL_CONST) {
            if (!is_integer_type(symbol->val.type)) {
                // TODO: other types for consts.
                type_error(expr->anchor, "Consts can only be of integer type");
                break;
            }
            *expr = (struct ast_expr) {
                .anchor = expr->anchor,
                .kind = AST_EXPR_INTEGER,
                .integer = {
                    .value = fold_integer_constant(symbol->const_.value, "Expect integer constant")}};
            result_type = symbol->const_.type;
        }
        else if (symbol->kind == SYMBOL_FUNC) {
            if (!symbol->resolved) {
                resolve_func_sig(symbol->func.decl.sig);
                symbol->resolved = true;
            }
            struct type_info function_info = {
                .kind = KIND_FUNCTION,
                .function = {.sig = symbol->func.sig}};
            result_type = get_or_add_type(function_info);
            *expr = (struct ast_expr) {
                .anchor = expr->anchor,
                .kind = AST_EXPR_FUNC_EXPR,
                .func_expr = {
                    .name = name,
                    .symbol = &symbol->func}};
        }
        else if (symbol->kind == SYMBOL_TYPE_ALIAS) {
            struct ast_type *aliased_type = &symbol->type_alias.type;
            *expr = (struct ast_expr) {
                .anchor = expr->anchor,
                .kind = AST_EXPR_TYPE_EXPR,
                .type = resolve_type(aliased_type),
                .type_expr = {
                    .type = aliased_type}};
            result_type = TYPE_TYPE_EXPR;
        }
        else if (symbol->kind == SYMBOL_MAGIC_FUNC) {
            *expr = (struct ast_expr) {
                .anchor = expr->anchor,
                .kind = AST_EXPR_MAGIC_FUNC,
                .magic_func = {.name = name}};
        }
        else {
            name_error(expr->anchor,
                       "Symbol '"LXL_SV_FMT_SPEC"' is not a variable or type", LXL_SV_FMT_ARG(name));
        }
    } break;
    case AST_EXPR_INDEX: {
        TypeID array_type = type_check_expr(expr->index.array);
        struct type_info *array_info = get_type(array_type);
        TypeID dest_type =
            (array_info->kind  == KIND_ARRAY)
            ?   array_info->array.dest_type
            : (array_info->kind == KIND_SLICE)
            ?   array_info->slice.dest_type
            :   array_info->pointer.dest_type
            ;
        if (!is_array_like_type(array_type)) {
            struct lxl_string_view array_type_name = get_type_sv(array_type);
            type_error(expr->anchor,
                       "Only array-like types can be indexed, not '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(array_type_name));
            break;
        }
        TypeID index_type = type_check_expr(expr->index.index);
        if (!is_integer_type(index_type)) {
            struct lxl_string_view index_type_name = get_type_sv(index_type);
            type_error(expr->anchor,
                       "Expect integer type for array index, not '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(index_type_name));
            break;
        }
        result_type = dest_type;
    } break;
    case AST_EXPR_INTEGER:
        result_type = TYPE_CONST_INT;
        break;
    case AST_EXPR_LOGICAL: {
        TypeID ltype = type_check_expr(expr->logical.lhs);
        TypeID rtype = type_check_expr(expr->logical.rhs);
        if (ltype != rtype) {
            struct lxl_string_view lname = get_type_sv(ltype);
            struct lxl_string_view rname = get_type_sv(rtype);
            type_error(expr->anchor,
                       "Operands of logical operators must be of the same type, not '"
                       LXL_SV_FMT_SPEC"' and '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(lname), LXL_SV_FMT_ARG(rname));
            break;
        }
        // TODO: other types (probably not in rVic).
        if (ltype != TYPE_BOOL) {
            struct lxl_string_view lname = get_type_sv(ltype);
            type_error(expr->anchor,
                       "Only booleans are allowed in logical operators in rVic, not '"
                       LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(lname));
            break;
        }
        result_type = ltype;
    } break;
    case AST_EXPR_MAGIC_FUNC:
        UNREACHABLE();
        break;
    case AST_EXPR_MODULE_IDENTIFIER: {
        struct lxl_string_view name = expr->module_identifier.module_name;
        struct module *module = find_module(&package, name);
        if (!module) {
            name_error(expr->anchor, "Unknown module '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(name));
            break;
        }
        expr->module_identifier.module = module;
        struct symbol_table *old_symbols = enter_function(module->globals);
        result_type = type_check_expr(expr->module_identifier.identifier);
        leave_function(old_symbols);
    } break;
    case AST_EXPR_NOT:
        type_check_expr(expr->not.operand);
        result_type = TYPE_BOOL;
        break;
    case AST_EXPR_NULL:
        result_type = TYPE_NULLPTR_TYPE;
        break;
    case AST_EXPR_STRING: {
        result_type = TYPE_STRING;
        break;
    }
    case AST_EXPR_TYPE_EXPR:
        resolve_type(expr->type_expr.type);
        result_type = TYPE_TYPE_EXPR;
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
            type_error(expr->anchor,
                       "Types for 'then' branch and 'else' branch of 'when' expression must match");
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

static void type_check_function(struct lxl_token anchor, struct ast_decl_func *func) {
    struct func_sig *sig = resolve_func_sig(func->sig);
    struct st_key key = st_key_of(sig->name);
    struct symbol *symbol = lookup_symbol(symbols, key);
    // We insert the function symbol during parsing.
    assert(symbol != NULL);
    assert(symbol->kind == SYMBOL_FUNC);
    if (!symbol->resolved) {
        for (struct ast_decl_func *decl = &symbol->func.decl; decl != NULL; decl = decl->prev_decl) {
            if (!compare_sigs(resolve_func_sig(decl->sig), sig)) {
                type_error(anchor,
                           "Redeclaration of function '"LXL_SV_FMT_SPEC"' with a different signature",
                           LXL_SV_FMT_ARG(sig->name));
            }
        }
        if (symbol->func.decl.link_kind != func->link_kind) {
            // Cannot redeclare with different linkage.
            const char *prev_linkage = "external";
            const char *new_linkage = "internal";
            if (symbol->func.decl.link_kind == FUNC_EXTERNAL) {
                assert(func->link_kind == FUNC_INTERNAL);
            }
            else {
                assert(func->link_kind == FUNC_EXTERNAL);
                prev_linkage = "internal";
                new_linkage = "external";
            }
            type_error(anchor,
                       "Cannot redeclare function '"LXL_SV_FMT_SPEC"' as '%s'"
                       " following previous declaration as '%s'",
                       LXL_SV_FMT_ARG(sig->name), prev_linkage, new_linkage);
        }
        symbol->resolved = true;
    }
    if (func->decl_kind == AST_FUNC_DECL) return;
    struct symbol_table *old_symbols = enter_function(func->symbols);
    for (int i = 0; i < sig->params.count; ++i) {
        struct type_decl *param = &sig->params.items[i];
        struct symbol param_symbol = {
            .kind = SYMBOL_PARAM,
            .param = {.type = param->type}};
        insert_symbol(symbols, st_key_of(param->name), param_symbol);
    }
    enter_function(func->body.symbols);  // Return value not needed; jump to previous symbol table.
    FOR_DLLIST (struct ast_node *, node, &func->body.stmts) {
        assert(node->kind == AST_STMT);
        struct ast_stmt *stmt = &node->stmt;
        type_check_stmt(stmt, sig->ret_type);
    }
    leave_function(old_symbols);
}

static void type_check_decl(struct ast_decl *decl) {
    switch (decl->kind) {
    case AST_DECL_VAR: {
        assert(decl->var.value != NULL || decl->var.type != NULL);
        TypeID value_type = (decl->var.value) ? type_check_expr(decl->var.value) : TYPE_NO_TYPE;
        if (!decl->var.type) {
            if (!value_type) return;
            if (value_type == TYPE_CONST_INT) {
                // Infer `int` type.
                value_type = TYPE_INT;
            }
            decl->var.type = copy_type(RESOLVED_TYPE(value_type));
        }
        assert(decl->var.type);
        TypeID var_type = resolve_type(decl->var.type);
        if (!var_type) return;  // Exit if type resolution failed.
        if (var_type == TYPE_ABSURD) {
            type_error(decl->anchor, "Cannot create variable of absurd type '!'");
        }
        if (value_type && !check_assignable(var_type, value_type)) {
            type_error(decl->anchor, "Mismatched types in variable definition");
        }
        struct symbol symbol;
        switch (decl->var.kind) {
        case AST_VAR_VAR:
            symbol = (struct symbol) {
                .kind = SYMBOL_VAR,
                .var = {.type = var_type}};
            break;
        case AST_VAR_VAL:
            symbol = (struct symbol) {
                .kind = SYMBOL_VAL,
                .val = {.type = var_type}};
            break;
        case AST_VAR_CONST:
            assert(decl->var.value);
            symbol = (struct symbol) {
                .kind = SYMBOL_CONST,
                .const_ = {
                    .type = var_type,
                    .value = decl->var.value}};
            break;
        }
        if (!insert_symbol(symbols, st_key_of(decl->var.name), symbol)) {
            name_error(decl->anchor,
                       "Redeclaration of symbol '"LXL_SV_FMT_SPEC"'", LXL_SV_FMT_ARG(decl->var.name));
        }
    } return;
    case AST_DECL_FUNC:
        type_check_function(decl->anchor, &decl->func);
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
    case AST_DECL_PACKAGE:
        // This is a syntax error, but we only catch it after parsing.
        parse_error(decl->anchor, "Package declaration is only allowed at the beginning of a file module");
        return;
    }
    UNREACHABLE();
}

static void type_check_block(struct ast_stmt_block block, TypeID ret_type) {
    struct symbol_table *old_symbols = enter_function(block.symbols);
    FOR_DLLIST (struct ast_node *, node, &block.stmts) {
        switch (node->kind) {
        case AST_EXPR: case AST_TYPE: UNREACHABLE(); break;
        case AST_STMT: type_check_stmt(&node->stmt, ret_type); break;
        case AST_DECL: type_check_decl(&node->decl); break;
        }
    }
    leave_function(old_symbols);
}

static void type_check_stmt(struct ast_stmt *stmt, TypeID ret_type) {
    switch (stmt->kind) {
    case AST_STMT_BLOCK:
        type_check_block(stmt->block, ret_type);
        return;
    case AST_STMT_DECL:
        type_check_decl(stmt->decl.decl);
        return;
    case AST_STMT_EXPR:
        type_check_expr(stmt->expr.expr);
        return;
    case AST_STMT_IF:
        type_check_expr(stmt->if_.cond);
        type_check_block(stmt->if_.then_clause, ret_type);
        if (stmt->if_.else_clause) {
            // Should the NULL check be moved to the top of this function?
            type_check_stmt(stmt->if_.else_clause, ret_type);
        }
        return;
    case AST_STMT_RETURN:
        if (stmt->return_.expr) {
            TypeID return_expr_type = type_check_expr(stmt->return_.expr);
            if (!check_assignable(ret_type, return_expr_type)) {
                struct lxl_string_view expected_sv = get_type_sv(ret_type);
                struct lxl_string_view actual_sv = get_type_sv(return_expr_type);
                type_error(stmt->anchor,
                           "Expected return type '"LXL_SV_FMT_SPEC"' but got '"LXL_SV_FMT_SPEC"'",
                           LXL_SV_FMT_ARG(expected_sv), LXL_SV_FMT_ARG(actual_sv));
            }
        }
        else if (ret_type == TYPE_ABSURD) {
            type_error(stmt->anchor, "Cannot return from non-returning function");
        }
        else if (ret_type != TYPE_UNIT) {
            struct lxl_string_view expected_sv = get_type_sv(ret_type);
            type_error(stmt->anchor,
                       "Expression-less return from function with non-unit return type '"LXL_SV_FMT_SPEC"'",
                       LXL_SV_FMT_ARG(expected_sv));
        }
        return;
    case AST_STMT_WHILE:
        type_check_expr(stmt->while_.cond);
        type_check_block(stmt->while_.body, ret_type);
        return;
    }
    UNREACHABLE();
}

static void type_check_module(struct module *module) {
    enter_function(module->globals);
    if (module->decls.count == 0) return;
    struct ast_list rest = module->decls;
    struct ast_node *first = module->decls.head;
    assert(first);
    if (first->kind == AST_DECL && first->decl.kind == AST_DECL_PACKAGE) {
        if (!check_or_add_package(first->decl.package.name)) {
            parse_error(first->decl.anchor,
                        "At most one package is allowed; compiling '"LXL_SV_FMT_SPEC"'"
                        " but found declaration for package '"LXL_SV_FMT_SPEC"'",
                        LXL_SV_FMT_ARG(package.name), LXL_SV_FMT_ARG(first->decl.package.name));
        }
        rest = (struct ast_list) {
            .head = module->decls.head->next,
            .tail = module->decls.tail,
            .count = module->decls.count - 1};
    }
    FOR_DLLIST (struct ast_node *, node, &rest) {
        switch (node->kind) {
        case AST_EXPR:
        case AST_TYPE:
            // Cannot have an expression here.
            UNREACHABLE();
            break;
        case AST_STMT:
            type_error(node->stmt.anchor, "Only declarations and definitons are allowed at module scope");
            break;
        case AST_DECL:
            type_check_decl(&node->decl);
            break;
        }
    }
}

bool type_check(void) {
    FOR_DLLIST (struct module *, module, &package.modules) {
        filename = module->filepath.start;
        type_check_module(module);
    }
    return !type_checker.had_error;
}
