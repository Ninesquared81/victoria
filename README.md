# victoria
A C-like systems programming language for my own needs

## Language overview

NOTE: The following overview is mainly so I can get some ideas on (virtual) paper. The actual implementation
is nowhere near what is described here. I may even remove stuff from here once I have proper docs.

### Pointers

A pointer to type T is deonted by the syntax following syntax `^` _modifiers_ `T`.

For example, `^u8` deontes a read-only pointer to an unsigned byte (unsigned 8-bit integer).

#### Generic pointers

The type `!` may be used as the pointed-to type to create a generic pointer. Generic pointers carry
no type information and cannot be dereferenced. A pointer to any type can be implcitly converted to
a generic pointer. N.B., a generic pointer assumes its pointed-to value is byte-aligned, although this
behaviout can be overridden (see [Alignment modifiers](#alignment-modifiers)).

#### Modifiers

A pointer type may include modifiers. Several modifiers may be combined, but some are mutually-exclusive.
They are separated into different categories.

###### Read-/Write-access modifiers

> [!NOTE]
> `out` modifiers currently have only partial support

These modifiers affect how pointers can be used in derefence operations.

* _no modifier_ &rarr; Read-only pointer. The pointed-to value cannot be assigned to.

* `mut` &rarr; Read and write. The pointed to value may be mutated freely.

* `out` &rarr; Write before read. The pointed to value must be assigned to before it can be read from.
This is usually used in function parameter lists to denote an out paramater &ndash; a pointer argument
whose pointed-to value is meangingless when the function is called and will be written to by the function.
Once an `out` pointer has been written to in the scope in which it is defined (along all execution
paths), it acts like a `mut` pointer.

These three modifiers are all mutually exclusive with each other.

###### Alignment modifiers

> [!NOTE]
> Alignment modifiers are not yet supported

These modifiers turn a normal pointer into a specific-alignment pointer. A specific-alignment pointer
points to a value which may have stricter alignment requirements than the pointed-to type.

The syntax for a specific-alignment pointer is as follows `^%A T`. This denotes a pointer to type `T`
with the same alignment requirements as type `A`. Note that `^` and `%` are treated as separate tokens, so
need not be directly juxtaposed.

A specific-alignment pointer cannot specify a weaker alignment requirement than its pointed-to type.

At most one alignment modifier is allowed per pointer. A pointer with no alignment modifier acts like
a pointer specified thusly, `^%T T`, i.e., it assumes its pointed-to value is aligned per its own alignment
requirements.

##### Combining modifiers

Modifiers from different categories may be combined together. For example, a mutable generic pointer to
`u32`-aligned data is specified as `^%u32 mut!`. The pointed-to type must come last, but modifiers
can be specified in any order, so `^mut %u32!` is an equivalent declaration. Since `u32` and `i32` have
the same alignment requirement, `^%i32 mut!` may be used in any context where the previous type is
expected and vice-versa.
