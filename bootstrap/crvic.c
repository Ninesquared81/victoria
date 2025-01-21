#include <stdlib.h>  // EXIT_SUCCESS, EXIT_FAILURE.
#include <stdio.h>

#include "crvic_ast.h"
#include "crvic_backend.h"
#include "crvic_type.h"

int main(void) {
    const char *filename = "out.c";
    struct ast_node nodes[] = {
        {.kind = AST_DECL,
         .decl = &(struct ast_decl) {
             .kind = AST_DECL_FUNC_DECL,
             .func_decl = {
                 .sig = &(struct ast_func_sig) {
                     .name = "add",
                     .ret_type = TYPE_INT,
                     .param_count = 2,
                     .params = (TypeID[]){TYPE_INT, TYPE_INT}},
                 .kind = AST_FUNC_INTERNAL}}},
        {.kind = AST_DECL,
         .decl = &(struct ast_decl) {
             .kind = AST_DECL_FUNC_DEFN,
             .func_defn = {
                 .sig = &(struct ast_func_sig) {
                     .name = "main",
                     .ret_type = TYPE_I32,
                     .param_count = 0,
                     .params = NULL},
                 .body_node_count = 2,
                 .body = (struct ast_node[]) {
                     {.kind = AST_DECL,
                      .decl = &(struct ast_decl) {
                          .kind = AST_DECL_VAR_DEFN,
                          .var_defn = {
                              .name = "x",
                              .value = &(struct ast_expr) {
                                  .kind = AST_EXPR_INTEGER,
                                  .integer = {-5}}}}},
                     {.kind = AST_STMT,
                      .stmt = &(struct ast_stmt) {
                          .kind = AST_STMT_EXPR,
                          .expr = &(struct ast_expr) {
                              .kind = AST_EXPR_BINARY,
                              .binary = {
                                  .lhs = &(struct ast_expr) {
                                      .kind = AST_EXPR_GET,
                                      .get = {"x"}},
                                  .rhs = &(struct ast_expr) {
                                      .kind = AST_EXPR_INTEGER,
                                      .integer = {42}},
                                  .op = AST_BIN_ADD}}}},
                 }  // Func body end.
             }}},
    };
    int node_count = sizeof nodes / sizeof nodes[0];
    static struct string_buffer sb = {0};
    enum cgen_error error = crvic_generate_c_file(node_count, nodes, &sb);
    if (error) {
        fprintf(stderr, "Something went wrong: %d\n", error);
        return EXIT_FAILURE;
    }
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        perror("fopen() failed");
        return EXIT_FAILURE;
    }
    int exit_code = EXIT_SUCCESS;
    printf("%zu:\n%s", sb.count, sb.buffer);
    if ((size_t)fprintf(f, "%s", sb.buffer) != sb.count) {
        perror("Failed to write output file");
        exit_code = EXIT_FAILURE;
    }
    if (fclose(f) == EOF) {
        fprintf(stderr, "fclose() failed!\n");
        exit_code = EXIT_FAILURE;
    }
    return exit_code;
}
