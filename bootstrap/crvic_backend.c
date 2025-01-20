#include <assert.h>
#include <inttypes.h>

#include "crvic_backend.h"

enum cgen_error crvic_generate_c_file(int n, const struct ast_node nodes[static const n], FILE *f) {
    assert(n >= 0);
    assert(f != NULL);
    int indent_step = 4;  // Number of spaces to indent by for each indent/dedent.
    int indent = 0;  // Current indenation level.
    return crvic_generate_c_nodes(n, nodes, indent, indent_step, f);
}

enum cgen_error crvic_generate_c_nodes(int n, const struct ast_node nodes[static const n],
                                       int indent, int indent_step, FILE *f) {
    assert(n >= 0);
    assert(indent >= 0 && indent_step >= 0);
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
    fprintf(f, "%*s", indent, "");
    enum cgen_error error = CGEN_OK;
    switch (decl->kind) {
    case AST_DECL_VAR_DECL:
        if (!fprintf(f, "int %s = 0;\n", decl->var_decl.name)) return CGEN_IO_ERROR;
        break;
    case AST_DECL_VAR_DEFN:
        if (!fprintf(f, "int %s = ", decl->var_defn.name)) return CGEN_IO_ERROR;
        if ((error = crvic_generate_c_expr(decl->var_defn.value, f))) return error;
        if (!fprintf(f, ";\n")) return CGEN_IO_ERROR;
        break;
    case AST_DECL_FUNC_DECL:
        if ((error = crvic_generate_c_func_header(decl->func_decl.sig, f))) return error;
        fprintf(f, ";\n");
        break;
    case AST_DECL_FUNC_DEFN:
        if ((error = crvic_generate_c_func_header(decl->func_defn.sig, f))) return error;
        if (!fprintf(f, "{\n")) return CGEN_IO_ERROR;
        assert(indent == 0 && "Cannot have nested function in C!");
        if ((error = crvic_generate_c_func_body(decl->func_defn.body_node_count,
                                                decl->func_defn.body,
                                                indent_step, f))) return error;
        if (!fprintf(f, "}\n")) return CGEN_IO_ERROR;
        break;
    }
    return CGEN_OK;
}

enum cgen_error crvic_generate_c_func_header(struct ast_func_sig *sig, FILE *f) {
    if (!fprintf(f, "%s %s(", crvic_get_c_type(sig->ret_type), sig->name)) return CGEN_IO_ERROR;
    if (sig->param_count >= 1) {
        if (!fprintf(f, "%s", crvic_get_c_type(sig->params[0]))) return CGEN_IO_ERROR;
    }
    for (int i = 1; i < sig->param_count; ++i) {
        if (!fprintf(f, ", %s", crvic_get_c_type(sig->params[i]))) return CGEN_IO_ERROR;
    }
    if (!fprintf(f, ")")) return CGEN_IO_ERROR;
    return CGEN_OK;
}

enum cgen_error crvic_generate_c_func_body(int n, const struct ast_node nodes[static const n],
                                           int indent_step, FILE *f) {
    if (!fprintf(f, " {\n")) return CGEN_IO_ERROR;
    int indent = indent_step;
    enum cgen_error error = crvic_generate_c_nodes(n, nodes, indent, indent_step, f);
    if (error) return error;
    if (!fprintf(f, "%*s}\n", indent, "")) return CGEN_IO_ERROR;
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
    case AST_EXPR_GET:
        if (!fprintf(f, "%s", expr->get.target)) return CGEN_IO_ERROR;
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
