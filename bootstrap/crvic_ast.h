#ifndef CRVIC_AST
#define CRVIC_AST

#include <stdint.h>  // int64_t

enum ast_node_kind {
    AST_EXPR,  // Expression.
    AST_STMT,  // Statement.
};

enum ast_expr_kind {
    AST_EXPR_INTEGER,  // Integer literal expression.
    AST_EXPR_BINARY,   // Binary operation.
};

enum ast_stmt_kind {
    AST_STMT_EXPR,  // Expression statement.
};

struct ast_expr {
    enum ast_expr_kind kind;
    union {
        struct {
            int64_t value;
        } integer;
        struct {
            struct ast_expr *lhs;
            struct ast_expr *rhs;
            const char *op;
        } binary;
    };
};

struct ast_stmt {
    enum ast_stmt_kind kind;
    union {
        struct {
            struct ast_expr *expr;
        } expr;
    };
};

struct ast_node {
    enum ast_node_kind kind;
    union {
        struct ast_expr *expr;
        struct ast_stmt *stmt;
    };
};

#endif
