#include <assert.h>
#include <inttypes.h>

#include "crvic_backend.h"

enum cgen_error crvic_generate_c_file(int n, const struct ast_node nodes[static const n], FILE *f) {
    assert(n >= 0);
    fprintf(f, "int main(void) {\n");
    int indent_step = 4;  // Number of spaces to indent by for each indent/dedent.
    int indent = indent_step;  // Current indenation level.
    enum cgen_error error = CGEN_OK;
    for (int i = 0; i < n; ++i) {
        struct ast_node node = nodes[i];
        switch (node.kind) {
        case AST_EXPR:
            return CGEN_UNEXPECTED_EXPR;
        case AST_STMT:
            error = crvic_generate_c_stmt(node.stmt, indent, indent_step, f);
            break;
        case AST_DECL:
            error = crvic_generate_c_decl(node.decl, indent, indent_step, f);
            break;
        }
        if (error) return error;
    }
    fprintf(f, "%*sreturn 0;\n}\n", indent, "");
    return CGEN_OK;
}

enum cgen_error crvic_generate_c_stmt(struct ast_stmt *stmt, int indent, int indent_step, FILE *f) {
    (void)indent_step;
    fprintf(f, "%*s", indent, "");  // Prepend indentation.
    enum cgen_error error = CGEN_OK;
    switch (stmt->kind) {
    case AST_STMT_EXPR:
        error = crvic_generate_c_expr(stmt->expr.expr, f);
        fprintf(f, ";\n");
        break;
    }
    return error;
}

enum cgen_error crvic_generate_c_decl(struct ast_decl *decl, int indent, int indent_step, FILE *f) {
    (void)indent_step;
    fprintf(f, "%*s", indent, "");
    enum cgen_error error = CGEN_OK;
    switch (decl->kind) {
    case AST_DECL_VAR_DECL:
        if (!fprintf(f, "int %s;\n", decl->var_decl.name)) return CGEN_IO_ERROR;
        break;
    case AST_DECL_VAR_DEFN:
        if (!fprintf(f, "int %s = ", decl->var_defn.name)) return CGEN_IO_ERROR;
        if ((error = crvic_generate_c_expr(decl->var_defn.value, f))) return error;
        if (!fprintf(f, ";\n")) return CGEN_IO_ERROR;
    }
    return CGEN_OK;
}

enum cgen_error crvic_generate_c_expr(struct ast_expr *expr, FILE *f) {
    enum cgen_error error = CGEN_OK;
    switch (expr->kind) {
    case AST_EXPR_INTEGER:
        if (!fprintf(f, "%"PRId64, expr->integer.value)) return CGEN_IO_ERROR;
        break;
    case AST_EXPR_BINARY:
        if (!fprintf(f, "(")) return CGEN_IO_ERROR;  // Brackets to ensure precedence is preserved.
        if ((error = crvic_generate_c_expr(expr->binary.lhs, f))) return error;
        if (!fprintf(f, " %s ", crvic_get_c_op(expr->binary.op))) return CGEN_IO_ERROR;
        if ((error = crvic_generate_c_expr(expr->binary.rhs, f))) return error;
        if (!fprintf(f, ")")) return CGEN_IO_ERROR;
        break;
    case AST_EXPR_ASSIGN:
        if (!fprintf(f, "(")) return CGEN_IO_ERROR;
        if (!fprintf(f, "%s = ", expr->assign.target)) return CGEN_IO_ERROR;
        if ((error - crvic_generate_c_expr(expr->assign.value, f))) return error;
        if (!fprintf(f, ")")) return CGEN_IO_ERROR;
        break;
    }
    return CGEN_OK;
}

const char *crvic_get_c_op(enum ast_bin_op_kind op) {
    switch (op) {
    case AST_BIN_ADD:
        return "+";
    case AST_BIN_MUL:
        return "*";
    }
    assert(0 && "Unreachable");
    return NULL;
}
