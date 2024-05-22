#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define COMPRESSION_NO 0
#define COMPRESSION_FIXED 1
#define COMPRESSION_DYNAMIC 2

static const uint8_t LIT_EXTRA_BITS[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
                                         1, 1, 1, 2, 2, 2, 2, 3, 3, 3,
                                         3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

static const uint16_t LIT_EXTRA_OFFSETS[] = {
    0,  3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23,
    27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};

static const uint8_t DIST_EXTRA_BITS[] = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                          4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                          9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static const uint16_t DIST_EXTRA_OFFSETS[] = {
    1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
    33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};

static const uint8_t CLEN_ORDER[] = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                     11, 4,  12, 3, 13, 2, 14, 1, 15};

typedef struct {
    uint64_t bits_buffer;
    uint32_t bits_left;
    const char *bytes_buffer;
    size_t bytes_left;
} BitStream;

typedef struct {
    uint16_t prefixes[16];
    uint16_t offsets[16];
    uint16_t sorted_values[288];
    uint16_t min_length;
} HuffmanLookup;

static void bs_init(BitStream * /*out*/ bit_stream, const char *bytes_buffer,
                    size_t bytes_left) {
    bit_stream->bytes_left = bytes_left;
    bit_stream->bytes_buffer = bytes_buffer;
    bit_stream->bits_buffer = 0;
    bit_stream->bits_left = 0;
}

static void bs_ensure_enough_bits(BitStream * /*mut*/ bit_stream) {
    uint32_t next_dword;
    if (bit_stream->bits_left < sizeof(next_dword) * CHAR_BIT) {
        memcpy(&next_dword, bit_stream->bytes_buffer, sizeof(next_dword));
        bit_stream->bytes_buffer += sizeof(next_dword);
        bit_stream->bytes_left -= sizeof(next_dword);
        bit_stream->bits_buffer |= ((uint64_t)next_dword)
                                   << bit_stream->bits_left;
        bit_stream->bits_left += sizeof(next_dword) * CHAR_BIT;
    }
}

static uint32_t bs_read_lsbf_field(BitStream * /*mut*/ bit_stream,
                                   uint32_t width) {
    const uint32_t result = bit_stream->bits_buffer & ((1 << width) - 1);
    bit_stream->bits_buffer >>= width;
    bit_stream->bits_left -= width;
    return result;
}

static uint32_t bs_advace_msbf_field(BitStream * /*mut*/ bit_stream,
                                     uint32_t current_value) {
    current_value = (current_value << 1) | (bit_stream->bits_buffer & 1);
    bit_stream->bits_buffer >>= 1;
    bit_stream->bits_left -= 1;
    return current_value;
}

static uint32_t bs_read_msbf_field(BitStream * /*mut*/ bit_stream,
                                   uint32_t width) {
    uint32_t value = 0;
    for (size_t i = 0; i < width; ++i) {
        value = bs_advace_msbf_field(bit_stream, value);
    }
    return value;
}

static void bs_skip_bits_till_byte_boundary(BitStream * /*mut*/ bit_stream) {
    const uint32_t extra_bits = bit_stream->bits_left & 0x07;
    bit_stream->bits_left -= extra_bits;
    bit_stream->bits_buffer >>= extra_bits;
}

static const char *bs_get_bytes_buffer(BitStream * /*mut*/ bit_stream,
                                       uint32_t bytes_count) {
    const uint32_t extra_bytes = (bit_stream->bits_left >> 3);
    bit_stream->bytes_buffer -= extra_bytes;
    const char *result = bit_stream->bytes_buffer;
    bit_stream->bytes_buffer += bytes_count;
    bit_stream->bits_left = 0;
    return result;
}

static void hl_construct(HuffmanLookup * /*out*/ hl, const uint8_t *lengths,
                         uint32_t lengths_len) {
    for (size_t i = 0; i < lengths_len; ++i) {
        if (lengths[i] > 0) {
            hl->prefixes[lengths[i]] += 1;
        }
    }

    uint16_t prefix = 0;
    uint16_t offset = 0;
    for (size_t i = 1; i < 16; ++i) {
        const uint16_t values_count = hl->prefixes[i];
        if (values_count != 0 && hl->min_length == 0) {
            hl->min_length = i;
        }
        hl->offsets[i] = offset;
        hl->prefixes[i] = prefix;
        offset += values_count;
        prefix = (prefix + values_count) << 1;
    }

    for (size_t i = 0; i < lengths_len; ++i) {
        if (lengths[i] == 0) {
            continue;
        }
        hl->sorted_values[hl->offsets[lengths[i]]] = i;
        hl->offsets[lengths[i]] += 1;
        hl->prefixes[lengths[i]] += 1;
    }
}

