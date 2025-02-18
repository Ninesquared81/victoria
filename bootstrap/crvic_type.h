#ifndef CRVIC_TYPE_H
#define CRVIC_TYPE_H

#include <stdbool.h>  // bool.

#include "lexel.h"  // struct lxl_string_view.

#include "ubiqs.h"  // Allocator interface.

#define PTR_WIDTH 64

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

struct type_decl {
    struct lxl_string_view name;
    TypeID type;
};

struct type_decl_list {
    struct allocatorARD allocator;
    size_t capacity;
    size_t count;
    struct type_decl *items;
};

enum signedness {
    SIGN_SIGNED = -1,     // Can be tested with x < 0.
    SIGN_NO_SIGN = 0,     // Can be tested with !x.
    SIGN_UNSIGNED = 1,    // Can be tested with x > 0.
};

bool is_integer_type(TypeID type);
enum signedness sign_of_type(TypeID type);
struct lxl_string_view get_type_sv(TypeID type);
TypeID get_sized_int(TypeID type);
TypeID max_type_rank(TypeID type1, TypeID type2);

#endif
