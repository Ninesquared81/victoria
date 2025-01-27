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
        "{",  TOKEN_BKT_CURLY_LEFT,
        "}",  TOKEN_BKT_CURLY_RIGHT,
        "(",  TOKEN_BKT_ROUND_LEFT,
        ")",  TOKEN_BKT_ROUND_RIGHT,
        "[",  TOKEN_BKT_SQUARE_LEFT,
        "]",  TOKEN_BKT_SQUARE_RIGHT,
        "&=", TOKEN_AMPERSAND_EQUALS,
        "&",  TOKEN_AMPERSAND,
        "@",  TOKEN_AT,
        "!=", TOKEN_BANG_EQUALS,
        "!",  TOKEN_BANG,
        "^",  TOKEN_CARET,
        ",",  TOKEN_COMMA,
        ":=", TOKEN_COLON_EQUALS,
        ":",  TOKEN_COLON,
        "$",  TOKEN_DOLLAR,
        "==", TOKEN_EQUALS_EQUALS,
        "=",  TOKEN_EQUALS,
        "-=", TOKEN_MINUS_EQUALS,
        "-",  TOKEN_MINUS,
        "%=", TOKEN_PERCENT_EQUALS,
        "%",  TOKEN_PERCENT,
        "+=", TOKEN_PLUS_EQUALS,
        "+",  TOKEN_PLUS,
        "?",  TOKEN_QMARK,
        ";",  TOKEN_SEMICOLON,
        "/=", TOKEN_SLASH_EQUALS,
        "/",  TOKEN_SLASH,
        "*=", TOKEN_STAR_EQUALS,
        "*",  TOKEN_STAR,
        "~=", TOKEN_TILDE_EQUALS,
        "~",  TOKEN_TILDE,
        "|=", TOKEN_VBAR_EQUALS,
        "|",  TOKEN_VBAR);
    // Keywords.
    add_string_type_pairs(
        &lexer.keywords, &lexer.keyword_types,
        "_",            TOKEN_UNDERSCORE,
        "and",          TOKEN_KW_AND,
        "as",           TOKEN_KW_AS,
        "complex32_32", TOKEN_KW_COMPLEX32_32,
        "complex64_64", TOKEN_KW_COMPLEX64_64,
        "else",         TOKEN_KW_ELSE,
        "enum",         TOKEN_KW_ENUM,
        "external",     TOKEN_KW_EXTERNAL,
        "f32",          TOKEN_KW_F32,
        "f64",          TOKEN_KW_F64,
        "for",          TOKEN_KW_FOR,
        "func",         TOKEN_KW_FUNC,
        "i8",           TOKEN_KW_I8,
        "i16",          TOKEN_KW_I16,
        "i32",          TOKEN_KW_I32,
        "i64",          TOKEN_KW_I64,
        "if",           TOKEN_KW_IF,
        "int",          TOKEN_KW_INT,
        "loop",         TOKEN_KW_LOOP,
        "mut",          TOKEN_KW_MUT,
        "or",           TOKEN_KW_OR,
        "out",          TOKEN_KW_OUT,
        "return",       TOKEN_KW_RETURN,
        "then",         TOKEN_KW_THEN,
        "to",           TOKEN_KW_TO,
        "u8",           TOKEN_KW_U8,
        "u16",          TOKEN_KW_U16,
        "u32",          TOKEN_KW_U32,
        "u64",          TOKEN_KW_U64,
        "uint",         TOKEN_KW_UINT,
        "uninit",       TOKEN_KW_UNINIT,
        "when",         TOKEN_KW_WHEN,
        "while",        TOKEN_KW_WHILE);
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

