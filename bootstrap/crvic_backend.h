#ifndef CRVIC_BACKEND
#define CRVIC_BACKEND

#include "crvic_ast.h"
#include "crvic_string_buffer.h"  // struct string_buffer.
#include "crvic_type.h"  // TypeID.

enum cgen_error {
    CGEN_OK,
    CGEN_GENERIC_ERROR,
    CGEN_UNEXPECTED_EXPR,
    CGEN_IO_ERROR,
};

enum cgen_error crvic_generate_c_file(int n, const struct ast_node nodes[const n], struct string_buffer *sb);
enum cgen_error crvic_generate_c_nodes(int n, const struct ast_node nodes[static const n],
                                       int indent, int indent_step, struct string_buffer *sb);
enum cgen_error crvic_generate_c_stmt(struct ast_stmt *stmt, int indent, int indent_step,
                                      struct string_buffer *sb);
enum cgen_error crvic_generate_c_decl(struct ast_decl *decl, int indent, int indent_step,
                                      struct string_buffer *sb);
enum cgen_error crvic_generate_c_func_header(struct ast_func_sig *sig, struct string_buffer *sb);
enum cgen_error crvic_generate_c_func_body(int n, const struct ast_node nodes[static const n],
                                           int indent_step, struct string_buffer *sb);
enum cgen_error crvic_generate_c_expr(struct ast_expr *expr, struct string_buffer *sb);
const char *crvic_get_c_op(enum ast_bin_op_kind op);
const char *crvic_get_c_type(TypeID type);

#endif
