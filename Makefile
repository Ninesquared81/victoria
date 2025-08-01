CC = clang

CFLAGS = -g

CRVIC = bootstrap/crvic.exe

BIN_DIR = bin
VIC = $(BIN_DIR)/vic.exe
MAIN_C = src/main.c
MAIN_VIC = src/main.vic
LEXER = bootstrap/lexer.c

.PHONY: all

all: $(VIC)

$(VIC): $(MAIN_C) $(LEXER) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(MAIN_C) $(LEXER) -o $(VIC)

$(MAIN_C): $(MAIN_VIC) $(CRVIC)
	$(CRVIC) $(MAIN_VIC)

bootstrap:
	$(MAKE) -C bootstrap

$(BIN_DIR):
	mkdir $(BIN_DIR)
