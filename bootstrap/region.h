#ifndef REGION_H
#define REGION_H

#include <stddef.h>  // size_t.

#include "ubiqs.h"


struct region {
    struct allocatorAD parent_allocator;
    size_t capacity;
    size_t alloc_count;
    char *data;
};

// Create a region that can hold `size` bytes using the given allocator.
struct region *create_region(struct allocatorAD allocator, size_t size);
// Detsroy the region.
void destroy_region(struct region *region);

// Make a region with the given backing buffer.
struct region region_from_buffer(size_t buf_size, void *buffer);

#define REGION_FROM_ARRAY(arr) \
    (struct region) {.capacity = sizeof arr, .data = (char *)arr}

// Allocate inside region.
void *region_allocate(size_t size, void *region);
// Reset region (effectively deallocates all allocations since last reset).
void region_reset(struct region *region);
// Reallocate inside region. If this cannot be done in-place, a new allocation is made
// at the end of the region.
void *region_reallocate(void *orig, size_t new_size, size_t old_size, void *region);
// Deallocate inside region. If `orig` was not the last allocation, do nothing.
void region_deallocate(void *orig, size_t size, void *region);

#define region_allocatorARD(region) ((struct allocatorARD) {            \
            .ctx = region, .allocate = region_allocate,                 \
            .deallocate = region_deallocate, .reallocate = region_reallocate})

#endif
