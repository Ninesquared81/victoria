# bootsrap

This directory contains all the files necessary to bootstrap the compiler.

The bootstrap process as follows:

* Compile restricted Victoria (rVic) compiler, which is itself written in C (crvic).

* Compile (full) Victoria compiler written in rVic (vrvic) to C using rvic-c, then compile the
generated C code.

* Use vrvic to compile the self-hosted Victoria compiler (vicc).
