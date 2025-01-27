#ifndef CRVIC_AST
#define CRVIC_AST

#include <stdint.h>  // int64_t.

#include "lexel.h"  // struct lxl_string_view.

#include "ubiqs.h"  // struct allocatorARD.

#include "crvic_type.h"  // TypeID.

enum ast_node_kind {
    AST_EXPR,  // Expression.
    AST_STMT,  // Statement.
    AST_DECL,  // Declaration/definition.
};

enum ast_expr_kind {
    AST_EXPR_INTEGER,  // Integer literal expression.
    AST_EXPR_BINARY,   // Binary operation.
    AST_EXPR_ASSIGN,   // Assignment expression.
    AST_EXPR_GET,      // Get (the value of a variable, etc.) expression.
};

enum ast_stmt_kind {
    AST_STMT_EXPR,  // Expression statement.
};

enum ast_decl_kind {
    AST_DECL_VAR_DECL,  // Variable declaration.
    AST_DECL_VAR_DEFN,  // Variable definition.
    AST_DECL_FUNC_DECL, // (External) function declaration.
    AST_DECL_FUNC_DEFN, // Function definition.
};

enum ast_bin_op_kind {
    AST_BIN_ADD,  // Addition operator `+`.
    AST_BIN_MUL,  // Multiplication operator `*`.
};

enum ast_func_decl_kind {
    AST_FUNC_EXTERNAL,  // A function defined externally (possibly in a different language, like C).
    AST_FUNC_INTERNAL,  // A function defined within this source file.
};

struct ast_func_sig {
    struct lxl_string_view name;
    TypeID ret_type;
    int arity;
    struct type_list param_types;
};

struct ast_expr {
    enum ast_expr_kind kind;
    TypeID type;
    union {
        struct {
            int64_t value;
        } integer;
        struct {
            struct ast_expr *lhs;
            struct ast_expr *rhs;
            enum ast_bin_op_kind op;
        } binary;
        struct {
            struct lxl_string_view target;
            struct ast_expr *value;
        } assign;
        struct {
            struct lxl_string_view target;
        } get;
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

struct ast_decl {
    enum ast_decl_kind kind;
    union {
        struct {
            struct lxl_string_view name;
        } var_decl;
        struct {
            struct lxl_string_view name;
            struct ast_expr *value;
        } var_defn;
        struct {
            struct ast_func_sig *sig;
            enum ast_func_decl_kind kind;
        } func_decl;
        struct {
            struct ast_func_sig *sig;
            size_t body_node_count;
            struct ast_node *body;
        } func_defn;
    };
};

struct ast_node {
    enum ast_node_kind kind;
    union {
        struct ast_expr *expr;
        struct ast_stmt *stmt;
        struct ast_decl *decl;
    };
};

struct ast_list {
    struct allocatorARD allocator;
    size_t capacity;
    size_t count;
    struct ast_node *items;
};

#endif
