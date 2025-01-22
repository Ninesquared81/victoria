#ifndef CRVIC_FRONTEND_H
#define CRVIC_FRONTEND_H

#include "lexel.h"  // struct lxl_token, LXL_TOKENS_END, LXL_LINE_ENDING.

enum token_type {
    // Identifier.
    TOKEN_IDENTIFIER,
    // Literals.
    TOKEN_LIT_FLOAT,
    TOKEN_LIT_INTEGER,
    TOKEN_LIT_STRING,
    // Brackets.
    TOKEN_BKT_CURLY_LEFT,
    TOKEN_BKT_CURLY_RIGHT,
    TOKEN_BKT_ROUND_LEFT,
    TOKEN_BKT_ROUND_RIGHT,
    TOKEN_BKT_SQUARE_LEFT,
    TOKEN_BKT_SQUARE_RIGHT,
    // Other Punctuation.
    TOKEN_AT,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_MINUS,
    TOKEN_PERCENT,
    TOKEN_PLUS,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_STAR,
    // Keywords.
    TOKEN_KW_AND,
    TOKEN_KW_AS,
    TOKEN_KW_ENUM,
    TOKEN_KW_EXTERNAL,
    TOKEN_KW_FUNC,
    TOKEN_KW_OR,
    TOKEN_KW_RECORD,
    TOKEN_KW_RETURN,
    TOKEN_KW_TO,
};

void init_lexer(const char *source, size_t length);
struct lxl_token next_token(void);

void print_token(struct lxl_token token);
void print_tokens(void);

#endif
