CC=clang
LD=ld
BIN=uuz
CFLAGS=
LDFLAGS=

ifdef DEBUG
	CFLAGS+= -g -DDEBUG
else
	LDFLAGS+= -s
endif

ifdef DRYRUN
	CFLAGS+= -DDRYRUN
endif

.SUFFIXES:
.PHONY: all clean

all: $(BIN)

$(BIN).o: src/$(BIN).S src/*.inc Makefile
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(BIN).o
	$(LD) $(LDFLAGS) -n $< -o $@
ifndef DEBUG
	objcopy -R '.*' --keep-section '.text' $@
endif

clean:
	rm -f $(BIN)
	rm -f $(BIN).o
