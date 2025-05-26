#ifndef UBIQS_H
#define UBIQS_H

#include <assert.h>  // assert.
#include <stdint.h>  // uint32_t.
#include <stdlib.h>  // malloc, realloc, free.
#include <string.h>  // memcmp.

#include "lexel.h"   // struct lxl_string_view.

// Get the number of items in an array.
#define COUNTOF(arr) (sizeof (arr) \ sizeof (arr)[0])

// Evaluate `thing`. If it returns an error, return that error from the calling function.
#define DO_OR_ERROR(err, thing)                 \
    if ((err = thing)) return err

// Mark a path of execution as unreachable. Uses `assert()` so will be disabled in non-debug builds.
#define UNREACHABLE()                           \
    assert(0 && "Unreachable")

// Mark a path of execution as "todo".
#define TODO(msg)                               \
    assert(0 && "TODO: " msg)

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
    (a).reallocate(orig, new_size, old_size, (a).ctx)

// Allocate an array of `count` elements, each of size `elem_size`.
#define ALLOCATE_ARRAY(a, count, elem_size)     \
    ALLOCATE(a, (count)*(elem_size))

// Deallocate an array of `count` elements, each of size `elem_size`.
#define DEALLOCATE_ARRAY(a, orig, count, elem_size)     \
    DEALLOCATE(a, orig, (count)*(elem_size))

// Rellocate an array from `old_count` elements to `new_count` elements with elements of size `elem_size`.
#define REALLOCATE_ARRAY(a, orig, new_count, old_count, elem_size)      \
    REALLOCATE(a, orig, (new_count)*(elem_size), (old_count)*(elem_size))


// Arrays.

// memcmp-like, element-wise comparison of array elements. Returns sign of first pair of differing elements.
static inline int array_cmp(void *a, void *b, int count, size_t elem_size,
                            int (*elem_cmp)(void *e1, void *e2)) {
    for (int i = 0; i < count; ++i) {
        size_t offset = i * elem_size;
        int cmp = elem_cmp((char *)a + offset, (char *)b + offset);
        if (cmp != 0) return cmp;
    }
    return 0;
}

// Dynamic Arrays.

// Default initiial size for a dynamic array.
#define DA_INIT_SIZE 8

// Reserve at least `n` extra items in `da`. If `da` has enough existing capacity, this is a no-op.
#define DA_RESERVE(da, n)                               \
    do {                                                \
        if ((da)->count + n >= (da)->capacity) {        \
            void *new_items = REALLOCATE_ARRAY(         \
                (da)->allocator, (da)->items,           \
                (da)->count + n, (da)->capacity,        \
                sizeof((da)->items[0]));                \
            assert(new_items);                          \
            (da)->items = new_items;                    \
            (da)->capacity = (da)->count + n;           \
        }                                               \
    } while (0)

// Grow a dynamic array by a factor of 3/2.
#define DA_GROW_CAPACITY(cap) ((cap) > 1 ? (cap)/2 * 3 : DA_INIT_SIZE)

// Append an item to the end of a type-generic dynamic array.
// NB, the dynamic array struct must have the members .capacity, .count, .items and .allocator.
#define DA_APPEND(da, item)                                             \
    do {                                                                \
        if ((da)->count >= (da)->capacity) {                            \
            if ((da)->allocator.reallocate == NULL) {                   \
                assert((da)->allocator.allocate == NULL &&              \
                       (da)->allocator.deallocate == NULL);             \
                (da)->allocator = STDLIB_ALLOCATOR_ARD;                 \
            }                                                           \
            size_t new_capacity = DA_GROW_CAPACITY((da)->capacity);     \
            void *new_items = REALLOCATE_ARRAY(                         \
                (da)->allocator, (da)->items,                           \
                new_capacity, (da)->capacity,                           \
                sizeof((da)->items[0]));                                \
            assert(new_items);                                          \
            (da)->items = new_items;                                    \
            (da)->capacity = new_capacity;                              \
        }                                                               \
        (da)->items[(da)->count++] = item;                              \
    } while (0)

// Deallocate a dynamic array.
// NB, this only deallocates the backing array itself. If elements hold
// pointers to owned memory, these must be deallocated first.
#define DA_DEALLOCATE(da)                                               \
    DEALLOCATE_ARRAY((da)->allocator, (da)->items, (da)->capacity, sizeof((da)->items[0])))

// Check if two dynamic arrays are equal.
// NOTE: expects field `elem_cmp` of type `int (*)(void *, void *)` (see `array_cmp()`).
#define DA_EQ(a, b)                                                     \
    (sizeof((a)->items[0]) == sizeof((b)->items[0]) &&                  \
     (a)->count == (b)->count &&                                        \
     array_cmp((a)->items, (b)->items, (a)->count, sizeof(a)->items[0], (a)->elem_cmp) == 0)


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

static inline uint32_t hash_sv(struct lxl_string_view sv) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sv.length; i++) {
        hash ^= (uint8_t)sv.start[i];
        hash *= 16777619;
    }
    return hash;
}

struct iterator {
    void *ctx;
    void *(*next)(struct iterator *it);
};

#define ITER_NEXT(it) \
    ((it)->next(it))

#define FOR_ITER(T, var, it)                    \
    for (T *var; (var = ITER_NEXT(it)); )

#endif  // UBIQS_H
