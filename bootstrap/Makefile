CC = gcc
CFLAGS = -Wall -Werror -Wextra -pedantic -g

.PHONY: all

all: crvic.exe

crvic.exe: crvic.c crvic_backend.c crvic_ast.h crvic_backend.h
	$(CC) $(CFLAGS) crvic.c crvic_backend.c -o crvic.exe
