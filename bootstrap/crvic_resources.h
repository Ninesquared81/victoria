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
            .items = promote_allocation((temp_da)->items, (temp_da)->count * sizeof((temp_da)->items[0])), \
            .elem_eq = (temp_da)->elem_eq}

// Begin a series of temporary allocations.
REGION_RESTORE begin_temp(void);
// End a series of temporary allocations.
// N.B., `restore_point` should be the unmodified return value of the corresponding call to `begin_temp()`.
void end_temp(REGION_RESTORE restore_point);

// Begin a series of temporary allocations with an automatically-named variable `AUTO_restore_point`.
#define AUTO_BEGIN_TEMP() REGION_RESTORE AUTO_restore_point = begin_temp()
// End a series of temporary allocations begun by `AUTO_BEGIN_TEMP()`.
#define AUTO_END_TEMP() end_temp(AUTO_restore_point)

#endif
