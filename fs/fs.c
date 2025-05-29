#include "serv.h"
#include <mmu.h>

// 本文件实现文件系统的基本功能函数

struct Super *super;

// 管理磁盘块的位图
uint32_t *bitmap;

void file_flush(struct File *);
int block_is_free(u_int);

int find_files(const char *path, const char *name, struct Find_res *res) {
        struct File *file;
        // 用 walk_path 来找到 path 对应的文件夹
        // Lab5-Exam: Your code here. (1/2)
	try(walk_path(path, 0, &file, 0));

        // 在 path 对应的文件夹下面遍历，找到所有名字为 name 的文件，你可以调用下面的参考函数 traverse_file
        // Lab5-Exam: Your code here. (2/2)
	try(traverse_file(path, file, name, res));
}

int traverse_file(const char *path, struct File *file, const char *name, struct Find_res *res) {

	u_int nblock;
	nblock = file->f_size / BLOCK_SIZE;

	// 1. 检查路径长度是否符合要求，如不符合，直接返回
	if (strlen(path) == 0 || strlen(path) > MAXPATHLEN) {
		/*返回*/
		return 0;
	}

	// 2. 比较当前文件名是否等于 name，如果相等则更改 res
	if (strcmp(file->f_name, name) == 0) {
		/*增加 res->count*/
		res->count++;
		/*添加 res 的路径*/
		strcpy(res->file_path[res->count - 1],path);
	}
	if (file->f_type == FTYPE_DIR) {
		for (int i = 0; i < nblock; i++) {
			void *blk;
			try(file_get_block(file, i ,&blk));
			struct File *files = (struct File *)blk;

			for (struct File *f = files; f < files + FILE2BLK; ++f) {
				char curpath[MAXPATHLEN + MAXNAMELEN + 1];
				// 3. 把 path 和 name 拼接起来得到下一层文件路径，注意结尾的 '\0'
				// 提示：我们没有实现 strcat 工具函数，你可以用 strcpy 实现拼接
				for (int i = 0; i < strlen(path);i++) {
					curpath[i] = path[i];
				}
				//if (path[strlen(path)] != '/') {
				//	curpath[strlen(path)] = '/';
				//}
				if (curpath[strlen(curpath)] != '/') {
					curpath[strlen(curpath)] = '/';
				}
				int k = strlen(curpath);
				for (int i = 0;i < strlen(name);i++) {
					curpath[k] = name[i];
					k++;
				}
				curpath[strlen(curpath)] = '\0';
				// 4. 递归调用 traverse_file 函数
				traverse_file(curpath, f, name, res);
			}
		}
	}
	return 0;
}

// Overview:
//  Return the virtual address of this disk block in cache.
// Hint: Use 'DISKMAP' and 'BLOCK_SIZE' to calculate the address.
// 获取磁盘块号映射在虚存中的地址
void *disk_addr(u_int blockno) {
	/* Exercise 5.6: Your code here. */
	return (void *)(DISKMAP + blockno * BLOCK_SIZE);
}

// Overview:
//  Check if this virtual address is mapped to a block. (check PTE_V bit)
// 检查虚拟地址是否已经存在映射关系，本质是在检查页表是否有效
int va_is_mapped(void *va) {
	return (vpd[PDX(va)] & PTE_V) && (vpt[VPN(va)] & PTE_V);
}

// Overview:
//  Check if this disk block is mapped in cache.
//  Returns the virtual address of the cache page if mapped, 0 otherwise.
// 检查磁盘是否已经在内存中分配了内存块（或者说是与物理内存之间建立映射），如果没有返回null
void *block_is_mapped(u_int blockno) {
	// 获取磁盘块号在内存中对应的虚拟地址
	void *va = disk_addr(blockno);
	// 如果已经存在映射关系，则返回相应的虚拟地址
	if (va_is_mapped(va)) {
		return va;
	}
	return NULL;
}

// Overview:
//  Check if this virtual address is dirty. (check PTE_DIRTY bit)
// 检查虚拟地址对应的内存是否已经被修改
int va_is_dirty(void *va) {
	return vpt[VPN(va)] & PTE_DIRTY;
}

// Overview:
//  Check if this block is dirty. (check corresponding `va`)
// 检查磁盘块是否已经被修改
int block_is_dirty(u_int blockno) {
	void *va = disk_addr(blockno);
	return va_is_mapped(va) && va_is_dirty(va);
}