const char *token_type_string(struct lxl_token token) {
    int type = token.token_type;
    if (type <= LXL_LERR_GENERIC) return "<Error Token>";
    if (type < 0) {
        switch (type) {
        case LXL_TOKENS_END: return "<End of Tokens>";
        case LXL_TOKENS_END_ABNORMAL: return "<End of Tokens (Abnormal)>";
        case LXL_TOKEN_UNINIT: return "<unitialised token>";
        case LXL_TOKEN_LINE_ENDING: return "<Line Ending Token>";
        }
        return "<unknown negative token type>";
    }
    switch ((enum token_type)type) {
    case TOKEN_LIT_FLOAT:        return "TOKEN_LIT_FLOAT";
    case TOKEN_LIT_INTEGER:      return "TOKEN_LIT_INTEGER";
    case TOKEN_LIT_STRING:       return "TOKEN_LIT_STRING";
    case TOKEN_BKT_CURLY_LEFT:   return "TOKEN_BKT_CURLY_LEFT";
    case TOKEN_BKT_CURLY_RIGHT:  return "TOKEN_BKT_CURLY_RIGHT";
    case TOKEN_BKT_ROUND_LEFT:   return "TOKEN_BKT_ROUND_LEFT";
    case TOKEN_BKT_ROUND_RIGHT:  return "TOKEN_BKT_ROUND_RIGHT";
    case TOKEN_BKT_SQUARE_LEFT:  return "TOKEN_BKT_SQUARE_LEFT";
    case TOKEN_BKT_SQUARE_RIGHT: return "TOKEN_BKT_SQUARE_RIGHT";
    case TOKEN_AMPERSAND:        return "TOKEN_AMPERSAND";
    case TOKEN_AMPERSAND_EQUALS: return "TOKEN_AMPERSAND_EQUALS";
    case TOKEN_AT:               return "TOKEN_AT";
    case TOKEN_BANG:             return "TOKEN_BANG";
    case TOKEN_BANG_EQUALS:      return "TOKEN_BANG_EQUALS";
    case TOKEN_CARET:            return "TOKEN_CARET";
    case TOKEN_COLON:            return "TOKEN_COLON";
    case TOKEN_COLON_EQUALS:     return "TOKEN_COLON_EQUALS";
    case TOKEN_COMMA:            return "TOKEN_COMMA";
    case TOKEN_DOLLAR:           return "TOKEN_DOLLAR";
    case TOKEN_DOT:              return "TOKEN_DOT";
    case TOKEN_EQUALS:           return "TOKEN_EQUALS";
    case TOKEN_EQUALS_EQUALS:    return "TOKEN_EQUALS_EQUALS";
    case TOKEN_MINUS:            return "TOKEN_MINUS";
    case TOKEN_MINUS_EQUALS:     return "TOKEN_MINUS_EQUALS";
    case TOKEN_PERCENT:          return "TOKEN_PERCENT";
    case TOKEN_PERCENT_EQUALS:   return "TOKEN_PERCENT_EQUALS";
    case TOKEN_PLUS:             return "TOKEN_PLUS";
    case TOKEN_PLUS_EQUALS:      return "TOKEN_PLUS_EQUALS";
    case TOKEN_QMARK:            return "TOKEN_QMARK";
    case TOKEN_SEMICOLON:        return "TOKEN_SEMICOLON";
    case TOKEN_SLASH:            return "TOKEN_SLASH";
    case TOKEN_SLASH_EQUALS:     return "TOKEN_SLASH_EQUALS";
    case TOKEN_STAR:             return "TOKEN_STAR";
    case TOKEN_STAR_EQUALS:      return "TOKEN_STAR_EQUALS";
    case TOKEN_TILDE:            return "TOKEN_TILDE";
    case TOKEN_TILDE_EQUALS:     return "TOKEN_TILDE_EQUALS";
    case TOKEN_VBAR:             return "TOKEN_VBAR";
    case TOKEN_VBAR_EQUALS:      return "TOKEN_VBAR_EQUALS";
    case TOKEN_IDENTIFIER:       return "TOKEN_IDENTIFIER";
    case TOKEN_UNDERSCORE:       return "TOKEN_UNDERSCORE";
    case TOKEN_KW_AND:           return "TOKEN_KW_AND";
    case TOKEN_KW_AS:            return "TOKEN_KW_AS";
    case TOKEN_KW_COMPLEX32_32:  return "TOKEN_KW_COMPLEX32_32";
    case TOKEN_KW_COMPLEX64_64:  return "TOKEN_KW_COMPLEX64_64";
    case TOKEN_KW_ELSE:          return "TOKEN_KW_ELSE";
    case TOKEN_KW_ENUM:          return "TOKEN_KW_ENUM";
    case TOKEN_KW_EXTERNAL:      return "TOKEN_KW_EXTERNAL";
    case TOKEN_KW_F32:           return "TOKEN_KW_F32";
    case TOKEN_KW_F64:           return "TOKEN_KW_F64";
    case TOKEN_KW_FOR:           return "TOKEN_KW_FOR";
    case TOKEN_KW_FUNC:          return "TOKEN_KW_FUNC";
    case TOKEN_KW_I8:            return "TOKEN_KW_I8";
    case TOKEN_KW_I16:           return "TOKEN_KW_I16";
    case TOKEN_KW_I32:           return "TOKEN_KW_I32";
    case TOKEN_KW_I64:           return "TOKEN_KW_I64";
    case TOKEN_KW_IF:            return "TOKEN_KW_IF";
    case TOKEN_KW_INT:           return "TOKEN_KW_INT";
    case TOKEN_KW_LOOP:          return "TOKEN_KW_LOOP";
    case TOKEN_KW_MUT:           return "TOKEN_KW_MUT";
    case TOKEN_KW_OUT:           return "TOKEN_KW_OUT";
    case TOKEN_KW_OR:            return "TOKEN_KW_OR";
    case TOKEN_KW_RECORD:        return "TOKEN_KW_RECORD";
    case TOKEN_KW_RETURN:        return "TOKEN_KW_RETURN";
    case TOKEN_KW_THEN:          return "TOKEN_KW_THEN";
    case TOKEN_KW_TO:            return "TOKEN_KW_TO";
    case TOKEN_KW_U8:            return "TOKEN_KW_U8";
    case TOKEN_KW_U16:           return "TOKEN_KW_U16";
    case TOKEN_KW_U32:           return "TOKEN_KW_U32";
    case TOKEN_KW_U64:           return "TOKEN_KW_U64";
    case TOKEN_KW_UINT:          return "TOKEN_KW_UINT";
    case TOKEN_KW_UNINIT:        return "TOKEN_KW_UNINIT";
    case TOKEN_KW_WHEN:          return "TOKEN_KW_WHEN";
    case TOKEN_KW_WHILE:         return "TOKEN_KW_WHILE";
    }
    return "<unknown token>";
}

void print_token(struct lxl_token token, FILE *f) {
    if (LXL_TOKEN_IS_ERROR(token)) {
        fprintf(f, "%s\n", lxl_error_message(token.token_type));
        return;
    }
    if (LXL_TOKEN_IS_END(token)) {
        fprintf(f, "END OF TOKEN STREAM token\n");
        return;
    }
    if (token.token_type == LXL_TOKEN_LINE_ENDING) {
        fprintf(f, "LINE ENDING token\n");
        return;
    }
    const char *type_string = token_type_string(token);
    struct lxl_string_view value = lxl_token_value(token);
    fprintf(f, "%s token: '"LXL_SV_FMT_SPEC"'\n", type_string, LXL_SV_FMT_ARG(value));
}

void print_tokens(FILE *f) {
    for (struct lxl_token token = next_token(); !LXL_TOKEN_IS_END(token); token = next_token()) {
        print_token(token, f);
    }
    lxl_lexer_reset(&lexer);
}
