#ifndef CRVIC_PACKAGE_H
#define CRVIC_PACKAGE_H

#include "lexel.h"

#include "crvic_ast.h"
#include "crvic_symbol_table.h"

struct module {
    struct module *next, *prev;
    struct lxl_string_view name;
    struct symbol_table *globals;
    struct ast_list decls;
};

struct module_list {
    struct module *head, *tail;
    int count;
};

struct package {
    struct lxl_string_view name;
    struct module_list modules;
};

struct module *add_new_module(struct package *package, struct lxl_string_view name);

#endif
