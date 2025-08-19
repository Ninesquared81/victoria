#ifndef UBIQS_H
#define UBIQS_H

#include <assert.h>  // assert.
#include <stdbool.h> // bool, true, false.
#include <stdint.h>  // uint32_t.
#include <stdlib.h>  // malloc, realloc, free.
#include <string.h>  // memcmp.

#include "lexel.h"   // struct lxl_string_view.

// Explicitly mark switch statement fallthrough
#if __STDC_VERSION__ >= 202311L || defined(_msc_ver)
# define FALLTHROUGH [[fallthrough]]
#elif defined(__clang__) || defined(__gcc__)
# define FALLTHROUGH __attribute__((fallthrough))
#else
# define FALLTHROUGH
#endif

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

// Element-wise equality check of array elements.
static inline int array_eq(void *a, void *b, int count, size_t elem_size,
                           bool (*elem_eq)(void *e1, void *e2)) {
    for (int i = 0; i < count; ++i) {
        size_t offset = i * elem_size;
        if (!elem_eq((char *)a + offset, (char *)b + offset)) return false;
    }
    return true;
}

// Dynamic Arrays.

// Required members:
//  - da: `.allocator` (A-R-D), `.capacity`, `.count`, `.items`

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
// NOTE: expects field `.elem_eq` of type `bool (*)(void *, void *)` (see `array_eq()`).
#define DA_EQ(a, b)                                                     \
    (sizeof((a)->items[0]) == sizeof((b)->items[0]) &&                  \
     (a)->count == (b)->count &&                                        \
     array_eq((a)->items, (b)->items, (a)->count, sizeof(a)->items[0], (a)->elem_eq))

// Doubly-linked lists.
// Required members:
//  - list: `.head`, `.tail`, `.count`
//  - node: `.next`, `.prev`

// Insert a (pre-allocated) node at the head of a list.
#define DLLIST_PREPEND(list, new_node)                  \
    do {                                                \
        (new_node)->prev = NULL;                        \
        (new_node)->next = (list)->head;                \
        if ((list)->head != NULL) {                     \
            (list)->head->prev = new_node;              \
        }                                               \
        else {                                          \
            (list)->tail = new_node;                    \
        }                                               \
        (list)->head = new_node;                        \
        ++(list)->count;                                \
    } while (0)

// Insert a (pre-allocated) node at the tail of a list.
#define DLLIST_APPEND(list, new_node)                   \
    do {                                                \
        (new_node)->prev = (list)->tail;                \
        (new_node)->next = NULL;                        \
        if ((list)->tail != NULL) {                     \
            (list)->tail->next = new_node;              \
        }                                               \
        else {                                          \
            (list)->head = new_node;                    \
        }                                               \
        (list)->tail = new_node;                        \
        ++(list)->count;                                \
    } while (0)

// Insert a (pre-allocated) node before an internal node.
#define DLLIST_INSERT_BEFORE(list, next_node, new_node) \
    do {                                                \
        (new_node)->next = next_node;                   \
        (new_node)->prev = (next_node)->prev;           \
        (next_node)->prev = new_node;                   \
        ++(list)->count;                                \
    } while (0)

// Insert a (pre-allocated) node after an internal node.
#define DLLIST_INSERT_AFTER(list, prev_node, new_node)  \
    do {                                                \
        (new_node)->next = (prev_node)->next;           \
        (new_node)->prev = prev_node;                   \
        (prev_node)->next = new_node;                   \
        ++(list)->count;                                \
    } while (0)

// Check whether the list is empty.
#define DLLIST_IS_EMPTY(list)                   \
    (list)->count == 0

// Loop over list elements in forward direction.
#define FOR_DLLIST(T, var, list)                                \
    for (T var = (list)->head; var != NULL; var = var->next)

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
    for (T var; (var = ITER_NEXT(it)); )

struct sb_node {
    struct sb_node *next, *prev;
    struct lxl_string_view sv;
};

struct sb_head {
    struct sb_node *head, *tail;
    int count;
    struct allocatorAD allocator;
};

static inline void sb_append(struct sb_head *sb, struct lxl_string_view sv) {
    struct sb_node *node = ALLOCATE(sb->allocator, sizeof *node);
    node->sv = sv;
    DLLIST_APPEND(sb, node);
}

static inline void sb_append_char(struct sb_head *sb, char c) {
    char *p = ALLOCATE(sb->allocator, 1);
    *p = c;
    sb_append(sb, (struct lxl_string_view) {.start = p, .length = 1});
}

static inline struct lxl_string_view sb_to_sv(struct sb_head *sb, struct allocatorAD allocator) {
    size_t length = 0;
    FOR_DLLIST (struct sb_node *, node, sb) {
        length += node->sv.length;
    }
    char *start = ALLOCATE(allocator, length + 1);  // +1 for '\0'.
    char *p = start;
    FOR_DLLIST (struct sb_node *, node, sb) {
        memcpy(p, node->sv.start, node->sv.length);
        p += node->sv.length;
    }
    assert(p == start + length);
    *p = '\0';
    return (struct lxl_string_view) {.start = start, .length = length};
}

static inline struct lxl_string_view remove_filename_extension(struct lxl_string_view filename) {
    for (int i = filename.length - 1; i >= 0; --i) {
        if (filename.start[i] == '.') {
            // Note: i is one past the end of the name.
            return (struct lxl_string_view) {
                .start = filename.start,
                .length = i};
        }
    }
    // No extension; return entire name.
    return filename;
}

#endif  // UBIQS_H
