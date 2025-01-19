#ifndef CRVIC_TYPE_H
#define CRVIC_TYPE_H

enum type_primitive {
    TYPE_NO_TYPE,      // Sentinel type denoting no type.
    TYPE_ABSURD,       // Absurd type `!`. Used to denote expressions that do not return.
    TYPE_UNIT,         // Unit type `()`. Used to denote an expression with no meaningful value.
    TYPE_U8,           // Unsigned 8-bit integer.
    TYPE_U16,          // Unsigned 16-bit integer.
    TYPE_U32,          // Unsigned 32-bit integer.
    TYPE_U64,          // Unsigned 64-bit integer.
    TYPE_S8,           // Signed 8-bit integer.
    TYPE_S16,          // Signed 16-bit integer.
    TYPE_S32,          // Signed 32-bit integer.
    TYPE_S64,          // Signed 64-bit integer.
};

typedef int TypeID;   // Type for types.

#endif
