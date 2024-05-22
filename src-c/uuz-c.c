#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static const char* mmap_in_file(const char* /*in*/ file_name,
                                size_t* /*out*/ buffer_size);
static char* mmap_out_file(const char* /*in*/ file_name, size_t file_size);
static bool read_compressed_file_name_data_offset(
    const char* /*in*/ zip_file_header_buffer, size_t buffer_size,
    char* /*out*/ file_name_buffer, size_t* /*out*/ data_offset);
static bool read_compressed_file_uncompressed_size(
    const char* /*in*/ zip_file_header_buffer, size_t buffer_size,
    size_t* /*out*/ file_size);
static bool read_compressed_file_compression_method(
    const char* /*in*/ zip_file_header_buffer, size_t bufer_size,
    uint16_t* /*out*/ compression_type);

extern bool deflate(const char* /*in*/ src, size_t src_size, char* /*out*/ dst,
                    size_t dst_size);

static const int MAX_FILE_NAME_LEN = 1024;
static const int FILE_NAME_OFFSET = 26;
static const int UNCOMPRESSED_SIZE_OFFSET = 22;
static const int COMPRESSION_METHOD_OFFSET = 8;
#define COMPRESSION_TYPE_NONE 0
#define COMPRESSION_TYPE_DEFLATE 8

int main(int argc, char** argv) {
    if (argc < 2) {
        return 1;
    }

    size_t zip_file_buffer_size = 0;
    const char* zip_file_buffer =
        mmap_in_file(argv[1], /*out*/ &zip_file_buffer_size);
    if (zip_file_buffer == NULL) {
        return 1;
    }

    char out_file_name_buffer[MAX_FILE_NAME_LEN];
    size_t compressed_data_offset = 0;
    bool read_result = read_compressed_file_name_data_offset(
        zip_file_buffer, zip_file_buffer_size, out_file_name_buffer,
        /*out*/ &compressed_data_offset);
    if (!read_result) {
        return 1;
    }

    size_t uncompressed_size = 0;
    read_result = read_compressed_file_uncompressed_size(
        zip_file_buffer, zip_file_buffer_size, &uncompressed_size);
    if (!read_result) {
        return 1;
    }

    char* out_file_buffer =
        mmap_out_file(out_file_name_buffer, uncompressed_size);

    uint16_t compression_method;
    read_result = read_compressed_file_compression_method(
        zip_file_buffer, zip_file_buffer_size, &compression_method);
    if (!read_result) {
        return 1;
    }

    const char* const compressed_data =
        zip_file_buffer + compressed_data_offset;
    const size_t compressed_data_size =
        zip_file_buffer_size - compressed_data_offset;

    switch (compression_method) {
        case COMPRESSION_TYPE_NONE:
            memcpy(out_file_buffer, compressed_data, compressed_data_size);
            break;
        case COMPRESSION_TYPE_DEFLATE: {
            bool deflate_result = deflate(compressed_data, compressed_data_size,
                                          out_file_buffer, uncompressed_size);
            if (!deflate_result) {
                return 1;
            }
            break;
        }
        default:
            return 1;
    }

    return 0;
}

static const char* mmap_in_file(const char* file_name,
                                size_t* /*out*/ buffer_size) {
    const int in_fd = open(file_name, O_RDONLY);
    if (in_fd == -1) {
        return NULL;
    }
    const off_t in_size = lseek(in_fd, 0, SEEK_END);
    if (in_size == (off_t)-1) {
        close(in_fd);
        return NULL;
    }
    *buffer_size = (size_t)in_size;

    const char* const in_buffer =
        mmap(NULL, (size_t)in_size, PROT_READ, MAP_PRIVATE, in_fd, 0);
    if (in_buffer == MAP_FAILED) {
        close(in_fd);
        return NULL;
    }

    return in_buffer;
}

static char* mmap_out_file(const char* file_name, size_t file_size) {
    const int out_fd =
        open(file_name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (out_fd == -1) {
        return NULL;
    }

    const int ftruncate_result = ftruncate(out_fd, (off_t)file_size);
    if (ftruncate_result == -1) {
        close(out_fd);
        return NULL;
    }

    char* const out_buffer =
        mmap(NULL, file_size, PROT_WRITE, MAP_SHARED, out_fd, 0);
    if (out_buffer == MAP_FAILED) {
        close(out_fd);
        return NULL;
    }

    return out_buffer;
}

static bool read_compressed_file_name_data_offset(
    const char* /*in*/ zip_file_header, size_t buffer_size,
    char* /*out*/ file_name_buffer, size_t* /*out*/ data_offset) {
    if (buffer_size < (FILE_NAME_OFFSET + 2 * sizeof(uint16_t))) {
        return false;
    }
    uint16_t file_name_len = 0;
    uint16_t extra_field_len = 0;
    memcpy((char*)&file_name_len, zip_file_header + FILE_NAME_OFFSET,
           sizeof(uint16_t));
    memcpy((char*)&extra_field_len,
           zip_file_header + FILE_NAME_OFFSET + sizeof(uint16_t),
           sizeof(uint16_t));
    if (file_name_len + 1 > MAX_FILE_NAME_LEN) {
        return false;
    }
    if (buffer_size <
        (FILE_NAME_OFFSET + 2 * sizeof(uint16_t) + file_name_len)) {
        return false;
    }
    memcpy(file_name_buffer,
           zip_file_header + FILE_NAME_OFFSET + 2 * sizeof(uint16_t),
           file_name_len);
    file_name_buffer[file_name_len] = 0;
    *data_offset = FILE_NAME_OFFSET + 2 * sizeof(uint16_t) + file_name_len +
                   extra_field_len;
    return true;
}

static bool read_compressed_file_uncompressed_size(
    const char* /*in*/ zip_file_header, size_t buffer_size,
    size_t* /*out*/ file_size) {
    if (buffer_size < (UNCOMPRESSED_SIZE_OFFSET + sizeof(uint32_t))) {
        return false;
    }
    uint32_t uncompressed_size = 0;
    memcpy((char*)&uncompressed_size,
           zip_file_header + UNCOMPRESSED_SIZE_OFFSET, sizeof(uint32_t));
    *file_size = (size_t)uncompressed_size;
    return true;
}

static bool read_compressed_file_compression_method(
    const char* /*in*/ zip_file_header, size_t buffer_size,
    uint16_t* /*out*/ compression_method) {
    if (buffer_size < (COMPRESSION_METHOD_OFFSET + sizeof(uint16_t))) {
        return false;
    }
    memcpy(compression_method, zip_file_header + COMPRESSION_METHOD_OFFSET,
           sizeof(uint16_t));
    return true;
}
