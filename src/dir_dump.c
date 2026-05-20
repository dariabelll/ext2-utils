#include "ext2_reader.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define INITIAL_BUFFER_SIZE 4096

int main(void) {

    uint32_t capacity = INITIAL_BUFFER_SIZE;
    uint32_t size = 0;

    uint8_t *data = malloc(capacity);

    if (data == NULL) {
        fprintf(stderr, "malloc failed\n");
        return 1;
    }

    while (1) {
        if (size == capacity) {
            uint32_t new_capacity = capacity * 2;
            uint8_t *new_data = realloc(data, new_capacity);

            if (new_data == NULL) {
                fprintf(stderr, "realloc failed\n");
                free(data);
                return 1;
            }

            data = new_data;
            capacity = new_capacity;
        }

        ssize_t result = read(
            STDIN_FILENO,
            data + size,
            capacity - size
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "read failed\n");
            free(data);
            return 1;
        }

        if (result == 0) {
            break;
        }

        size += (uint32_t)result;
    }

    uint32_t offset = 0;

    while (offset < size) {
        if (size - offset < 8) {
            fprintf(stderr, "invalid directory entry\n");
            free(data);
            return 1;
        }

        uint32_t inode = read_little_endian_32(data, offset);
        uint16_t entry_size = read_little_endian_16(data, offset + 4);
        uint8_t name_length = data[offset + 6];

        if (entry_size < 8 || entry_size % 4 != 0 || offset + entry_size > size) {
            fprintf(stderr, "invalid directory entry size\n");
            free(data);
            return 1;
        }

        if (name_length > entry_size - 8) {
            fprintf(stderr, "invalid directory entry name length\n");
            free(data);
            return 1;
        }

        if (inode != 0) {
            printf(
                "inode: %u, name: %.*s\n",
                inode,
                name_length,
                (const char *)(data + offset + 8)
            );
        }

        offset += entry_size;
    }

    free(data);
    return 0;
}