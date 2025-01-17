CC = gcc
# -Wno-missing-braces because gcc does not seem to like initialiser list + C11 anonymous struct/union.
CFLAGS = -Wall -Werror -Wextra -pedantic -g -std=c11 -Wno-missing-braces

.PHONY: all

all: crvic.exe

crvic.exe: crvic.c crvic_backend.c crvic_ast.h crvic_backend.h
	$(CC) $(CFLAGS) crvic.c crvic_backend.c -o crvic.exe

out.c: crvic.exe
	crvic.exe

out.exe: out.c
	$(CC) out.c -o out.exe
