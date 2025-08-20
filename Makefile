CC = gcc

CFLAGS = -g

CRVIC = bootstrap/crvic.exe

BIN_DIR = bin
VIC = $(BIN_DIR)/vic.exe
VIC_C = src/main.c
VIC_SRCS = $(wildcard src/*.vic)
LEXER = bootstrap/lexer.c

.PHONY: all bootstrap

all: $(VIC)

$(VIC): $(VIC_C) $(LEXER) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(VIC_C) $(LEXER) -o $(VIC)

$(VIC_C): $(VIC_SRCS) $(CRVIC)
	$(CRVIC) $(VIC_SRCS) -o $(VIC_C)

bootstrap:
	$(MAKE) -C bootstrap

$(BIN_DIR):
	mkdir $(BIN_DIR)
