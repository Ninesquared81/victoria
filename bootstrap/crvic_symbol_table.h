#ifndef CRVIC_SYMBOL_TABLE_H
#define CRVIC_SYMBOL_TABLE_H

#include "crvic_function.h"

enum symbol_kind {
    SYMBOL_FUNC,       // Function symbol.
    SYMBOL_VAR,        // Variable symbol.
};

struct symbol_func {
    struct func_sig *sig;
    enum func_link_kind kind;
    struct ast_list *body;
};

struct symbol_var {
    TypeID type;
};

struct symbol {
    enum symbol_kind kind;
    union {
        struct symbol_func func;
        struct symbol_var var;
    };
};

struct st_key {
    struct lxl_string_view sv;
    uint32_t hash;
};

struct st_slot {
    struct st_slot *next;
    struct st_key key;
    struct symbol symbol;
};

struct symbol_table {
    size_t capacity;
    struct st_slot **slots;
    struct region *node_pool;
};

struct st_key st_key_of(struct lxl_string_view sv);

struct st_slot **find_slot(struct symbol_table *symbols, struct st_key key);
bool insert_symbol(struct symbol_table *symbols, struct st_key key, struct symbol symbol);
struct symbol *lookup_symbol(struct symbol_table *symbols, struct st_key key);

#endif
