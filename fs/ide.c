/*
 * operations on IDE disk.
 */

#include "serv.h"
#include <lib.h>
#include <malta.h>
#include <mmu.h>

// 本文件实现通过系统调用与磁盘镜像进行交互

/* Overview:
 *   Wait for the IDE device to complete previous requests and be ready
 *   to receive subsequent requests.
 */
// 等待IDE的操作就绪，当没有就绪时就忙等待
static uint8_t wait_ide_ready() {
	uint8_t flag;
	while (1) {
		// 读取相应信息到flag
		panic_on(syscall_read_dev(&flag, MALTA_IDE_STATUS, 1));
		if ((flag & MALTA_IDE_BUSY) == 0) {
			break;
		}
		// 避免CPU轮询
		syscall_yield();
	}
	return flag;
}

/* Overview:
 *  read data from IDE disk. First issue a read request through
 *  disk register and then copy data from disk buffer
 *  (512 bytes, a sector) to destination array.
 *
 * Parameters:
 *  diskno: disk number.
 *  secno: start sector number.
 *  dst: destination for data read from IDE disk.
 *  nsecs: the number of sectors to read.
 *
 * Post-Condition:
 *  Panic if any error occurs. (you may want to use 'panic_on')
 *
 * Hint: Use syscalls to access device registers and buffers.
 * Hint: Use the physical address and offsets defined in 'include/malta.h'.
 */
// 实现用户态下对磁盘的读操作
void ide_read(u_int diskno,   // 磁盘号
			  u_int secno, 	  // 扇区号
			  void *dst, 	  // 读取的地址
			  u_int nsecs	  // 读取n个扇区
			) {
	uint8_t temp;
	u_int offset = 0, max = nsecs + secno;
	panic_on(diskno >= 2);

	// Read the sector in turn
	// 依次读取n个扇区
	while (secno < max) {
		// 等待IDE设备就绪
		temp = wait_ide_ready();
		// Step 1: Write the number of operating sectors to NSECT register
		// 设置操作扇区数目
		temp = 1;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_NSECT, 1));

		// Step 2: Write the 7:0 bits of sector number to LBAL register
		// 设置操作扇区号的[7:0]位
		temp = secno & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAL, 1));

		// Step 3: Write the 15:8 bits of sector number to LBAM register
		/* Exercise 5.3: Your code here. (1/9) */
		// 设置操作扇区号的[15:8]位
		temp = (secno >> 8) & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAM, 1));

		// Step 4: Write the 23:16 bits of sector number to LBAH register
		/* Exercise 5.3: Your code here. (2/9) */
		// 设置操作扇区号的[23:16]位
		temp = (secno >> 16) & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAH, 1));

		// Step 5: Write the 27:24 bits of sector number, addressing mode
		// and diskno to DEVICE register
		// 设置操作扇区号的[27:24]位，并设置磁盘号和扇区寻址模式
		temp = ((secno >> 24) & 0x0f) | MALTA_IDE_LBA | (diskno << 4);
		panic_on(syscall_write_dev(&temp, MALTA_IDE_DEVICE, 1));

		// Step 6: Write the working mode to STATUS register
		// 配置设备工作状态
		temp = MALTA_IDE_CMD_PIO_READ;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_STATUS, 1));

		// Step 7: Wait until the IDE is ready
		// 等待IDE设备就绪
		temp = wait_ide_ready();

		// Step 8: Read the data from device
		// 循环读取完成整个扇区的数据，每次仅能读取4个字节
		for (int i = 0; i < SECT_SIZE / 4; i++) {
			panic_on(syscall_read_dev(dst + offset + i * 4, MALTA_IDE_DATA, 4));
		}

		// Step 9: Check IDE status
		// 检查IDE设备状态
		panic_on(syscall_read_dev(&temp, MALTA_IDE_STATUS, 1));

		// SECT_SIZE是512字节，表示一个扇区的大小，意味着进入下一个扇区的读取
		offset += SECT_SIZE;
		secno += 1;
	}
}

/* Overview:
 *  write data to IDE disk.
 *
 * Parameters:
 *  diskno: disk number.
 *  secno: start sector number.
 *  src: the source data to write into IDE disk.
 *  nsecs: the number of sectors to write.
 *
 * Post-Condition:
 *  Panic if any error occurs.
 *
 * Hint: Use syscalls to access device registers and buffers.
 * Hint: Use the physical address and offsets defined in 'include/malta.h'.
 */
// 实现用户态下对磁盘的写操作
// 整体流程与ide_read相似，不再赘述
void ide_write(u_int diskno, u_int secno, void *src, u_int nsecs) {
	uint8_t temp;
	u_int offset = 0, max = nsecs + secno;
	panic_on(diskno >= 2);

	// Write the sector in turn
	while (secno < max) {
		temp = wait_ide_ready();
		// Step 1: Write the number of operating sectors to NSECT register
		/* Exercise 5.3: Your code here. (3/9) */
		temp = 1;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_NSECT, 1));

		// Step 2: Write the 7:0 bits of sector number to LBAL register
		/* Exercise 5.3: Your code here. (4/9) */
		temp = secno & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAL, 1));

		// Step 3: Write the 15:8 bits of sector number to LBAM register
		/* Exercise 5.3: Your code here. (5/9) */
		temp = (secno >> 8) & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAM, 1));

		// Step 4: Write the 23:16 bits of sector number to LBAH register
		/* Exercise 5.3: Your code here. (6/9) */
		temp = (secno >> 16) & 0xff;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_LBAH, 1));

		// Step 5: Write the 27:24 bits of sector number, addressing mode
		// and diskno to DEVICE register
		/* Exercise 5.3: Your code here. (7/9) */
		temp = ((secno >> 24) & 0x0f) | MALTA_IDE_LBA | (diskno << 4);
		panic_on(syscall_write_dev(&temp, MALTA_IDE_DEVICE, 1));

		// Step 6: Write the working mode to STATUS register
		/* Exercise 5.3: Your code here. (8/9) */
		temp = MALTA_IDE_CMD_PIO_WRITE;
		panic_on(syscall_write_dev(&temp, MALTA_IDE_STATUS, 1));

		// Step 7: Wait until the IDE is ready
		temp = wait_ide_ready();

		// Step 8: Write the data to device
		for (int i = 0; i < SECT_SIZE / 4; i++) {
			/* Exercise 5.3: Your code here. (9/9) */
			panic_on(syscall_write_dev(src + offset + i * 4, MALTA_IDE_DATA, 4));
		}

		// Step 9: Check IDE status
		panic_on(syscall_read_dev(&temp, MALTA_IDE_STATUS, 1));

		offset += SECT_SIZE;
		secno += 1;
	}
}
