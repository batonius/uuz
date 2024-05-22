# uuz (µ-unzip)
A minimal viable `unzip` clone written in x86-64 assembler for Linux, and then ported to C. 

## What
The repo contains two implementations of the same algorithm both in x86-64 asm and C. 
The asm version was written first and is a 5kb libc-free static x86-64 ELF binary calling Linux syscalls directly.
The C version is a straightforward port using libc, tested on both x86-64 and aarch64.

## Performance
Using UnZip 6.0 as a yardstick, the performance is as follows:
- On x86-64 machine (Ryzen 3700X):
  1. `uuz-asm` is 1.3x faster than `unzip`.
  1. `uuz-c` is 1.4x faster than `unzip`, with both `gcc` and `clang`.
- On aarch64 machine (Cortex-A76, Raspberry Pi 5):
  1. `uuz-c` is 1.3 faster than `unzip`, with both `gcc` and `clang`, if I ignore the bizarre Micro-SD performance lags.

Note: the comparison with `unzip` is just for scale, in no way the functionality is comparable.

## Limitations
Both versions unpack only the first file in the archive.
**Neither version has bound checks, malformed archives will result in memory corruption.**

## How
Clang knows how to assemble .S files, so no external assembler like nasm is required.
The asm version is completely inlined and heavily unrolled, resulting in measurable performance benefits at the cost of an unnecessarily large disk size.
By replacing often-used macros with function calls and limiting loop unrolling I think it's possible to reduce the size of the binary to ~3kb.
The DEFLATE implementation is based on the description in [RFC1951](https://datatracker.ietf.org/doc/html/rfc1951), to simplify the code both versions just `mmap` input and output files, which is somewhat suboptimal.

## Why
20 years ago I attempted to implement an unzipper in [MASM32](https://masm32.com) but got lost in the process.
20 years later I thought it is a good little project to dust off my assembly skills.

## Conclusion
The compilers are better at writing an efficient asm code than me. Duh.
