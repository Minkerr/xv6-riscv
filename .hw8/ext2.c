#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define __bswap_16(x) OSSwapInt16(x)
#define __bswap_32(x) OSSwapInt32(x)
#else
#include <endian.h>
#endif

struct ext2_super_block {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    int16_t  s_max_mnt_count;
    uint16_t s_magic;
} __attribute__((packed));

struct ext2_group_desc {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed));

struct ext2_inode {
    uint16_t i_mode;
    uint16_t i_uid;
    uint32_t i_size;
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint8_t  i_osd2[12];
} __attribute__((packed));

void swap_superblock(struct ext2_super_block *sb) {
    sb->s_inodes_count = __bswap_32(sb->s_inodes_count);
    sb->s_blocks_count = __bswap_32(sb->s_blocks_count);
    sb->s_r_blocks_count = __bswap_32(sb->s_r_blocks_count);
    sb->s_free_blocks_count = __bswap_32(sb->s_free_blocks_count);
    sb->s_free_inodes_count = __bswap_32(sb->s_free_inodes_count);
    sb->s_first_data_block = __bswap_32(sb->s_first_data_block);
    sb->s_log_block_size = __bswap_32(sb->s_log_block_size);
    sb->s_blocks_per_group = __bswap_32(sb->s_blocks_per_group);
    sb->s_frags_per_group = __bswap_32(sb->s_frags_per_group);
    sb->s_inodes_per_group = __bswap_32(sb->s_inodes_per_group);
    sb->s_mtime = __bswap_32(sb->s_mtime);
    sb->s_wtime = __bswap_32(sb->s_wtime);
    sb->s_mnt_count = __bswap_16(sb->s_mnt_count);
    sb->s_max_mnt_count = __bswap_16(sb->s_max_mnt_count);
    sb->s_magic = __bswap_16(sb->s_magic);
}

void swap_group_desc(struct ext2_group_desc *gd) {
    gd->bg_block_bitmap = __bswap_32(gd->bg_block_bitmap);
    gd->bg_inode_bitmap = __bswap_32(gd->bg_inode_bitmap);
    gd->bg_inode_table = __bswap_32(gd->bg_inode_table);
    gd->bg_free_blocks_count = __bswap_16(gd->bg_free_blocks_count);
    gd->bg_free_inodes_count = __bswap_16(gd->bg_free_inodes_count);
    gd->bg_used_dirs_count = __bswap_16(gd->bg_used_dirs_count);
}

void swap_inode(struct ext2_inode *inode) {
    inode->i_mode = __bswap_16(inode->i_mode);
    inode->i_uid = __bswap_16(inode->i_uid);
    inode->i_size = __bswap_32(inode->i_size);
    inode->i_atime = __bswap_32(inode->i_atime);
    inode->i_ctime = __bswap_32(inode->i_ctime);
    inode->i_mtime = __bswap_32(inode->i_mtime);
    inode->i_dtime = __bswap_32(inode->i_dtime);
    inode->i_gid = __bswap_16(inode->i_gid);
    inode->i_links_count = __bswap_16(inode->i_links_count);
    inode->i_blocks = __bswap_32(inode->i_blocks);
    inode->i_flags = __bswap_32(inode->i_flags);
    inode->i_osd1 = __bswap_32(inode->i_osd1);
    for (int i = 0; i < 15; i++) {
        inode->i_block[i] = __bswap_32(inode->i_block[i]);
    }
    inode->i_generation = __bswap_32(inode->i_generation);
    inode->i_file_acl = __bswap_32(inode->i_file_acl);
    inode->i_dir_acl = __bswap_32(inode->i_dir_acl);
    inode->i_faddr = __bswap_32(inode->i_faddr);
}

void process_block(uint32_t blk, uint32_t block_size, int swap, FILE *f, uint64_t *remaining) {
    if (*remaining <= 0) return;
    size_t to_write = (*remaining > block_size) ? block_size : *remaining;

    if (blk == 0) {
        char *zero_block = calloc(1, block_size);
        if (!zero_block) {
            perror("calloc failed");
            exit(1);
        }
        fwrite(zero_block, 1, to_write, stdout);
        free(zero_block);
    } else {
        uint64_t offset = (uint64_t)blk * block_size;
        if (fseek(f, offset, SEEK_SET) != 0) {
            perror("fseek failed");
            exit(1);
        }
        char *buffer = malloc(block_size);
        if (!buffer) {
            perror("malloc failed");
            exit(1);
        }
        if (fread(buffer, 1, block_size, f) != block_size) {
            fprintf(stderr, "Warning: short read at block %u\n", blk);
        }
        fwrite(buffer, 1, to_write, stdout);
        free(buffer);
    }
    *remaining -= to_write;
}

