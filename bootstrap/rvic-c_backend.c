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
        }
        if (error) return error;
    }
    fprintf(f, "    return 0;\n}\n");
    return CGEN_OK;
}

enum cgen_error crvic_generate_c_stmt(struct ast_stmt *stmt, int indent, int indent_step, FILE *f) {
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

enum cgen_error crvic_generate_c_expr(struct ast_expr *expr, FILE *f) {
    enum cgen_error error = CGEN_OK;
    switch (expr->kind) {
    case AST_EXPR_INTEGER:
        if (!fprintf(f, "%"PRId64, expr->integer.value)) return CGEN_IO_ERROR;
        break;
    case AST_EXPR_BINARY:
        if (!fprintf(f, "(")) return CGEN_IO_ERROR;  // Brackets to ensure precedence is preserved.
        if ((error = crvic_generate_c_expr(expr->binary.lhs, f))) return error;
        if (!fprintf(f, " %s ", expr->binary.op)) return CGEN_IO_ERROR;
        if ((error = crvic_generate_c_expr(expr->binary.rhs, f))) return error;
        if (!fprintf(f, ")")) return CGEN_IO_ERROR;
        break;
    }
    return CGEN_OK;
}
