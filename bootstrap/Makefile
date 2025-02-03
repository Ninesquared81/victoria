CC = clang

CFLAGS = -Wall -Werror -Wextra -pedantic -g -std=c11 -D__USE_MINGW_ANSI_STDIO=1 -fsanitize=undefined -fsanitize-trap

.PHONY: all

all: crvic.exe

CRVIC_SOURCES = $(wildcard crvic*.c)
CRVIC_HEADERS = $(wildcard crvic*.h)

crvic.exe: $(CRVIC_SOURCES) $(CRVIC_HEADERS) lexer.c lexel.h region.c region.h ubiqs.h
	$(CC) $(CFLAGS) $(CRVIC_SOURCES) lexer.c region.c -o crvic.exe

out.c: crvic.exe
	crvic.exe

out.exe: out.c
	$(CC) out.c -o out.exe
