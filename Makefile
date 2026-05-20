CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -g -D_POSIX_C_SOURCE=200809L -D_FILE_OFFSET_BITS=64
CPPFLAGS = -Iinclude

SRC_DIR = src
BUILD_DIR = build

INODEINFO = inode_info

.PHONY: all clean valgrind

all: $(INODEINFO)

$(INODEINFO): $(BUILD_DIR)/inode_info.o $(BUILD_DIR)/ext2_reader.o
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/inode_info.o: $(SRC_DIR)/inode_info.c include/ext2_reader.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ext2_reader.o: $(SRC_DIR)/ext2_reader.c include/ext2_reader.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

valgrind: $(INODEINFO)
	valgrind --leak-check=full --track-origins=yes ./$(INODEINFO) ext2.img 2

clean:
	rm -rf $(BUILD_DIR) $(INODEINFO)