void process_indirect(uint32_t blk, int level, uint32_t block_size, int swap, FILE *f, uint64_t *remaining) {
    if (blk == 0) {
        uint32_t entries_per_block = block_size / 4;
        uint32_t num_blocks = 1;
        for (int i = 0; i < level; i++) {
            num_blocks *= entries_per_block;
        }
        for (uint32_t i = 0; i < num_blocks; i++) {
            process_block(0, block_size, swap, f, remaining);
            if (*remaining <= 0) return;
        }
        return;
    }

    uint32_t *indirect = malloc(block_size);
    if (!indirect) {
        perror("malloc failed");
        exit(1);
    }
    if (fseek(f, (uint64_t)blk * block_size, SEEK_SET) != 0) {
        perror("fseek failed");
        free(indirect);
        exit(1);
    }
    if (fread(indirect, 1, block_size, f) != block_size) {
        fprintf(stderr, "Warning: short read in indirect block %u\n", blk);
        free(indirect);
        return;
    }

    uint32_t entries_per_block = block_size / 4;
    for (uint32_t i = 0; i < entries_per_block; i++) {
        uint32_t entry = indirect[i];
        if (swap) {
            entry = __bswap_32(entry);
        }
        if (level == 1) {
            process_block(entry, block_size, swap, f, remaining);
        } else {
            process_indirect(entry, level - 1, block_size, swap, f, remaining);
        }
        if (*remaining <= 0) {
            break;
        }
    }
    free(indirect);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <image> <inode>\n", argv[0]);
        return 1;
    }

    const char *image_path = argv[1];
    uint32_t inode_num = atoi(argv[2]);

    FILE *f = fopen(image_path, "rb");
    if (!f) {
        perror("Failed to open image");
        return 1;
    }

    struct ext2_super_block sb;
    if (fseek(f, 1024, SEEK_SET) != 0) {
        perror("Seek failed");
        fclose(f);
        return 1;
    }
    if (fread(&sb, sizeof(sb), 1, f) != 1) {
        perror("Failed to read superblock");
        fclose(f);
        return 1;
    }

    int swap = 0;
    if (sb.s_magic != 0xEF53) {
        uint16_t magic = __bswap_16(sb.s_magic);
        if (magic == 0xEF53) {
            swap = 1;
            swap_superblock(&sb);
        } else {
            fprintf(stderr, "Not an ext2 filesystem (magic %04x)\n", sb.s_magic);
            fclose(f);
            return 1;
        }
    }

    if (sb.s_magic != 0xEF53) {
        fprintf(stderr, "Not an ext2 filesystem (magic %04x)\n", sb.s_magic);
        fclose(f);
        return 1;
    }

    uint32_t block_size = 1024 << sb.s_log_block_size;

    uint32_t inodes_per_group = sb.s_inodes_per_group;
    uint32_t group_index = (inode_num - 1) / inodes_per_group;

    struct ext2_group_desc gd;
    off_t group_desc_offset = 2048 + group_index * sizeof(struct ext2_group_desc);
    if (fseek(f, group_desc_offset, SEEK_SET) != 0) {
        perror("Failed to seek to group descriptor");
        fclose(f);
        return 1;
    }
    if (fread(&gd, sizeof(gd), 1, f) != 1) {
        perror("Failed to read group descriptor");
        fclose(f);
        return 1;
    }
    if (swap) {
        swap_group_desc(&gd);
    }


    uint32_t inode_table_block = gd.bg_inode_table;
    uint32_t inode_index = (inode_num - 1) % inodes_per_group;
    uint64_t inode_offset = (uint64_t)inode_table_block * block_size + inode_index * sizeof(struct ext2_inode);

    struct ext2_inode inode;
    if (fseek(f, inode_offset, SEEK_SET) != 0) {
        perror("Failed to seek to inode");
        fclose(f);
        return 1;
    }
    if (fread(&inode, sizeof(inode), 1, f) != 1) {
        perror("Failed to read inode");
        fclose(f);
        return 1;
    }
    if (swap) {
        swap_inode(&inode);
    }

    uint64_t file_size = (uint64_t)inode.i_dir_acl << 32 | inode.i_size;

    uint64_t remaining = file_size;

    for (int i = 0; i < 12; i++) {
        uint32_t blk = inode.i_block[i];
        process_block(blk, block_size, swap, f, &remaining);
        if (remaining <= 0) break;
    }

    if (remaining > 0) {
        process_indirect(inode.i_block[12], 1, block_size, swap, f, &remaining);
    }

    if (remaining > 0) {
        process_indirect(inode.i_block[13], 2, block_size, swap, f, &remaining);
    }

    if (remaining > 0) {
        process_indirect(inode.i_block[14], 3, block_size, swap, f, &remaining);
    }

    fclose(f);
    return 0;
}