#ifndef CRVIC_FRONTEND_H
#define CRVIC_FRONTEND_H

#include <stdio.h>
#include "lexel.h"  // struct lxl_token, LXL_TOKENS_END, LXL_LINE_ENDING.

#include "region.h"

enum token_type {
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
    // Arrows.
    TOKEN_ARROW_RIGHT,
    // Other Punctuation.
    TOKEN_AMPERSAND,
    TOKEN_AMPERSAND_EQUALS,
    TOKEN_AT,
    TOKEN_BANG,
    TOKEN_BANG_EQUALS,
    TOKEN_CARET,
    TOKEN_COLON,
    TOKEN_COLON_EQUALS,
    TOKEN_COMMA,
    TOKEN_DOLLAR,
    TOKEN_DOT,
    TOKEN_DOT_DOT,
    TOKEN_DOT_DOT_BANG,
    TOKEN_EQUALS,
    TOKEN_EQUALS_EQUALS,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUALS,
    TOKEN_GREATER_GREATER,
    TOKEN_GREATER_GREATER_EQUALS,
    TOKEN_LESS,
    TOKEN_LESS_EQUALS,
    TOKEN_LESS_LESS,
    TOKEN_LESS_LESS_EQUALS,
    TOKEN_MINUS,
    TOKEN_MINUS_EQUALS,
    TOKEN_PERCENT,
    TOKEN_PERCENT_EQUALS,
    TOKEN_PLUS,
    TOKEN_PLUS_EQUALS,
    TOKEN_QMARK,
    TOKEN_SEMICOLON,
    TOKEN_SLASH,
    TOKEN_SLASH_EQUALS,
    TOKEN_STAR,
    TOKEN_STAR_EQUALS,
    TOKEN_TILDE,
    TOKEN_TILDE_EQUALS,
    TOKEN_VBAR,
    TOKEN_VBAR_EQUALS,
    // Identifier.
    TOKEN_IDENTIFIER,
    TOKEN_UNDERSCORE,
    // Keywords.
    TOKEN_KW_AND,
    TOKEN_KW_AS,
    TOKEN_KW_COMPLEX32_32,
    TOKEN_KW_COMPLEX64_64,
    TOKEN_KW_CONST,
    TOKEN_KW_ELSE,
    TOKEN_KW_ENUM,
    TOKEN_KW_EXTERNAL,
    TOKEN_KW_F32,
    TOKEN_KW_F64,
    TOKEN_KW_FOR,
    TOKEN_KW_FUNC,
    TOKEN_KW_I8,
    TOKEN_KW_I16,
    TOKEN_KW_I32,
    TOKEN_KW_I64,
    TOKEN_KW_IF,
    TOKEN_KW_INT,
    TOKEN_KW_LOOP,
    TOKEN_KW_MUT,
    TOKEN_KW_OR,
    TOKEN_KW_OUT,
    TOKEN_KW_RECORD,
    TOKEN_KW_RETURN,
    TOKEN_KW_THEN,
    TOKEN_KW_TO,
    TOKEN_KW_TYPE,
    TOKEN_KW_U8,
    TOKEN_KW_U16,
    TOKEN_KW_U32,
    TOKEN_KW_U64,
    TOKEN_KW_UINT,
    TOKEN_KW_UNINIT,
    TOKEN_KW_VAL,
    TOKEN_KW_VAR,
    TOKEN_KW_WHEN,
    TOKEN_KW_WHILE,
};

// General.
void init_frontend(struct lxl_string_view source, const char *in_filename);

// Lexer.
struct lxl_lexer *init_lexer(struct lxl_string_view source);
struct lxl_token next_token(void);

const char *token_type_string(struct lxl_token token);
void print_token(struct lxl_token token, FILE *f);
void print_tokens(FILE *f);

// Parser.
struct ast_list parse(void);   // NOTE: call `init_parser()` first!

// Type Checker.
bool type_check(struct ast_list *nodes);

#endif