// Overview:
//  Mark this block as dirty (cache page has changed and needs to be written back to disk).
// 将磁盘块标记为已修改
int dirty_block(u_int blockno) {
	void *va = disk_addr(blockno);

	if (!va_is_mapped(va)) {
		return -E_NOT_FOUND;
	}

	if (va_is_dirty(va)) {
		return 0;
	}
	// 标记脏位的方式是修改页面权限
	return syscall_mem_map(0, va, 0, va, PTE_D | PTE_DIRTY);
}

// Overview:
//  Write the current contents of the block out to disk.
// 将在内存中的数据写回磁盘
void write_block(u_int blockno) {
	// Step 1: detect is this block is mapped, if not, can't write it's data to disk.
	// 检查是否为磁盘块在内存中分配了空间
	if (!block_is_mapped(blockno)) {
		user_panic("write unmapped block %08x", blockno);
	}

	// Step2: write data to IDE disk. (using ide_write, and the diskno is 0)
	void *va = disk_addr(blockno);
	ide_write(0, blockno * SECT2BLK, va, SECT2BLK);
}

// Overview:
//  Make sure a particular disk block is loaded into memory.
//
// Post-Condition:
//  Return 0 on success, or a negative error code on error.
//
//  If blk!=0, set *blk to the address of the block in memory.
//
//  If isnew!=0, set *isnew to 0 if the block was already in memory, or
//  to 1 if the block was loaded off disk to satisfy this request. (Isnew
//  lets callers like file_get_block clear any memory-only fields
//  from the disk blocks when they come in off disk.)
//
// Hint:
//  use disk_addr, block_is_mapped, syscall_mem_alloc, and ide_read.
// 将指定编号的磁盘块读入内存中，将内存中的虚拟地址保存到指针中
int read_block(u_int blockno, void **blk, u_int *isnew) {
	// Step 1: validate blockno. Make file the block to read is within the disk.
	if (super && blockno >= super->s_nblocks) {
		user_panic("reading non-existent block %08x\n", blockno);
	}

	// Step 2: validate this block is used, not free.
	// Hint:
	//  If the bitmap is NULL, indicate that we haven't read bitmap from disk to memory
	//  until now. So, before we check if a block is free using `block_is_free`, we must
	//  ensure that the bitmap blocks are already read from the disk to memory.
	if (bitmap && block_is_free(blockno)) {
		user_panic("reading free block %08x\n", blockno);
	}

	// Step 3: transform block number to corresponding virtual address.
	void *va = disk_addr(blockno);

	// Step 4: read disk and set *isnew.
	// Hint:
	//  If this block is already mapped, just set *isnew, else alloc memory and
	//  read data from IDE disk (use `syscall_mem_alloc` and `ide_read`).
	//  We have only one IDE disk, so the diskno of ide_read should be 0.
	if (block_is_mapped(blockno)) { // the block is in memory
		if (isnew) {
			*isnew = 0;
		}
	} else { // the block is not in memory
		if (isnew) {
			*isnew = 1;
		}
		try(syscall_mem_alloc(0, va, PTE_D));
		ide_read(0, blockno * SECT2BLK, va, SECT2BLK);
	}

	// Step 5: if blk != NULL, assign 'va' to '*blk'.
	if (blk) {
		*blk = va;
	}
	return 0;
}

// Overview:
//  Allocate a page to cache the disk block.
// 为磁盘块在内存中分配物理内存，建立映射
int map_block(u_int blockno) {
	// Step 1: If the block is already mapped in cache, return 0.
	// Hint: Use 'block_is_mapped'.
	/* Exercise 5.7: Your code here. (1/5) */
	// 如果已经建立了映射，返回0
	if (block_is_mapped(blockno)) {
		return 0;
	}

	// Step 2: Alloc a page in permission 'PTE_D' via syscall.
	// Hint: Use 'disk_addr' for the virtual address.
	/* Exercise 5.7: Your code here. (2/5) */
	// 为磁盘在内存中分配物理内存，权限设置为可写
	try(syscall_mem_alloc(env->env_id, disk_addr(blockno), PTE_D));
}

