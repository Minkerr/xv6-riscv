#!/bin/bash

set -e

gcc -Wall -o ext2 ext2.c
TEST_DIR="ext2_test"
mkdir -p $TEST_DIR

IMG_SIZE=20M
IMG_FILE="$TEST_DIR/ext2.img"
truncate -s $IMG_SIZE $IMG_FILE

mkfs.ext2 -F $IMG_FILE
MOUNT_POINT="$TEST_DIR/mnt"
mkdir -p $MOUNT_POINT
sudo mount -t ext2 $IMG_FILE $MOUNT_POINT

sudo bash -c "echo 'smaaaaaaaall' > $MOUNT_POINT/small.txt"
sudo dd if=/dev/urandom of=$MOUNT_POINT/medium.bin bs=1K count=100
sudo dd if=/dev/urandom of=$MOUNT_POINT/large.bin bs=1M count=10

sudo mkdir -p $MOUNT_POINT/dir1/dir2
SMALL_INODE=$(sudo ls -i $MOUNT_POINT/small.txt | awk '{print $1}')
MEDIUM_INODE=$(sudo ls -i $MOUNT_POINT/medium.bin | awk '{print $1}')
LARGE_INODE=$(sudo ls -i $MOUNT_POINT/large.bin | awk '{print $1}')

echo "Inode numbers:"
echo "small.txt: $SMALL_INODE"
echo "medium.bin: $MEDIUM_INODE"
echo "large.bin: $LARGE_INODE"

SMALL_ORIG_SUM=$(sudo sha256sum $MOUNT_POINT/small.txt | awk '{print $1}')
MEDIUM_ORIG_SUM=$(sudo sha256sum $MOUNT_POINT/medium.bin | awk '{print $1}')
LARGE_ORIG_SUM=$(sudo sha256sum $MOUNT_POINT/large.bin | awk '{print $1}')

sudo umount $MOUNT_POINT

# ########################

echo "Extracting small.txt (inode $SMALL_INODE)..."
sudo ./ext2 $IMG_FILE $SMALL_INODE > $TEST_DIR/small_extracted.txt
SMALL_EXTRACTED_SUM=$(sha256sum $TEST_DIR/small_extracted.txt | awk '{print $1}')

echo "Extracting medium.bin (inode $MEDIUM_INODE)..."
sudo ./ext2 $IMG_FILE $MEDIUM_INODE > $TEST_DIR/medium_extracted.bin
MEDIUM_EXTRACTED_SUM=$(sha256sum $TEST_DIR/medium_extracted.bin | awk '{print $1}')

echo "Extracting large.bin (inode $LARGE_INODE)..."
sudo ./ext2 $IMG_FILE $LARGE_INODE > $TEST_DIR/large_extracted.bin
LARGE_EXTRACTED_SUM=$(sha256sum $TEST_DIR/large_extracted.bin | awk '{print $1}')


echo "small.txt: Original=$SMALL_ORIG_SUM, Extracted=$SMALL_EXTRACTED_SUM"
if [ "$SMALL_ORIG_SUM" = "$SMALL_EXTRACTED_SUM" ]; then
    echo "passed"
else
    echo "failed"
fi

echo "medium.bin: Original=$MEDIUM_ORIG_SUM, Extracted=$MEDIUM_EXTRACTED_SUM"
if [ "$MEDIUM_ORIG_SUM" = "$MEDIUM_EXTRACTED_SUM" ]; then
    echo "passed"
else
    echo "failed"
fi

echo "large.bin: Original=$LARGE_ORIG_SUM, Extracted=$LARGE_EXTRACTED_SUM"
if [ "$LARGE_ORIG_SUM" = "$LARGE_EXTRACTED_SUM" ]; then
    echo "passed"
else
    echo "failed"
fi

