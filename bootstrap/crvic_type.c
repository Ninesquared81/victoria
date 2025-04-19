#include <assert.h>

#include "crvic_type.h"

#define TYPE_TABLE_CAPACITY 1024

static struct type_info types[TYPE_TABLE_CAPACITY] = {
    [TYPE_NO_TYPE] = {.kind = KIND_NO_KIND,   .size = 0, .repr = LXL_SV_FROM_STRLIT("<No type>")},
    [TYPE_ABSURD]  = {.kind = KIND_PRIMITIVE, .size = 0, .repr = LXL_SV_FROM_STRLIT("!")},
    [TYPE_UNIT]    = {.kind = KIND_PRIMITIVE, .size = 0, .repr = LXL_SV_FROM_STRLIT("()")},
    [TYPE_I8]      = {.kind = KIND_PRIMITIVE, .size = 1, .repr = LXL_SV_FROM_STRLIT("i8")},
    [TYPE_I16]     = {.kind = KIND_PRIMITIVE, .size = 2, .repr = LXL_SV_FROM_STRLIT("i16")},
    [TYPE_I32]     = {.kind = KIND_PRIMITIVE, .size = 4, .repr = LXL_SV_FROM_STRLIT("i32")},
    [TYPE_I64]     = {.kind = KIND_PRIMITIVE, .size = 8, .repr = LXL_SV_FROM_STRLIT("i64")},
    [TYPE_U8]      = {.kind = KIND_PRIMITIVE, .size = 1, .repr = LXL_SV_FROM_STRLIT("u8")},
    [TYPE_U16]     = {.kind = KIND_PRIMITIVE, .size = 1, .repr = LXL_SV_FROM_STRLIT("u16")},
    [TYPE_U32]     = {.kind = KIND_PRIMITIVE, .size = 4, .repr = LXL_SV_FROM_STRLIT("u32")},
    [TYPE_U64]     = {.kind = KIND_PRIMITIVE, .size = 8, .repr = LXL_SV_FROM_STRLIT("u64")},
    [TYPE_INT]     = {.kind = KIND_PRIMITIVE, .size = sizeof(VIC_INT),  .repr = LXL_SV_FROM_STRLIT("int")},
    [TYPE_UINT]    = {.kind = KIND_PRIMITIVE, .size = sizeof(VIC_UINT), .repr = LXL_SV_FROM_STRLIT("uint")},
    /* ... Other types to be filled in later ... */
};

static int type_count = TYPE_PRIMITIVE_COUNT;
static_assert(TYPE_PRIMITIVE_COUNT <= TYPE_TABLE_CAPACITY, "Increase type table capacity");

TypeID add_type(struct type_info info) {
    assert(type_count < TYPE_TABLE_CAPACITY && "We need a bigger type table!");
    types[type_count++] = info;
    return type_count - 1;
}

struct type_info *get_type(TypeID id) {
    assert(0 <= id && id < type_count);
    return &types[id];
}

TypeID find_record_type(struct type_decl_list fields) {
    for (int i = TYPE_PRIMITIVE_COUNT; i < type_count; ++i) {
        struct type_info *info = get_type(i);
        if (info->kind == KIND_RECORD && DA_EQ(&info->record_type.fields, &fields)) return i;
    }
    return TYPE_NO_TYPE;
}

size_t calculate_record_size(struct type_decl_list fields) {
    size_t size = 0;
    for (int i = 0; i < fields.count; ++i) {
        size += get_type(fields.items[i].type)->size;
    }
    return size;
}

bool is_integer_type(TypeID type) {
    static_assert(TYPE_I8 < TYPE_U8, "Signed types assumed before unsigned");
    static_assert(TYPE_INT < TYPE_UINT, "Signed types assumed before unsigned");
    return TYPE_I8 <= type && type <= TYPE_UINT;
}

enum signedness sign_of_type(TypeID type) {
    if ((TYPE_I8 <= type && type <= TYPE_I64) || type == TYPE_INT) return SIGN_SIGNED;
    if ((TYPE_U8 <= type && type <= TYPE_U64) || type == TYPE_UINT) return SIGN_UNSIGNED;
    return SIGN_NO_SIGN;
}

struct lxl_string_view get_type_sv(TypeID type) {
    struct type_info *info = get_type(type);
    assert(info);
    return info->repr;
}

TypeID get_sized_int(TypeID type) {
    if (!is_integer_type(type)) return TYPE_NO_TYPE;
    if (type == TYPE_INT)  return (sizeof(VIC_INT)  == 64) ? TYPE_I64 : TYPE_I32;
    if (type == TYPE_UINT) return (sizeof(VIC_UINT) == 64) ? TYPE_U64 : TYPE_U32;
    return type;
}