// Overview:
//  Unmap a disk block in cache.
// 取消原先给磁盘分配的物理内存
void unmap_block(u_int blockno) {
	// Step 1: Get the mapped address of the cache page of this block using 'block_is_mapped'.
	void *va;
	/* Exercise 5.7: Your code here. (3/5) */
	// 磁盘已经建立了映射，获取相应的虚拟地址
	va = block_is_mapped(blockno);

	// Step 2: If this block is used (not free) and dirty in cache, write it back to the disk
	// first.
	// Hint: Use 'block_is_free', 'block_is_dirty' to check, and 'write_block' to sync.
	/* Exercise 5.7: Your code here. (4/5) */
	// 如果磁盘被使用并且响应数据被修改，先将修改数据写回磁盘
	if (!block_is_free(blockno) && block_is_dirty(blockno)) {
		write_block(blockno);
	}

	// Step 3: Unmap the virtual address via syscall.
	/* Exercise 5.7: Your code here. (5/5) */
	// 通过系统调用取消原先的映射关系
	try(syscall_mem_unmap(env->env_id, disk_addr(blockno)));
	// 检查是否真的已经取消了映射关系
	user_assert(!block_is_mapped(blockno));
}

// Overview:
//  Check if the block 'blockno' is free via bitmap.
//
// Post-Condition:
//  Return 1 if the block is free, else 0.
// 根据位图来判断指定的磁盘块是否被占用
int block_is_free(u_int blockno) {
	// 判断磁盘块号是否合法
	if (super == 0 || blockno >= super->s_nblocks) {
		return 0;
	}
	// 位图为1代表空闲
	if (bitmap[blockno / 32] & (1 << (blockno % 32))) {
		return 1;
	}

	return 0;
}

// Overview:
//  Mark a block as free in the bitmap.
// 通过位图设置第no个磁盘块为空闲
void free_block(u_int blockno) {
	// You can refer to the function 'block_is_free' above.
	// Step 1: If 'blockno' is invalid (0 or >= the number of blocks in 'super'), return.
	/* Exercise 5.4: Your code here. (1/2) */
	// 判断磁盘块号是否合法
	if (blockno == 0 || blockno >= super->s_nblocks) {
		return;
	}

	// Step 2: Set the flag bit of 'blockno' in 'bitmap'.
	// Hint: Use bit operations to update the bitmap, such as b[n / W] |= 1 << (n % W).
	/* Exercise 5.4: Your code here. (2/2) */
	// 设置位图为1，将磁盘块标记为空闲
	bitmap[blockno / 32] |= 1 << (blockno % 32);
}

// Overview:
//  Search in the bitmap for a free block and allocate it.
//
// Post-Condition:
//  Return block number allocated on success,
//  Return -E_NO_DISK if we are out of blocks.
// 获得一个空闲磁盘块，返回其磁盘块号
int alloc_block_num(void) {
	int blockno;
	// walk through this bitmap, find a free one and mark it as used, then sync
	// this block to IDE disk (using `write_block`) from memory.
	// 遍历位图，找到一个空闲磁盘块
	// 返回前写回磁盘块的内容到磁盘
	for (blockno = 3; blockno < super->s_nblocks; blockno++) {
		// 通过位图判断磁盘块未被使用
		if (bitmap[blockno / 32] & (1 << (blockno % 32))) { // the block is free
			// 将这个磁盘块标记为在被使用（位图对应位置写0）
			bitmap[blockno / 32] &= ~(1 << (blockno % 32));
			// 将磁盘块中的内容写回到磁盘中去
			write_block(blockno / BLOCK_SIZE_BIT + 2); // write to disk.
			return blockno;
		}
	}
	// 没有空闲的磁盘块
	return -E_NO_DISK;
}

// Overview:
//  Allocate a block -- first find a free block in the bitmap, then map it into memory.
// 找到一个空闲的磁盘块，返回对应的磁盘块号
int alloc_block(void) {
	int r, bno;
	// 找到一个磁盘块
	if ((r = alloc_block_num()) < 0) { // failed.
		return r;
	}
	bno = r;

	// 将磁盘块加载到内存中，建立映射
	if ((r = map_block(bno)) < 0) {
		// 如果失败，则不占用磁盘，恢复位图
		free_block(bno);
		return r;
	}

	// 成功则返回磁盘号
	return bno;
}

