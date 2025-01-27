#include <stdlib.h>

#include "lexel.h"

#include "region.h"
#include "ubiqs.h"

#include "crvic_ast.h"
#include "crvic_frontend.h"

#define NOT_SUPPORTED_YET(token)                                        \
    do {                                                                \
        fprintf(stderr, "%s token not supported yet\n", token_type_string(token)); \
        abort();                                                        \
    } while (0)

struct ast_list parse(struct region *region) {
    struct ast_list nodes = {.allocator = STDLIB_ALLOCATOR_ARD};
    struct lxl_token current_token = next_token();
    for (;;) {
        struct ast_node current_node = {0};
        DA_APPEND(&nodes, current_node);
    }
    return nodes;
}