static uint16_t hl_read_value(const HuffmanLookup * /*in*/ hl,
                              BitStream * /*mut*/ bit_stream) {
    uint16_t prefix_len = hl->min_length;
    uint16_t value = bs_read_msbf_field(/*mut*/ bit_stream, prefix_len);
    while (true) {
        if (value < hl->prefixes[prefix_len]) {
            return hl->sorted_values[hl->offsets[prefix_len] -
                                     (hl->prefixes[prefix_len] - value)];
        }
        prefix_len += 1;
        value = bs_advace_msbf_field(/*mut*/ bit_stream, value);
    }
}

static void overlapping_memcpy(char *dst, const char *src, size_t len,
                               size_t distance) {
    if (distance > len) {
        memcpy(dst, src, len);
        return;
    }
    while (len-- > 0) {
        *dst++ = *src++;
    }
}

static bool copy_literal_block(BitStream * /*mut*/ bit_stream,
                               char ** /*mut*/ dst) {
    bs_skip_bits_till_byte_boundary(/*mut*/ bit_stream);
    bs_ensure_enough_bits(/*mut*/ bit_stream);
    const uint32_t len =
        bs_read_lsbf_field(/*mut*/ bit_stream, sizeof(uint16_t) * CHAR_BIT);
    (void)bs_read_lsbf_field(/*mut*/ bit_stream, sizeof(uint16_t) * CHAR_BIT);
    memcpy(*dst, bs_get_bytes_buffer(/*mut*/ bit_stream, len), len);
    *dst += len;
    return true;
}

static void apply_fixed_litlen(BitStream * /*mut*/ bit_stream,
                               char ** /*mut*/ dst, uint32_t litlen) {
    const uint32_t len =
        LIT_EXTRA_OFFSETS[litlen] +
        bs_read_lsbf_field(/*mut*/ bit_stream, LIT_EXTRA_BITS[litlen]);
    const uint32_t dist_value = bs_read_msbf_field(/*mut*/ bit_stream, 5);
    const uint32_t dist =
        DIST_EXTRA_OFFSETS[dist_value] +
        bs_read_lsbf_field(/*mut*/ bit_stream, DIST_EXTRA_BITS[dist_value]);
    overlapping_memcpy(*dst, *dst - dist, len, dist);
    *dst += len;
}

static bool uncompress_fixed_block(BitStream * /*mut*/ bit_stream,
                                   char ** /*mut*/ dst) {
    while (true) {
        bs_ensure_enough_bits(/*mut*/ bit_stream);
        uint32_t litlen = bs_read_msbf_field(/*mut*/ bit_stream, 7);
        if (litlen == 0) {
            break;
        }
        if (litlen <= 0x17) {
            apply_fixed_litlen(/*mut*/ bit_stream, /*mut*/ dst, litlen);
            continue;
        }
        litlen = bs_advace_msbf_field(bit_stream, litlen);
        if (litlen <= 0xbf) {
            **dst = litlen - 0x30;
            *dst += 1;
            continue;
        }
        if (litlen <= 0xc7) {
            apply_fixed_litlen(/*mut*/ bit_stream, /*mut*/ dst, litlen - 0xc0);
            continue;
        }
        litlen = bs_advace_msbf_field(/*mut*/ bit_stream, litlen);
        **dst = litlen - 0x100;
        *dst += 1;
    }
    return true;
}

static void read_clen_code_lengths(BitStream * /*mut*/ bit_stream,
                                   uint8_t * /*out*/ lengths, uint32_t count) {
    for (size_t i = 0; i < count; ++i) {
        bs_ensure_enough_bits(/*mut*/ bit_stream);
        lengths[CLEN_ORDER[i]] = bs_read_lsbf_field(/*mut*/ bit_stream, 3);
    }
}