// Overview:
//  Read and validate the file system super-block.
//
// Post-condition:
//  If error occurred during read super block or validate failed, panic.
// 读入超级块到磁盘，并检查正确性
void read_super(void) {
	int r;
	void *blk;

	// 将超级块读入内存，获得其地址
	if ((r = read_block(1, &blk, 0)) < 0) {
		user_panic("cannot read superblock: %d", r);
	}

	super = blk;

	// 检查超级块的魔数
	if (super->s_magic != FS_MAGIC) {
		user_panic("bad file system magic number %x %x", super->s_magic, FS_MAGIC);
	}

	// 检查超级块大小
	if (super->s_nblocks > DISKMAX / BLOCK_SIZE) {
		user_panic("file system is too large");
	}

	debugf("superblock is good\n");
}

// Overview:
//  Read and validate the file system bitmap.
//
// Hint:
//  Read all the bitmap blocks into memory.
//  Set the 'bitmap' to point to the first bitmap block.
//  For each block i, user_assert(!block_is_free(i))) to check that they're all marked as in use.
// 读入位图至内存并检查正确性
void read_bitmap(void) {
	u_int i;
	void *blk = NULL;

	// Step 1: Calculate the number of the bitmap blocks, and read them into memory.
	// 计算位图所需的磁盘块数
	u_int nbitmap = super->s_nblocks / BLOCK_SIZE_BIT + 1;
	for (i = 0; i < nbitmap; i++) {
		read_block(i + 2, blk, 0);
	}
	// 设置位图的地址
	bitmap = disk_addr(2);

	// Step 2: Make sure the reserved and root blocks are marked in-use.
	// Hint: use `block_is_free`
	// 检查根和超级块的使用情况
	user_assert(!block_is_free(0));
	user_assert(!block_is_free(1));

	// Step 3: Make sure all bitmap blocks are marked in-use.
	// 确定位图所有所需块被载入内存
	for (i = 0; i < nbitmap; i++) {
		user_assert(!block_is_free(i + 2));
	}

	debugf("read_bitmap is good\n");
}

// Overview:
//  Test that write_block works, by smashing the superblock and reading it back.
void check_write_block(void) {
	super = 0;

	// backup the super block.
	// copy the data in super block to the first block on the disk.
	panic_on(read_block(0, 0, 0));
	memcpy((char *)disk_addr(0), (char *)disk_addr(1), BLOCK_SIZE);

	// smash it
	strcpy((char *)disk_addr(1), "OOPS!\n");
	write_block(1);
	user_assert(block_is_mapped(1));

	// clear it out
	panic_on(syscall_mem_unmap(0, disk_addr(1)));
	user_assert(!block_is_mapped(1));

	// validate the data read from the disk.
	panic_on(read_block(1, 0, 0));
	user_assert(strcmp((char *)disk_addr(1), "OOPS!\n") == 0);

	// restore the super block.
	memcpy((char *)disk_addr(1), (char *)disk_addr(0), BLOCK_SIZE);
	write_block(1);
	super = (struct Super *)disk_addr(1);
}

// Overview:
//  Initialize the file system.
// Hint:
//  1. read super block.
//  2. check if the disk can work.
//  3. read bitmap blocks from disk to memory.
void fs_init(void) {
	// 检查超级块
	read_super();
	// 检查磁盘能否工作
	check_write_block();
	// 检查位图
	read_bitmap();
}

// Overview:
//  Like pgdir_walk but for files.
//  Find the disk block number slot for the 'filebno'th block in file 'f'. Then, set
//  '*ppdiskbno' to point to that slot. The slot will be one of the f->f_direct[] entries,
//  or an entry in the indirect block.
//  When 'alloc' is set, this function will allocate an indirect block if necessary.
//
// Post-Condition:
//  Return 0 on success, and set *ppdiskbno to the pointer to the target block.
//  Return -E_NOT_FOUND if the function needed to allocate an indirect block, but alloc was 0.
//  Return -E_NO_DISK if there's no space on the disk for an indirect block.
//  Return -E_NO_MEM if there's not enough memory for an indirect block.
//  Return -E_INVAL if filebno is out of range (>= NINDIRECT).
int file_block_walk(struct File *f, u_int filebno, uint32_t **ppdiskbno, u_int alloc) {
	int r;
	uint32_t *ptr;
	uint32_t *blk;

	if (filebno < NDIRECT) {
		// Step 1: if the target block is corresponded to a direct pointer, just return the
		// disk block number.
		ptr = &f->f_direct[filebno];
	} else if (filebno < NINDIRECT) {
		// Step 2: if the target block is corresponded to the indirect block, but there's no
		//  indirect block and `alloc` is set, create the indirect block.
		if (f->f_indirect == 0) {
			if (alloc == 0) {
				return -E_NOT_FOUND;
			}

			if ((r = alloc_block()) < 0) {
				return r;
			}
			f->f_indirect = r;
		}

		// Step 3: read the new indirect block to memory.
		if ((r = read_block(f->f_indirect, (void **)&blk, 0)) < 0) {
			return r;
		}
		ptr = blk + filebno;
	} else {
		return -E_INVAL;
	}

	// Step 4: store the result into *ppdiskbno, and return 0.
	*ppdiskbno = ptr;
	return 0;
}

