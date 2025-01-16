#ifndef RVIC_C_BACKEND
#define RVIC_C_BACKEND

#include <stdio.h>  // FILE.

#include "ast.h"

enum cgen_error {
    CGEN_OK,
    CGEN_GENERIC_ERROR,
    CGEN_UNEXPECTED_EXPR,
    CGEN_IO_ERROR,
};

enum cgen_error rvic_c_generate_c_file(int n, const struct ast_node nodes[const n], FILE *f);
enum cgen_error rvic_c_generate_c_stmt(struct ast_stmt stmt, int indent, int indent_step, FILE *f);
enum cgen_error rvic_c_generate_e_expr(struct ast_expr expr, FILE *f);

#endif
