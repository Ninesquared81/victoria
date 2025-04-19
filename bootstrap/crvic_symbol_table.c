#include "lexel.h"

#include "region.h"

#include "crvic_resources.h"
#include "crvic_symbol_table.h"

static bool keys_equal(struct st_key a, struct st_key b) {
    return a.hash == b.hash && lxl_sv_equal(a.sv, b.sv);
}

struct st_key st_key_of(struct lxl_string_view sv) {
    return (struct st_key) {.sv = sv, .hash = hash_sv(sv)};
}

struct st_slot **find_slot(struct symbol_table *symbols, struct st_key key) {
    int index = key.hash % symbols->capacity;
    static_assert(offsetof(struct st_slot, next) == 0, ".next field assumed to be at start of st_slot.");
    struct st_slot **slot = &symbols->slots[index];
    for (; *slot != NULL; slot = (struct st_slot **)*slot) {
        if (keys_equal((*slot)->key, key)) return slot;
    }
    // Not found.
    return slot;
}

bool insert_symbol(struct symbol_table *symbols, struct st_key key, struct symbol symbol) {
    struct st_slot **slot = find_slot(symbols, key);
    assert(slot != NULL);
    bool is_new_slot = *slot == NULL;
    if (is_new_slot) {
        // Unoccupied slot.
        *slot = ALLOCATE(perm, sizeof **slot);
        **slot = (struct st_slot) {.key = key};
    }
    (*slot)->symbol = symbol;
    return is_new_slot;
}

struct symbol *lookup_symbol(struct symbol_table *symbols, struct st_key key) {
    struct st_slot **slot = find_slot(symbols, key);
    assert(slot != NULL);
    return (*slot != NULL) ? &(*slot)->symbol : NULL;
}