// OVerview:
//  Set *diskbno to the disk block number for the filebno'th block in file f.
//  If alloc is set and the block does not exist, allocate it.
//
// Post-Condition:
//  Returns 0: success, < 0 on error.
//  Errors are:
//   -E_NOT_FOUND: alloc was 0 but the block did not exist.
//   -E_NO_DISK: if a block needed to be allocated but the disk is full.
//   -E_NO_MEM: if we're out of memory.
//   -E_INVAL: if filebno is out of range.
// 获取文件块对应磁盘块号（相对文件）对应的磁盘块号（相对磁盘）
// 如果磁盘块没有被加载到内存中，按alloc设置加载
int file_map_block(struct File *f, u_int filebno, u_int *diskbno, u_int alloc) {
	int r;
	uint32_t *ptr;

	// 找到文件的第f_no个磁盘块，将文件控制块中存有磁盘块号的地址保存到指针中
	if ((r = file_block_walk(f, filebno, &ptr, alloc)) < 0) {
		return r;
	}

	// 如果磁盘块不存在，按alloc创建
	if (*ptr == 0) {
		// 如果不需要创建，则报错
		if (alloc == 0) {
			return -E_NOT_FOUND;
		}
		// 创建一个磁盘块供使用
		if ((r = alloc_block()) < 0) {
			return r;
		}
		*ptr = r;
	}

	// 将对应的结果保存到指针中
	*diskbno = *ptr;
	return 0;
}

// Overview:
//  Remove a block from file f. If it's not there, just silently succeed.
int file_clear_block(struct File *f, u_int filebno) {
	int r;
	uint32_t *ptr;

	if ((r = file_block_walk(f, filebno, &ptr, 0)) < 0) {
		return r;
	}

	if (*ptr) {
		free_block(*ptr);
		*ptr = 0;
	}

	return 0;
}

// Overview:
//  Set *blk to point at the filebno'th block in file f.
//
// Hint: use file_map_block and read_block.
//
// Post-Condition:
//  return 0 on success, and read the data to `blk`, return <0 on error.
// 将某个指定的文件指向的磁盘块读入内存
// 获取文件第f_no个磁盘块，保存到指针中，没有则创建
int file_get_block(struct File *f, u_int filebno, void **blk) {
	int r;
	u_int diskbno;
	u_int isnew;

	// Step 1: find the disk block number is `f` using `file_map_block`.
	// 获取文件块对应磁盘块号（相对文件）对应的磁盘块号（相对磁盘）
	if ((r = file_map_block(f, filebno, &diskbno, 1)) < 0) {
		return r;
	}

	// Step 2: read the data in this disk to blk.
	// 将磁盘内容以块为单位读入内存中的相应位置
	if ((r = read_block(diskbno, blk, &isnew)) < 0) {
		return r;
	}
	return 0;
}

// Overview:
//  Mark the offset/BLOCK_SIZE'th block dirty in file f.
// 将文件控制块标记为脏
int file_dirty(struct File *f, u_int offset) {
	int r;
	u_int diskbno;

	if ((r = file_map_block(f, offset / BLOCK_SIZE, &diskbno, 0)) < 0) {
		return r;
	}

	return dirty_block(diskbno);
}

