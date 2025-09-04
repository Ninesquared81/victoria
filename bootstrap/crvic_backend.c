#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "crvic_backend.h"
#include "crvic_resources.h"
#include "crvic_type.h"

struct module *module = NULL;

enum cgen_error crvic_generate_c_file(struct package *package, struct string_buffer *sb) {
    int indent_step = 4;  // Number of spaces to indent by for each indent/dedent.
    int indent = 0;  // Current indentation level.
    sb_add_string(sb,
                  "#include <stdbool.h> // bool, true, false.\n"
                  "#include <stddef.h>  // NULL.\n"
                  "#include <stdint.h>  // Fixed-width types.\n"
                  "#include <string.h>  // strlen().\n"
        );
    enum cgen_error ret = CGEN_OK;
    DO_OR_ERROR(ret, crvic_generate_c_types(indent_step, sb));
    FOR_DLLIST (, module, &package->modules) {
        FOR_DLLIST (struct function *, func, &module->funcs) {
            // Forward-declare all functions.
            DO_OR_ERROR(ret, crvic_generate_c_func_header(&func->decl, sb));
            sb_add_string(sb, ";\n");
        }
    }
    FOR_DLLIST (, module, &package->modules) {
        DO_OR_ERROR(ret, crvic_generate_c_nodes(module->decls, indent, indent_step, sb));
    }
    return CGEN_OK;
}

