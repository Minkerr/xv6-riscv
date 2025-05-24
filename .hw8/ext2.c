#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef __APPLE__
#include <libkern/OSByteOrder.h>
#define __bswap_16(x) OSSwapInt16(x)
#define __bswap_32(x) OSSwapInt32(x)
#define __bswap_64(x) OSSwapInt64(x)
#else
#include <byteswap.h>
#include <endian.h>
#endif

#define EXT2_SUPER_MAGIC 0xEF53

struct ext2_super_block {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    int16_t  s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algorithm_usage_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding1;
    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint32_t s_reserved[190];
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

int is_big_endian() {
    uint16_t x = 1;
    return !(*((uint8_t *)&x));
}

void swap_superblock(struct ext2_super_block *sb) {
    sb->s_inodes_count = __bswap_32(sb->s_inodes_count);
    sb->s_blocks_count = __bswap_32(sb->s_blocks_count);
    sb->s_r_blocks_count = __bswap_32(sb->s_r_blocks_count);
    sb->s_free_blocks_count = __bswap_32(sb->s_free_blocks_count);
    sb->s_free_inodes_count = __bswap_32(sb->s_free_inodes_count);
    sb->s_first_data_block = __bswap_32(sb->s_first_data_block);
    sb->s_log_block_size = __bswap_32(sb->s_log_block_size);
    sb->s_log_frag_size = __bswap_32(sb->s_log_frag_size);
    sb->s_blocks_per_group = __bswap_32(sb->s_blocks_per_group);
    sb->s_frags_per_group = __bswap_32(sb->s_frags_per_group);
    sb->s_inodes_per_group = __bswap_32(sb->s_inodes_per_group);
    sb->s_mtime = __bswap_32(sb->s_mtime);
    sb->s_wtime = __bswap_32(sb->s_wtime);
    sb->s_mnt_count = __bswap_16(sb->s_mnt_count);
    sb->s_max_mnt_count = __bswap_16(sb->s_max_mnt_count);
    sb->s_magic = __bswap_16(sb->s_magic);
    sb->s_state = __bswap_16(sb->s_state);
    sb->s_errors = __bswap_16(sb->s_errors);
    sb->s_minor_rev_level = __bswap_16(sb->s_minor_rev_level);
    sb->s_lastcheck = __bswap_32(sb->s_lastcheck);
    sb->s_checkinterval = __bswap_32(sb->s_checkinterval);
    sb->s_creator_os = __bswap_32(sb->s_creator_os);
    sb->s_rev_level = __bswap_32(sb->s_rev_level);
    sb->s_def_resuid = __bswap_16(sb->s_def_resuid);
    sb->s_def_resgid = __bswap_16(sb->s_def_resgid);
    
    sb->s_first_ino = __bswap_32(sb->s_first_ino);
    sb->s_inode_size = __bswap_16(sb->s_inode_size);
    sb->s_block_group_nr = __bswap_16(sb->s_block_group_nr);
    sb->s_feature_compat = __bswap_32(sb->s_feature_compat);
    sb->s_feature_incompat = __bswap_32(sb->s_feature_incompat);
    sb->s_feature_ro_compat = __bswap_32(sb->s_feature_ro_compat);
    sb->s_algorithm_usage_bitmap = __bswap_32(sb->s_algorithm_usage_bitmap);
    sb->s_journal_inum = __bswap_32(sb->s_journal_inum);
    sb->s_journal_dev = __bswap_32(sb->s_journal_dev);
    sb->s_last_orphan = __bswap_32(sb->s_last_orphan);
    
    for (int i = 0; i < 4; i++) {
        sb->s_hash_seed[i] = __bswap_32(sb->s_hash_seed[i]);
    }
    
    sb->s_default_mount_opts = __bswap_32(sb->s_default_mount_opts);
    sb->s_first_meta_bg = __bswap_32(sb->s_first_meta_bg);
}

void swap_group_desc(struct ext2_group_desc *gd) {
    gd->bg_block_bitmap = __bswap_32(gd->bg_block_bitmap);
    gd->bg_inode_bitmap = __bswap_32(gd->bg_inode_bitmap);
    gd->bg_inode_table = __bswap_32(gd->bg_inode_table);
    gd->bg_free_blocks_count = __bswap_16(gd->bg_free_blocks_count);
    gd->bg_free_inodes_count = __bswap_16(gd->bg_free_inodes_count);
    gd->bg_used_dirs_count = __bswap_16(gd->bg_used_dirs_count);
    gd->bg_pad = __bswap_16(gd->bg_pad);
    
    for (int i = 0; i < 3; i++) {
        gd->bg_reserved[i] = __bswap_32(gd->bg_reserved[i]);
    }
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
        char *zero_block = calloc(1, to_write);
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
        
        size_t read_size = fread(buffer, 1, block_size, f);
        if (read_size != block_size) {
            fprintf(stderr, "Warning: short read at block %u (read %zu bytes)\n", blk, read_size);
        }
        
        fwrite(buffer, 1, to_write, stdout);
        free(buffer);
    }
    
    *remaining -= to_write;
}

