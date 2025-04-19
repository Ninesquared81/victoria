#include <assert.h>
#include <inttypes.h>
#include <string.h>

#include "crvic_backend.h"
#include "crvic_function.h"

enum cgen_error crvic_generate_c_file(struct ast_list nodes, struct string_buffer *sb) {
    int indent_step = 4;  // Number of spaces to indent by for each indent/dedent.
    int indent = 0;  // Current indenation level.
    sb_add_string(sb, "#include <stdint.h>  // Fixed-width types.\n");
    return crvic_generate_c_nodes(nodes, indent, indent_step, sb);
}

enum cgen_error crvic_generate_c_nodes(struct ast_list nodes, int indent, int indent_step,
                                       struct string_buffer *sb) {
    assert(indent >= 0 && indent_step >= 0);
    enum cgen_error error = CGEN_OK;
    for (int i = 0; i < nodes.count; ++i) {
        struct ast_node node = nodes.items[i];
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
    sb_add_formatted(sb, "%*s", indent, "");  // Prepend indentation.
    enum cgen_error error = CGEN_OK;
    switch (stmt->kind) {
    case AST_STMT_DECL:
        error = crvic_generate_c_decl(stmt->decl.decl, indent - indent_step, indent_step, sb);
        break;
    case AST_STMT_EXPR:
        error = crvic_generate_c_expr(stmt->expr.expr, sb);
        sb_add_string(sb, ";\n");
        break;
    case AST_STMT_IF:
        sb_add_string(sb, "if (");
        if ((error = crvic_generate_c_expr(stmt->if_.cond, sb))) return error;
        sb_add_string(sb, ") {\n");
        if ((error = crvic_generate_c_nodes(
                 stmt->if_.then_clause, indent + indent_step, indent_step, sb))) return error;
        sb_add_formatted(sb, "%*s}\n", indent, "");
        if (stmt->if_.else_clause.count == 0) break;  // No else clause.
        sb_add_formatted(sb, "%*selse {\n", indent, "");
        if ((error = crvic_generate_c_nodes(
                 stmt->if_.else_clause, indent + indent_step, indent_step, sb))) return error;
        sb_add_formatted(sb, "%*s}\n", indent, "");
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
        sb_add_formatted(sb, "int "LXL_SV_FMT_SPEC" = 0;\n", LXL_SV_FMT_ARG(decl->var_decl.name));
        break;
    case AST_DECL_VAR_DEFN:
        sb_add_formatted(sb, "int "LXL_SV_FMT_SPEC" = ", LXL_SV_FMT_ARG(decl->var_defn.name));
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
        if ((error = crvic_generate_c_func_body(decl->func_defn.body, indent_step, sb))) return error;
        if (lxl_sv_equal(decl->func_defn.sig->name, LXL_SV_FROM_STRLIT("main"))) {
            sb_add_formatted(sb, "%*sreturn 0;\n", indent_step, "");
        }
        sb_add_string(sb, "}\n");
        break;
    case AST_DECL_TYPE_DEFN:
        /* No code generation. (?) */
        break;
    }
    return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_expr(struct ast_expr *expr, struct string_buffer *sb) {
    enum cgen_error error = CGEN_OK;
    switch (expr->kind) {
    case AST_EXPR_ASSIGN:
        sb_add_string(sb, "(");
        sb_add_formatted(sb, ""LXL_SV_FMT_SPEC" = ", LXL_SV_FMT_ARG(expr->assign.target));
        if ((error - crvic_generate_c_expr(expr->assign.value, sb))) return error;
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_BINARY:
        sb_add_string(sb, "(");  // Brackets to ensure precedence is preserved.
        if ((error = crvic_generate_c_expr(expr->binary.lhs, sb))) return error;
        sb_add_formatted(sb, " %s ", crvic_get_c_op(expr->binary.op));
        if ((error = crvic_generate_c_expr(expr->binary.rhs, sb))) return error;
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_CALL:
        sb_add_formatted(sb, ""LXL_SV_FMT_SPEC"(", LXL_SV_FMT_ARG(expr->call.callee_name));
        if ((error = crvic_generate_c_expr_sep_list(expr->call.args, ", ", sb))) return error;
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_CONVERT:
        if (expr->convert.kind == CONVERT_AS) {
            // 'as' conversion.
            sb_add_formatted(sb, "((union {%s value; %s as;}){.value = ",
                             crvic_get_c_type(expr->convert.operand->type),
                             crvic_get_c_type(expr->convert.target_type));
            if ((error = crvic_generate_c_expr(expr->convert.operand, sb))) return error;
            sb_add_string(sb, "}.as)");
        }
        else {
            // 'to' conversion.
            sb_add_formatted(sb, "((%s)", crvic_get_c_type(expr->convert.target_type));
            if ((error = crvic_generate_c_expr(expr->convert.operand, sb))) return error;
            sb_add_string(sb, ")");
        }
        break;
    case AST_EXPR_GET:
        sb_add_formatted(sb, ""LXL_SV_FMT_SPEC"", LXL_SV_FMT_ARG(expr->get.target));
        break;
    case AST_EXPR_INTEGER:
        sb_add_formatted(sb, "%"PRId64, expr->integer.value);
        break;
    case AST_EXPR_WHEN:
        sb_add_string(sb, "((");  // Outer '(', Conditon '('.
        if ((error = crvic_generate_c_expr(expr->when.cond, sb))) return error;
        sb_add_string(sb, ") ? (");  // Condition ')', Then '('.
        if ((error = crvic_generate_c_expr(expr->when.then_expr, sb))) return error;
        sb_add_string(sb, ") : (");  // Then ')', Else '('.
        if ((error = crvic_generate_c_expr(expr->when.else_expr, sb))) return error;
        sb_add_string(sb, "))");   // Else ')', Outer ')'.
        break;
    }
    return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_main_header(struct func_sig *sig, struct string_buffer *sb) {
    assert(lxl_sv_equal(sig->name, LXL_SV_FROM_STRLIT("main")));
    assert(sig->params.count == 0 && "Only allow parameter-less version for now.");
    sb_add_string(sb, "int main(void)");
        return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_func_header(struct func_sig *sig, struct string_buffer *sb) {
    if (lxl_sv_equal(sig->name, LXL_SV_FROM_STRLIT("main"))) return crvic_generate_c_main_header(sig, sb);
    sb_add_formatted(sb, "%s "LXL_SV_FMT_SPEC"(",
                     crvic_get_c_type(sig->ret_type), LXL_SV_FMT_ARG(sig->name));
    if (sig->params.count >= 1) {
        struct type_decl param = sig->params.items[0];
        sb_add_formatted(sb, "%s "LXL_SV_FMT_SPEC"",
                         crvic_get_c_type(param.type), LXL_SV_FMT_ARG(param.name));
    }
    else {
        // No parameters; use `void` in parameter list for strict adherence to (pre-C23) C standard.
        sb_add_string(sb, "void");
    }
    for (int i = 1; i < sig->params.count; ++i) {
        struct type_decl param = sig->params.items[i];
        sb_add_formatted(sb, ", %s "LXL_SV_FMT_SPEC"",
                         crvic_get_c_type(param.type), LXL_SV_FMT_ARG(param.name));
    }
    sb_add_string(sb, ")");
    return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_func_body(struct ast_list nodes, int indent_step, struct string_buffer *sb) {
    int indent = indent_step;
    return crvic_generate_c_nodes(nodes, indent, indent_step, sb);
}

enum cgen_error crvic_generate_c_expr_sep_list(struct ast_list nodes, const char *sep,
                                               struct string_buffer *sb) {
    if (nodes.count <= 0) return CGEN_OK;
    enum cgen_error error = CGEN_OK;
    struct ast_node node = nodes.items[0];
    if (node.kind != AST_EXPR) return CGEN_UNEXPECTED_STMT;
    if ((error = crvic_generate_c_expr(node.expr, sb))) return error;
    for (int i = 1; i < nodes.count; ++i) {
        node = nodes.items[i];
        if (node.kind != AST_EXPR) return CGEN_UNEXPECTED_STMT;
        sb_add_string(sb, sep);
        if ((error = crvic_generate_c_expr(node.expr, sb))) return error;
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
