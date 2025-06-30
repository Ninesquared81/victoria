#ifndef CRVIC_AST
#define CRVIC_AST

#include <stdint.h>  // int64_t.

#include "lexel.h"  // struct lxl_string_view.

#include "ubiqs.h"  // struct allocatorARD.

#include "crvic_function.h"
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
    AST_EXPR_CONSTRUCTOR, // Type constructor expression.
    AST_EXPR_CONVERT,  // Explicit type conversion.
    AST_EXPR_GET,      // Get (the value of a variable, etc.) expression.
    AST_EXPR_INTEGER,  // Integer literal expression.
    AST_EXPR_WHEN,     // When (conditional) expression.
};

enum ast_stmt_kind {
    AST_STMT_DECL,  // Declaration statement.
    AST_STMT_EXPR,  // Expression statement.
    AST_STMT_IF,    // If statement.
    AST_STMT_RETURN, // Return statment.
};

enum ast_decl_kind {
    AST_DECL_VAR_DECL,  // Variable declaration.
    AST_DECL_VAR_DEFN,  // Variable definition.
    AST_DECL_FUNC,      // Function definition/declaration.
    AST_DECL_EXTERNAL_BLOCK,  // Block of external function declarations.
    AST_DECL_TYPE_DEFN, // Type (alias) definition.
};

enum ast_bin_op_kind {
    AST_BIN_ADD,  // Addition operator `+`.
    AST_BIN_MUL,  // Multiplication operator `*`.
};

enum ast_target_kind {
    AST_TARGET_IDENTIFIER,  // Identifier (variable, field, etc.).
};

enum ast_var_kind {
    AST_VAR_VAR,  // Mutable variable.
    AST_VAR_VAL,  // Immutable value.
};

enum ast_func_decl_kind {
    AST_FUNC_DECL,
    AST_FUNC_DEFN,
};

enum ast_type_kind {
    AST_TYPE_PRIMITIVE,
    AST_TYPE_ALIAS,
    AST_TYPE_RECORD,
    AST_TYPE_ENUM,
};

struct ast_list {
    struct ast_node *head;
    struct ast_node *tail;
    int count;
};

struct ast_type_decl {
    struct lxl_string_view name;
    struct ast_type *type;
};

struct ast_type_decl_list {
    struct allocatorARD allocator;
    int capacity;
    int count;
    struct ast_type_decl *items;
};

#define AST_TYPE_DECL_LIST(ALLOCATOR)           \
    ((struct ast_type_decl_list) {.allocator = ALLOCATOR})

struct ast_enum_field {
    struct lxl_string_view name;
    struct ast_expr *value;  // Specified value, can be NULL for default value.
};

struct ast_enum_field_list {
    struct allocatorARD allocator;
    int capacity;
    int count;
    struct ast_enum_field *items;
};

#define AST_ENUM_FIELD_LIST(ALLOCATOR)          \
    ((struct ast_enum_field_list) {.allocator = ALLOCATOR})

struct ast_sig {
    struct func_sig resolved_sig;
    struct ast_type_decl_list params;
    struct ast_type *ret_type;
    bool c_variadic;
    bool resolved;
};

struct ast_type {
    enum ast_type_kind kind;
    TypeID resolved_type;
    union {
        /* struct {} primitive; */
        struct {
            struct lxl_string_view name;
        } alias;
        struct {
            struct ast_type_decl_list fields;
        } record_lit;
        struct {
            struct ast_type *underlying_type;
            struct ast_enum_field_list fields;
        } enum_lit;
    };
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
            struct lxl_string_view callee_name;
            int arity;  // Number of arguments at call site.
            struct ast_list args;
        } call;
        struct {
            struct lxl_string_view name;
            struct ast_list init_list;
        } constructor;
        struct {
            struct ast_expr *operand;
            struct ast_type *target_type;
            enum type_conv_kind kind;
        } convert;
        struct ast_expr_get {
            struct ast_expr_get *rest;  // RHS of `.` operator, optional.
            union {
                struct lxl_string_view identifier;
            } target;
            enum ast_target_kind kind;
        } get;
        struct {
            int64_t value;
        } integer;
        struct {
            struct ast_expr *cond;
            struct ast_expr *then_expr;
            struct ast_expr *else_expr;
        } when;
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
        struct {
            struct ast_expr *expr;  // Note: can be NULL for expression-less return.
        } return_;
    };
};

struct ast_decl {
    enum ast_decl_kind kind;
    union {
        // TODO: merge `.var_decl` and `.var_defn`.
        struct {
            struct lxl_string_view name;
            struct ast_type *type;
            enum ast_var_kind kind;
        } var_decl;
        struct {
            struct lxl_string_view name;
            struct ast_type *type;
            enum ast_var_kind kind;
            struct ast_expr *value;
        } var_defn;
        // TODO: remove _defn from this name.
        struct ast_decl_func_defn {
            struct ast_sig *sig;
            struct ast_decl *prev_decl;
            enum func_link_kind link_kind;
            enum ast_func_decl_kind decl_kind;
            struct ast_list body;
        } func;
        struct {
            struct ast_list decls;
        } external_block;
        struct {
            struct lxl_string_view alias;
            struct ast_type *type;
        } type_defn;
    };
};

struct ast_node {
    struct ast_node *next, *prev;

    enum ast_node_kind kind;
    union {
        struct ast_expr expr;
        struct ast_stmt stmt;
        struct ast_decl decl;
        struct ast_type type;
    };
};

#define EXPR_NODE(EXPR) ((struct ast_node) {.kind = AST_EXPR, .expr = EXPR})
#define STMT_NODE(STMT) ((struct ast_node) {.kind = AST_STMT, .stmt = STMT})
#define DECL_NODE(DECL) ((struct ast_node) {.kind = AST_DECL, .decl = DECL})
#define TYPE_NODE(TYPE) ((struct ast_node) {.kind = AST_TYPE, .type = TYPE})

#define RESOLVED_TYPE(type_id) ((struct ast_type) {.resolved_type = type_id})

#endif
