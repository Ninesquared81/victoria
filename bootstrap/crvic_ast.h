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
    AST_EXPR_ASSIGN,   // Assignment expression.
    AST_EXPR_BINARY,   // Binary operation.
    AST_EXPR_CALL,     // Function call.
    AST_EXPR_GET,      // Get (the value of a variable, etc.) expression.
    AST_EXPR_INTEGER,  // Integer literal expression.
};

enum ast_stmt_kind {
    AST_STMT_DECL,  // Declaration statement.
    AST_STMT_EXPR,  // Expression statement.
    AST_STMT_IF,    // If statement.
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

struct ast_list {
    struct allocatorARD allocator;
    size_t capacity;
    size_t count;
    struct ast_node *items;
};

struct ast_func_sig {
    struct lxl_string_view name;
    TypeID ret_type;
    int arity;
    struct type_decl_list params;
};

struct ast_expr {
    enum ast_expr_kind kind;
    TypeID type;
    union {
        struct {
            struct lxl_string_view target;
            struct ast_expr *value;
        } assign;
        struct {
            struct ast_expr *lhs;
            struct ast_expr *rhs;
            enum ast_bin_op_kind op;
        } binary;
        struct {
            struct ast_expr *callee;
            struct ast_list args;
        } call;
        struct {
            struct lxl_string_view target;
        } get;
        struct {
            int64_t value;
        } integer;
    };
};

struct ast_stmt {
    enum ast_stmt_kind kind;
    union {
        struct {
            struct ast_decl *decl;
        } decl;
        struct {
            struct ast_expr *expr;
        } expr;
        struct {
            struct ast_expr *cond;
            struct ast_list then_clause;
            struct ast_list else_clause;
        } if_;
    };
};

struct ast_decl {
    enum ast_decl_kind kind;
    union {
        struct {
            struct lxl_string_view name;
            TypeID type;
        } var_decl;
        struct {
            struct lxl_string_view name;
            TypeID type;
            struct ast_expr *value;
        } var_defn;
        struct {
            struct ast_func_sig *sig;
            enum ast_func_decl_kind kind;
        } func_decl;
        struct {
            struct ast_func_sig *sig;
            struct ast_list body;
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

#define EXPR_NODE(EXPR) ((struct ast_node) {.kind = AST_EXPR, .expr = EXPR})
#define STMT_NODE(STMT) ((struct ast_node) {.kind = AST_STMT, .stmt = STMT})
#define DECL_NODE(DECL) ((struct ast_node) {.kind = AST_DECL, .decl = DECL})

#endif