void process_indirect(uint32_t blk, int level, uint32_t block_size, int swap, FILE *f, uint64_t *remaining) {
    if (*remaining <= 0) return;
    
    if (blk == 0) {
        return;
    }
    
    uint32_t *indirect_block = malloc(block_size);
    if (!indirect_block) {
        perror("malloc failed");
        exit(1);
    }
    
    if (fseek(f, (uint64_t)blk * block_size, SEEK_SET) != 0) {
        perror("fseek failed");
        free(indirect_block);
        exit(1);
    }
    
    if (fread(indirect_block, 1, block_size, f) != block_size) {
        fprintf(stderr, "Warning: short read in indirect block %u\n", blk);
        free(indirect_block);
        return;
    }
    
    uint32_t entries_per_block = block_size / 4;
    for (uint32_t i = 0; i < entries_per_block && *remaining > 0; i++) {
        uint32_t entry = indirect_block[i];
        if (swap) {
            entry = __bswap_32(entry);
        }
        
        if (entry == 0) continue;
        
        if (level == 1) {
            process_block(entry, block_size, swap, f, remaining);
        } else {
            process_indirect(entry, level - 1, block_size, swap, f, remaining);
        }
    }
    
    free(indirect_block);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <image_or_device> <inode_number>\n", argv[0]);
        return 1;
    }
    
    const char *image_path = argv[1];
    uint32_t inode_num = atoi(argv[2]);
    
    if (inode_num == 0) {
        fprintf(stderr, "Invalid inode number: %s\n", argv[2]);
        return 1;
    }
    
    FILE *f = fopen(image_path, "rb");
    if (!f) {
        perror("Failed to open image or device");
        return 1;
    }
    
    struct ext2_super_block sb;
    if (fseek(f, 1024, SEEK_SET) != 0) {
        perror("Seek to superblock failed");
        fclose(f);
        return 1;
    }
    
    if (fread(&sb, sizeof(sb), 1, f) != 1) {
        perror("Failed to read superblock");
        fclose(f);
        return 1;
    }
    
    int need_swap = 0;
    if (sb.s_magic != EXT2_SUPER_MAGIC) {
        uint16_t swapped_magic = __bswap_16(sb.s_magic);
        if (swapped_magic == EXT2_SUPER_MAGIC) {
            need_swap = 1;
            swap_superblock(&sb);
        } else {
            fprintf(stderr, "Not an ext2 filesystem (magic %04x)\n", sb.s_magic);
            fclose(f);
            return 1;
        }
    }
    
    uint32_t block_size = 1024 << sb.s_log_block_size;
    uint32_t inodes_per_group = sb.s_inodes_per_group;
    uint32_t inode_size = sb.s_rev_level >= 1 ? sb.s_inode_size : 128;
    
    uint32_t group_index = (inode_num - 1) / inodes_per_group;
    uint32_t inode_index = (inode_num - 1) % inodes_per_group;
    
    uint32_t gdt_block = sb.s_first_data_block + 1;
    uint64_t gdt_offset = (uint64_t)gdt_block * block_size + group_index * sizeof(struct ext2_group_desc);
    
    struct ext2_group_desc gd;
    if (fseek(f, gdt_offset, SEEK_SET) != 0) {
        perror("Failed to seek to group descriptor");
        fclose(f);
        return 1;
    }
    
    if (fread(&gd, sizeof(gd), 1, f) != 1) {
        perror("Failed to read group descriptor");
        fclose(f);
        return 1;
    }
    
    if (need_swap) {
        swap_group_desc(&gd);
    }
    
    uint64_t inode_table_offset = (uint64_t)gd.bg_inode_table * block_size;
    uint64_t inode_offset = inode_table_offset + inode_index * inode_size;
    
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
    
    if (need_swap) {
        swap_inode(&inode);
    }
    
    uint64_t file_size;
    if ((inode.i_mode & 0xF000) == 0x8000) {
        file_size = ((uint64_t)inode.i_dir_acl << 32) | inode.i_size;
    } else {
        file_size = inode.i_size;
    }
    
    fprintf(stderr, "Inode: %u\n", inode_num);
    fprintf(stderr, "Mode: %o\n", inode.i_mode);
    fprintf(stderr, "Size: %llu bytes\n", (unsigned long long)file_size);
    fprintf(stderr, "Blocks: %u\n", inode.i_blocks);
    
    uint64_t remaining = file_size;
    
    for (int i = 0; i < 12 && remaining > 0; i++) {
        process_block(inode.i_block[i], block_size, need_swap, f, &remaining);
    }
    
    if (remaining > 0 && inode.i_block[12] != 0) {
        process_indirect(inode.i_block[12], 1, block_size, need_swap, f, &remaining);
    }
    
    if (remaining > 0 && inode.i_block[13] != 0) {
        process_indirect(inode.i_block[13], 2, block_size, need_swap, f, &remaining);
    }
    
    if (remaining > 0 && inode.i_block[14] != 0) {
        process_indirect(inode.i_block[14], 3, block_size, need_swap, f, &remaining);
    }
    
    fclose(f);
    return 0;
}