// Overview:
//  Find a file named 'name' in the directory 'dir'. If found, set *file to it.
//
// Post-Condition:
//  Return 0 on success, and set the pointer to the target file in `*file`.
//  Return the underlying error if an error occurs.
// 查找某个目录下是否存在指定的文件（使用文件名来查找）
int dir_lookup(struct File *dir, char *name, struct File **file) {
	// Step 1: Calculate the number of blocks in 'dir' via its size.
	u_int nblock;
	/* Exercise 5.8: Your code here. (1/3) */
	// 获取目录占有的总磁盘块数，dir 意为 dictionary
	nblock = dir->f_size / BLOCK_SIZE;


	// Step 2: Iterate through all blocks in the directory.
	// 遍历目录占据的所有磁盘块，寻找文件控制块
	for (int i = 0; i < nblock; i++) {
		// Read the i'th block of 'dir' and get its address in 'blk' using 'file_get_block'.
		void *blk;
		/* Exercise 5.8: Your code here. (2/3) */
		// 获取文件第i个磁盘块，保存到blk指针中
		try(file_get_block(dir, i, &blk));

		struct File *files = (struct File *)blk;

		// Find the target among all 'File's in this block.
		// 遍历磁盘块中所有的文件控制块，比较文件名
		for (struct File *f = files; f < files + FILE2BLK; ++f) {
			// Compare the file name against 'name' using 'strcmp'.
			// If we find the target file, set '*file' to it and set up its 'f_dir'
			// field.
			/* Exercise 5.8: Your code here. (3/3) */
			// 比较文件名来判断是否为所需文件
			if (strcmp(f->f_name, name) == 0) {
				*file = f;
				// 设置文件的所属目录
				f->f_dir = dir;
				return 0;
			}
		}
	}

	return -E_NOT_FOUND;
}

// Overview:
//  Alloc a new File structure under specified directory. Set *file
//  to point at a free File structure in dir.
// 在目录下创建文件，把文件保存到指针
int dir_alloc_file(struct File *dir, struct File **file) {
	int r;
	u_int nblock, i, j;
	void *blk;
	struct File *f;

	nblock = dir->f_size / BLOCK_SIZE;

	for (i = 0; i < nblock; i++) {
		// read the block.
		if ((r = file_get_block(dir, i, &blk)) < 0) {
			return r;
		}

		f = blk;

		for (j = 0; j < FILE2BLK; j++) {
			if (f[j].f_name[0] == '\0') { // found free File structure.
				*file = &f[j];
				return 0;
			}
		}
	}

	// no free File structure in exists data block.
	// new data block need to be created.
	dir->f_size += BLOCK_SIZE;
	if ((r = file_get_block(dir, i, &blk)) < 0) {
		return r;
	}
	f = blk;
	*file = &f[0];

	return 0;
}

// Overview:
//  Skip over slashes.
char *skip_slash(char *p) {
	while (*p == '/') {
		p++;
	}
	return p;
}

// Overview:
//  Evaluate a path name, starting at the root.
//
// Post-Condition:
//  On success, set *pfile to the file we found and set *pdir to the directory
//  the file is in.
//  If we cannot find the file but find the directory it should be in, set
//  *pdir and copy the final path element into lastelem.
int walk_path(char *path, struct File **pdir, struct File **pfile, char *lastelem) {
	char *p;
	char name[MAXNAMELEN];
	struct File *dir, *file;
	int r;

	// start at the root.
	path = skip_slash(path);
	file = &super->s_root;
	dir = 0;
	name[0] = 0;

	if (pdir) {
		*pdir = 0;
	}

	*pfile = 0;

	// find the target file by name recursively.
	while (*path != '\0') {
		dir = file;
		p = path;

		while (*path != '/' && *path != '\0') {
			path++;
		}

		if (path - p >= MAXNAMELEN) {
			return -E_BAD_PATH;
		}

		memcpy(name, p, path - p);
		name[path - p] = '\0';
		path = skip_slash(path);
		if (dir->f_type != FTYPE_DIR) {
			return -E_NOT_FOUND;
		}

		if ((r = dir_lookup(dir, name, &file)) < 0) {
			if (r == -E_NOT_FOUND && *path == '\0') {
				if (pdir) {
					*pdir = dir;
				}

				if (lastelem) {
					strcpy(lastelem, name);
				}

				*pfile = 0;
			}

			return r;
		}
	}

	if (pdir) {
		*pdir = dir;
	}

	*pfile = file;
	return 0;
}

// Overview:
//  Open "path".
//
// Post-Condition:
//  On success set *pfile to point at the file and return 0.
//  On error return < 0.
int file_open(char *path, struct File **file) {
	return walk_path(path, 0, file, 0);
}

