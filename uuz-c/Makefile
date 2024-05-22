CC=clang
CFLAGS=-Wall -Wextra -Werror -pedantic -std=c2x -march=native -fuse-ld=mold
BIN=uuz

ifdef DEBUG
	CFLAGS+=-g
else ifdef DEBUGO3
	CFLAGS+=-g -O3
else
	CFLAGS+=-s -O3
endif

.PHONY: all clean

all: $(BIN)

$(BIN): *.c
	$(CC) $(CFLAGS) $? -o $@

clean:
	rm -rf $(BIN)
