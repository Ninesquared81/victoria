#ifndef CRVIC_SYMBOL_TABLE_H
#define CRVIC_SYMBOL_TABLE_H

#include "crvic_ast.h"
#include "crvic_function.h"

enum symbol_kind {
    SYMBOL_FUNC,       // Function symbol.
    SYMBOL_TYPE_ALIAS, // Type alias symbol.
    SYMBOL_VAR,        // Variable symbol.
    SYMBOL_VAL,        // Immutable value symbol.
};

struct symbol_func {
    struct ast_decl func_decl;
    struct func_sig *sig;  // NOTE: points to `.func_decl.sig.resolved_sig`.
};

struct symbol_type_alias {
    struct ast_type type;  // Type not yet resolved for aliases.
};

struct symbol_var {
    TypeID type;  // Type already resolved for vars.
};

struct symbol_val {
    TypeID type;  // Type already resolved for vals.
};

struct symbol {
    enum symbol_kind kind;
    bool resolved;
    union {
        struct symbol_func func;
        struct symbol_type_alias type_alias;
        struct symbol_var var;
        struct symbol_val val;
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
};

struct st_key st_key_of(struct lxl_string_view sv);

struct st_slot **find_slot(struct symbol_table *symbols, struct st_key key);
bool insert_symbol(struct symbol_table *symbols, struct st_key key, struct symbol symbol);
struct symbol *lookup_symbol(struct symbol_table *symbols, struct st_key key);

#endif