// Overview:
//  Create "path".
//
// Post-Condition:
//  On success set *file to point at the file and return 0.
//  On error return < 0.
int file_create(char *path, struct File **file) {
	char name[MAXNAMELEN];
	int r;
	struct File *dir, *f;

	if ((r = walk_path(path, &dir, &f, name)) == 0) {
		return -E_FILE_EXISTS;
	}

	if (r != -E_NOT_FOUND || dir == 0) {
		return r;
	}

	if (dir_alloc_file(dir, &f) < 0) {
		return r;
	}

	strcpy(f->f_name, name);
	*file = f;
	return 0;
}

// Overview:
//  Truncate file down to newsize bytes.
//
//  Since the file is shorter, we can free the blocks that were used by the old
//  bigger version but not by our new smaller self. For both the old and new sizes,
//  figure out the number of blocks required, and then clear the blocks from
//  new_nblocks to old_nblocks.
//
//  If the new_nblocks is no more than NDIRECT, free the indirect block too.
//  (Remember to clear the f->f_indirect pointer so you'll know whether it's valid!)
//
// Hint: use file_clear_block.
void file_truncate(struct File *f, u_int newsize) {
	u_int bno, old_nblocks, new_nblocks;

	old_nblocks = ROUND(f->f_size, BLOCK_SIZE) / BLOCK_SIZE;
	new_nblocks = ROUND(newsize, BLOCK_SIZE) / BLOCK_SIZE;

	if (newsize == 0) {
		new_nblocks = 0;
	}

	if (new_nblocks <= NDIRECT) {
		for (bno = new_nblocks; bno < old_nblocks; bno++) {
			panic_on(file_clear_block(f, bno));
		}
		if (f->f_indirect) {
			free_block(f->f_indirect);
			f->f_indirect = 0;
		}
	} else {
		for (bno = new_nblocks; bno < old_nblocks; bno++) {
			panic_on(file_clear_block(f, bno));
		}
	}
	f->f_size = newsize;
}

// Overview:
//  Set file size to newsize.
int file_set_size(struct File *f, u_int newsize) {
	if (f->f_size > newsize) {
		file_truncate(f, newsize);
	}

	f->f_size = newsize;

	if (f->f_dir) {
		file_flush(f->f_dir);
	}

	return 0;
}

// Overview:
//  Flush the contents of file f out to disk.
//  Loop over all the blocks in file.
//  Translate the file block number into a disk block number and then
//  check whether that disk block is dirty. If so, write it out.
//
// Hint: use file_map_block, block_is_dirty, and write_block.
void file_flush(struct File *f) {
	u_int nblocks;
	u_int bno;
	u_int diskbno;
	int r;

	nblocks = ROUND(f->f_size, BLOCK_SIZE) / BLOCK_SIZE;

	for (bno = 0; bno < nblocks; bno++) {
		if ((r = file_map_block(f, bno, &diskbno, 0)) < 0) {
			continue;
		}
		if (block_is_dirty(diskbno)) {
			write_block(diskbno);
		}
	}
}

// Overview:
//  Sync the entire file system.  A big hammer.
void fs_sync(void) {
	int i;
	for (i = 0; i < super->s_nblocks; i++) {
		if (block_is_dirty(i)) {
			write_block(i);
		}
	}
}

// Overview:
//  Close a file.
void file_close(struct File *f) {
	// Flush the file itself, if f's f_dir is set, flush it's f_dir.
	file_flush(f);
	if (f->f_dir) {
		u_int nblock = f->f_dir->f_size / BLOCK_SIZE;
		for (int i = 0; i < nblock; i++) {
			u_int diskbno;
			struct File *files;
			if (file_map_block(f->f_dir, i, &diskbno, 0) < 0) {
				debugf("file_close: file_map_block failed\n");
				break;
			}
			if (read_block(diskbno, (void **)&files, 0) < 0) {
				debugf("file_close: read_block failed\n");
				break;
			}
			if (files <= f && f < files + FILE2BLK) {
				dirty_block(diskbno);
				break;
			}
		}
		file_flush(f->f_dir);
	}
}

// Overview:
//  Remove a file by truncating it and then zeroing the name.
int file_remove(char *path) {
	int r;
	struct File *f;

	// Step 1: find the file on the disk.
	if ((r = walk_path(path, 0, &f, 0)) < 0) {
		return r;
	}

	// Step 2: truncate it's size to zero.
	file_truncate(f, 0);

	// Step 3: clear it's name.
	f->f_name[0] = '\0';

	// Step 4: flush the file.
	file_flush(f);
	if (f->f_dir) {
		file_flush(f->f_dir);
	}

	return 0;
}
