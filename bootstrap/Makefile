CC = clang

BASIC_FLAGS := -Wall -Werror -Wextra -Wimplicit-fallthrough -pedantic -g -std=c11 -D__USE_MINGW_ANSI_STDIO=1
CLANG_FLAGS := -fsanitize=undefined -fsanitize-trap -target x86_64-w64-mingw32

ifeq ($(CC),clang)
  CFLAGS = $(BASIC_FLAGS) $(CLANG_FLAGS)
else
  CFLAGS = $(BASIC_FLAGS)
endif

.PHONY: all

all: crvic.exe

CRVIC_SOURCES = $(wildcard crvic*.c)
CRVIC_HEADERS = $(wildcard crvic*.h)

crvic.exe: $(CRVIC_SOURCES) $(CRVIC_HEADERS) lexer.c lexel.h region.c region.h ubiqs.h
	$(CC) $(CFLAGS) $(CRVIC_SOURCES) lexer.c region.c -o crvic.exe

out.c: crvic.exe in.vic
	crvic.exe in.vic out.c

out.exe: out.c
	$(CC) out.c -o out.exe
