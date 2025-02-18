#include <assert.h>

#include "crvic_type.h"

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
    assert(type < TYPE_PRIMITIVE_COUNT);
    switch ((enum type_primitive)type) {
    case TYPE_NO_TYPE: return LXL_SV_FROM_STRLIT("<No type>");
    case TYPE_ABSURD:  return LXL_SV_FROM_STRLIT("!");
    case TYPE_UNIT:    return LXL_SV_FROM_STRLIT("()");
    case TYPE_I8:      return LXL_SV_FROM_STRLIT("i8");
    case TYPE_I16:     return LXL_SV_FROM_STRLIT("i16");
    case TYPE_I32:     return LXL_SV_FROM_STRLIT("i32");
    case TYPE_I64:     return LXL_SV_FROM_STRLIT("i64");
    case TYPE_U8:      return LXL_SV_FROM_STRLIT("u8");
    case TYPE_U16:     return LXL_SV_FROM_STRLIT("u16");
    case TYPE_U32:     return LXL_SV_FROM_STRLIT("u32");
    case TYPE_U64:     return LXL_SV_FROM_STRLIT("u64");
    case TYPE_INT:     return LXL_SV_FROM_STRLIT("int");
    case TYPE_UINT:    return LXL_SV_FROM_STRLIT("uint");
    // Not a type:
    case TYPE_PRIMITIVE_COUNT:
        break;
    }
    UNREACHABLE();
    return LXL_SV_FROM_STRLIT("<TYPE_PRIMITIVE_COUNT: not a valid type>");
}

TypeID get_sized_int(TypeID type) {
    if (!is_integer_type(type)) return TYPE_NO_TYPE;
    if (type == TYPE_INT)  return (PTR_WIDTH == 64) ? TYPE_I64 : TYPE_I32;
    if (type == TYPE_UINT) return (PTR_WIDTH == 64) ? TYPE_U64 : TYPE_U32;
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
