#include <assert.h>
#include <inttypes.h>  // PRId64.
#include <stdio.h>  // snprintf.

#include "lexel.h"

#include "crvic_resources.h"
#include "crvic_string_buffer.h"
#include "crvic_type.h"

#define TYPE_TABLE_CAPACITY 1024

#define TYPE_ENTRY_KIND(NAME, KIND, SIZE, REPR) \
    [NAME] = {.id = NAME, .kind = KIND, .size = SIZE, .repr = LXL_SV_FROM_STRLIT(REPR)}

#define TYPE_ENTRY_PRIM(NAME, SIZE, REPR) \
    TYPE_ENTRY_KIND(NAME, KIND_PRIMITIVE, SIZE, REPR)

static struct type_info types[TYPE_TABLE_CAPACITY] = {
    TYPE_ENTRY_KIND(TYPE_NO_TYPE, KIND_NO_KIND, 0,      "<No type>"),
    TYPE_ENTRY_PRIM(TYPE_TYPE_EXPR,     0,              "<Type expression>"),
    TYPE_ENTRY_PRIM(TYPE_ABSURD,        0,              "!"),
    TYPE_ENTRY_PRIM(TYPE_UNIT,          0,              "()"),
    TYPE_ENTRY_PRIM(TYPE_NULLPTR_TYPE,  VIC_PTR_SIZE,   "type_of(null)"),
    TYPE_ENTRY_PRIM(TYPE_CONST_BOOL,    1,              "<Constant bool>"),
    TYPE_ENTRY_PRIM(TYPE_CONST_INT,     8,              "<Constant int>"),
    TYPE_ENTRY_PRIM(TYPE_BOOL,          1,              "bool"),
    TYPE_ENTRY_PRIM(TYPE_I8,            1,              "i8"),
    TYPE_ENTRY_PRIM(TYPE_I16,           2,              "i16"),
    TYPE_ENTRY_PRIM(TYPE_I32,           4,              "i32"),
    TYPE_ENTRY_PRIM(TYPE_I64,           8,              "i64"),
    TYPE_ENTRY_PRIM(TYPE_U8,            1,              "u8"),
    TYPE_ENTRY_PRIM(TYPE_U16,           1,              "u16"),
    TYPE_ENTRY_PRIM(TYPE_U32,           4,              "u32"),
    TYPE_ENTRY_PRIM(TYPE_U64,           8,              "u64"),
    TYPE_ENTRY_PRIM(TYPE_STRING,        VIC_PTR_SIZE,   "string"),
    TYPE_ENTRY_PRIM(TYPE_C_STRING,      VIC_PTR_SIZE,   "c_string"),
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

TypeID get_or_add_type(struct type_info info) {
    for (int i = 0; i < type_count; ++i) {
        if (types_equal(get_type(i), &info)) return i;
    }
    init_type(&info);
    return add_type(info);
}

void init_type(struct type_info *info) {
    switch (info->kind) {
    case KIND_ENUM:
        info->size = get_type_size(info->enum_.underlying_type);
        info->repr = make_enum_repr(info->enum_);
        break;
    case KIND_RECORD:
        info->size = calculate_record_size(info->record);
        info->repr = make_record_repr(info->record);
        break;
    case KIND_POINTER:
        info->size = VIC_PTR_SIZE;
        info->repr = make_pointer_repr(info->pointer);
        break;
    case KIND_ARRAY:
        info->size = info->array.count * get_type_size(info->array.dest_type);
        info->repr = make_array_repr(info->array);
        break;
    case KIND_FUNCTION:
        info->size = 0;  // Functions types have no meaningful size.
        info->repr = make_function_repr(info->function);
        break;
    case KIND_SLICE:
        info->size = VIC_PTR_SIZE + sizeof(VIC_INT);
        info->repr = make_slice_repr(info->slice);
        break;
    case KIND_UNION:
        info->size = calculate_union_size(info->union_);
        info->repr = make_union_repr(info->union_);
        break;
    case KIND_UNRESOLVED:
        info->size = 0;
        info->repr = LXL_SV_FROM_STRLIT("<Unresolved type>");
        break;
    case KIND_PRIMITIVE:
    case KIND_NO_KIND:
        UNREACHABLE();
    }
}

bool types_equal(struct type_info *a, struct type_info *b) {
    if (!a || !b)           return false;
    if (a->kind != b->kind) return false;
    switch (a->kind) {
    case KIND_UNRESOLVED:
    case KIND_NO_KIND:      return false; // ???
    case KIND_PRIMITIVE:    return a->id == b->id;
    case KIND_ENUM:         return enum_types_equal(&a->enum_, &b->enum_);
    case KIND_RECORD:       return record_types_equal(&a->record, &b->record);
    case KIND_POINTER:      return pointer_types_equal(&a->pointer, &b->pointer);
    case KIND_ARRAY:        return array_types_equal(&a->array, &b->array);
    case KIND_FUNCTION:     return function_types_equal(&a->function, &b->function);
    case KIND_SLICE:        return slice_types_equal(&a->slice, &b->slice);
    case KIND_UNION:        return union_types_equal(&a->union_, &b->union_);
    }
}

bool enum_types_equal(struct enum_info *a, struct enum_info *b) {
    return a->underlying_type == b->underlying_type && DA_EQ(&a->fields, &b->fields);
}

bool record_types_equal(struct record_info *a, struct record_info *b) {
    return DA_EQ(&a->fields, &b->fields);
}

bool pointer_types_equal(struct pointer_info *a, struct pointer_info *b) {
    return a->kind == b->kind && a->rw == b->rw && a->dest_type == b->dest_type;

}

bool array_types_equal(struct array_info *a, struct array_info *b) {
    return a->count == b->count && a->rw == b->rw && a->dest_type == b->dest_type;
}

bool function_types_equal(struct function_info *a, struct function_info *b) {
    return a->sig->ret_type == b->sig->ret_type && DA_EQ(&a->sig->params, &b->sig->params);
}

bool slice_types_equal(struct slice_info *a, struct slice_info *b) {
    return a->rw == b->rw && a->dest_type == b->dest_type;
}

bool union_types_equal(struct union_info *a, struct union_info *b) {
    return DA_EQ(&a->fields, &b->fields);
}

struct lxl_string_view make_record_repr(struct record_info info) {
    assert(info.fields.count >= 1);  // An empty record is the unit type `()`.
    ptrdiff_t capacity = 32;
    char *repr = ALLOCATE(perm, capacity);
    char *ptr = repr;
    const char header[] = "record {";
    assert((ptrdiff_t)(sizeof header) <= capacity);
    memcpy(ptr, header, sizeof header - 1);
    ptr += sizeof header - 1;
    for (int i = 0; i < info.fields.count; ++i) {
        struct lxl_string_view name_sv = info.fields.items[i].name;
        struct lxl_string_view type_sv = get_type_sv(info.fields.items[i].type);
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

struct lxl_string_view make_enum_repr(struct enum_info info) {
    assert(info.fields.count >= 1);  // An empty enum is the absurd type `!`.
    ptrdiff_t capacity = 32;
    char *repr = ALLOCATE(perm, capacity);
    char *ptr = repr;
    const char header[] = "enum {";
    assert((ptrdiff_t)(sizeof header) <= capacity);
    memcpy(ptr, header, sizeof header - 1);
    ptr += sizeof header - 1;
    for (int i = 0; i < info.fields.count; ++i) {
        struct lxl_string_view name_sv = info.fields.items[i].name;
        int64_t value = info.fields.items[i].value;
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

static const char *get_modifier(enum rw_access rw) {
    switch (rw) {
    case RW_READ_ONLY:          return "";
    case RW_READ_WRITE:         return "mut "; // Note extra space.
    case RW_WRITE_BEFORE_READ:  return "out "; // Note extra space.
    }
}

struct lxl_string_view make_pointer_repr(struct pointer_info info) {
    const char *prefix = (info.kind == POINTER_PROPER) ? "^" : "[^]";
    const char *modifier = get_modifier(info.rw);
    struct lxl_string_view dest_sv = get_type_sv(info.dest_type);
    size_t repr_length = snprintf(NULL, 0, "%s%s"LXL_SV_FMT_SPEC,
                                  prefix, modifier, LXL_SV_FMT_ARG(dest_sv));
    char *repr = ALLOCATE(perm, repr_length + 1);
    snprintf(repr, repr_length + 1, "%s%s"LXL_SV_FMT_SPEC,
             prefix, modifier, LXL_SV_FMT_ARG(dest_sv));
    return (struct lxl_string_view) {.start = repr, .length = repr_length};
}

struct lxl_string_view make_array_repr(struct array_info info) {
    const char *modifier = get_modifier(info.rw);
    struct lxl_string_view dest_sv = get_type_sv(info.dest_type);
    size_t repr_length = snprintf(NULL, 0, "[%"PRId64"]%s"LXL_SV_FMT_SPEC,
                                  info.count, modifier, LXL_SV_FMT_ARG(dest_sv));
    char *repr = ALLOCATE(perm, repr_length + 1);
    snprintf(repr, repr_length + 1, "[%"PRId64"]%s"LXL_SV_FMT_SPEC,
             info.count, modifier, LXL_SV_FMT_ARG(dest_sv));
    return (struct lxl_string_view) {.start = repr, .length = repr_length};
}

struct lxl_string_view make_function_repr(struct function_info info) {
    static struct string_buffer sb = {0};
    sb_clear(&sb);
    sb_add_string(&sb, "func (");
    sb_add_string(&sb, ")");
    if (info.sig->params.count >= 1) {
        struct type_decl *param = &info.sig->params.items[0];
        struct lxl_string_view param_type_sv = get_type_sv(param->type);
        sb_add_formatted(&sb, ""LXL_SV_FMT_SPEC": "LXL_SV_FMT_SPEC"",
                         LXL_SV_FMT_ARG(param->name), LXL_SV_FMT_ARG(param_type_sv));
    }
    for (int i = 1; i < info.sig->params.count; ++i) {
        struct type_decl *param = &info.sig->params.items[i];
        struct lxl_string_view param_type_sv = get_type_sv(param->type);
        sb_add_formatted(&sb, ""LXL_SV_FMT_SPEC": "LXL_SV_FMT_SPEC"",
                         LXL_SV_FMT_ARG(param->name), LXL_SV_FMT_ARG(param_type_sv));
    }
    if (info.sig->ret_type != TYPE_UNIT) {
        struct lxl_string_view ret_sv = get_type_sv(info.sig->ret_type);
        sb_add_formatted(&sb, " -> "LXL_SV_FMT_SPEC"", LXL_SV_FMT_ARG(ret_sv));
    }
    return sb_export(&sb, ALLOCATOR_ARD2AD(perm));
}

struct lxl_string_view make_slice_repr(struct slice_info info) {
    const char *modifier = get_modifier(info.rw);
    struct lxl_string_view dest_sv = get_type_sv(info.dest_type);
    size_t repr_length = snprintf(NULL, 0, "[]%s"LXL_SV_FMT_SPEC"", modifier, LXL_SV_FMT_ARG(dest_sv));
    char *repr = ALLOCATE(perm, repr_length + 1);
    snprintf(repr, repr_length + 1, "[]%s"LXL_SV_FMT_SPEC"", modifier, LXL_SV_FMT_ARG(dest_sv));
    return (struct lxl_string_view) {.start = repr, .length = repr_length};
}

struct lxl_string_view make_union_repr(struct union_info info) {
    assert(info.fields.count >= 1);  // An empty union is the absurd type `!`.
    ptrdiff_t capacity = 32;
    char *repr = ALLOCATE(perm, capacity);
    char *ptr = repr;
    const char header[] = "union {";
    assert((ptrdiff_t)(sizeof header) <= capacity);
    memcpy(ptr, header, sizeof header - 1);
    ptr += sizeof header - 1;
    for (int i = 0; i < info.fields.count; ++i) {
        struct lxl_string_view name_sv = info.fields.items[i].name;
        struct lxl_string_view type_sv = get_type_sv(info.fields.items[i].type);
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
    // Overwrite last ', ' with '}\0'. We catch the empty union case at the top of the function,
    // so this is well-defined.
    ptr[-2] = '}';
    ptr[-1] = '\0';
    // Shrink allocation to avoid over-allocation.
    repr = REALLOCATE(perm, repr, ptr - repr, capacity);
    assert(repr);
    return lxl_sv_from_startend(repr, ptr);
}

size_t calculate_record_size(struct record_info info) {
    size_t size = 0;
    for (int i = 0; i < info.fields.count; ++i) {
        size += get_type(info.fields.items[i].type)->size;
    }
    return size;
}

size_t calculate_union_size(struct union_info info) {
    assert(info.fields.count >= 1);
    size_t size = get_type_size(info.fields.items[0].type);
    for (int i = 1; i < info.fields.count; ++i) {
        size_t field_size = get_type_size(info.fields.items[i].type);
        if (field_size > size) {
            size = field_size;
        }
    }
    return size;
}

TypeID get_record_field_type(struct record_info info, struct lxl_string_view field_name) {
    for (int i = 0; i < info.fields.count; ++i) {
        if (lxl_sv_equal(info.fields.items[i].name, field_name)) return info.fields.items[i].type;
    }
    return TYPE_NO_TYPE;
}

bool get_enum_field_value(struct enum_info info, struct lxl_string_view field_name, VIC_INT *OUT_value) {
    for (int i = 0; i < info.fields.count; ++i) {
        if (lxl_sv_equal(info.fields.items[i].name, field_name)) {
            *OUT_value = info.fields.items[i].value;
            return true;
        }
    }
    return false;
}

bool type_is_kind(TypeID type, enum kind kind) {
    struct type_info *info = get_type(type);
    assert(info);
    return info->kind == kind;
}

bool is_integer_type(TypeID type) {
    static_assert(TYPE_I8 < TYPE_U8, "Signed types assumed before unsigned");
    return type == TYPE_CONST_INT || (TYPE_I8 <= type && type <= TYPE_U64);
}

bool is_ordered_type(TypeID type) {
    // Only numbers are ordered.
    // TODO: Should we allow compound types to be compared element-wise?
    // What about strings?
    return is_integer_type(type);
}

bool is_array_like_type(TypeID type) {
    struct type_info *info = get_type(type);
    return info->kind == KIND_ARRAY
        || info->kind == KIND_SLICE
        || (info->kind == KIND_POINTER && info->pointer.kind == POINTER_ARRAY_LIKE)
        ;
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

size_t get_type_size(TypeID type) {
    struct type_info *info = get_type(type);
    assert(info);
    return info->size;
}

TypeID max_type_rank(TypeID type1, TypeID type2) {
    if (!is_integer_type(type1) && !is_integer_type(type2)) return TYPE_NO_TYPE;
    assert(type1 < TYPE_PRIMITIVE_COUNT && type2 < TYPE_PRIMITIVE_COUNT);
    if (type1 == TYPE_CONST_INT && type2 == TYPE_CONST_INT) return TYPE_CONST_INT;
    if (type1 == TYPE_CONST_INT) type1 = type2;
    if (type2 == TYPE_CONST_INT) type2 = type1;
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
