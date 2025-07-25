#include <assert.h>
#include <inttypes.h>  // PRId64.
#include <stdio.h>  // snprintf.

#include "lexel.h"

#include "crvic_resources.h"
#include "crvic_type.h"

#define TYPE_TABLE_CAPACITY 1024

static struct type_info types[TYPE_TABLE_CAPACITY] = {
    [TYPE_NO_TYPE] = {.id = TYPE_NO_TYPE, .kind = KIND_NO_KIND,   .size = 0, .repr = LXL_SV_FROM_STRLIT("<No type>")},
    [TYPE_TYPE_EXPR] = {.id = TYPE_TYPE_EXPR, .kind = KIND_PRIMITIVE, .size = 0, .repr = LXL_SV_FROM_STRLIT("<Type expression>")},
    [TYPE_ABSURD]  = {.id = TYPE_ABSURD,  .kind = KIND_PRIMITIVE, .size = 0, .repr = LXL_SV_FROM_STRLIT("!")},
    [TYPE_UNIT]    = {.id = TYPE_UNIT,    .kind = KIND_PRIMITIVE, .size = 0, .repr = LXL_SV_FROM_STRLIT("()")},
    [TYPE_NULLPTR_TYPE] = {.id = TYPE_NULLPTR_TYPE, .kind = KIND_PRIMITIVE, .size = sizeof(VIC_INT), .repr = LXL_SV_FROM_STRLIT("nullptr_t")},
    [TYPE_I8]      = {.id = TYPE_I8,      .kind = KIND_PRIMITIVE, .size = 1, .repr = LXL_SV_FROM_STRLIT("i8")},
    [TYPE_I16]     = {.id = TYPE_I16,     .kind = KIND_PRIMITIVE, .size = 2, .repr = LXL_SV_FROM_STRLIT("i16")},
    [TYPE_I32]     = {.id = TYPE_I32,     .kind = KIND_PRIMITIVE, .size = 4, .repr = LXL_SV_FROM_STRLIT("i32")},
    [TYPE_I64]     = {.id = TYPE_I64,     .kind = KIND_PRIMITIVE, .size = 8, .repr = LXL_SV_FROM_STRLIT("i64")},
    [TYPE_U8]      = {.id = TYPE_U8,      .kind = KIND_PRIMITIVE, .size = 1, .repr = LXL_SV_FROM_STRLIT("u8")},
    [TYPE_U16]     = {.id = TYPE_U16,     .kind = KIND_PRIMITIVE, .size = 1, .repr = LXL_SV_FROM_STRLIT("u16")},
    [TYPE_U32]     = {.id = TYPE_U32,     .kind = KIND_PRIMITIVE, .size = 4, .repr = LXL_SV_FROM_STRLIT("u32")},
    [TYPE_U64]     = {.id = TYPE_U64,     .kind = KIND_PRIMITIVE, .size = 8, .repr = LXL_SV_FROM_STRLIT("u64")},
    [TYPE_C_STRING] = {.id = TYPE_C_STRING, .kind = KIND_PRIMITIVE, .size = sizeof(VIC_INT), .repr = LXL_SV_FROM_STRLIT("c_string")},
    /* ... Other types to be filled in later ... */
};

static int type_count = TYPE_PRIMITIVE_COUNT;
static_assert(TYPE_PRIMITIVE_COUNT <= TYPE_TABLE_CAPACITY, "Increase type table capacity");

