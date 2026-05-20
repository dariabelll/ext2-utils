#include "ext2_reader.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

static int read_bytes_at(
    int fd, 
    void *buffer, 
    uint32_t size, 
    uint64_t offset
) {
    uint8_t *bytes = buffer;
    uint32_t read_bytes = 0;

    while (read_bytes < size) {
        ssize_t result = pread(
            fd,
            bytes + read_bytes,
            size - read_bytes,
            offset + read_bytes
        );

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            fprintf(stderr, "pread failed\n");
            return -1;
        }

        if (result == 0) {
            fprintf(stderr, "unexpected end of file\n");
            return -1;
        }

        read_bytes += (uint32_t)result;

    }

    return 0;

}

uint16_t read_little_endian_16(const uint8_t *data, uint32_t offset) {
    return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

uint32_t read_little_endian_32(const uint8_t *data, uint32_t offset) {
    return (uint32_t)data[offset] 
        | ((uint32_t)data[offset + 1] << 8)
        | ((uint32_t)data[offset + 2] << 16) 
        | ((uint32_t)data[offset + 3] << 24);
}

uint64_t read_little_endian_64(const uint8_t *data, uint32_t offset) {
    return (uint64_t)data[offset]
        | ((uint64_t)data[offset + 1] << 8)
        | ((uint64_t)data[offset + 2] << 16)
        | ((uint64_t)data[offset + 3] << 24)
        | ((uint64_t)data[offset + 4] << 32)
        | ((uint64_t)data[offset + 5] << 40)
        | ((uint64_t)data[offset + 6] << 48)
        | ((uint64_t)data[offset + 7] << 56);
}

int ext2_reader_open(struct ext2_reader *reader, const char *path) {

    uint8_t superblock_data[SUPERBLOCK_SIZE];

    memset(reader, 0, sizeof(*reader));

    reader->fd = open(path, O_RDONLY);
    if (reader->fd < 0) {
        fprintf(stderr, "open failed\n");
        return -1;
    }

    if (read_bytes_at(
        reader->fd,
        superblock_data,
        SUPERBLOCK_SIZE,
        SUPERBLOCK_OFFSET
    ) < 0) {
        close(reader->fd);
        reader->fd = -1;
        return -1;
    }

    reader->superblock.total_inodes = read_little_endian_32(
        superblock_data,
        SUPERBLOCK_TOTAL_INODES_OFFSET
    );

    reader->superblock.total_blocks = read_little_endian_32(
        superblock_data,
        SUPERBLOCK_TOTAL_BLOCKS_OFFSET
    );

    reader->superblock.shifted_block_size = read_little_endian_32(
        superblock_data,
        SUPERBLOCK_SHIFTED_BLOCK_SIZE_OFFSET
    );

    reader->superblock.group_blocks = read_little_endian_32(
        superblock_data,
        SUPERBLOCK_GROUP_BLOCKS_OFFSET
    );

    reader->superblock.group_inodes = read_little_endian_32(
        superblock_data,
        SUPERBLOCK_GROUP_INODES_OFFSET
    );

    reader->superblock.signature = read_little_endian_16(
        superblock_data,
        SUPERBLOCK_SIGNATURE_OFFSET
    );

    reader->superblock.minor_portion_ver = read_little_endian_16(
        superblock_data,
        SUPERBLOCK_MINOR_PORTION_VER_OFFSET
    );

    reader->superblock.major_portion_ver = read_little_endian_32(
        superblock_data,
        SUPERBLOCK_MAJOR_PORTION_VER_OFFSET
    );

    if (reader->superblock.signature != SIGNATURE) {
        fprintf(stderr, "invalid ext2 signature\n");
        close(reader->fd);
        reader->fd = -1;
        return -1;
    }

    reader->block_size = BASE_BLOCK_SIZE << reader->superblock.shifted_block_size;

    if (reader->superblock.major_portion_ver == 0) {
        reader->inode_size = OLD_INODE_SIZE;
    } else {
        reader->inode_size = read_little_endian_16(
            superblock_data,
            SUPERBLOCK_INODE_SIZE_OFFSET
        );
    }

    if (reader->superblock.group_blocks == 0) {
        fprintf(stderr, "invalid blocks per group\n");
        close(reader->fd);
        reader->fd = -1;
        return -1;
    }

    reader->block_groups = (reader->superblock.total_blocks 
        + reader->superblock.group_blocks - 1) / reader->superblock.group_blocks;

    return 0;
}

void ext2_reader_close(struct ext2_reader *reader) {
    if (reader->fd >= 0) {
        close(reader->fd);
        reader->fd = -1;
    }
}

int ext2_reader_read_block_group_descriptor(
    const struct ext2_reader *reader,
    uint32_t group_number,
    struct block_group_descriptor_info *descriptor
) {
    if (group_number >= reader->block_groups) {
        fprintf(stderr, "invalid block group number\n");
        return -1;
    }

    uint32_t descriptor_table_block;

    if (reader->block_size == BASE_BLOCK_SIZE) {
        descriptor_table_block = START_DESCRIPTOR_BLOCK_1024;
    } else {
        descriptor_table_block = START_DESCRIPTOR_BLOCK_OTHER;
    }

    uint64_t descriptor_offset = (uint64_t)descriptor_table_block * reader->block_size
        + (uint64_t)group_number * BLOCK_GROUP_DESCRIPTOR_SIZE;

    uint8_t descriptor_data[BLOCK_GROUP_DESCRIPTOR_SIZE];

    if (read_bytes_at(
        reader->fd,
        descriptor_data,
        BLOCK_GROUP_DESCRIPTOR_SIZE,
        descriptor_offset
    ) < 0) {
        return -1;
    }

    descriptor->block_usage_bitmap = read_little_endian_32(
        descriptor_data,
        BLOCK_GROUP_BLOCK_USAGE_BITMAP_OFFSET
    );

    descriptor->inode_usage_bitmap = read_little_endian_32(
        descriptor_data,
        BLOCK_GROUP_INODE_USAGE_BITMAP_OFFSET
    );

    descriptor->inode_table_start = read_little_endian_32(
        descriptor_data,
        BLOCK_GROUP_INODE_TABLE_START_OFFSET
    );

    descriptor->unallocated_blocks = read_little_endian_16(
        descriptor_data,
        BLOCK_GROUP_UNALLOCATED_BLOCKS_OFFSET
    );

    descriptor->unallocated_inodes = read_little_endian_16(
        descriptor_data,
        BLOCK_GROUP_UNALLOCATED_INODES_OFFSET
    );

    descriptor->group_directories = read_little_endian_16(
        descriptor_data,
        BLOCK_GROUP_DIRECTORIES_OFFSET
    );

    return 0;
}

int ext2_reader_read_inode(
    const struct ext2_reader *reader,
    uint32_t inode_number,
    struct inode_info *inode
) {
    if (inode_number == 0 || inode_number > reader->superblock.total_inodes) {
        fprintf(stderr, "invalid inode number\n");
        return -1;
    }

    uint32_t group_number = (inode_number - 1) / reader->superblock.group_inodes;
    uint32_t inode_index = (inode_number - 1) % reader->superblock.group_inodes;

    struct block_group_descriptor_info descriptor;

    if (ext2_reader_read_block_group_descriptor(
        reader,
        group_number,
        &descriptor
    ) < 0) {
        return -1;
    }

    uint64_t inode_offset = (uint64_t)descriptor.inode_table_start * reader->block_size
        + (uint64_t)inode_index * reader->inode_size;

    uint8_t inode_data[OLD_INODE_SIZE];

    if (read_bytes_at(
        reader->fd,
        inode_data,
        OLD_INODE_SIZE,
        inode_offset
    ) < 0) {
        return -1;
    }

    inode->type_and_permissions = read_little_endian_16(
        inode_data,
        INODE_TYPE_AND_PERMISSIONS_OFFSET
    );

    inode->user_id = read_little_endian_16(
        inode_data,
        INODE_USER_ID_OFFSET
    );

    inode->lower_size_bits = read_little_endian_32(
        inode_data,
        INODE_LOWER_SIZE_BITS_OFFSET
    );

    inode->last_access_time = read_little_endian_32(
        inode_data,
        INODE_LAST_ACCESS_TIME_OFFSET
    );

    inode->creation_time = read_little_endian_32(
        inode_data,
        INODE_CREATION_TIME_OFFSET
    );

    inode->last_modification_time = read_little_endian_32(
        inode_data,
        INODE_LAST_MODIFICATION_TIME_OFFSET
    );

    inode->deletion_time = read_little_endian_32(
        inode_data,
        INODE_DELETION_TIME_OFFSET
    );

    inode->group_id = read_little_endian_16(
        inode_data,
        INODE_GROUP_ID_OFFSET
    );

    inode->hard_links = read_little_endian_16(
        inode_data,
        INODE_HARD_LINKS_OFFSET
    );

    inode->disk_sectors = read_little_endian_32(
        inode_data,
        INODE_DISK_SECTORS_OFFSET
    );

    inode->flags = read_little_endian_32(
        inode_data,
        INODE_FLAGS_OFFSET
    );

    for (uint32_t i = 0; i < DIRECT_BLOCK_POINTERS; ++i) {
        inode->direct_block_pointers[i] = read_little_endian_32(
            inode_data,
            INODE_DIRECT_BLOCK_POINTERS_OFFSET + i * 4
        );
    }

    inode->singly_indirect_pointer = read_little_endian_32(
        inode_data,
        INODE_SINGLY_INDIRECT_POINTER_OFFSET
    );

    inode->doubly_indirect_pointer = read_little_endian_32(
        inode_data,
        INODE_DOUBLY_INDIRECT_POINTER_OFFSET
    );

    inode->triply_indirect_pointer = read_little_endian_32(
        inode_data,
        INODE_TRIPLY_INDIRECT_POINTER_OFFSET
    );

    inode->upper_size_bits = read_little_endian_32(
        inode_data,
        INODE_UPPER_SIZE_BITS_OFFSET
    );

    return 0;
}

int ext2_reader_read_block(
    const struct ext2_reader *reader,
    uint32_t block_number,
    void *buffer
) {
    if (block_number == 0 || block_number >= reader->superblock.total_blocks) {
        fprintf(stderr, "invalid block number\n");
        return -1;
    }

    uint64_t block_offset = (uint64_t)block_number * reader->block_size;

    return read_bytes_at(
        reader->fd,
        buffer,
        reader->block_size,
        block_offset
    );
}
