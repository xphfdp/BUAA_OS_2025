#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#include "../user/include/fs.h"

// 本文件用于创建磁盘镜像

/* Static assert, for compile-time assertion checking */
#define static_assert(c) (void)(char(*)[(c) ? 1 : -1])0

#define nelem(x) (sizeof(x) / sizeof((x)[0]))
typedef struct Super Super;
typedef struct File File;

// 磁盘中磁盘块的数量
#define NBLOCK 1024
// 存储位图所需要的磁盘块（位图块）的数量
uint32_t nbitblock;
// 下一个可用磁盘块的id
uint32_t nextbno;

// 本磁盘中的超级块
struct Super super;

// 磁盘块的类型
enum {
	BLOCK_FREE = 0,
	BLOCK_BOOT = 1,
	BLOCK_BMAP = 2,
	BLOCK_SUPER = 3,
	BLOCK_DATA = 4,
	BLOCK_FILE = 5,
	BLOCK_INDEX = 6,
};

// 磁盘块是一个虚拟概念，是OS与磁盘交互的最小单位
// 将磁盘分为了若干个磁盘块，使用disk数组进行管理
struct Block {
	// 磁盘块中存储的数据
	uint8_t data[BLOCK_SIZE];
	// 磁盘块的类型
	uint32_t type;
} disk[NBLOCK];

// reverse: mutually transform between little endian and big endian.
// 进行大小尾端转换
void reverse(uint32_t *p) {
	uint8_t *x = (uint8_t *)p;
	uint32_t y = *(uint32_t *)x;
	x[3] = y & 0xFF;
	x[2] = (y >> 8) & 0xFF;
	x[1] = (y >> 16) & 0xFF;
	x[0] = (y >> 24) & 0xFF;
}

// reverse_block: reverse proper filed in a block.
void reverse_block(struct Block *b) {
	int i, j;
	struct Super *s;
	struct File *f, *ff;
	uint32_t *u;

	switch (b->type) {
	case BLOCK_FREE:
	case BLOCK_BOOT:
		break; // do nothing.
	case BLOCK_SUPER:
		s = (struct Super *)b->data;
		reverse(&s->s_magic);
		reverse(&s->s_nblocks);

		ff = &s->s_root;
		reverse(&ff->f_size);
		reverse(&ff->f_type);
		for (i = 0; i < NDIRECT; ++i) {
			reverse(&ff->f_direct[i]);
		}
		reverse(&ff->f_indirect);
		break;
	case BLOCK_FILE:
		f = (struct File *)b->data;
		for (i = 0; i < FILE2BLK; ++i) {
			ff = f + i;
			if (ff->f_name[0] == 0) {
				break;
			} else {
				reverse(&ff->f_size);
				reverse(&ff->f_type);
				for (j = 0; j < NDIRECT; ++j) {
					reverse(&ff->f_direct[j]);
				}
				reverse(&ff->f_indirect);
			}
		}
		break;
	case BLOCK_INDEX:
	case BLOCK_BMAP:
		u = (uint32_t *)b->data;
		for (i = 0; i < BLOCK_SIZE / 4; ++i) {
			reverse(u + i);
		}
		break;
	}
}

// 磁盘初始化，对位图和超级块进行设置，将所有的块都标记为空闲块
void init_disk() {
	int i, diff;

	// 将第一个磁盘块设置为主引导扇区，作为引导扇区和分区表使用
	disk[0].type = BLOCK_BOOT;

	// 存储位图所需要的磁盘块数量
	nbitblock = (NBLOCK + BLOCK_SIZE_BIT - 1) / BLOCK_SIZE_BIT;
	// 从第三个磁盘块开始设置磁盘的位图（第二个磁盘块是超级块）
	nextbno = 2 + nbitblock;

	// 设置位图块
	for (i = 0; i < nbitblock; ++i) {
		disk[2 + i].type = BLOCK_BMAP;
	}
	// 初始化位图，将位图中的每一位都设置为1，表示磁盘块处于空闲状态
	for (i = 0; i < nbitblock; ++i) {
		memset(disk[2 + i].data, 0xff, BLOCK_SIZE);
	}
	// 如果位图无法完全占满磁盘块，将多余的位设置为0
	if (NBLOCK != nbitblock * BLOCK_SIZE_BIT) {
		diff = NBLOCK % BLOCK_SIZE_BIT / 8;
		memset(disk[2 + (nbitblock - 1)].data + diff, 0x00, BLOCK_SIZE - diff);
	}

	// 将第二个磁盘块设置为超级块
	disk[1].type = BLOCK_SUPER;
	// 初始化超级块
	super.s_magic = FS_MAGIC;
	super.s_nblocks = NBLOCK;
	super.s_root.f_type = FTYPE_DIR;
	super.s_root.f_dir = NULL;
	strcpy(super.s_root.f_name, "/");
}

