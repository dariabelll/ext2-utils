#include "ext2_reader.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int write_bytes(const uint8_t *data, uint32_t size) {
    uint32_t written_bytes = 0;

    while (written_bytes < size) {
        ssize_t result = write(
            STDOUT_FILENO,
            data + written_bytes,
            size - written_bytes
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "write failed\n");
            return -1;
        }

        if (result == 0) {
            fprintf(stderr, "write failed\n");
            return -1;
        }

        written_bytes += (uint32_t)result;
    }

    return 0;
}

static int read_pointer_from_block(
    const struct ext2_reader *reader,
    uint32_t block_number,
    uint32_t pointer_index,
    uint32_t *result_pointer
) {
    if (block_number == 0) {
        *result_pointer = 0;
        return 0;
    }

    uint8_t *block_data = malloc(reader->block_size);

    if (block_data == NULL) {
        fprintf(stderr, "malloc failed\n");
        return -1;
    }

    if (ext2_reader_read_block(reader, block_number, block_data) < 0) {
        fprintf(stderr, "block read failed\n");
        free(block_data);
        return -1;
    }

    *result_pointer = read_little_endian_32(
        block_data,
        pointer_index * 4
    );

    free(block_data);
    return 0;
}

static int get_data_block_number(
    const struct ext2_reader *reader,
    const struct inode_info *inode,
    uint64_t logical_block_number,
    uint32_t *data_block_number
) {
    uint32_t pointers_per_block = reader->block_size / 4;

    if (logical_block_number < DIRECT_BLOCK_POINTERS) {
        *data_block_number = inode->direct_block_pointers[logical_block_number];
        return 0;
    }

    logical_block_number -= DIRECT_BLOCK_POINTERS;

    if (logical_block_number < pointers_per_block) {
        return read_pointer_from_block(
            reader,
            inode->singly_indirect_pointer,
            (uint32_t)logical_block_number,
            data_block_number
        );
    }

    logical_block_number -= pointers_per_block;

    uint64_t doubly_indirect_capacity =
        (uint64_t)pointers_per_block * pointers_per_block;

    if (logical_block_number < doubly_indirect_capacity) {
        uint32_t singly_indirect_index =
            (uint32_t)(logical_block_number / pointers_per_block);

        uint32_t data_block_index =
            (uint32_t)(logical_block_number % pointers_per_block);

        uint32_t singly_indirect_block;

        if (read_pointer_from_block(
            reader,
            inode->doubly_indirect_pointer,
            singly_indirect_index,
            &singly_indirect_block
        ) < 0) {
            return -1;
        }

        return read_pointer_from_block(
            reader,
            singly_indirect_block,
            data_block_index,
            data_block_number
        );
    }

    logical_block_number -= doubly_indirect_capacity;

    uint64_t doubly_area =
        (uint64_t)pointers_per_block * pointers_per_block;

    uint64_t triply_indirect_capacity =
        doubly_area * pointers_per_block;

    if (logical_block_number < triply_indirect_capacity) {
        uint32_t doubly_indirect_index =
            (uint32_t)(logical_block_number / doubly_area);

        uint64_t rest = logical_block_number % doubly_area;

        uint32_t singly_indirect_index =
            (uint32_t)(rest / pointers_per_block);

        uint32_t data_block_index =
            (uint32_t)(rest % pointers_per_block);

        uint32_t doubly_indirect_block;
        uint32_t singly_indirect_block;

        if (read_pointer_from_block(
            reader,
            inode->triply_indirect_pointer,
            doubly_indirect_index,
            &doubly_indirect_block
        ) < 0) {
            return -1;
        }

        if (read_pointer_from_block(
            reader,
            doubly_indirect_block,
            singly_indirect_index,
            &singly_indirect_block
        ) < 0) {
            return -1;
        }

        return read_pointer_from_block(
            reader,
            singly_indirect_block,
            data_block_index,
            data_block_number
        );
    }

    fprintf(stderr, "file is too large\n");
    return -1;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "invalid args\n");
        return 1;
    }

    errno = 0;

    char *end = NULL;
    unsigned long parsed_inode_number = strtoul(argv[2], &end, 10);

    if (
        errno != 0 ||
        end == argv[2] ||
        *end != '\0' ||
        parsed_inode_number > UINT32_MAX
    ) {
        fprintf(stderr, "invalid inode number\n");
        return 1;
    }

    uint32_t inode_number = (uint32_t)parsed_inode_number;

    struct ext2_reader reader;

    if (ext2_reader_open(&reader, argv[1]) < 0) {
        fprintf(stderr, "reader open failed\n");
        return 1;
    }

    struct inode_info inode;

    if (ext2_reader_read_inode(&reader, inode_number, &inode) < 0) {
        fprintf(stderr, "inode read failed\n");
        ext2_reader_close(&reader);
        return 1;
    }

    uint64_t file_size = inode.lower_size_bits;

    if ((inode.type_and_permissions & INODE_TYPE_MASK) == INODE_TYPE_REGULAR_FILE) {
        file_size = ((uint64_t)inode.upper_size_bits << 32) | inode.lower_size_bits;
    }

    uint8_t *block_data = malloc(reader.block_size);

    if (block_data == NULL) {
        fprintf(stderr, "malloc failed\n");
        ext2_reader_close(&reader);
        return 1;
    }

    uint8_t *zero_block = calloc(reader.block_size, 1);

    if (zero_block == NULL) {
        fprintf(stderr, "calloc failed\n");
        free(block_data);
        ext2_reader_close(&reader);
        return 1;
    }

    uint64_t written_file_bytes = 0;
    uint64_t logical_block_number = 0;

    while (written_file_bytes < file_size) {
        uint32_t bytes_to_write = reader.block_size;

        if (file_size - written_file_bytes < reader.block_size) {
            bytes_to_write = (uint32_t)(file_size - written_file_bytes);
        }

        uint32_t data_block_number;

        if (get_data_block_number(
            &reader,
            &inode,
            logical_block_number,
            &data_block_number
        ) < 0) {
            free(zero_block);
            free(block_data);
            ext2_reader_close(&reader);
            return 1;
        }

        if (data_block_number == 0) {
            if (write_bytes(zero_block, bytes_to_write) < 0) {
                free(zero_block);
                free(block_data);
                ext2_reader_close(&reader);
                return 1;
            }
        } else {
            if (ext2_reader_read_block(&reader, data_block_number, block_data) < 0) {
                fprintf(stderr, "block read failed\n");
                free(zero_block);
                free(block_data);
                ext2_reader_close(&reader);
                return 1;
            }

            if (write_bytes(block_data, bytes_to_write) < 0) {
                free(zero_block);
                free(block_data);
                ext2_reader_close(&reader);
                return 1;
            }
        }

        written_file_bytes += bytes_to_write;
        logical_block_number++;
    }

    free(zero_block);
    free(block_data);
    ext2_reader_close(&reader);

    return 0;
}