enum cgen_error crvic_generate_c_nodes(struct ast_list nodes, int indent, int indent_step,
                                       struct string_buffer *sb) {
    assert(indent >= 0 && indent_step >= 0);
    enum cgen_error error = CGEN_OK;
    FOR_DLLIST (struct ast_node *, node, &nodes) {
        switch (node->kind) {
        case AST_EXPR:
            return CGEN_UNEXPECTED_EXPR;
        case AST_STMT:
            error = crvic_generate_c_stmt(&node->stmt, indent, indent_step, sb);
            break;
        case AST_DECL:
            error = crvic_generate_c_decl(&node->decl, indent, indent_step, sb);
            break;
        case AST_TYPE:
            return CGEN_UNEXPECTED_TYPE;
        }
        if (error) return error;
    }
    return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_stmt(struct ast_stmt *stmt, int indent, int indent_step,
                                      struct string_buffer *sb) {
    if (stmt->kind != AST_STMT_DECL) {
        sb_add_formatted(sb, "%*s", indent, "");  // Prepend indentation.
    }
    enum cgen_error error = CGEN_OK;
    switch (stmt->kind) {
    case AST_STMT_BLOCK:
        sb_add_string(sb, "{\n");
        error = crvic_generate_c_nodes(stmt->block.stmts, indent + indent_step, indent_step, sb);
        sb_add_formatted(sb, "%*s}\n", indent, "");
        break;
    case AST_STMT_DECL:
        error = crvic_generate_c_decl(stmt->decl.decl, indent, indent_step, sb);
        break;
    case AST_STMT_EXPR:
        error = crvic_generate_c_expr(stmt->expr.expr, sb);
        sb_add_string(sb, ";\n");
        break;
    case AST_STMT_IF:
        sb_add_string(sb, "if (");
        DO_OR_ERROR(error, crvic_generate_c_expr(stmt->if_.cond, sb));
        sb_add_string(sb, ") {\n");
        DO_OR_ERROR(error, crvic_generate_c_nodes(stmt->if_.then_clause.stmts,
                                                  indent + indent_step, indent_step, sb));
        sb_add_formatted(sb, "%*s}\n", indent, "");
        if (!stmt->if_.else_clause) break;  // No else clause.
        sb_add_formatted(sb, "%*selse {\n", indent, "");
        DO_OR_ERROR(error, crvic_generate_c_stmt(stmt->if_.else_clause,
                                                 indent + indent_step, indent_step, sb));
        sb_add_formatted(sb, "%*s}\n", indent, "");
        break;
    case AST_STMT_RETURN:
        sb_add_string(sb, "return");
        if (stmt->return_.expr) {
            sb_add_string(sb, " ");
            DO_OR_ERROR(error, crvic_generate_c_expr(stmt->return_.expr, sb));
        }
        sb_add_string(sb, ";\n");
        break;
    case AST_STMT_WHILE:
        sb_add_string(sb, "while (");
        DO_OR_ERROR(error, crvic_generate_c_expr(stmt->while_.cond, sb));
        sb_add_string(sb, ") {\n");
        DO_OR_ERROR(error, crvic_generate_c_nodes(stmt->while_.body.stmts,
                                                  indent + indent_step, indent_step, sb));
        sb_add_formatted(sb, "%*s}\n", indent, "");
    }
    return error;
}

enum cgen_error crvic_generate_c_decl(struct ast_decl *decl, int indent, int indent_step,
                                      struct string_buffer *sb) {
    if (decl->kind == AST_DECL_TYPE_DEFN) return CGEN_OK;  // No codegen for type definitions.
    if (decl->kind == AST_DECL_VAR && decl->var.kind == AST_VAR_CONST) return CGEN_OK;
    sb_add_formatted(sb, "%*s", indent, "");
    enum cgen_error error = CGEN_OK;
    switch (decl->kind) {
    case AST_DECL_VAR:
        assert(decl->var.kind != AST_VAR_CONST);
        sb_add_formatted(sb, "%s ", crvic_get_c_type(decl->var.type->resolved_type));
        DO_OR_ERROR(error, crvic_generate_c_identifier(module, decl->var.name, true, sb));
        sb_add_string(sb, " = ");
        if (decl->var.value) {
            // Initial value.
            DO_OR_ERROR(error, crvic_generate_c_expr(decl->var.value, sb));
        }
        else {
            // Implicit zero-initialisation.
            assert(decl->var.type);
            assert(decl->var.type->resolved_type);
            sb_add_string(sb, crvic_get_c_zero_value(decl->var.type->resolved_type));
        }
        sb_add_string(sb, ";\n");
        break;
    case AST_DECL_FUNC:
        if (decl->func.decl_kind == AST_FUNC_DECL) break;  // Declaration; function already forward-declared.
        // Definition.
        assert(decl->func.decl_kind == AST_FUNC_DEFN);
        DO_OR_ERROR(error, crvic_generate_c_func_header(&decl->func, sb));
        struct lxl_string_view name = decl->func.sig->resolved_sig.name;
        bool is_main = lxl_sv_equal(name, LXL_SV_FROM_STRLIT("main"));
        sb_add_string(sb, " {\n");
        assert(indent == 0 && "Cannot have nested function in C!");
        if (is_main && decl->func.sig->resolved_sig.arity > 0) {
            // Create local variable with name of "args" paramater (usually "args" itself).
            struct type_decl args = decl->func.sig->resolved_sig.params.items[0];
            // TODO: add alternative strategies for compilers/platforms that don't support VLAs (alloca()?).
            sb_add_formatted(sb, "%*sVIC_STRING VIC_args_data__[argc];\n", indent_step, "");
            sb_add_formatted(sb,
                             "%*sfor (int i = 0; i < argc; ++i) {\n"
                             "%*sVIC_args_data__[i] = (VIC_STRING){argv[i], strlen(argv[i])};\n"
                             "%*s};\n",
                             indent_step, "", indent_step * 2, "", indent_step, "");
            sb_add_formatted(sb, "%*s%s ", indent_step, "", crvic_get_c_type(args.type));
            DO_OR_ERROR(error, crvic_generate_c_identifier(module, args.name, false, sb));
            sb_add_string(sb, " = {VIC_args_data__, argc};\n");
        }
        DO_OR_ERROR(error, crvic_generate_c_func_body(decl->func.body.stmts, indent_step, sb));
        if (is_main) {
            sb_add_formatted(sb, "%*sreturn 0;\n", indent_step, "");
        }
        sb_add_string(sb, "}\n");
        break;
    case AST_DECL_EXTERNAL_BLOCK:
        FOR_DLLIST (struct ast_node *, node, &decl->external_block.decls) {
            assert(node->kind == AST_DECL);
            DO_OR_ERROR(error, crvic_generate_c_decl(&node->decl, indent, indent_step, sb));
        }
        break;
    case AST_DECL_TYPE_DEFN:
        UNREACHABLE();
        break;
    case AST_DECL_PACKAGE:
        // Do nothing.
        break;
    }
    return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_expr(struct ast_expr *expr, struct string_buffer *sb) {
    enum cgen_error error = CGEN_OK;
    switch (expr->kind) {
    case AST_EXPR_ADDRESS_OF:
        sb_add_string(sb, "&(");
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->address_of.target, sb));
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_ASSIGN:
        sb_add_string(sb, "(");
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->assign.target, sb));
        sb_add_string(sb, " = ");
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->assign.value, sb));
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_BINARY:
        sb_add_string(sb, "(");  // Brackets to ensure precedence is preserved.
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->binary.lhs, sb));
        sb_add_formatted(sb, " %s ", crvic_get_c_op(expr->binary.op));
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->binary.rhs, sb));
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_BOOLEAN:
        sb_add_string(sb, ((expr->boolean.value) ? "true" : "false"));
        break;
    case AST_EXPR_CALL:
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->call.callee, sb));
        sb_add_string(sb, "(");
        DO_OR_ERROR(error, crvic_generate_c_expr_sep_list(expr->call.args, ", ", sb));
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_COMPARE:
        sb_add_string(sb, "(");  // Brackets to ensure precedence is preserved.
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->compare.lhs, sb));
        sb_add_formatted(sb, " %s ", crvic_get_c_cmp(expr->compare.op));
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->compare.rhs, sb));
        sb_add_string(sb, ")");
        break;
    case AST_EXPR_CONSTRUCTOR:
        sb_add_formatted(sb, "((%s) {", crvic_get_c_type(expr->type));
        DO_OR_ERROR(error, crvic_generate_c_expr_sep_list(expr->constructor.init_list, ", ", sb));
        sb_add_string(sb, "})");
        break;
    case AST_EXPR_CONVERT:
        // TODO: extract this to a function(s).
        assert(expr->type == expr->convert.target_type->resolved_type);
        if (expr->convert.kind == CONVERT_AS) {
            // 'as' conversion.
            sb_add_formatted(sb, "((union {%s value; %s as;}){.value = ",
                             crvic_get_c_type(expr->convert.operand->type),
                             crvic_get_c_type(expr->convert.target_type->resolved_type));
            DO_OR_ERROR(error, crvic_generate_c_expr(expr->convert.operand, sb));
            sb_add_string(sb, "}.as)");
        }
        else {
            // 'to' conversion.
            sb_add_string(sb, "(");
            bool str2cstr = expr->convert.operand->type == TYPE_STRING && expr->type == TYPE_C_STRING;
            if (!str2cstr) {
                sb_add_formatted(sb, "(%s)", crvic_get_c_type(expr->convert.target_type->resolved_type));
            }
            DO_OR_ERROR(error, crvic_generate_c_expr(expr->convert.operand, sb));
            if (type_is_kind(expr->convert.operand->type, KIND_ARRAY)) {
                sb_add_string(sb, "._");
            }
            else if (str2cstr) {
                sb_add_string(sb, ".ptr");
            }
            sb_add_string(sb, ")");
        }
        break;
    case AST_EXPR_COUNT_OF: {
        struct type_info *info = get_type(expr->count_of.operand->type);
        assert(info->kind == KIND_SLICE || info->id == TYPE_STRING);
        sb_add_string(sb, "((");
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->count_of.operand, sb));
        sb_add_string(sb, ").len)");
    } break;
    case AST_EXPR_DEREF:
        sb_add_string(sb, "(*(");
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->deref.pointer, sb));
        sb_add_string(sb, "))");
        break;
    case AST_EXPR_FIELD:
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->field.target, sb));
        sb_add_formatted(sb, "."LXL_SV_FMT_SPEC"", LXL_SV_FMT_ARG(expr->field.name));
        break;
    case AST_EXPR_FUNC_EXPR: {
        struct lxl_string_view name = expr->func_expr.name;
        enum func_link_kind link_kind = expr->func_expr.symbol->decl.link_kind;
        DO_OR_ERROR(error, crvic_generate_c_identifier(module, name, link_kind == FUNC_INTERNAL, sb));
    } break;
    case AST_EXPR_IDENTIFIER:
        DO_OR_ERROR(error, crvic_generate_c_identifier(module, expr->identifier.name,
                                                       expr->identifier.symbol->kind != SYMBOL_PARAM, sb));
        break;
    case AST_EXPR_INDEX: {
        // TODO: bounds checking.
        sb_add_string(sb, "(");
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->index.array, sb));
        struct type_info *array_info = get_type(expr->index.array->type);
        if (array_info->kind == KIND_ARRAY) {
            sb_add_string(sb, "._[");  // N.B. arrays are wrapped in a struct.
        }
        else if (array_info->kind == KIND_SLICE) {
            sb_add_string(sb, ".ptr[");
        }
        else {
            assert(array_info->kind == KIND_POINTER && array_info->pointer.kind == POINTER_ARRAY_LIKE);
            sb_add_string(sb, "[");
        }
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->index.index, sb));
        sb_add_string(sb, "])");
    } break;
    case AST_EXPR_INTEGER:
        sb_add_formatted(sb, "%"PRId64, expr->integer.value);
        break;
    case AST_EXPR_LOGICAL:
        assert(expr->type == TYPE_BOOL);
        sb_add_string(sb, "((");
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->logical.lhs, sb));
        sb_add_formatted(sb, ")%s(", ((expr->logical.op == AST_LOG_AND) ? "&&" : "||"));
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->logical.rhs, sb));
        sb_add_string(sb, "))");
        break;
    case AST_EXPR_MAGIC_FUNC:
        UNREACHABLE();
        break;
    case AST_EXPR_MODULE_IDENTIFIER: {
        struct module *old_module = module;
        module = expr->module_identifier.module;
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->module_identifier.identifier, sb));
        module = old_module;
    } break;
    case AST_EXPR_NOT:
        sb_add_string(sb, "(!(");
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->not.operand, sb));
        sb_add_string(sb, "))");
        break;
    case AST_EXPR_NULL:
        sb_add_string(sb, "NULL");
        break;
    case AST_EXPR_STRING:
        sb_add_string(sb, "((VIC_STRING) {\"");
        for (size_t i = 0; i < expr->string.value.length; ++i) {
            char c = expr->string.value.start[i];
            if (isprint(c) && c != '"' && c != '\\') {
                // Printable.
                sb_add_formatted(sb, "%c", c);
            }
            else {
                // Escaped.
                sb_add_formatted(sb, "\\x%x", c);
            }
        }
        sb_add_formatted(sb, "\", %zd})", expr->string.value.length);
        break;
    case AST_EXPR_TYPE_EXPR:
        return CGEN_UNEXPECTED_TYPE;
    case AST_EXPR_WHEN:
        sb_add_string(sb, "((");  // Outer '(', Conditon '('.
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->when.cond, sb));
        sb_add_string(sb, ") ? (");  // Condition ')', Then '('.
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->when.then_expr, sb));
        sb_add_string(sb, ") : (");  // Then ')', Else '('.
        DO_OR_ERROR(error, crvic_generate_c_expr(expr->when.else_expr, sb));
        sb_add_string(sb, "))");   // Else ')', Outer ')'.
        break;
    }
    return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_main_header(struct func_sig *sig, struct string_buffer *sb) {
    assert(lxl_sv_equal(sig->name, LXL_SV_FROM_STRLIT("main")));
    if (sig->arity == 0) {
        sb_add_string(sb, "int main(void)");
    }
    else {
        struct type_info *info = get_type(sig->params.items[0].type);
        assert(info->kind == KIND_SLICE && info->slice.dest_type == TYPE_STRING);
        sb_add_string(sb, "int main(int argc, char *argv[])");
    }
    return (!sb->had_error) ? CGEN_OK : CGEN_IO_ERROR;
}

