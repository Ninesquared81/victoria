#ifndef CRVIC_TYPE_H
#define CRVIC_TYPE_H

#include <stdbool.h>  // bool.

#include "lexel.h"  // struct lxl_string_view.

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

    // NOTE: TYPE_INT and TYPE_UINT have been removed and replaced with type aliases.

    TYPE_PRIMITIVE_COUNT  // Number of primitive types. Not an actual type.
};

typedef int TypeID;             // Unique ID for a type (i.e., an index into the type table).

typedef intptr_t  VIC_INT;      // C type for Victoria `int` type.
typedef uintptr_t VIC_UINT;     // C type for Victoria `uint` type.

struct type_decl {
    struct lxl_string_view name;
    TypeID type;
};

struct type_decl_list {
    struct allocatorARD allocator;
    int capacity;
    int count;
    struct type_decl *items;
};

enum signedness {
    SIGN_SIGNED = -1,     // Can be tested with x < 0.
    SIGN_NO_SIGN = 0,     // Can be tested with !x.
    SIGN_UNSIGNED = 1,    // Can be tested with x > 0.
};

enum type_conv_kind {
    CONVERT_AS,
    CONVERT_TO,
};

// A 'kind' is the type of a type.
enum kind {
    KIND_NO_KIND,               // Sentinel kind denoting no kind.
    KIND_PRIMITIVE,             // Primitive type (see above).
    KIND_ENUM,                  // Enumeration type.
    KIND_RECORD,                // Record type (named product type).
};

struct enum_field {
    struct lxl_string_view name;
    VIC_INT value;
};

struct enum_field_list {
    struct allocatorARD allocator;
    int capacity;
    int count;
    struct enum_field *items;
};

struct type_info {
    enum kind kind;               // Kind of type T.
    size_t size;                  // sizeof(T).
    struct lxl_string_view repr;  // Textual representation of T.
    union {
        /* struct {} primitive_type; */
        struct {
            // Note: NO underlying type (default is Victoria's `int` type).
            struct enum_field_list fields;
        } enum_type;
        struct {
            struct type_decl_list fields;
        } record_type;
    };
};

TypeID add_type(struct type_info info);
struct type_info *get_type(TypeID type);

TypeID find_record_type(struct type_decl_list fields);
size_t calculate_record_size(struct type_decl_list fields);
struct lxl_string_view make_record_repr(struct type_decl_list fields);
TypeID get_record_field_type(struct type_decl_list fields, struct lxl_string_view field_name);

TypeID find_enum_type(struct enum_field_list fields);
struct lxl_string_view make_enum_repr(struct enum_field_list fields);


bool is_integer_type(TypeID type);
enum signedness sign_of_type(TypeID type);
struct lxl_string_view get_type_sv(TypeID type);
TypeID get_sized_int(TypeID type);
TypeID max_type_rank(TypeID type1, TypeID type2);

#endif
