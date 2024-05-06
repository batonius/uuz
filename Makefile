CC=clang
LD=ld
BIN=uuz

.SUFFIXES:
.PHONY: all clean

all: $(BIN)

$(BIN).o: src/$(BIN).S src/*.inc Makefile
	$(CC) -c $< -o $@

$(BIN): $(BIN).o
	# objcopy -R '.*' --keep-section '.text' $<
	# $(LD) -s -n $< -o $@
	# objcopy -R '.*' --keep-section '.text' $@
	$(LD) -n $< -o $@

clean:
	rm -f $(BIN)
	rm -f $(BIN).o
