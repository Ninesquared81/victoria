# victoria
A C-like systems programming language for my own needs

## Language overview

NOTE: The following overview is mainly so I can get some ideas on (virtual) paper. The actual implementation
is nowhere near what is described here. I may even remove stuff from here once I have proper docs.

### Pointers

A pointer to type T is deonted by the syntax following syntax `@` _qualifiers_ `T`.

For example, `@u8` deontes a read-only pointer to an unsigned byte (unsigned 8-bit integer).

The type `void` may be used to denote a pointer to unknown type. A pointer to `void` assumes its
pointed-to value is byte-aligned. This can be overridden, however.

#### Qualifiers

A pointer type may include qualifiers. Several qualifiers may be combined, but some are mutually-exclusive.
They are spearated into different categories.

###### Read-/Write-access qualifiers

These qualifiers affect how pointers can be used in derefence operations.

* _no qualifier_ &rarr; Read-only pointer. The pointed-to value cannot be assigned to.

* `mut` &rarr; Read and write. The pointed to value may be mutated freely.

* `out` &rarr; Write before read. The pointed to value must be assigned to before it can be read from.
This is usually used in function parameter lists to denote an out paramater &ndash; a pointer argument
whose pointed-to value is meangingless when the function is called and will be written to by the function.
Once an `out`-qualified pointer has been written to in the scope in which it is defined (along all execution
paths), it acts like a `mut`-qualified pointer.

These three qualifiers are all mutually exclusive with each other.

###### Alignemnt qualifiers

An alignment-qualified pointer points to value which may have stricter alignment requirements than the
pointed-to type.

The syntax for an alignment-qualified pointer is as follows `@%A T`. This denotes a pointer to type `T`
with the same alignment requirements as type `A`. Note that `@` and `%` are treated as separate tokens, so
need not be directly juxtaposed.

An alignment-qualified pointer cannot specify a weaker alignment requirement than its
pointed-to type.

At most one alignment qualifier is allowed per pointer. A pointer with no alignment qualifier acts like
a pointer qualified as `@%T T`, i.e., it assumes its pointed-to value is aligned per its own alignment
requirements.


###### Relative pointer qualifiers

A relative-qualified pointer turns an absolute pointer into relative pointer. Instead of storing an
absolute address, a realtive pointer stores an offset from its own address. The pointer itself may be stored
in any integer type, allowing for a smaller memory footprint than an absolute pointer, at the cost of
representable range.

The syntax is as follows `@~R T`, where `R` is the integer type to store the relative pointer as.

A relative pointer cannot store its address. This would be repesented as the integer zero, which is instead
interpreted as the special value `null`. A `null` relative pointer converts to a `null` absolute pointer and
vice-versa.

At most one relative pointer qualifier is allowed per pointer. A pointer with no relative qaulifier is an
absolute pointer.

The alignment requirement of a realtive pointer is the same as its underlying type.

##### Combining qualifiers

Qualifiers from different categories may be combined together. For example, a mutable pointer to
`u32`-aligned `void` is specified as `@%u32 mut void`. The pointed-to type must come last, but qualifiers
can be specified in any order, so `@mut %u32 void` is an equivalent declaration. Since `u32` and `i32` have
the same alignment requirement, `@%i32 mut void` may be used in any context where the previous type is
expected and vice-versa.
