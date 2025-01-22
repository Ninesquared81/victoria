CC = gcc
# -Wno-missing-braces because gcc does not seem to like initialiser list + C11 anonymous struct/union.
CFLAGS = -Wall -Werror -Wextra -pedantic -g -std=c11 -Wno-missing-braces -D__USE_MINGW_ANSI_STDIO=1

.PHONY: all

all: crvic.exe

CRVIC_SOURCES = $(wildcard crvic*.c)
CRVIC_HEADERS = $(wildcard crvic*.h)

crvic.exe: $(CRVIC_SOURCES) $(CRVIC_HEADERS) lexer.c lexel.h
	$(CC) $(CFLAGS) $(CRVIC_SOURCES) lexer.c -o crvic.exe

out.c: crvic.exe
	crvic.exe

out.exe: out.c
	$(CC) out.c -o out.exe
