CC=clang
LD=ld
ASM_BIN=uuz-asm
C_BIN=uuz-c
ASM_SRC=src-asm
C_SRC=src-c
ASMFLAGS=
CFLAGS=-Wall -Wextra -Werror -pedantic -std=c2x -march=native
LDFLAGS=

ifdef DEBUG
	CFLAGS+=-g
	ASMFLAGS+=-g -DDEBUG
else ifdef DEBUGO3
	CFLAGS+=-g -O3
else
	CFLAGS+=-s -O3
	LDFLAGS+=-s
endif

ifdef DRYRUN
	ASMFLAGS+= -DDRYRUN
endif

.PHONY: all clean

all: $(ASM_BIN) $(C_BIN)

$(ASM_BIN).o: $(ASM_SRC)/$(ASM_BIN).S $(ASM_SRC)/*.inc Makefile
	$(CC) $(ASMFLAGS) -c $< -o $@

$(ASM_BIN): $(ASM_BIN).o
	$(LD) $(LDFLAGS) -n $< -o $@
ifndef DEBUG
	objcopy -R '.*' --keep-section '.text' $@
endif

$(C_BIN): $(C_SRC)/*.c
	$(CC) $(CFLAGS) $? -o $@

clean:
	rm -f $(ASM_BIN)
	rm -f $(C_BIN)
	rm -f *.o
