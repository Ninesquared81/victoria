#include <string.h>  // memcpy.

#include "ubiqs.h"

#include "crvic_resources.h"

struct allocatorARD perm = {0};  // Allocator for permanent allocations.
struct allocatorARD temp = {0};  // Allocator for temporary allocations.

static struct region *perm_region = NULL;
static struct region *temp_region = NULL;

void init_crvic(void) {
    perm_region = create_region(STDLIB_ALLOCATOR_AD, REGION_SIZE);
    temp_region = create_region(STDLIB_ALLOCATOR_AD, REGION_SIZE);
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

REGION_RESTORE begin_temp(void) {
    return region_save(temp_region);
}

void end_temp(REGION_RESTORE restore_point) {
    region_restore(temp_region, restore_point);
}