static void decode_code_lengths(BitStream * /*mut*/ bit_stream,
                                const HuffmanLookup * /*in*/ clen_hl,
                                uint8_t * /*out*/ lengths,
                                uint32_t lengths_len) {
    while (true) {
        bs_ensure_enough_bits(/*mut*/ bit_stream);
        if (lengths_len == 0) {
            break;
        }
        const uint16_t code = hl_read_value(/*in*/ clen_hl, /*mut*/ bit_stream);
        if (code < 16) {
            *lengths = code;
            lengths += 1;
            lengths_len -= 1;
            continue;
        }
        switch (code) {
            case 16: {
                const uint8_t prev_count = *(lengths - 1);
                const uint32_t repeat_count =
                    bs_read_lsbf_field(/*mut*/ bit_stream, 2) + 3;
                for (size_t i = 0; i < repeat_count; ++i) {
                    *lengths = prev_count;
                    lengths += 1;
                }
                lengths_len -= repeat_count;
                break;
            }
            case 17: {
                const uint32_t repeat_count =
                    bs_read_lsbf_field(/*mut*/ bit_stream, 3) + 3;
                for (size_t i = 0; i < repeat_count; ++i) {
                    *lengths = 0;
                    lengths += 1;
                }
                lengths_len -= repeat_count;
                break;
            }
            case 18: {
                const uint32_t repeat_count =
                    bs_read_lsbf_field(/*mut*/ bit_stream, 7) + 11;
                for (size_t i = 0; i < repeat_count; ++i) {
                    *lengths = 0;
                    lengths += 1;
                }
                lengths_len -= repeat_count;
                break;
            }
        }
    }
}

static bool uncompress_dynamic_block(BitStream * /*mut*/ bit_stream,
                                     char ** /*mut*/ dst) {
    uint8_t lengths[288] = {0};
    HuffmanLookup clen_lookup = {0};
    HuffmanLookup litlen_lookup = {0};
    HuffmanLookup dist_lookup = {0};
    bs_ensure_enough_bits(/*mut*/ bit_stream);
    const uint32_t litlen_count =
        bs_read_lsbf_field(/*mut*/ bit_stream, 5) + 257;
    const uint32_t dist_count = bs_read_lsbf_field(/*mut*/ bit_stream, 5) + 1;
    const uint32_t clen_count = bs_read_lsbf_field(/*mut*/ bit_stream, 4) + 4;
    read_clen_code_lengths(/*mut*/ bit_stream, /*out*/ lengths, clen_count);
    hl_construct(/*out*/ &clen_lookup, /*in*/ lengths, 19);
    decode_code_lengths(/*mut*/ bit_stream, /*in*/ &clen_lookup,
                        /*out*/ lengths, litlen_count);
    hl_construct(/*out*/ &litlen_lookup, /*in*/ lengths, litlen_count);
    decode_code_lengths(/*mut*/ bit_stream, /*in*/ &clen_lookup,
                        /*out*/ lengths, dist_count);
    hl_construct(/*out*/ &dist_lookup, /*in*/ lengths, dist_count);

    while (true) {
        bs_ensure_enough_bits(/*mut*/ bit_stream);
        uint16_t litlen =
            hl_read_value(/*in*/ &litlen_lookup, /*mut*/ bit_stream);
        if (litlen == 256) {
            break;
        }
        if (litlen < 256) {
            **dst = litlen;
            *dst += 1;
            continue;
        }
        litlen -= 256;
        const uint32_t len =
            LIT_EXTRA_OFFSETS[litlen] +
            bs_read_lsbf_field(/*mut*/ bit_stream, LIT_EXTRA_BITS[litlen]);
        bs_ensure_enough_bits(/*mut*/ bit_stream);
        const uint32_t dist_value =
            hl_read_value(/*in*/ &dist_lookup, /*mut*/ bit_stream);
        const uint32_t dist =
            DIST_EXTRA_OFFSETS[dist_value] +
            bs_read_lsbf_field(/*mut*/ bit_stream, DIST_EXTRA_BITS[dist_value]);
        overlapping_memcpy(*dst, *dst - dist, len, dist);
        *dst += len;
    }
    return true;
}

bool deflate(const char * /*in*/ src, size_t src_size, char * /*out*/ dst,
             size_t dst_size) {
    (void)dst_size;
    BitStream bit_stream;
    bs_init(/*out*/ &bit_stream, src, src_size);
    bool last_block = false;
    while (true) {
        if (last_block) {
            break;
        }
        bs_ensure_enough_bits(/*mut*/ &bit_stream);
        last_block = (bool)bs_read_lsbf_field(/*mut*/ &bit_stream, 1);
        switch (bs_read_lsbf_field(/*mut*/ &bit_stream, 2)) {
            case COMPRESSION_NO:
                if (!copy_literal_block(/*mut*/ &bit_stream, /*mut*/ &dst)) {
                    return false;
                }
                break;
            case COMPRESSION_FIXED:
                if (!uncompress_fixed_block(/*mut*/ &bit_stream,
                                            /*mut*/ &dst)) {
                    return false;
                }
                break;
            case COMPRESSION_DYNAMIC:
                if (!uncompress_dynamic_block(/*mut*/ &bit_stream,
                                              /*mut*/ &dst)) {
                    return false;
                }
                break;
            default:
                return false;
        }
    }
    return true;
}
