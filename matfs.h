#ifndef __MATFS_H__
#define __MATFS_H__

#include <stdint.h>

#define MATFS_FILENAME_LENGTH 128
struct matfs_superblock {
    uint32_t magic;
    uint32_t sb_nr_inodes_total;
    uint32_t sb_nr_blocks_total;
    uint32_t sb_nr_inode_free;
    uint32_t sb_nr_block_free;

    uint32_t sb_inode_table;
    uint32_t sb_inode_bitmap;
    uint32_t sb_block_table;
    uint32_t sb_block_bitmap;
};


struct matfs_inode {
    uint32_t i_mode;
    uint32_t i_uid;
    uint32_t i_gid;
    uint32_t i_size;
    uint32_t i_ctime;
    uint32_t i_atime;
    uint32_t i_mtime;
    uint32_t i_blocks;
};


struct matfs_file {
    uint32_t inode;
    char filename[MATFS_FILENAME_LENGTH];
};






#endif