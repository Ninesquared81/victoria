#include <stdalign.h>
#include <string.h>

#include "region.h"

struct region *create_region(struct allocatorAD allocator, size_t size) {
    struct region *r = ALLOCATE(allocator, size + sizeof *r);
    if (!r) return NULL;
    r->parent_allocator = allocator;
    r->capacity = size;
    r->alloc_count = 0;
    return r;
}

void destroy_region(struct region *r) {
    size_t orig_size = r->capacity + sizeof *r;
    r->capacity = 0;
    r->alloc_count = 0;
    DEALLOCATE(r->parent_allocator, r, orig_size);
}

static size_t alignment_offset(intptr_t iptr, size_t size) {
    size_t align_guess = 1 + (size - 1) % alignof(max_align_t);
    int shift_by = 0;
    for (; align_guess && (~align_guess & 1); align_guess >>= 1) {
        ++shift_by;
    }
    int align_real = 1 << shift_by;
    assert(iptr > 1);
    // Note: iptr_align_shifted is defined on the shifted interval [1, align_real]
    // rather than the usual [0, align_real - 1]. This is so we cann subtract it from
    // align_real to get the needed offset (if we're already aligned, this should be 0).
    int iptr_align_shifted = (iptr-1) % align_real + 1;
    assert(0 < iptr_align_shifted && iptr_align_shifted <= align_real);
    return align_real - iptr_align_shifted;
}

void *region_allocate(size_t size, void *region) {
    assert(region);
    struct region *r = region;
    r->alloc_count += alignment_offset((intptr_t)&r->data[r->alloc_count], size);
    void *ptr = &r->data[r->alloc_count];
    r->alloc_count += size;
    return ptr;
}

void region_reset(struct region *r) {
    assert(r);
    r->alloc_count = 0;
}

void *region_reallocate(void *orig, size_t new_size, size_t old_size, void *region) {
    assert(region);
    struct region *r = region;
    assert(old_size <= r->alloc_count);
    if (orig == &r->data[r->alloc_count - old_size]) {
        // Grow/shrink in-place.
        r->alloc_count -= old_size;
        r->alloc_count += new_size;
        return orig;
    }
    // New allocation.
    void *new = region_allocate(new_size, r);
    if (new == NULL) return NULL;
    memcpy(new, orig, old_size);
    return new;
}

void region_deallocate(void *orig, size_t size, void *region) {
    assert(region);
    struct region *r = region;
    assert(size <= r->alloc_count);
    if (orig == &r->data[r->alloc_count - size]) {
        r->alloc_count -= size;
    }
}
