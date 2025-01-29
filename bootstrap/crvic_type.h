#ifndef CRVIC_TYPE_H
#define CRVIC_TYPE_H

#include "ubiqs.h"  // Allocator interface.

enum type_primitive {
    TYPE_NO_TYPE,      // Sentinel type denoting no type.
    TYPE_ABSURD,       // Absurd type `!`. Used to denote expressions that do not return.
    TYPE_UNIT,         // Unit type `()`. Used to denote an expression with no meaningful value.
    TYPE_I8,           // Signed 8-bit integer.
    TYPE_I16,          // Signed 16-bit integer.
    TYPE_I32,          // Signed 32-bit integer.
    TYPE_I64,          // Signed 64-bit integer.
    TYPE_U8,           // Unsigned 8-bit integer.
    TYPE_U16,          // Unsigned 16-bit integer.
    TYPE_U32,          // Unsigned 32-bit integer.
    TYPE_U64,          // Unsigned 64-bit integer.
    TYPE_INT,          // Signed pointer-sized integer.
    TYPE_UINT,         // Unsigned pointer-sized integer.

    TYPE_PRIMITIVE_COUNT  // Number of primitive types. Not an actual type.
};

typedef int TypeID;   // Type for types.

struct type_list {
    struct allocatorARD allocator;
    size_t capacity;
    size_t count;
    TypeID *items;
};

#endif
