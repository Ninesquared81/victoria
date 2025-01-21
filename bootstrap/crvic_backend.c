#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "crvic_backend.h"

enum cgen_error crvic_generate_c_file(int n, const struct ast_node nodes[static const n],
                                      struct string_buffer *sb) {
    assert(n >= 0);
    int indent_step = 4;  // Number of spaces to indent by for each indent/dedent.
    int indent = 0;  // Current indenation level.
    sb_add_string(sb, "#include <stdint.h>  // Fixed-width types.\n");
    return crvic_generate_c_nodes(n, nodes, indent, indent_step, sb);
}

enum cgen_error crvic_generate_c_nodes(int n, const struct ast_node nodes[static const n],
                                       int indent, int indent_step, struct string_buffer *sb) {
    assert(n >= 0);
    assert(indent >= 0 && indent_step >= 0);
    enum cgen_error error = CGEN_OK;
    for (int i = 0; i < n; ++i) {
        struct ast_node node = nodes[i];
        switch (node.kind) {
        case AST_EXPR:
            return CGEN_UNEXPECTED_EXPR;
        case AST_STMT:
            error = crvic_generate_c_stmt(node.stmt, indent, indent_step, sb);
            break;
        case AST_DECL:
            error = crvic_generate_c_decl(node.decl, indent, indent_step, sb);
            break;
        }
        if (error) return error;
    }
        return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_stmt(struct ast_stmt *stmt, int indent, int indent_step,
                                      struct string_buffer *sb) {
    (void)indent_step;
    sb_add_formatted(sb, "%*s", indent, "");  // Prepend indentation.
    enum cgen_error error = CGEN_OK;
    switch (stmt->kind) {
    case AST_STMT_EXPR:
        error = crvic_generate_c_expr(stmt->expr.expr, sb);
        sb_add_string(sb, ";\n");
        break;
    }
    return error;
}

enum cgen_error crvic_generate_c_decl(struct ast_decl *decl, int indent, int indent_step,
                                      struct string_buffer *sb) {
    sb_add_formatted(sb, "%*s", indent, "");
    enum cgen_error error = CGEN_OK;
    switch (decl->kind) {
    case AST_DECL_VAR_DECL:
        sb_add_formatted(sb, "int %s = 0;\n", decl->var_decl.name);
        break;
    case AST_DECL_VAR_DEFN:
        sb_add_formatted(sb, "int %s = ", decl->var_defn.name);
        if ((error = crvic_generate_c_expr(decl->var_defn.value, sb))) return error;
        sb_add_string(sb, ";\n");
        break;
    case AST_DECL_FUNC_DECL:
        if ((error = crvic_generate_c_func_header(decl->func_decl.sig, sb))) return error;
        sb_add_string(sb, ";\n");
        break;
    case AST_DECL_FUNC_DEFN:
        if ((error = crvic_generate_c_func_header(decl->func_defn.sig, sb))) return error;
        sb_add_string(sb, " {\n");
        assert(indent == 0 && "Cannot have nested function in C!");
        if ((error = crvic_generate_c_func_body(decl->func_defn.body_node_count,
                                                decl->func_defn.body,
                                                indent_step, sb))) return error;
        if (strcmp(decl->func_defn.sig->name, "main") == 0) {
            sb_add_formatted(sb, "%*sreturn 0;\n", indent_step, "");
        }
        sb_add_string(sb, "}\n");
        break;
    }
        return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_main_header(struct ast_func_sig *sig, struct string_buffer *sb) {
    assert(strcmp(sig->name, "main") == 0);
    assert(sig->param_count == 0 && "Only allow parameter-less version for now.");
    sb_add_string(sb, "int main(void)");
        return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_func_header(struct ast_func_sig *sig, struct string_buffer *sb) {
    if (strcmp(sig->name, "main") == 0) return crvic_generate_c_main_header(sig, sb);
    sb_add_formatted(sb, "%s %s(", crvic_get_c_type(sig->ret_type), sig->name);
    assert(sig->param_count >= 0);
    if (sig->param_count >= 1) {
        sb_add_formatted(sb, "%s", crvic_get_c_type(sig->params[0]));
    }
    else {
        // No parameters; use `void` in parameter list for strict adherence to (pre-C23) C standard.
        sb_add_string(sb, "void");
    }
    for (int i = 1; i < sig->param_count; ++i) {
        sb_add_formatted(sb, ", %s", crvic_get_c_type(sig->params[i]));
    }
    sb_add_string(sb, ")");
    return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_func_body(int n, const struct ast_node nodes[static const n],
                                           int indent_step, struct string_buffer *sb) {
    int indent = indent_step;
    return crvic_generate_c_nodes(n, nodes, indent, indent_step, sb);
}

enum cgen_error crvic_generate_c_expr(struct ast_expr *expr, struct string_buffer *sb) {
    enum cgen_error error = CGEN_OK;
    switch (expr->kind) {
    case AST_EXPR_INTEGER:
        sb_add_formatted(sb, "%"PRId64, expr->integer.value);
        break;
    case AST_EXPR_BINARY:
        sb_add_string(sb, "(");  // Brackets to ensure precedence is preserved.
        if ((error = crvic_generate_c_expr(expr->binary.lhs, sb))) return error;
        sb_add_formatted(sb, " %s ", crvic_get_c_op(expr->binary.op));
        if ((error = crvic_generate_c_expr(expr->binary.rhs, sb))) return error;
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_ASSIGN:
        sb_add_string(sb, "(");
        sb_add_formatted(sb, "%s = ", expr->assign.target);
        if ((error - crvic_generate_c_expr(expr->assign.value, sb))) return error;
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_GET:
        sb_add_formatted(sb, "%s", expr->get.target);
        break;
    }
    return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
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

const char *crvic_get_c_type(TypeID type) {
    assert(type < TYPE_PRIMITIVE_COUNT);
    switch ((enum type_primitive)type) {
    case TYPE_NO_TYPE:
    case TYPE_ABSURD:
    case TYPE_UNIT:
        return "void";
    case TYPE_I8: return "int8_t";
    case TYPE_I16: return "int16_t";
    case TYPE_I32: return "int32_t";
    case TYPE_I64: return "int64_t";
    case TYPE_U8: return "uint8_t";
    case TYPE_U16: return "uint16_t";
    case TYPE_U32: return "uint32_t";
    case TYPE_U64: return "uint64_t";
    case TYPE_INT: return "intptr_t";
    case TYPE_UINT: return "uintptr_t";
    case TYPE_PRIMITIVE_COUNT:
        assert(0 && "Bad type 'TYPE_PRIMITIVE_COUNT'");
        break;
    }
    return NULL;
}
