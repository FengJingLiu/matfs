#include <fcntl.h>
#include <linux/fs.h>
#include <linux/nvme_ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "matfs.h"

#define NVME_WRITE 1
#define NVME_READ 2
#define SECTOR_SIZE 512
#define INODE_SECTOR_SIZE 512
#define BLOCK_SECTOR_SIZE 2 * 1024 * 1024
#define MATFS_MAGIC 0xFEEDBABE

struct superblock {
  struct matfs_superblock info;
};

int nvme_write(int fd, __u64 slab, void* blocks, int length) {
  struct nvme_user_io io;
  memset(&io, 0, sizeof(struct nvme_user_io));
  io.addr = (__u64)blocks;
  io.slba = slab;
  io.nblocks = length / SECTOR_SIZE - 1;
  io.opcode = NVME_WRITE;

  if (-1 == ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io)) {
    perror("ioctl write");
    return -1;
  }
  // printf("write lba: %lld, nblocks:%d\n", slab, length / SECTOR_SIZE + 1);
  return 0;
}

int nvme_read(int fd, __u64 slab, void* blocks, int length) {
  struct nvme_user_io io;
  memset(&io, 0, sizeof(struct nvme_user_io));
  io.addr = (__u64)blocks;
  io.slba = slab;
  io.nblocks = length / SECTOR_SIZE - 1;
  io.opcode = NVME_READ;

  if (-1 == ioctl(fd, NVME_IOCTL_SUBMIT_IO, &io)) {
    perror("ioctl read");
    return -1;
  }
  // printf("read lba: %lld, nblocks:%d\n", slab, length / SECTOR_SIZE + 1);
  return 0;
}

struct superblock* matfs_write_superblock(int fd, struct stat* fstat) {
  struct superblock* sb = malloc(sizeof(struct superblock));  //512
  if (!sb) {
    perror("malloc");
    return NULL;
  }

  long int disk_size = fstat->st_size;
  uint32_t sector_cnt = disk_size / SECTOR_SIZE;

  sb->info.magic = MATFS_MAGIC;

  // 元数据占用 1/1000 空间
  sb->info.sb_nr_inodes_total = sector_cnt / 1000;
  // block 2M 一块
  sb->info.sb_nr_blocks_total =
      (sector_cnt - sb->info.sb_nr_inodes_total) /
      (BLOCK_SECTOR_SIZE / INODE_SECTOR_SIZE);  // 2M per block

  // inode bitmap 从 idx=1 sector 开始
  sb->info.sb_inode_bitmap = 1;
  // 8bits per byte 算出总共需要多少 sector 来保存 inode bitmap 向上取整
  uint32_t inode_bitmap_sectors =
      (sb->info.sb_nr_inodes_total + (INODE_SECTOR_SIZE * 8 - 1)) /
      (INODE_SECTOR_SIZE * 8);

  sb->info.sb_inode_table = sb->info.sb_inode_bitmap + inode_bitmap_sectors;

  sb->info.sb_block_bitmap = sb->info.sb_nr_inodes_total;

  // 算出总共需要多少 sector 来保存 block bitmap 向上取整
  uint32_t block_bitmap_sectors =
      (sb->info.sb_nr_blocks_total + (INODE_SECTOR_SIZE * 8 - 1)) /
      (INODE_SECTOR_SIZE * 8);

  // block 起始扇区
  sb->info.sb_block_table = sb->info.sb_block_bitmap + block_bitmap_sectors;

  sb->info.sb_nr_inode_free = sb->info.sb_nr_inodes_total - 1;
  sb->info.sb_nr_block_free = sb->info.sb_nr_blocks_total;

  printf(
      "sb_nr_inodes_total = %d\n"
      "sb_nr_blocks_total = %d\n"
      "inode_bitmap_sectors = %d\n"
      "sb_inode_table = %d\n"
      "sb_block_bitmap = %d\n"
      "block_bitmap_sectors = %d\n"
      "sb_block_table = %d\n",
      sb->info.sb_nr_inodes_total, sb->info.sb_nr_blocks_total,
      inode_bitmap_sectors, sb->info.sb_inode_table, sb->info.sb_block_bitmap,
      block_bitmap_sectors, sb->info.sb_block_table);

  if (nvme_write(fd, 0, (char*)sb, SECTOR_SIZE)) {
    printf("matfs_write_superblock failed");
    free(sb);
    return NULL;
  }

  return sb;
}

