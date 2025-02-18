#ifndef CRVIC_FUNCTION_H
#define CRVIC_FUNCTION_H

#include "lexel.h"       // struct lxl_string_view.

#include "crvic_type.h"  // struct type_decl_list, TypeID.

enum func_link_kind {
    FUNC_EXTERNAL,  // A function defined externally (possibly in a different language, like C).
    FUNC_INTERNAL,  // A function defined within this source file.
};

struct func_sig {
    struct lxl_string_view name;
    TypeID ret_type;
    int arity;
    struct type_decl_list params;
};

#endif
