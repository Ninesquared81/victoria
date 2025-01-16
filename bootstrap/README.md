# bootsrap

This directory contains all the files necessary to bootstrap the compiler.

The bootstrap process as follows:

* Compile restricted Victoria (rVic) compiler, which is itself written in C (rvic-c).

* Compile (full) Victoria compiler written in rVic (vic-rv) to C using rvic-c, then compile the
generated C code.

* Use rvic-v to compile the self-hosted Victoria compiler (vicc).
