#include <string.h>

#include "lexel.h"

#include "region.h"

#include "crvic_resources.h"
#include "crvic_symbol_table.h"

struct symbol_table *new_symbol_table(struct allocatorAD allocator, size_t capacity) {
    struct symbol_table *symbols = ALLOCATE(allocator, sizeof *symbols + capacity * sizeof(*symbols->slots));
    memset(symbols->slots, 0, capacity * sizeof(*symbols->slots));
    symbols->parent = NULL;  // This will be set by the caller if appropriate.
    symbols->capacity = capacity;
    return symbols;
}

static bool keys_equal(struct st_key a, struct st_key b) {
    return a.hash == b.hash && lxl_sv_equal(a.sv, b.sv);
}

struct st_key st_key_of(struct lxl_string_view sv) {
    return (struct st_key) {.sv = sv, .hash = hash_sv(sv)};
}

struct st_slot **find_slot(struct symbol_table *symbols, struct st_key key) {
    // NOTE: this function does NOT look in the parent symbol table.
    // HINT: call find_slot(symbols->parent, key) to search there.
    int index = key.hash % symbols->capacity;
    static_assert(offsetof(struct st_slot, next) == 0, ".next field assumed to be at start of st_slot.");
    struct st_slot **slot = &symbols->slots[index];
    for (; *slot != NULL; slot = (struct st_slot **)*slot) {
        if (keys_equal((*slot)->key, key)) return slot;
    }
    // Not found.
    return slot;
}

struct symbol *insert_symbol(struct symbol_table *symbols, struct st_key key, struct symbol symbol) {
    struct st_slot **slot = find_slot(symbols, key);
    assert(slot != NULL);
    bool is_new_slot = *slot == NULL;
    if (is_new_slot) {
        // Unoccupied slot.
        *slot = ALLOCATE(perm, sizeof **slot);
        **slot = (struct st_slot) {.key = key};
    }
    (*slot)->symbol = symbol;
    return (is_new_slot) ? &(*slot)->symbol : NULL;
}

struct symbol *lookup_symbol(struct symbol_table *symbols, struct st_key key) {
    struct st_slot **slot = find_slot(symbols, key);
    assert(slot != NULL);
    // If we didn't find the symbol, look in the parent symbol table.
    if (*slot == NULL && symbols->parent) return lookup_symbol(symbols->parent, key);
    return (*slot != NULL) ? &(*slot)->symbol : NULL;
}
