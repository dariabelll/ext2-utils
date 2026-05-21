#!/usr/bin/env bash

set -e

IMG="ext2.img"
MNT="mnt"
OUT="out"

if [ -e "$IMG" ]; then
    echo "image exists"
    exit 1
fi

mkdir -p "$MNT"
mkdir -p "$OUT"

truncate --size 512M "$IMG"
mkfs.ext2 -b 2048 "$IMG"

sudo mount -t ext2 "$IMG" "$MNT"
sudo chown "$USER:$USER" "$MNT"

printf "hello from ext2\n" > "$MNT/small.txt"

mkdir -p "$MNT/dir_a"
mkdir -p "$MNT/dir_b"
mkdir -p "$MNT/dir_a/subdir"

printf "subdir file\n" > "$MNT/dir_a/subdir/note.txt"

dd if=/dev/urandom of="$MNT/indirect.bin" bs=2048 count=40 status=none

truncate -s 5G "$MNT/sparse.bin"
printf "end" | dd of="$MNT/sparse.bin" bs=1 seek=5368709117 conv=notrunc status=none

SMALL_INODE=$(stat -c %i "$MNT/small.txt")
INDIRECT_INODE=$(stat -c %i "$MNT/indirect.bin")
DIR_A_INODE=$(stat -c %i "$MNT/dir_a")
DIR_B_INODE=$(stat -c %i "$MNT/dir_b")
SUBDIR_INODE=$(stat -c %i "$MNT/dir_a/subdir")
NOTE_INODE=$(stat -c %i "$MNT/dir_a/subdir/note.txt")
SPARSE_INODE=$(stat -c %i "$MNT/sparse.bin")

echo "small.txt $SMALL_INODE" > "$OUT/inodes.txt"
echo "indirect.bin $INDIRECT_INODE" >> "$OUT/inodes.txt"
echo "dir_a $DIR_A_INODE" >> "$OUT/inodes.txt"
echo "dir_b $DIR_B_INODE" >> "$OUT/inodes.txt"
echo "dir_a/subdir $SUBDIR_INODE" >> "$OUT/inodes.txt"
echo "dir_a/subdir/note.txt $NOTE_INODE" >> "$OUT/inodes.txt"
echo "sparse.bin $SPARSE_INODE" >> "$OUT/inodes.txt"

SMALL_SUM=$(sha512sum "$MNT/small.txt" | awk '{print $1}')
INDIRECT_SUM=$(sha512sum "$MNT/indirect.bin" | awk '{print $1}')
NOTE_SUM=$(sha512sum "$MNT/dir_a/subdir/note.txt" | awk '{print $1}')
SPARSE_SUM=$(sha512sum "$MNT/sparse.bin" | awk '{print $1}')

echo "small.txt $SMALL_SUM" > "$OUT/checksums.txt"
echo "indirect.bin $INDIRECT_SUM" >> "$OUT/checksums.txt"
echo "dir_a/subdir/note.txt $NOTE_SUM" >> "$OUT/checksums.txt"
echo "sparse.bin $SPARSE_SUM" >> "$OUT/checksums.txt"

sync
sudo umount "$MNT"

./inode_info "$IMG" "$SMALL_INODE" > "$OUT/small_info.txt"
./inode_info "$IMG" "$INDIRECT_INODE" > "$OUT/indirect_info.txt"
./inode_info "$IMG" "$DIR_A_INODE" > "$OUT/dir_a_info.txt"
./inode_info "$IMG" "$NOTE_INODE" > "$OUT/note_info.txt"
./inode_info "$IMG" "$SPARSE_INODE" > "$OUT/sparse_info.txt"

SMALL_OUT=$(./get_inode_data "$IMG" "$SMALL_INODE" | sha512sum | awk '{print $1}')
INDIRECT_OUT=$(./get_inode_data "$IMG" "$INDIRECT_INODE" | sha512sum | awk '{print $1}')
NOTE_OUT=$(./get_inode_data "$IMG" "$NOTE_INODE" | sha512sum | awk '{print $1}')
SPARSE_OUT=$(./get_inode_data "$IMG" "$SPARSE_INODE" | sha512sum | awk '{print $1}')

if [ "$SMALL_SUM" != "$SMALL_OUT" ]; then
    echo "bad small"
    exit 1
fi

if [ "$INDIRECT_SUM" != "$INDIRECT_OUT" ]; then
    echo "bad indirect"
    exit 1
fi

if [ "$NOTE_SUM" != "$NOTE_OUT" ]; then
    echo "bad note"
    exit 1
fi

if [ "$SPARSE_SUM" != "$SPARSE_OUT" ]; then
    echo "bad sparse"
    exit 1
fi

./get_inode_data "$IMG" 2 | ./dir_dump > "$OUT/root.txt"
./get_inode_data "$IMG" "$DIR_A_INODE" | ./dir_dump > "$OUT/dir_a.txt"
./get_inode_data "$IMG" "$SUBDIR_INODE" | ./dir_dump > "$OUT/subdir.txt"

if ! grep -q "inode: $SMALL_INODE, name: small.txt" "$OUT/root.txt"; then
    echo "bad root"
    exit 1
fi

if ! grep -q "inode: $DIR_A_INODE, name: dir_a" "$OUT/root.txt"; then
    echo "bad root"
    exit 1
fi

if ! grep -q "inode: $DIR_B_INODE, name: dir_b" "$OUT/root.txt"; then
    echo "bad root"
    exit 1
fi

if ! grep -q "inode: $SUBDIR_INODE, name: subdir" "$OUT/dir_a.txt"; then
    echo "bad dir_a"
    exit 1
fi

if ! grep -q "inode: $NOTE_INODE, name: note.txt" "$OUT/subdir.txt"; then
    echo "bad subdir"
    exit 1
fi

LOOP=$(sudo losetup -f)
sudo losetup "$LOOP" "$IMG"
sudo chmod a+r "$LOOP"

losetup -a > "$OUT/losetup.txt"
lsblk > "$OUT/lsblk.txt"
lsblk -o name,size,fstype > "$OUT/lsblk_fstype.txt"

./inode_info "$LOOP" "$SMALL_INODE" > "$OUT/loop_info.txt"

LOOP_OUT=$(./get_inode_data "$LOOP" "$SMALL_INODE" | sha512sum | awk '{print $1}')
LOOP_NOTE_OUT=$(./get_inode_data "$LOOP" "$NOTE_INODE" | sha512sum | awk '{print $1}')

if [ "$SMALL_SUM" != "$LOOP_OUT" ]; then
    echo "bad loop"
    sudo losetup -d "$LOOP"
    exit 1
fi

if [ "$NOTE_SUM" != "$LOOP_NOTE_OUT" ]; then
    echo "bad loop note"
    sudo losetup -d "$LOOP"
    exit 1
fi

./get_inode_data "$LOOP" "$DIR_A_INODE" | ./dir_dump > "$OUT/loop_dir_a.txt"
./get_inode_data "$LOOP" "$SUBDIR_INODE" | ./dir_dump > "$OUT/loop_subdir.txt"

if ! grep -q "inode: $SUBDIR_INODE, name: subdir" "$OUT/loop_dir_a.txt"; then
    echo "bad loop dir"
    sudo losetup -d "$LOOP"
    exit 1
fi

if ! grep -q "inode: $NOTE_INODE, name: note.txt" "$OUT/loop_subdir.txt"; then
    echo "bad loop subdir"
    sudo losetup -d "$LOOP"
    exit 1
fi

sudo losetup -d "$LOOP"

echo "ok"
