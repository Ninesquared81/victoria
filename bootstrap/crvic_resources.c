#include <string.h>  // memcpy.

#include "ubiqs.h"

#include "crvic_resources.h"

struct allocatorARD perm = {0};  // Allocator for permanent allocations.
struct allocatorARD temp = {0};  // Allocator for temporary allocations.

void crvic_init(void) {
    static struct region *perm_region = create_region(STDLIB_ALLOCATOR_AD, REGION_SIZE);
    static struct region *temp_region = create_region(STDLIB_ALLOCATOR_AD, REGION_SIZE);
    perm = region_allocatorARD(perm_region);
    temp = region_allocatorARD(temp_region);
    assert(perm_region && temp_region && "Not enough memory :o");
}

void *promote_allocation(void *temp_mem, size_t size) {
    void *new = region_allocate(size, perm_region);
    if (!new) return NULL;
    memcpy(new, temp_mem, size);
    return new;
}

void begin_temp(void) {
    reset_region(temp);
}
