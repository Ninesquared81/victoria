#ifndef CRVIC_RESOURCES_H
#define CRVIC_RESOURCES_H

#include "region.h"

#define REGION_SIZE 0x400000

extern struct allocatorARD perm;  // Allocator for permanent allocations.
extern struct allocatorARD temp;  // Allocator for temporary allocations.

void init_crvic(void);  // Initialise crvic resources. Must be called at start of program.

// Promote a temporary allocation to a permanent allocation.
// NOTE: the temporary allocation is NOT cleaned up. Temporary allocation cleanup is left to the caller.
void *promote_allocation(void *temp_mem, size_t size);

#define PROMOTE_DA(temp_da)                                             \
    {       .allocator = perm,                                          \
            .capacity = (temp_da)->count,                               \
            .count = (temp_da)->count,                                  \
            .items = promote_allocation((temp_da)->items, (temp_da)->count * sizeof((temp_da)->items[0]))}

// Begin a series of temporary allocations.
void begin_temp(void);

#endif
