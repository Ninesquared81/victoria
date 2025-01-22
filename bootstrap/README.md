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
