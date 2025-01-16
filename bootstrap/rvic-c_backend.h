#ifndef CRVIC_BACKEND
#define CRVIC_BACKEND

#include <stdio.h>  // FILE.

#include "crvic_ast.h"

enum cgen_error {
    CGEN_OK,
    CGEN_GENERIC_ERROR,
    CGEN_UNEXPECTED_EXPR,
    CGEN_IO_ERROR,
};

enum cgen_error crvic_generate_c_file(int n, const struct ast_node nodes[const n], FILE *f);
enum cgen_error crvic_generate_c_stmt(struct ast_stmt *stmt, int indent, int indent_step, FILE *f);
enum cgen_error crvic_generate_c_expr(struct ast_expr *expr, FILE *f);

#endif