// 获取下一个可用磁盘块的id
int next_block(int type) {
	disk[nextbno].type = type;
	return nextbno++;
}

// Flush disk block usage to bitmap.
// 根据磁盘块的使用情况设置位图
void flush_bitmap() {
	int i;
	// update bitmap, mark all bit where corresponding block is used.
	for (i = 0; i < nextbno; ++i) {
		((uint32_t *)disk[2 + i / BLOCK_SIZE_BIT].data)[(i % BLOCK_SIZE_BIT) / 32] &=
		    ~(1 << (i % 32));
	}
}

// Finish all work, dump block array into physical file.
void finish_fs(char *name) {
	int fd, i;

	// Prepare super block.
	memcpy(disk[1].data, &super, sizeof(super));

	// Dump data in `disk` to target image file.
	fd = open(name, O_RDWR | O_CREAT, 0666);
	for (i = 0; i < 1024; ++i) {
#ifdef CONFIG_REVERSE_ENDIAN
		reverse_block(disk + i);
#endif
		ssize_t n = write(fd, disk[i].data, BLOCK_SIZE);
		assert(n == BLOCK_SIZE);
	}

	// Finish.
	close(fd);
}

// Save block link.
// 将磁盘块添加到目录下
void save_block_link(struct File *f, int nblk, int bno) {
	// 检查文件是否过大
	assert(nblk < NINDIRECT); // if not, file is too large !

	// 目录下属的文件较少，可以使用直接指针
	if (nblk < NDIRECT) {
		f->f_direct[nblk] = bno;
	} else { // 使用间接指针
		// 为间接指针分配一个空闲磁盘块，用于存储其他磁盘块
		if (f->f_indirect == 0) {
			// create new indirect block.
			f->f_indirect = next_block(BLOCK_INDEX);
		}
		((uint32_t *)(disk[f->f_indirect].data))[nblk] = bno;
	}
}

// Make new block contains link to files in a directory.
// 获取下一个空闲的磁盘控制块，并添加到对应目录下
int make_link_block(struct File *dirf, int nblk) {
	// 获取一个可用的磁盘控制块id
	int bno = next_block(BLOCK_FILE);
	// 将磁盘块添加到目录下
	save_block_link(dirf, nblk, bno);
	// 增加文件大小，dirf 意为 dictionary_file
	dirf->f_size += BLOCK_SIZE;
	return bno;
}

// Overview:
//  Allocate an unused 'struct File' under the specified directory.
//
//  Note that when we delete a file, we do not re-arrange all
//  other 'File's, so we should reuse existing unused 'File's here.
//
// Post-Condition:
//  Return a pointer to an unused 'struct File'.
//  We assume that this function will never fail.
//
// Hint:
//  Use 'make_link_block' to allocate a new block for the directory if there are no existing unused
//  'File's.
// 在目录下寻找可用的文件控制块，返回相应的文件控制块指针
struct File *create_file(struct File *dirf) {
	// dirf 意为 dictionary_file
	// 目录占据的磁盘块数量
	int nblk = dirf->f_size / BLOCK_SIZE;

