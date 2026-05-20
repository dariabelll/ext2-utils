CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64
CPPFLAGS = -Iinclude

SRC_DIR = src
BUILD_DIR = build

.PHONY: all clean valgrind

all: inode_info get_inode_data

inode_info: $(BUILD_DIR)/inodeinfo.o $(BUILD_DIR)/ext2_reader.o
	$(CC) $(CFLAGS) -o $@ $^

get_inode_data: $(BUILD_DIR)/get_inode_data.o $(BUILD_DIR)/ext2_reader.o
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/inodeinfo.o: $(SRC_DIR)/inodeinfo.c include/ext2_reader.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/get_inode_data.o: $(SRC_DIR)/get_inode_data.c include/ext2_reader.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ext2_reader.o: $(SRC_DIR)/ext2_reader.c include/ext2_reader.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

valgrind: all
	valgrind --leak-check=full --track-origins=yes ./inode_info ext2.img 2

clean:
	rm -rf $(BUILD_DIR) inode_info get_inode_data