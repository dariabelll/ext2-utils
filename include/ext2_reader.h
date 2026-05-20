#ifndef EXT2_READER_H 
#define EXT2_READER_H

#include <stdint.h>

#define SUPERBLOCK_OFFSET 1024
#define SUPERBLOCK_SIZE 1024
#define SIGNATURE 0xEF53
#define OLD_INODE_SIZE 128
#define BASE_BLOCK_SIZE 1024

#define SUPERBLOCK_TOTAL_INODES_OFFSET 0
#define SUPERBLOCK_TOTAL_BLOCKS_OFFSET 4
#define SUPERBLOCK_SHIFTED_BLOCK_SIZE_OFFSET 24
#define SUPERBLOCK_GROUP_BLOCKS_OFFSET 32
#define SUPERBLOCK_GROUP_INODES_OFFSET 40
#define SUPERBLOCK_SIGNATURE_OFFSET 56
#define SUPERBLOCK_MINOR_PORTION_VER_OFFSET 62
#define SUPERBLOCK_MAJOR_PORTION_VER_OFFSET 76
#define SUPERBLOCK_INODE_SIZE_OFFSET 88

#define BLOCK_GROUP_DESCRIPTOR_SIZE 32
#define START_DESCRIPTOR_BLOCK_1024 2
#define START_DESCRIPTOR_BLOCK_OTHER 1

#define BLOCK_GROUP_BLOCK_USAGE_BITMAP_OFFSET 0
#define BLOCK_GROUP_INODE_USAGE_BITMAP_OFFSET 4
#define BLOCK_GROUP_INODE_TABLE_START_OFFSET 8
#define BLOCK_GROUP_UNALLOCATED_BLOCKS_OFFSET 12
#define BLOCK_GROUP_UNALLOCATED_INODES_OFFSET 14
#define BLOCK_GROUP_DIRECTORIES_OFFSET 16

#define ROOT_INODE 2
#define DIRECT_BLOCK_POINTERS 12

#define SINGLY_INDIRECT_POINTER 12
#define DOUBLY_INDIRECT_POINTER 13
#define TRIPLY_INDIRECT_POINTER 14
#define TOTAL_BLOCK_POINTERS 15

#define INODE_TYPE_MASK 0xF000

#define INODE_TYPE_FIFO 0x1000
#define INODE_TYPE_CHARACTER_DEVICE 0x2000
#define INODE_TYPE_DIRECTORY 0x4000
#define INODE_TYPE_BLOCK_DEVICE 0x6000
#define INODE_TYPE_REGULAR_FILE 0x8000
#define INODE_TYPE_SYMBOLIC_LINK 0xA000
#define INODE_TYPE_UNIX_SOCKET 0xC000

#define INODE_TYPE_AND_PERMISSIONS_OFFSET 0
#define INODE_USER_ID_OFFSET 2
#define INODE_LOWER_SIZE_BITS_OFFSET 4
#define INODE_LAST_ACCESS_TIME_OFFSET 8
#define INODE_CREATION_TIME_OFFSET 12
#define INODE_LAST_MODIFICATION_TIME_OFFSET 16
#define INODE_DELETION_TIME_OFFSET 20
#define INODE_GROUP_ID_OFFSET 24
#define INODE_HARD_LINKS_OFFSET 26
#define INODE_DISK_SECTORS_OFFSET 28
#define INODE_FLAGS_OFFSET 32

#define INODE_DIRECT_BLOCK_POINTERS_OFFSET 40
#define INODE_SINGLY_INDIRECT_POINTER_OFFSET 88
#define INODE_DOUBLY_INDIRECT_POINTER_OFFSET 92
#define INODE_TRIPLY_INDIRECT_POINTER_OFFSET 96
#define INODE_UPPER_SIZE_BITS_OFFSET 108


struct superblock_info {
    uint32_t total_inodes;
    uint32_t total_blocks;
    uint32_t shifted_block_size;
    uint32_t group_blocks;
    uint32_t group_inodes;
    uint16_t signature;
    uint16_t minor_portion_ver;
    uint32_t major_portion_ver;
};

struct block_group_descriptor_info {
    uint32_t block_usage_bitmap;
    uint32_t inode_usage_bitmap;
    uint32_t inode_table_start;
    uint16_t unallocated_blocks;
    uint16_t unallocated_inodes;
    uint16_t group_directories;
};

struct inode_info {
    uint16_t type_and_permissions;
    uint16_t user_id;
    uint32_t lower_size_bits;
    uint32_t last_access_time;
    uint32_t creation_time;
    uint32_t last_modification_time;
    uint32_t deletion_time;
    uint16_t group_id;
    uint16_t hard_links;
    uint32_t disk_sectors;
    uint32_t flags;
    uint32_t direct_block_pointers[DIRECT_BLOCK_POINTERS];
    uint32_t singly_indirect_pointer;
    uint32_t doubly_indirect_pointer;
    uint32_t triply_indirect_pointer;
    uint32_t upper_size_bits;
};

struct ext2_reader {
    int fd;
    struct superblock_info superblock;
    uint32_t block_size;
    uint32_t inode_size;
    uint32_t block_groups;
};

int ext2_reader_open(struct ext2_reader *reader, const char *path);
void ext2_reader_close(struct ext2_reader *reader);

int ext2_reader_read_block_group_descriptor(
    const struct ext2_reader *reader,
    uint32_t group_number,
    struct block_group_descriptor_info *descriptor
);

int ext2_reader_read_inode(
    const struct ext2_reader *reader,
    uint32_t inode_number,
    struct inode_info *inode
);

int ext2_reader_read_block(
    const struct ext2_reader *reader,
    uint32_t block_number,
    void *buffer
);

uint16_t read_little_endian_16(const uint8_t *data, uint32_t offset);
uint32_t read_little_endian_32(const uint8_t *data, uint32_t offset);
uint64_t read_little_endian_64(const uint8_t *data, uint32_t offset);

#endif