enum cgen_error crvic_generate_c_func_header(struct ast_decl_func *func, struct string_buffer *sb) {
    struct func_sig *sig = &func->sig->resolved_sig;
    if (lxl_sv_equal(sig->name, LXL_SV_FROM_STRLIT("main"))) return crvic_generate_c_main_header(sig, sb);
    sb_add_formatted(sb, "%s ", crvic_get_c_type(sig->ret_type));
    enum cgen_error error = CGEN_OK;
    DO_OR_ERROR(error, crvic_generate_c_identifier(module, sig->name, func->link_kind == FUNC_INTERNAL, sb));
    sb_add_string(sb, "(");
    if (sig->params.count >= 1) {
        struct type_decl param = sig->params.items[0];
        sb_add_formatted(sb, "%s ", crvic_get_c_type(param.type));
        DO_OR_ERROR(error, crvic_generate_c_identifier(module, param.name, false, sb));
    }
    else {
        // No parameters; use `void` in parameter list for strict adherence to (pre-C23) C standard.
        sb_add_string(sb, "void");
    }
    for (int i = 1; i < sig->params.count; ++i) {
        struct type_decl param = sig->params.items[i];
        sb_add_formatted(sb, ", %s ", crvic_get_c_type(param.type));
        DO_OR_ERROR(error, crvic_generate_c_identifier(module, param.name, false, sb));
    }
    if (sig->c_variadic) {
        sb_add_string(sb, ", ...");
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
    if (DLLIST_IS_EMPTY(&nodes)) return CGEN_OK;
    enum cgen_error error = CGEN_OK;
    struct ast_node *node = nodes.head;
    if (node->kind != AST_EXPR) return CGEN_UNEXPECTED_STMT;
    DO_OR_ERROR(error, crvic_generate_c_expr(&node->expr, sb));
    while ((node = node->next) != NULL) {
        if (node->kind != AST_EXPR) return CGEN_UNEXPECTED_STMT;
        sb_add_string(sb, sep);
        DO_OR_ERROR(error, crvic_generate_c_expr(&node->expr, sb));
    }
    return CGEN_OK;
}

enum cgen_error crvic_generate_c_types(int indent_step, struct string_buffer *sb) {
    const char *vic_int_string = crvic_get_c_type(TYPE_INT);
    // Builtin typdefs.
    sb_add_formatted(sb, "typedef struct {const char *ptr; %s len;} VIC_STRING;\n", vic_int_string);
    // User-defined types.
    struct iterator it = get_type_iterator();
    FOR_ITER(struct type_info *, info, &it) {
        enum cgen_error ret = CGEN_OK;
        const char *this_type_name = crvic_get_c_type(info->id);
        if (info->kind == KIND_RECORD) {
            sb_add_formatted(sb, "typedef struct %s %s;\n", this_type_name, this_type_name);
            DO_OR_ERROR(ret, crvic_generate_c_record_defn(*info, indent_step, this_type_name, sb));
        }
        else if (info->kind == KIND_ARRAY) {
            sb_add_formatted(sb, "typedef struct %s %s;\nstruct %s {%s _[%d];};\n",
                             this_type_name, this_type_name, this_type_name,
                             crvic_get_c_type(info->array.dest_type),
                             info->array.count);
        }
        else if (info->kind == KIND_FUNCTION) {
            struct func_sig *sig = info->function.sig;
            sb_add_formatted(sb, "typedef %s %s(", crvic_get_c_type(sig->ret_type), this_type_name);
            if (sig->params.count >= 1) {
                sb_add_formatted(sb, "%s", crvic_get_c_type(sig->params.items[0].type));
            }
            for (int i = 1; i < sig->params.count; ++i) {
                sb_add_formatted(sb, ", %s", crvic_get_c_type(sig->params.items[i].type));
            }
            sb_add_string(sb, ");\n");
        }
        else if (info->kind == KIND_SLICE) {
            sb_add_formatted(sb, "typedef struct %s %s;\nstruct %s {%s *ptr; %s len;};\n",
                             this_type_name, this_type_name, this_type_name,
                             crvic_get_c_type(info->slice.dest_type), vic_int_string);
        }
    }
    return CGEN_OK;
}

enum cgen_error crvic_generate_c_record_defn(struct type_info info, int indent_step,
                                             const char *type_name, struct string_buffer *sb) {
    assert(info.kind == KIND_RECORD);
    sb_add_formatted(sb, "struct %s {\n", type_name);
    for (int i = 0; i < info.record.fields.count; ++i) {
        struct type_decl field = info.record.fields.items[i];
        sb_add_formatted(sb, "%*s%s "LXL_SV_FMT_SPEC";\n", indent_step, "",
                         crvic_get_c_type(field.type),
                         LXL_SV_FMT_ARG(field.name));
    }
    sb_add_string(sb, "};\n");
    return CGEN_OK;
}

enum cgen_error crvic_generate_c_identifier(struct module *module, struct lxl_string_view name,
                                            bool should_mangle, struct string_buffer *sb) {
    if (should_mangle) {
        sb_add_formatted(sb, "VIC_IDENT__"LXL_SV_FMT_SPEC"__"LXL_SV_FMT_SPEC"__",
                         LXL_SV_FMT_ARG(module->name), LXL_SV_FMT_ARG(name));
    }
    else {
        sb_add_formatted(sb, ""LXL_SV_FMT_SPEC"", LXL_SV_FMT_ARG(name));
    }
    return CGEN_OK;
}

const char *crvic_get_c_op(enum ast_bin_op_kind op) {
    switch (op) {
    case AST_BIN_ADD:
        return "+";
    case AST_BIN_MUL:
        return "*";
    case AST_BIN_SUB:
        return "-";
    }
    assert(0 && "Unreachable");
    return NULL;
}

const char *crvic_get_c_cmp(enum ast_cmp_op_kind op) {
    switch (op) {
    case AST_CMP_EQ:    return "==";
    case AST_CMP_NEQ:   return "!=";
    case AST_CMP_LT:    return "<";
    case AST_CMP_LE:    return "<=";
    case AST_CMP_GT:    return ">";
    case AST_CMP_GE:    return ">=";
    }
    UNREACHABLE();
    return NULL;
}

const char *crvic_get_c_type(TypeID type) {
    if (type < TYPE_PRIMITIVE_COUNT) {
        switch ((enum type_primitive)type) {
        case TYPE_NO_TYPE:
        case TYPE_TYPE_EXPR:
        case TYPE_ABSURD:
        case TYPE_UNIT:
            return "void";
        case TYPE_NULLPTR_TYPE:
            return "void*";
        case TYPE_CONST_BOOL: return "bool";
        case TYPE_CONST_INT: return crvic_get_c_type(TYPE_INT);
        case TYPE_BOOL: return "bool";
        case TYPE_I8: return "int8_t";
        case TYPE_I16: return "int16_t";
        case TYPE_I32: return "int32_t";
        case TYPE_I64: return "int64_t";
        case TYPE_U8: return "uint8_t";
        case TYPE_U16: return "uint16_t";
        case TYPE_U32: return "uint32_t";
        case TYPE_U64: return "uint64_t";
        case TYPE_C_STRING: return "const char*";
        case TYPE_STRING: return "VIC_STRING";
        case TYPE_PRIMITIVE_COUNT:
            UNREACHABLE();
            return NULL;
        }
    }
    struct type_info *info = get_type(type);
    assert(info);
    switch (info->kind) {
    case KIND_UNION:
    case KIND_SLICE:
    case KIND_ARRAY:
    case KIND_FUNCTION:
    case KIND_RECORD: {
        int count = snprintf(NULL, 0, "VICTYPE_%d__", info->id);
        // +1 for null terminator.
        char *name = ALLOCATE(perm, count + 1);
        snprintf(name, count + 1, "VICTYPE_%d__", info->id);
        return name;
    }
    case KIND_ENUM:
        return crvic_get_c_type(info->enum_.underlying_type);
    case KIND_POINTER:
        return crvic_get_c_pointer(*info);
    case KIND_NO_KIND:
    case KIND_PRIMITIVE:
        UNREACHABLE();
    }
    return NULL;
}

const char *crvic_get_c_pointer(struct type_info info) {
    assert(info.kind == KIND_POINTER);
    // NOTE: complex types are typdef'd, so we /should/ be able to use them as a normal type name.
    const char *dest_type_string = crvic_get_c_type(info.pointer.dest_type);
    const char *qualifier = (info.pointer.rw == RW_READ_ONLY) ? "const " : "";
    if (type_is_kind(info.pointer.dest_type, KIND_FUNCTION)) {
        // We cannot have qualifiers on function types.
        qualifier = "";
    }
    size_t count = snprintf(NULL, 0, "%s%s*", qualifier, dest_type_string);
    char *name = ALLOCATE(perm, count + 1);
    snprintf(name, count + 1, "%s%s*", qualifier, dest_type_string);
    return name;
}

const char *crvic_get_c_zero_value(TypeID type) {
    struct type_info *info = get_type(type);
    assert(info);
    switch (info->kind) {
    case KIND_PRIMITIVE:
        if (is_integer_type(type)) return "0";
        if (type == TYPE_NULLPTR_TYPE) return "NULL";
        if (type == TYPE_C_STRING) return "\"\"";
        break;
    case KIND_ENUM:
        TODO("enum zero value");
        break;
    case KIND_UNION:
    case KIND_RECORD:
    case KIND_ARRAY:
    case KIND_SLICE:
        return "{0}";
    case KIND_POINTER:
        return "NULL";
        /* Unreachable.*/
    case KIND_FUNCTION:
        // You cannot create a variable with function type.
    case KIND_NO_KIND:
        UNREACHABLE();
    }
    return "THIS_TYPE_HAS_NO_VALID_ZERO_VALUE";
}
