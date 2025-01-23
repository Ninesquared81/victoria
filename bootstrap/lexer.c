#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define LEXEL_IMPLEMENTATION
#include "lexel.h"

#include "crvic_frontend.h"  // enum token_type.

#define STRING_POOL_COUNT 1000
static const char *string_ptr_pool[STRING_POOL_COUNT];
int strings_head = 0;

#define DELIM_POOL_COUNT 50
static struct lxl_delim_pair delim_pool[DELIM_POOL_COUNT];
int delims_head = 0;

#define TYPE_POOL_COUNT 100
int type_pool[TYPE_POOL_COUNT];
int types_head = 0;

static struct lxl_lexer lexer = {0};
static bool lexer_is_initialised = false;

static void add_string(const char *s) {
    if (strings_head >= STRING_POOL_COUNT) {
        fprintf(stderr, "No space in string pointer pool!\n");
        exit(1);
    }
    string_ptr_pool[strings_head++] = s;
}

static void add_delim_pair(const char *opener, const char *closer) {
    if (delims_head >= DELIM_POOL_COUNT) {
        fprintf(stderr, "No space in delim pool!\n");
        exit(1);
    }
    delim_pool[delims_head++] = (struct lxl_delim_pair) {opener, closer};
}

static void add_type(int token_type) {
    if (delims_head >= TYPE_POOL_COUNT) {
        fprintf(stderr, "No space in types pool!\n");
        exit(1);
    }
    type_pool[types_head++] = token_type;
}

#define add_strings(...) add_strings_impl(__VA_ARGS__, NULL)

static const char *const *add_strings_impl(const char *s, ...) {
    assert(s != NULL);
    const int start = strings_head;
    va_list args;
    va_start(args, s);
    for (; s != NULL; s = va_arg(args, const char *)) {
        add_string(s);
    }
    va_end(args);
    add_string(NULL);
    return &string_ptr_pool[start];
}

#define add_delim_pairs(...) add_delim_pairs_impl(__VA_ARGS__, NULL)

static const struct lxl_delim_pair *add_delim_pairs_impl(const char *opener, const char *closer, ...) {
    const int start = delims_head;
    va_list args;
    va_start(args, closer);
    for (; opener != NULL; (void)((opener = va_arg(args, const char *)) &&
                                  (closer = va_arg(args, const char *)))) {
        add_delim_pair(opener, closer);
    }
    va_end(args);
    add_delim_pair(NULL, NULL);
    return &delim_pool[start];
}

#define add_types(...) add_types_impl(__VA_ARGS__, -1)

static const int *add_types_impl(int token_type, ...) {
    const int start = types_head;
    va_list args;
    va_start(args, token_type);
    for (; token_type > 0; token_type = va_arg(args, int)) {
        add_type(token_type);
    }
    va_end(args);
    return &type_pool[start];
}

#define add_string_type_pairs(strings, types, ...)                      \
    add_string_type_pairs_impl(strings, types, __VA_ARGS__, NULL)

static void add_string_type_pairs_impl(const char *const **strings, const int **types, ...) {
    va_list args;
    va_start(args, types);
    *strings = &string_ptr_pool[strings_head];
    *types = &type_pool[types_head];
    const char *s;
    while ((s = va_arg(args, const char *))) {
        add_string(s);
        add_type(va_arg(args, int));
    }
    va_end(args);
    add_string(NULL);
}

// Create lexer object.
void init_lexer(const char *source, size_t length) {
    if (lexer_is_initialised) return;
    struct lxl_string_view sv = {.start = source, .length = length};
    lexer = lxl_lexer_from_sv(sv);
    // Comments.
    lexer.line_comment_openers = add_strings("#");
    lexer.nestable_comment_delims = add_delim_pairs("~#", "#~");
    // Strings.
    lexer.line_string_delims = add_delim_pairs("\"", "\"");
    lexer.line_string_types = add_types(TOKEN_LIT_STRING);
    // Numbers.
    lexer.digit_separators = "_";
    //// Ints.
    lexer.default_int_type = TOKEN_LIT_INTEGER;
    lexer.default_int_base = 10;
    //// Floats.
    lexer.default_float_type = TOKEN_LIT_FLOAT;
    lexer.default_float_base = 10;
    // Punctation.
    add_string_type_pairs(
        &lexer.puncts, &lexer.punct_types,
        "{", TOKEN_BKT_CURLY_LEFT,
        "}", TOKEN_BKT_CURLY_RIGHT,
        "(", TOKEN_BKT_ROUND_LEFT,
        ")", TOKEN_BKT_ROUND_RIGHT,
        "[", TOKEN_BKT_SQUARE_LEFT,
        "]", TOKEN_BKT_SQUARE_RIGHT,
        "@", TOKEN_AT,
        ",", TOKEN_COMMA,
        "-", TOKEN_MINUS,
        "%", TOKEN_PERCENT,
        "+", TOKEN_PLUS,
        ";", TOKEN_SEMICOLON,
        "/", TOKEN_SLASH,
        "*", TOKEN_STAR);
    // Keywords.
    add_string_type_pairs(
        &lexer.keywords, &lexer.keyword_types,
        "and",      TOKEN_KW_AND,
        "as",       TOKEN_KW_AS,
        "enum",     TOKEN_KW_ENUM,
        "external", TOKEN_KW_EXTERNAL,
        "func",     TOKEN_KW_FUNC,
        "or",       TOKEN_KW_OR,
        "return",   TOKEN_KW_RETURN,
        "to",       TOKEN_KW_TO);
    // Identifiers.
    lexer.default_word_type = TOKEN_IDENTIFIER;
    lexer.word_lexing_rule = LXL_LEX_WORD;
    // Line endings.
    lexer.emit_line_endings = true;

    // Initialisation complete!
    lexer_is_initialised = true;
}

