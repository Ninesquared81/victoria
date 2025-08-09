# bootsrap

This directory contains all the files necessary to bootstrap the compiler.

The bootstrap process as follows:

* Compile restricted Victoria (rVic) compiler, which is itself written in C (crvic).

* Compile (full) Victoria compiler written in rVic (vrvic) to C using crvic, then compile the
generated C code.

* Use vrvic to compile the self-hosted Victoria compiler (vicc).

Lexing is currently done using [lexel](https://github.com/Ninesquared81/lexel) (my own lexing library).
Lexel is sufficient for now so will likely be used even by the self-hosted compiler (at first, at least).
The file `lexer.c` provides a convenient wrapper around lexel which makes calling it from Victoria easier
(we won't have to worry about the lexer struct itself, only the token struct).

## rVic

Restricted Victoria (rVic) is a minimal subset of Victoria. That is, it is forward-compatible with the full
Victoria langauge. Its full feature list is summarised below (ticked off when they're implemented in the
bootstrap compiler).

- [x] External function declaration.

- [x] (Internal) function definition/declaration.

- [x] Function calls (internal/external). NOTE: ~~must be declared before call~~
can be declared in any order.

- [x] Integer types (`i8`&ndash;`i64`, `u8`&ndash;`u16`, `int`, `uint`).

- [x] Object pointer types (`^T`, `^mut T`).

- [x] Function ~~pointer~~ types (NOTE: function pointers are an emergent feature from
the confluence of function types and pointer types).

- [x] Array types.

- [x] Record types.

- [x] Enum types.

### Features deemed necessary after starting compiler:

- [x] Slices.

- [x] Command line arguments (`args: []string`).

- [x] String type.

- [x] `count_of()`.

- [x] `while` loops.

- [x] Binary `-` operator.

- [x] Logical NOT operator `!`.

- [x] Logical operators `and`, `or`.
