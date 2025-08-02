#ifndef CRVIC_TYPE_H
#define CRVIC_TYPE_H

#include <stdbool.h>  // bool.

#include "lexel.h"  // struct lxl_string_view.

#include "ubiqs.h"  // Allocator & iterator interfaces.

enum type_primitive {
    TYPE_NO_TYPE,      // Sentinel type denoting no type.
    TYPE_TYPE_EXPR,    // `type_of(T)`.
    TYPE_ABSURD,       // Absurd type `!`. Used to denote expressions that do not return.
    TYPE_UNIT,         // Unit type `()`. Used to denote an expression with no meaningful value.
    TYPE_NULLPTR_TYPE, // `type_of(null)`.
    TYPE_I8,           // Signed 8-bit integer.
    TYPE_I16,          // Signed 16-bit integer.
    TYPE_I32,          // Signed 32-bit integer.
    TYPE_I64,          // Signed 64-bit integer.
    TYPE_U8,           // Unsigned 8-bit integer.
    TYPE_U16,          // Unsigned 16-bit integer.
    TYPE_U32,          // Unsigned 32-bit integer.
    TYPE_U64,          // Unsigned 64-bit integer.
    // TYPE_STRING,       // String type.
    TYPE_C_STRING,     // C-style string type.

    // NOTE: TYPE_INT and TYPE_UINT have been removed and replaced with type aliases.

    TYPE_PRIMITIVE_COUNT  // Number of primitive types. Not an actual type.
};

// Aliases for 'int' and 'uint' (also added to symbol table).
// TODO: allow different sizes of `int`.
#define TYPE_INT  TYPE_I64
#define TYPE_UINT TYPE_U64

typedef int TypeID;             // Unique ID for a type (i.e., an index into the type table).

typedef intptr_t  VIC_INT;      // C type for Victoria `int` type.
typedef uintptr_t VIC_UINT;     // C type for Victoria `uint` type.

#define VIC_PTR_SIZE sizeof(VIC_INT)

struct type_decl {
    struct lxl_string_view name;
    TypeID type;
};

struct type_decl_list {
    struct allocatorARD allocator;
    int capacity;
    int count;
    struct type_decl *items;
    bool (*elem_eq)(void *e1, void *e2);
};

static inline bool type_decl_eq(void *e1, void *e2) {
    struct type_decl a = *(struct type_decl *)e1;
    struct type_decl b = *(struct type_decl *)e2;
    return a.type == b.type && ((memcmp(&a.name, &b.name, sizeof a.name) == 0
                                 || lxl_sv_equal(a.name, b.name)));
}

#define TYPE_DECL_LIST(ALLOCATOR)                                       \
    ((struct type_decl_list) {.allocator = ALLOCATOR, .elem_eq = type_decl_eq})

enum func_link_kind {
    FUNC_EXTERNAL,  // A function defined externally (possibly in a different language, like C).
    FUNC_INTERNAL,  // A function defined within this source file.
};

struct func_sig {
    struct lxl_string_view name;
    TypeID ret_type;
    int arity;
    struct type_decl_list params;
    bool c_variadic;
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
    KIND_POINTER,               // Pointer type, ^T, [^]T.
    KIND_ARRAY,                 // Array type, [n]T.
    KIND_FUNCTION,              // Function type, func f(p1: P1, p2: P2) -> R.
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
    bool (*elem_eq)(void *e1, void *e2);
};

static inline bool enum_field_eq(void *e1, void *e2) {
    struct enum_field a = *(struct enum_field *)e1;
    struct enum_field b = *(struct enum_field *)e2;
    return a.value == b.value && ((memcmp(&a.name, &b.name, sizeof a.name) == 0
                                   || lxl_sv_equal(a.name, b.name)));
}

#define ENUM_FIELD_LIST(ALLOCATOR)              \
    ((struct enum_field_list) {.allocator = ALLOCATOR, .elem_eq = enum_field_eq})

enum pointer_kind {
    POINTER_PROPER,
    POINTER_ARRAY_LIKE,
    // POINTER_FUNCTION?
};

enum rw_access {
    RW_READ_ONLY,               // No modifier.
    RW_READ_WRITE,              // `mut` modifier.
    RW_WRITE_BEFORE_READ,       // `out` modifier.
};

struct type_info {
    TypeID id;                    // ID of type T.
    enum kind kind;               // Kind of type T.
    size_t size;                  // sizeof(T).
    struct lxl_string_view repr;  // Textual representation of T.
    union {
        /* struct {} primitive_type; */
        struct enum_info {
            TypeID underlying_type;
            struct enum_field_list fields;
        } enum_;
        struct record_info {
            struct type_decl_list fields;
        } record;
        struct pointer_info {
            enum pointer_kind kind;
            enum rw_access rw;
            TypeID dest_type;
        } pointer;
        struct array_info {
            VIC_INT count;
            enum rw_access rw;
            TypeID dest_type;
        } array;
        struct function_info {
            struct func_sig *sig;
        } function;
    };
};

TypeID add_type(struct type_info info);
struct type_info *get_type(TypeID type);

TypeID get_or_add_type(struct type_info info);
void init_type(struct type_info *info);

bool types_equal(struct type_info *a, struct type_info *b);

bool enum_types_equal(struct enum_info *a, struct enum_info *b);
bool record_types_equal(struct record_info *a, struct record_info *b);
bool pointer_types_equal(struct pointer_info *a, struct pointer_info *b);
bool array_types_equal(struct array_info *a, struct array_info *b);
bool function_types_equal(struct function_info *a, struct function_info *b);

struct lxl_string_view make_record_repr(struct record_info info);
struct lxl_string_view make_enum_repr(struct enum_info info);
struct lxl_string_view make_pointer_repr(struct pointer_info info);
struct lxl_string_view make_array_repr(struct array_info info);
struct lxl_string_view make_function_repr(struct function_info info);

size_t calculate_record_size(struct record_info info);
TypeID get_record_field_type(struct record_info info, struct lxl_string_view field_name);
bool get_enum_field_value(struct enum_info info, struct lxl_string_view field_name, VIC_INT *OUT_value);

bool type_is_kind(TypeID type, enum kind kind);
bool is_integer_type(TypeID type);
bool is_ordered_type(TypeID type);
enum signedness sign_of_type(TypeID type);
struct lxl_string_view get_type_sv(TypeID type);
size_t get_type_size(TypeID type);
TypeID get_sized_int(TypeID type);
TypeID max_type_rank(TypeID type1, TypeID type2);

struct iterator get_type_iterator(void);
/* struct type_info * */ void *type_iterator_next(struct iterator *it);

#endif
