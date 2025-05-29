#ifndef _FS_H_
#define _FS_H_ 1

#include <stdint.h>

// File nodes (both in-memory and on-disk)

// Bytes per file system block - same as page size
// 磁盘块的大小，为一页的字节数，也就是4096字节
#define BLOCK_SIZE PAGE_SIZE
// 磁盘块的大小，不过单位是位
#define BLOCK_SIZE_BIT (BLOCK_SIZE * 8)

// Maximum size of a filename (a single path component), including null
// 文件名的最大长度
#define MAXNAMELEN 128

// Maximum size of a complete pathname, including null
// 路径的最大长度
#define MAXPATHLEN 1024

// Number of (direct) block pointers in a File descriptor
// 文件控制块中直接指针（直接索引）的个数
#define NDIRECT 10
// 一个磁盘块所能存储的最大指针数量，“4”指的是存储指针号所占用的空间为4B
#define NINDIRECT (BLOCK_SIZE / 4)
// 文件最大大小
#define MAXFILESIZE (NINDIRECT * BLOCK_SIZE)

#define FILE_STRUCT_SIZE 256

// 文件控制块
struct File {
	// 文件名，最大长度为128
	char f_name[MAXNAMELEN];
	// 文件大小，单位为字节
	uint32_t f_size;
	// 文件类型，分为普通文件(FTYPE_REG)和目录(FTYPE_DIR)两种
	uint32_t f_type;
	// 文件的直接指针（直接索引），直接指向磁盘块
	// 用来记录存储文件的磁盘块的磁盘控制块id
	// 最多存储10个磁盘控制块id，每个磁盘块的大小为4KB
	uint32_t f_direct[NDIRECT];
	// 文件的间接指针（间接索引），指向更多的直接指针
	// 在文件大小超过40KB时使用，共1024个指针，但是不使用前10个指针
	uint32_t f_indirect;
	// 指向文件所属的文件目录
	struct File *f_dir;
	// 让文件控制块和PAGE_SIZE对齐的填充部分
	char f_pad[FILE_STRUCT_SIZE - MAXNAMELEN - (3 + NDIRECT) * 4 - sizeof(void *)];
} __attribute__((aligned(4), packed));

// 一个磁盘块拥有的文件控制块的数目
#define FILE2BLK (BLOCK_SIZE / sizeof(struct File))

// File types
#define FTYPE_REG 0 // Regular file 普通文件
#define FTYPE_DIR 1 // Directory 目录

// File system super-block (both in-memory and on-disk)

#define FS_MAGIC 0x68286097 // Everyone's favorite OS class

// 文件系统中的超级块，是磁盘最开始的第二个磁盘块
// 用来描述文件系统的基本信息，如魔数(Magic Number)、磁盘大小以及根目录的位置
struct Super {
	// 魔数，用于标识该文件系统
	uint32_t s_magic;
	// 记录本文件系统有多少个磁盘块，本文件系统中为1024
	uint32_t s_nblocks;
	// 根目录节点，根目录的f_type为FTYPE_DIR，f_name为“/”
	struct File s_root;
};

#endif // _FS_H_
