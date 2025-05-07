#ifndef CRVIC_BACKEND
#define CRVIC_BACKEND

#include "crvic_ast.h"
#include "crvic_string_buffer.h"  // struct string_buffer.
#include "crvic_type.h"  // TypeID.

enum cgen_error {
    CGEN_OK,
    CGEN_GENERIC_ERROR,
    CGEN_UNEXPECTED_EXPR,
    CGEN_UNEXPECTED_STMT,
    CGEN_IO_ERROR,
};

enum cgen_error crvic_generate_c_file(struct ast_list nodes, struct string_buffer *sb);
enum cgen_error crvic_generate_c_nodes(struct ast_list nodes, int indent, int indent_step,
                                       struct string_buffer *sb);

enum cgen_error crvic_generate_c_stmt(struct ast_stmt *stmt, int indent, int indent_step,
                                      struct string_buffer *sb);
enum cgen_error crvic_generate_c_decl(struct ast_decl *decl, int indent, int indent_step,
                                      struct string_buffer *sb);
enum cgen_error crvic_generate_c_expr(struct ast_expr *expr, struct string_buffer *sb);

enum cgen_error crvic_generate_c_main_header(struct func_sig *sig, struct string_buffer *sb);
enum cgen_error crvic_generate_c_func_header(struct func_sig *sig, struct string_buffer *sb);
enum cgen_error crvic_generate_c_func_body(struct ast_list nodes, int indent_step, struct string_buffer *sb);
enum cgen_error crvic_generate_c_expr_sep_list(struct ast_list nodes, const char *sep,
                                               struct string_buffer *sb);

enum cgen_error crvic_generate_c_types(int indent_step, struct string_buffer *sb);
enum cgen_error crvic_generate_c_record_defn(struct type_info info, int indent_step,
                                             struct string_buffer *sb);

const char *crvic_get_c_op(enum ast_bin_op_kind op);
const char *crvic_get_c_type(TypeID type);


#endif