// Get the next token.
struct lxl_token next_token(void) {
    assert(lexer_is_initialised);
    return lxl_lexer_next_token(&lexer);
}

void print_token(struct lxl_token token) {
    if (LXL_TOKEN_IS_ERROR(token)) {
        printf("%s\n", lxl_error_message(token.token_type));
        return;
    }
    if (LXL_TOKEN_IS_END(token)) {
        printf("END OF TOKEN STREAM token\n");
        return;
    }
    if (token.token_type == LXL_TOKEN_LINE_ENDING) {
        printf("LINE ENDING token\n");
        return;
    }
    switch ((enum token_type)token.token_type) {
    case TOKEN_IDENTIFIER:
        printf("TOKEN_IDENTIFIER");
        break;
    case TOKEN_LIT_FLOAT:
        printf("TOKEN_LIT_FLOAT");
        break;
    case TOKEN_LIT_INTEGER:
        printf("TOKEN_LIT_INTEGER");
        break;
    case TOKEN_LIT_STRING:
        printf("TOKEN_LIT_STRING");
        break;
    case TOKEN_BKT_CURLY_LEFT:
        printf("TOKEN_BKT_CURLY_LEFT");
        break;
    case TOKEN_BKT_CURLY_RIGHT:
        printf("TOKEN_BKT_CURLY_RIGHT");
        break;
    case TOKEN_BKT_ROUND_LEFT:
        printf("TOKEN_BKT_ROUND_LEFT");
        break;
    case TOKEN_BKT_ROUND_RIGHT:
        printf("TOKEN_BKT_ROUND_RIGHT");
        break;
    case TOKEN_BKT_SQUARE_LEFT:
        printf("TOKEN_BKT_SQUARE_LEFT");
        break;
    case TOKEN_BKT_SQUARE_RIGHT:
        printf("TOKEN_BKT_SQUARE_RIGHT");
        break;
    case TOKEN_AT:
        printf("TOKEN_AT");
        break;
    case TOKEN_COMMA:
        printf("TOKEN_COMMA");
        break;
    case TOKEN_DOT:
        printf("TOKEN_DOT");
        break;
    case TOKEN_MINUS:
        printf("TOKEN_MINUS");
        break;
    case TOKEN_PERCENT:
        printf("TOKEN_PERCENT");
        break;
    case TOKEN_PLUS:
        printf("TOKEN_PLUS");
        break;
    case TOKEN_SEMICOLON:
        printf("TOKEN_SEMICOLON");
        break;
    case TOKEN_SLASH:
        printf("TOKEN_SLASH");
        break;
    case TOKEN_STAR:
        printf("TOKEN_STAR");
        break;
    case TOKEN_KW_AND:
        printf("TOKEN_KW_AND");
        break;
    case TOKEN_KW_AS:
        printf("TOKEN_KW_AS");
        break;
    case TOKEN_KW_ENUM:
        printf("TOKEN_KW_ENUM");
        break;
    case TOKEN_KW_EXTERNAL:
        printf("TOKEN_KW_EXTERNAL");
        break;
    case TOKEN_KW_FUNC:
        printf("TOKEN_KW_FUNC");
        break;
    case TOKEN_KW_OR:
        printf("TOKEN_KW_OR");
        break;
    case TOKEN_KW_RECORD:
        printf("TOKEN_RECORD");
        break;
    case TOKEN_KW_RETURN:
        printf("TOKEN_KW_RETURN");
        break;
    case TOKEN_KW_TO:
        printf("TOKEN_KW_TO");
        break;
    }
    struct lxl_string_view value = lxl_token_value(token);
    printf(" : '"LXL_SV_FMT_SPEC"'\n", LXL_SV_FMT_ARG(value));
}

void print_tokens(void) {
    for (struct lxl_token token = next_token(); !LXL_TOKEN_IS_END(token); token = next_token()) {
        print_token(token);
    }
    lxl_lexer_reset(&lexer);
}
