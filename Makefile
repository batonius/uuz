CC=clang
LD=ld
BIN=uuz

ifdef DEBUG
	CCFLAGS=-g -DDEBUG
else
	CCLFAGS=
endif

.SUFFIXES:
.PHONY: all clean

all: $(BIN)

$(BIN).o: src/$(BIN).S src/*.inc Makefile
	$(CC) $(CCFLAGS) -c $< -o $@

$(BIN): $(BIN).o
ifdef DEBUG
	$(LD) -n $< -o $@
else
	$(LD) -s -n $< -o $@
	objcopy -R '.*' --keep-section '.text' $@
endif

clean:
	rm -f $(BIN)
	rm -f $(BIN).o
