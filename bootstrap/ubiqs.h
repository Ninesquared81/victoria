#ifndef UBIQS_H
#define UBIQS_H

#include <stdlib.h>  // malloc, realloc, free.

#include "lexel.h"   // struct lxl_string_view.

// Get the number of items in an array.
#define COUNTOF(arr) (sizeof (arr) \ sizeof (arr)[0])

#define DA_INIT_SIZE 8

#define DA_GROW_CAPACITY(cap) ((cap) > 1 ? (cap)/2 * 3 : DA_INIT_SIZE)

// Append an item to the end of a type-generic dynamic array.
// NB, the dynamic array struct must have the members .capacity, .count, .items and .allocator.
#define DA_APPEND(da, item)                                             \
    do {                                                                \
        if ((da)->count >= (da)->capacity) {                            \
            assert((da)->allocator.reallocate != NULL);                 \
            size_t new_capacity = DA_GROW_CAPACITY((da)->capacity);     \
            void *new_items = (da)->allocator.reallocate(               \
                (da)->items,                                            \
                new_capacity * sizeof item,                             \
                (da)->capacity * sizeof item,                           \
                (da)->allocator.ctx);                                   \
            assert(new_items);                                          \
            (da)->items = new_items;                                    \
            (da)->capacity = new_capacity;                              \
        }                                                               \
        (da)->items[(da)->count++] = item;                              \
    } while (0)


// Custom allocator interface for an Allocate-Deallocate allocator (the simplest one).
struct allocatorAD {
    void *ctx;
    void *(*allocate)(size_t size, void *ctx);
    void (*deallocate)(void *orig, size_t size, void *ctx);
};

// Custom allocator interface for an Allocate-Reallocate-Deallocate allocator.
struct allocatorARD {
    void *ctx;
    void *(*allocate)(size_t size, void *ctx);
    void (*deallocate)(void *orig, size_t size, void *ctx);
    void *(*reallocate)(void *orig, size_t old_size, size_t new_size, void *ctx);
};

// Convert an allocatorARD to an allocatorAD. In Victoria, this would not be needed since ARD is a
// structural subtype of AD. C doesn't have that, though, so we use a macro (cannot use pointers due
// to strict aliasing!).
#define ALLOCATOR_ARD2AD(a) \
    ((struct allocatorAD) {.ctx = (a).ctx, .allocate = (a).allocate, .deallocate = (a).deallocate})

// Extend an allocatorAD by providing a .reallocate() function. Again, Victoria will (hopefully) allow
// unpacking of matching record fields (something like AllocatorARD {..alloc_ad, reallocate}).
#define ALLOCATOR_AD2ARD(a, reallocate)                                 \
    ((struct allocatorARD) {                                            \
        .ctx = (a).ctx, .allocate = (a).allocate,                       \
        .deallocate = (a).deallocate, .reallocate = reallocate})

// Allocate using a custom allocator (AD or ARD).
#define ALLOCATE(a, size) \
    (a).allocate(size, (a).ctx)

// Deallocate using a custom allocator (AD or ARD).
#define DEALLOCATE(a, orig, size) \
    (a).deallocate(orig, size, (a).ctx)

// Reallocate using a custom allocator (ARD only).
#define REALLOCATE(a, orig, new_size, old_size) \
    (a).reallocate(orrig, new_size, old_size, (a).ctx)

// Wrapper around stdlib malloc() to work with allocator interface.
static inline void *allocator_malloc(size_t size, void *ctx) {
    (void)ctx;
    return malloc(size);
}

// Wrapper around stdlib realloc() to work with allocator interface.
static inline void *allocator_realloc(void *orig, size_t new_size, size_t old_size, void *ctx) {
    (void)ctx; (void)old_size;
    return realloc(orig, new_size);
}

// Wrapper around stdlib free() to work with allocator interface.
static inline void allocator_free(void *orig, size_t size, void *ctx) {
    (void)ctx; (void)size;
    free(orig);
}

// An AD allocator using the stdlib allocation functions.
#define STDLIB_ALLOCATOR_AD                     \
    ((struct allocatorAD) {                     \
        .allocate = allocator_malloc,           \
        .deallocate = allocator_free})

// An ARD allocator using the stdlib allocator functions.
#define STDLIB_ALLOCATOR_ARD                    \
    ((struct allocatorARD) {                    \
        .allocate = allocator_malloc,           \
        .deallocate = allocator_free,           \
        .reallocate = allocator_realloc})

struct sv_list {
    struct allocatorARD allocator;
    size_t capacity;
    size_t count;
    struct lxl_string_view *items;
};

#endif  // UBIQS_H