TypeID add_type(struct type_info info) {
    assert(type_count < TYPE_TABLE_CAPACITY && "We need a bigger type table!");
    info.id = type_count;
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

struct type_info make_record_type(struct type_decl_list fields) {
    return (struct type_info) {
        .kind = KIND_RECORD,
        .repr = make_record_repr(fields),
        .size = calculate_record_size(fields),
        .record_type = {.fields = fields}};
}

size_t calculate_record_size(struct type_decl_list fields) {
    size_t size = 0;
    for (int i = 0; i < fields.count; ++i) {
        size += get_type(fields.items[i].type)->size;
    }
    return size;
}

struct lxl_string_view make_record_repr(struct type_decl_list fields) {
    if (fields.count == 0) return LXL_SV_FROM_STRLIT("record {}");
    ptrdiff_t capacity = 32;
    char *repr = ALLOCATE(perm, capacity);
    char *ptr = repr;
    const char header[] = "record {";
    assert((ptrdiff_t)(sizeof header) <= capacity);
    memcpy(ptr, header, sizeof header - 1);
    ptr += sizeof header - 1;
    for (int i = 0; i < fields.count; ++i) {
        struct lxl_string_view name_sv = fields.items[i].name;
        struct lxl_string_view type_sv = get_type_sv(fields.items[i].type);
        // NOTE: +2 for ': ', +2 for ', '.
        ptrdiff_t needed_size = name_sv.length + 2 + type_sv.length + 2;
        ptrdiff_t count = ptr - repr;
        if (count + needed_size > capacity) {
            ptrdiff_t old_capacity = capacity;
            capacity += capacity + needed_size;
            assert(capacity > count + needed_size);
            repr = REALLOCATE(perm, repr, capacity, old_capacity);
            assert(repr);
        }
        memcpy(ptr, name_sv.start, name_sv.length);
        ptr += name_sv.length;
        *ptr++ = ':';
        *ptr++ = ' ';
        memcpy(ptr, type_sv.start, type_sv.length);
        ptr += type_sv.length;
        *ptr++ = ',';
        *ptr++ = ' ';
    }
    // Overwrite last ', ' with '}\0'. We catch the empty record case at the top of the function,
    // so this is well-defined.
    ptr[-2] = '}';
    ptr[-1] = '\0';
    // Shrink allocation to avoid over-allocation.
    repr = REALLOCATE(perm, repr, ptr - repr, capacity);
    assert(repr);
    return lxl_sv_from_startend(repr, ptr);
}

TypeID get_record_field_type(struct type_decl_list fields, struct lxl_string_view field_name) {
    for (int i = 0; i < fields.count; ++i) {
        if (lxl_sv_equal(fields.items[i].name, field_name)) return fields.items[i].type;
    }
    return TYPE_NO_TYPE;
}


TypeID find_enum_type(TypeID underlying_type, struct enum_field_list fields) {
    for (int i = TYPE_PRIMITIVE_COUNT; i < type_count; ++i) {
        struct type_info *info = get_type(i);
        if (info->kind == KIND_ENUM
            && info->enum_type.underlying_type == underlying_type
            && DA_EQ(&info->enum_type.fields, &fields)
            ) return i;
    }
    return TYPE_NO_TYPE;
}

struct type_info make_enum_type(TypeID underlying_type, struct enum_field_list fields) {
    struct type_info *underlying_info = get_type(underlying_type);
    assert(underlying_info != NULL);
    return (struct type_info) {
        .kind = KIND_ENUM,
        .size = underlying_info->size,
        .repr = make_enum_repr(fields),
        .enum_type = {
            .underlying_type = underlying_type,
            .fields = fields}};
}

struct lxl_string_view make_enum_repr(struct enum_field_list fields) {
    if (fields.count == 0) return LXL_SV_FROM_STRLIT("enum {}");
    ptrdiff_t capacity = 32;
    char *repr = ALLOCATE(perm, capacity);
    char *ptr = repr;
    const char header[] = "enum {";
    assert((ptrdiff_t)(sizeof header) <= capacity);
    memcpy(ptr, header, sizeof header - 1);
    ptr += sizeof header - 1;
    for (int i = 0; i < fields.count; ++i) {
        struct lxl_string_view name_sv = fields.items[i].name;
        int64_t value = fields.items[i].value;
        int value_length = snprintf(NULL, 0, " := %"PRId64", ", value);
        // NOTE: extra chars habdled in snprintf().
        ptrdiff_t needed_size = name_sv.length + value_length;
        ptrdiff_t count = ptr - repr;
        if (count + needed_size > capacity) {
            ptrdiff_t old_capacity = capacity;
            capacity += capacity + needed_size;
            assert(capacity > count + needed_size);
            repr = REALLOCATE(perm, repr, capacity, old_capacity);
            assert(repr);
        }
        memcpy(ptr, name_sv.start, name_sv.length);
        ptr += name_sv.length;
        ptr += snprintf(ptr, capacity - count, " := %"PRId64", ", value);
    }
    // Overwrite last ', ' with '}\0'. We catch the empty enum case at the top of the function,
    // so this is well-defined.
    ptr[-2] = '}';
    ptr[-1] = '\0';
    // Shrink allocation to avoid over-allocation.
    repr = REALLOCATE(perm, repr, ptr - repr, capacity);
    assert(repr);
    return lxl_sv_from_startend(repr, ptr);
}

bool get_enum_field_value(struct enum_field_list fields, struct lxl_string_view field_name,
                          VIC_INT *OUT_value) {
    for (int i = 0; i < fields.count; ++i) {
        if (lxl_sv_equal(fields.items[i].name, field_name)) {
            *OUT_value = fields.items[i].value;
            return true;
        }
    }
    return false;
}

TypeID find_pointer_type(enum pointer_kind kind, enum rw_access rw, TypeID dest_type) {
    for (int i = TYPE_PRIMITIVE_COUNT; i < type_count; ++i) {
        struct type_info *info = get_type(i);
        assert(info);
        if (info->kind == KIND_POINTER
            && info->pointer_type.kind == kind
            && info->pointer_type.rw == rw
            && info->pointer_type.dest_type == dest_type) {
            return i;
        }
    }
    return TYPE_NO_TYPE;
}

struct type_info make_pointer_type(enum pointer_kind kind, enum rw_access rw, TypeID dest_type) {
    return (struct type_info) {
        .kind = KIND_POINTER,
        .size = sizeof(VIC_INT),
        .repr = make_pointer_repr(kind, rw, dest_type),
        .pointer_type = {
            .kind = kind,
            .rw = rw,
            .dest_type = dest_type}};
}

struct lxl_string_view make_pointer_repr(enum pointer_kind kind, enum rw_access rw, TypeID dest_type) {
    const char *prefix = (kind == POINTER_PROPER) ? "^" : "[^]";
    struct lxl_string_view dest_sv = get_type_sv(dest_type);
    const char *modifier = "";
    switch (rw) {
    case RW_READ_ONLY: break;
    case RW_READ_WRITE:
        modifier = "mut ";  // Note extra space.
        break;
    case RW_WRITE_BEFORE_READ:
        modifier = "out ";  // Note extra space.
        break;
    }
    size_t repr_length = snprintf(NULL, 0, "%s%s"LXL_SV_FMT_SPEC,
                                  prefix, modifier, LXL_SV_FMT_ARG(dest_sv));
    char *repr = ALLOCATE(perm, repr_length + 1);
    snprintf(repr, repr_length + 1, "%s%s"LXL_SV_FMT_SPEC,
             prefix, modifier, LXL_SV_FMT_ARG(dest_sv));
    return (struct lxl_string_view) {.start = repr, .length = repr_length};
}

bool is_integer_type(TypeID type) {
    static_assert(TYPE_I8 < TYPE_U8, "Signed types assumed before unsigned");
    return TYPE_I8 <= type && type <= TYPE_U64;
}

enum signedness sign_of_type(TypeID type) {
    if (TYPE_I8 <= type && type <= TYPE_I64) return SIGN_SIGNED;
    if (TYPE_U8 <= type && type <= TYPE_U64) return SIGN_UNSIGNED;
    return SIGN_NO_SIGN;
}

struct lxl_string_view get_type_sv(TypeID type) {
    struct type_info *info = get_type(type);
    assert(info);
    return info->repr;
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
    return ranks[type1][type2];
}

struct iterator get_type_iterator(void) {
    return (struct iterator) {.ctx = &types[0], .next = type_iterator_next};
}

void *type_iterator_next(struct iterator *it) {
    if ((struct type_info *)it->ctx >= &types[type_count]) return NULL;
    struct type_info *info = it->ctx;
    it->ctx = info + 1;
    return info;
}
