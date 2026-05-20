#include "ext2_reader.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const char *inode_type_to_string(uint16_t type_and_permissions) {
    uint16_t type = type_and_permissions & INODE_TYPE_MASK;

    switch (type) {
        case INODE_TYPE_FIFO:
            return "fifo";
        case INODE_TYPE_CHARACTER_DEVICE:
            return "character device";
        case INODE_TYPE_DIRECTORY:
            return "directory";
        case INODE_TYPE_BLOCK_DEVICE:
            return "block device";
        case INODE_TYPE_REGULAR_FILE:
            return "regular file";
        case INODE_TYPE_SYMBOLIC_LINK:
            return "symbolic link";
        case INODE_TYPE_UNIX_SOCKET:
            return "unix socket";
        default:
            return "unknown";
    }
}

static int print_indirect_block(
    const struct ext2_reader *reader,
    uint32_t block_number,
    uint32_t level,
    uint32_t depth
) {
    if (block_number == 0) {
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

    uint32_t pointers_count = reader->block_size / 4;

    for (uint32_t i = 0; i < pointers_count; ++i) {
        uint32_t child_block = read_little_endian_32(
            block_data,
            i * 4
        );

        if (child_block == 0) {
            continue;
        }

        for (uint32_t i = 0; i < depth; ++i) printf("  ");

        if (level == 1) {
            printf("data block: %u\n", child_block);
        } else {
            printf("indirect block: %u\n", child_block);

            if (print_indirect_block(
                reader,
                child_block,
                level - 1,
                depth + 1
            ) < 0) {
                free(block_data);
                return -1;
            }
        }
    }

    free(block_data);
    return 0;
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

    printf("inode: %u\n", inode_number);
    printf("type and permissions: 0x%04x\n", inode.type_and_permissions);
    printf("type: %s\n", inode_type_to_string(inode.type_and_permissions));
    printf("user id: %u\n", inode.user_id);
    printf("lower size bits: %u\n", inode.lower_size_bits);
    printf("upper size bits: %u\n", inode.upper_size_bits);
    printf("size: %llu\n", (unsigned long long)file_size);
    printf("last access time: %u\n", inode.last_access_time);
    printf("creation time: %u\n", inode.creation_time);
    printf("last modification time: %u\n", inode.last_modification_time);
    printf("deletion time: %u\n", inode.deletion_time);
    printf("group id: %u\n", inode.group_id);
    printf("hard links: %u\n", inode.hard_links);
    printf("disk sectors: %u\n", inode.disk_sectors);
    printf("flags: 0x%08x\n", inode.flags);

    for (uint32_t i = 0; i < DIRECT_BLOCK_POINTERS; ++i) {
        printf("direct block pointer %u: %u\n", i, inode.direct_block_pointers[i]);
    }

    printf("singly indirect block pointer: %u\n", inode.singly_indirect_pointer);
    printf("doubly indirect block pointer: %u\n", inode.doubly_indirect_pointer);
    printf("triply indirect block pointer: %u\n", inode.triply_indirect_pointer);

    if (inode.singly_indirect_pointer != 0) {
        printf("singly indirect data blocks:\n");

        if (print_indirect_block(&reader, inode.singly_indirect_pointer, 1, 1) < 0) {
            ext2_reader_close(&reader);
            return 1;
        }
    }

    if (inode.doubly_indirect_pointer != 0) {
        printf("doubly indirect data blocks:\n");

        if (print_indirect_block(&reader, inode.doubly_indirect_pointer, 2, 1) < 0) {
            ext2_reader_close(&reader);
            return 1;
        }
    }

    if (inode.triply_indirect_pointer != 0) {
        printf("triply indirect data blocks:\n");

        if (print_indirect_block(&reader, inode.triply_indirect_pointer, 3, 1) < 0) {
            ext2_reader_close(&reader);
            return 1;
        }
    }

    ext2_reader_close(&reader);
    return 0;
}
