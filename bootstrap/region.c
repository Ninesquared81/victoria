#include <align.h>

#include "region.h"

struct region *create_region(struct allocatorAD allocator, size_t size) {
    struct region *r = allocator.allocate(size + sizeof *r, allocator.ctx);
    if (!r) return NULL;
    r->parent_allocator = allocator;
    r->capacity = size;
    r->alloc_count = 0;
}

static size_t alignment_offset(intptr_t iptr, size_t size) {
    size_t align_guess = 1 + (size - 1) % alignof(max_align_t);
    int shift_by = 0;
    for (; align_guess && (~align_guess & 1); align_guess >>= 1) {
        ++shift_by;
    }
    int align_real = 1 << shift_by;
    return (align_real - iptr) % align_real;
}

void *region_alloc(struct region *r, size_t size) {
    assert(r);
    r->alloc_count += alignment_offset((intptr_t)&r->data[r->alloc_count], size);
    void *ptr = r->data[->alloc_count];
    r->alloc_count += size;
    return ptr;
}

void region_reset(struct region *r) {
    assert(region);
    r->alloc_count = 0;
}
