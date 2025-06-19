#include <fs.h>
#include <lib.h>
#include <mmu.h>

#define PTE_DIRTY 0x0004 // file system block cache is dirty

// 一个扇区的大小，为512字节
#define SECT_SIZE 512			  /* Bytes per disk sector */
// 每个磁盘块所含有的扇区数量
// BLOCK_SIZE：一个磁盘块的大小
#define SECT2BLK (BLOCK_SIZE / SECT_SIZE) /* sectors to a block */

/* Disk block n, when in memory, is mapped into the file system
 * server's address space at DISKMAP+(n*BLOCK_SIZE). */
// DISKMAP和DISKMAP+DISKMAX之间的这一段虚拟内存地址空间作为块缓冲区
// 磁盘块缓存区域的起始虚拟地址
#define DISKMAP 0x10000000

/* Maximum disk size we can handle (1GB) */
// 磁盘块缓存区域的终止虚拟地址
#define DISKMAX 0x40000000

/* ide.c */
void ide_read(u_int diskno, u_int secno, void *dst, u_int nsecs);
void ide_write(u_int diskno, u_int secno, void *src, u_int nsecs);

/* fs.c */
int file_open(u_int envid, char *path, struct File **pfile);
int file_create(u_int envid, char *path, struct File **file);
int file_get_block(struct File *f, u_int blockno, void **pblk);
int file_set_size(struct File *f, u_int newsize);
void file_close(struct File *f);
int file_remove(u_int envid, char *path);
int file_dirty(struct File *f, u_int offset);
void file_flush(struct File *);
int file_mkdir(u_int envid, char *path, int isRecursive);

void fs_init(void);
void fs_sync(void);
extern uint32_t *bitmap;
int map_block(u_int);
int alloc_block(void);