int matfs_write_inodes_bitmap(int fd, struct superblock* sb) {
  char buffer[INODE_SECTOR_SIZE] = {0};

  uint32_t bitmap_sectornum =
      (sb->info.sb_nr_inodes_total + (INODE_SECTOR_SIZE * 8 - 1)) /
      (INODE_SECTOR_SIZE * 8);

  for (int i = 0; i < bitmap_sectornum; ++i) {
    if (0 != nvme_write(fd, sb->info.sb_inode_bitmap + i, buffer,
                        INODE_SECTOR_SIZE)) {
      return -1;
    }
  }

  return 0;
}

int matfs_write_inodes_table(int fd, struct superblock* sb) {
  char buffer[INODE_SECTOR_SIZE] = {0};

  struct matfs_inode* root_inode = (struct matfs_inode*)buffer;
  root_inode->i_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP |
                       S_IXUSR | S_IXGRP | S_IXOTH;
  root_inode->i_uid = 0;
  root_inode->i_gid = 0;
  root_inode->i_size = SECTOR_SIZE;
  root_inode->i_blocks = 0;
  root_inode->i_ctime = root_inode->i_atime = root_inode->i_mtime = 0;

  if (0 != nvme_write(fd, sb->info.sb_inode_table, buffer, INODE_SECTOR_SIZE)) {
    return -1;
  }

  memset(buffer, 0, INODE_SECTOR_SIZE);

  uint32_t inode_count =
      sb->info.sb_nr_inodes_total - 2 -
      (sb->info.sb_nr_inodes_total / (INODE_SECTOR_SIZE * 8));

  for (int i = 1; i < inode_count; ++i) {
    if (0 != nvme_write(fd, sb->info.sb_inode_table + i, buffer,
                        INODE_SECTOR_SIZE)) {
      return -1;
    }
  }

  return 0;
}

int matfs_write_blocks_bitmap(int fd, struct superblock* sb) {
  char buffer[INODE_SECTOR_SIZE] = {0};

  uint32_t bitmap_sectornum =
      (sb->info.sb_nr_blocks_total + (INODE_SECTOR_SIZE * 8 - 1)) /
      (INODE_SECTOR_SIZE * 8);

  // 标记 root block 已用
  buffer[0] = 1;

  if (0 !=
      nvme_write(fd, sb->info.sb_block_bitmap, buffer, INODE_SECTOR_SIZE)) {
    return -1;
  }

  memset(buffer, 0, INODE_SECTOR_SIZE);

  for (int i = 1; i < bitmap_sectornum; ++i) {
    if (0 != nvme_write(fd, sb->info.sb_block_bitmap + i, buffer,
                        INODE_SECTOR_SIZE)) {
      return -1;
    }
  }

  return 0;
}

int matfs_write_blocks_table(int fd, struct superblock* sb) {
  char buffer[BLOCK_SECTOR_SIZE] = {0};

  uint32_t inode_count =
      sb->info.sb_nr_inodes_total - 2 -
      (sb->info.sb_nr_inodes_total / (INODE_SECTOR_SIZE * 8));

  for (int i = 0; i < inode_count; ++i) {
    if (0 != nvme_write(fd, sb->info.sb_block_table + i, buffer,
                        BLOCK_SECTOR_SIZE)) {
      return -1;
    }
  }

  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    perror("argc");
    return -1;
  }

  const char* filename = argv[1];

  int fd = open(filename, O_RDWR);
  if (fd < 0) {
    perror("open");
    return -1;
  }

  struct stat stat_buf;
  int ret = fstat(fd, &stat_buf);
  if (ret) {
    perror("fstat");
    return -1;
  }

  if ((stat_buf.st_mode & S_IFMT) == S_IFBLK) {
    long blksize = 0;
    if (0 != ioctl(fd, BLKGETSIZE64, &blksize)) {
      perror("ioctl");
      return -1;
    }
    stat_buf.st_size = blksize;
  }

  printf("size :%ld mb \n", stat_buf.st_size / 1024 / 1024);
  printf("sector :%ld \n", stat_buf.st_size / SECTOR_SIZE);

  struct superblock* sb = matfs_write_superblock(fd, &stat_buf);
  if (!sb) {
    return -1;
  }

  if (0 != matfs_write_inodes_bitmap(fd, sb)) {
    perror("matfs_write_inodes_bitmap");
    goto release;
  }

  if (0 != matfs_write_inodes_table(fd, sb)) {
    perror("matfs_write_inodes_table");
    goto release;
  }

  if (0 != matfs_write_blocks_bitmap(fd, sb)) {
    perror("matfs_write_blocks_bitmap");
    goto release;
  }

  if (0 != matfs_write_blocks_table(fd, sb)) {
    perror("matfs_write_blocks_table");
    goto release;
  }

  return 0;

release:
  free(sb);
  close(fd);
  return -1;
}