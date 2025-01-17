#include <stdio.h>

#include "crvic_ast.h"
#include "crvic_backend.h"

int main(void) {
    const char *filename = "out.c";
    FILE *f = fopen(filename, "w");
    struct ast_node nodes[] = {
        {.kind = AST_STMT,
         .stmt = &(struct ast_stmt){
             .kind = AST_STMT_EXPR,
             .expr = &(struct ast_expr){
                 .kind = AST_EXPR_INTEGER,
                 .integer = {42}}}},
        {.kind = AST_STMT,
         .stmt = &(struct ast_stmt){
             .kind = AST_STMT_EXPR,
             .expr = &(struct ast_expr){
                 .kind = AST_EXPR_BINARY,
                 .binary = {
                     .lhs = &(struct ast_expr){
                         .kind = AST_EXPR_INTEGER,
                         .integer = {1}},
                     .rhs = &(struct ast_expr){
                         .kind = AST_EXPR_BINARY,
                         .binary = {
                             .lhs = &(struct ast_expr){
                                 .kind = AST_EXPR_INTEGER,
                                 .integer = {2}},
                             .rhs = &(struct ast_expr){
                                 .kind = AST_EXPR_INTEGER,
                                 .integer = {2}},
                             .op = "+"}},
                     .op = "+"}}}},
    };
    int node_count = sizeof nodes / sizeof nodes[0];
    enum cgen_error error = crvic_generate_c_file(node_count, nodes, f);
    if (!error) {
        fprintf(stderr, "All good!\n");
    }
    else {
        fprintf(stderr, "Something went wrong: %d\n", error);
        return 1;
    }
    return 0;
}