TypeID max_type_rank(TypeID type1, TypeID type2) {
    assert(type1 < TYPE_PRIMITIVE_COUNT && type2 < TYPE_PRIMITIVE_COUNT);
    static TypeID ranks[TYPE_PRIMITIVE_COUNT][TYPE_PRIMITIVE_COUNT] = {
        [TYPE_I8][TYPE_I8]   = TYPE_I8,
        [TYPE_I8][TYPE_I16]  = TYPE_I16,
        [TYPE_I8][TYPE_I32]  = TYPE_I32,
        [TYPE_I8][TYPE_I64]  = TYPE_I64,
        [TYPE_I8][TYPE_U8]   = TYPE_U8,
        [TYPE_I8][TYPE_U16]  = TYPE_U16,
        [TYPE_I8][TYPE_U32]  = TYPE_U32,
        [TYPE_I8][TYPE_U64]  = TYPE_U64,
        [TYPE_I16][TYPE_I8]  = TYPE_I16,
        [TYPE_I16][TYPE_I16] = TYPE_I16,
        [TYPE_I16][TYPE_I32] = TYPE_I32,
        [TYPE_I16][TYPE_I64] = TYPE_I64,
        [TYPE_I16][TYPE_U8]  = TYPE_I16,
        [TYPE_I16][TYPE_U16] = TYPE_U16,
        [TYPE_I16][TYPE_U32] = TYPE_U32,
        [TYPE_I16][TYPE_U64] = TYPE_U64,
        [TYPE_I32][TYPE_I8]  = TYPE_I32,
        [TYPE_I32][TYPE_I16] = TYPE_I32,
        [TYPE_I32][TYPE_I32] = TYPE_I32,
        [TYPE_I32][TYPE_I64] = TYPE_I64,
        [TYPE_I32][TYPE_U8]  = TYPE_I32,
        [TYPE_I32][TYPE_U16] = TYPE_I32,
        [TYPE_I32][TYPE_U32] = TYPE_U32,
        [TYPE_I32][TYPE_U64] = TYPE_U64,
        [TYPE_I64][TYPE_I8]  = TYPE_I64,
        [TYPE_I64][TYPE_I16] = TYPE_I64,
        [TYPE_I64][TYPE_I32] = TYPE_I64,
        [TYPE_I64][TYPE_I64] = TYPE_I64,
        [TYPE_I64][TYPE_U8]  = TYPE_I64,
        [TYPE_I64][TYPE_U16] = TYPE_I64,
        [TYPE_I64][TYPE_U32] = TYPE_I64,
        [TYPE_I64][TYPE_U64] = TYPE_U64,
        [TYPE_U8][TYPE_I8]   = TYPE_U8,
        [TYPE_U8][TYPE_I16]  = TYPE_I16,
        [TYPE_U8][TYPE_I32]  = TYPE_I32,
        [TYPE_U8][TYPE_I64]  = TYPE_I64,
        [TYPE_U8][TYPE_U8]   = TYPE_U8,
        [TYPE_U8][TYPE_U16]  = TYPE_U16,
        [TYPE_U8][TYPE_U32]  = TYPE_U32,
        [TYPE_U8][TYPE_U64]  = TYPE_U64,
        [TYPE_U16][TYPE_I8]  = TYPE_U16,
        [TYPE_U16][TYPE_I16] = TYPE_U16,
        [TYPE_U16][TYPE_I32] = TYPE_I32,
        [TYPE_U16][TYPE_I64] = TYPE_I64,
        [TYPE_U16][TYPE_U8]  = TYPE_U16,
        [TYPE_U16][TYPE_U16] = TYPE_U16,
        [TYPE_U16][TYPE_U32] = TYPE_U32,
        [TYPE_U16][TYPE_U64] = TYPE_U64,
        [TYPE_U32][TYPE_I8]  = TYPE_U32,
        [TYPE_U32][TYPE_I16] = TYPE_U32,
        [TYPE_U32][TYPE_I32] = TYPE_U32,
        [TYPE_U32][TYPE_I64] = TYPE_I64,
        [TYPE_U32][TYPE_U8]  = TYPE_U32,
        [TYPE_U32][TYPE_U16] = TYPE_U32,
        [TYPE_U32][TYPE_U32] = TYPE_U32,
        [TYPE_U32][TYPE_U64] = TYPE_U64,
        [TYPE_U64][TYPE_I8]  = TYPE_U64,
        [TYPE_U64][TYPE_I16] = TYPE_U64,
        [TYPE_U64][TYPE_I32] = TYPE_U64,
        [TYPE_U64][TYPE_I64] = TYPE_U64,
        [TYPE_U64][TYPE_U8]  = TYPE_U64,
        [TYPE_U64][TYPE_U16] = TYPE_U64,
        [TYPE_U64][TYPE_U32] = TYPE_U64,
        [TYPE_U64][TYPE_U64] = TYPE_U64,
    };
    return ranks[get_sized_int(type1)][get_sized_int(type2)];
}
