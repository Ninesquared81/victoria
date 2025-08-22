#include "ubiqs.h"

#include "crvic_frontend.h"
#include "crvic_package.h"
#include "crvic_resources.h"

struct module *add_new_module(struct package *package, struct lxl_string_view name) {
    struct module *module = ALLOCATE(perm, sizeof *module);
    module->name = name;
    module->globals = new_globals();
    module->decls = (struct ast_list) {0};
    DLLIST_APPEND(&package->modules, module);
    return module;
}

struct module *find_module(struct package *package, struct lxl_string_view name) {
    FOR_DLLIST (struct module *, module, &package->modules) {
        if (lxl_sv_equal(name, module->name)) {
            return module;
        }
    }
    // Not found.
    return NULL;
}

struct function *add_new_function(struct module *module, struct ast_decl_func decl) {
    struct function *func = ALLOCATE(perm, sizeof *func);
    *func = (struct function) {.module = module, .decl = decl};
    DLLIST_APPEND(&module->funcs, func);
    return func;
}