	// 先检查原有目录中是否有现在不被使用的磁盘控制块
	// 遍历目录占据的所有磁盘块
	for (int i = 0; i < nblk; ++i) {
		// 磁盘块号
		int bno;
		// If the block number is in the range of direct pointers (NDIRECT), get the 'bno'
		// directly from 'f_direct'. Otherwise, access the indirect block on 'disk' and get
		// the 'bno' at the index.
		/* Exercise 5.5: Your code here. (1/3) */
		if (i < NDIRECT) {
			// 如果i在直接指针区域，可以直接获取磁盘块，因为直接指针直接指向磁盘块
			bno = dirf->f_direct[i];
		} else {
			// 否则访问间接指针指向的磁盘块，该磁盘块中存储着许多直接指向磁盘块的指针
			// 通过存储的指针获取磁盘块
			bno = ((uint32_t *)(disk[dirf->f_indirect].data))[i];
		}

		// Get the directory block using the block number.
		// 获取磁盘块的起始文件控制块
		struct File *blk = (struct File *)(disk[bno].data);

		// Iterate through all 'File's in the directory block.
		// 遍历磁盘块存储的文件控制块
		for (struct File *f = blk; f < blk + FILE2BLK; ++f) {
			// If the first byte of the file name is null, the 'File' is unused.
			// Return a pointer to the unused 'File'.
			/* Exercise 5.5: Your code here. (2/3) */
			// 文件名为null，代表文件未被使用，返回未被使用的文件控制块
			if (f->f_name[0] == NULL) {
				return f;
			}
		}
	}

	// Step 2: If no unused file is found, allocate a new block using 'make_link_block' function
	// and return a pointer to the new block on 'disk'.
	/* Exercise 5.5: Your code here. (3/3) */
	// 如果进行到这一步，说明磁盘中所有的文件控制块都被使用，需要新的磁盘控制块
	return (struct File *)(disk[make_link_block(dirf, nblk)].data);
}

// Write file to disk under specified dir.
// 将文件写入磁盘
void write_file(struct File *dirf, const char *path) {
	int iblk = 0, r = 0, n = sizeof(disk[0].data);
	// 在目录下创建一个文件控制块，已经初始化
	struct File *target = create_file(dirf);

	/* in case `create_file` is't filled */
	if (target == NULL) {
		return;
	}

	int fd = open(path, O_RDONLY);

	// Get file name with no path prefix.
	const char *fname = strrchr(path, '/');
	if (fname) {
		fname++;
	} else {
		fname = path;
	}
	strcpy(target->f_name, fname);

	target->f_size = lseek(fd, 0, SEEK_END);
	target->f_type = FTYPE_REG;

	// Start reading file.
	lseek(fd, 0, SEEK_SET);
	while ((r = read(fd, disk[nextbno].data, n)) > 0) {
		save_block_link(target, iblk++, next_block(BLOCK_DATA));
	}
	close(fd); // Close file descriptor.
}

// Overview:
//  Write directory to disk under specified dir.
//  Notice that we may use POSIX library functions to operate on
//  directory to get file infomation.
//
// Post-Condition:
//  We ASSUME that this funcion will never fail
void write_directory(struct File *dirf, char *path) {
	DIR *dir = opendir(path);
	if (dir == NULL) {
		perror("opendir");
		return;
	}
	struct File *pdir = create_file(dirf);
	strncpy(pdir->f_name, basename(path), MAXNAMELEN - 1);
	if (pdir->f_name[MAXNAMELEN - 1] != 0) {
		fprintf(stderr, "file name is too long: %s\n", path);
		// File already created, no way back from here.
		exit(1);
	}
	pdir->f_type = FTYPE_DIR;
	for (struct dirent *e; (e = readdir(dir)) != NULL;) {
		if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0) {
			char *buf = malloc(strlen(path) + strlen(e->d_name) + 2);
			sprintf(buf, "%s/%s", path, e->d_name);
			if (e->d_type == DT_DIR) {
				write_directory(pdir, buf);
			} else {
				write_file(pdir, buf);
			}
			free(buf);
		}
	}
	closedir(dir);
}

int main(int argc, char **argv) {
	static_assert(sizeof(struct File) == FILE_STRUCT_SIZE);
	init_disk();

	if (argc < 3) {
		fprintf(stderr, "Usage: fsformat <img-file> [files or directories]...\n");
		exit(1);
	}

	for (int i = 2; i < argc; i++) {
		char *name = argv[i];
		struct stat stat_buf;
		int r = stat(name, &stat_buf);
		assert(r == 0);
		if (S_ISDIR(stat_buf.st_mode)) {
			printf("writing directory '%s' recursively into disk\n", name);
			write_directory(&super.s_root, name);
		} else if (S_ISREG(stat_buf.st_mode)) {
			printf("writing regular file '%s' into disk\n", name);
			write_file(&super.s_root, name);
		} else {
			fprintf(stderr, "'%s' has illegal file mode %o\n", name, stat_buf.st_mode);
			exit(2);
		}
	}

	flush_bitmap();
	finish_fs(argv[1]);

	return 0;
}
