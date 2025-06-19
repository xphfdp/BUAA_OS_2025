#include <env.h>
#include <io.h>
#include <mmu.h>
#include <pmap.h>
#include <printk.h>
#include <sched.h>
#include <syscall.h>
#include "../user/include/variable.h"

extern struct Env *curenv;
void simplify_path(char *);

/* Overview:
 * 	This function is used to print a character on screen.
 *
 * Pre-Condition:
 * 	`c` is the character you want to print.
 */
void sys_putchar(int c) {
	printcharc((char)c);
	return;
}

/* Overview:
 * 	This function is used to print a string of bytes on screen.
 *
 * Pre-Condition:
 * 	`s` is base address of the string, and `num` is length of the string.
 */
int sys_print_cons(const void *s, u_int num) {
	if (((u_int)s + num) > UTOP || ((u_int)s) >= UTOP || (s > s + num)) {
		return -E_INVAL;
	}
	u_int i;
	for (i = 0; i < num; i++) {
		printcharc(((char *)s)[i]);
	}
	return 0;
}

/* Overview:
 *	This function provides the environment id of current process.
 *
 * Post-Condition:
 * 	return the current environment id
 */
u_int sys_getenvid(void) {
	return curenv->env_id;
}

/* Overview:
 *   Give up remaining CPU time slice for 'curenv'.
 *
 * Post-Condition:
 *   Another env is scheduled.
 *
 * Hint:
 *   This function will never return.
 */
// void sys_yield(void);
void __attribute__((noreturn)) sys_yield(void) {
	// Hint: Just use 'schedule' with 'yield' set.
	/* Exercise 4.7: Your code here. */
	/*yield = 1则表明要放弃当前进程从而调度其他进程*/
	schedule(1);
}

/* Overview:
 * 	This function is used to destroy the current environment.
 *
 * Pre-Condition:
 * 	The parameter `envid` must be the environment id of a
 * process, which is either a child of the caller of this function
 * or the caller itself.
 *
 * Post-Condition:
 *  Returns 0 on success.
 *  Returns the original error if underlying calls fail.
 */
int sys_env_destroy(u_int envid) {
	struct Env *e;
	try(envid2env(envid, &e, 1));

	// printk("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

/* Overview:
 *   Register the entry of user space TLB Mod handler of 'envid'.
 *
 * Post-Condition:
 *   The 'envid''s TLB Mod exception handler entry will be set to 'func'.
 *   Returns 0 on success.
 *   Returns the original error if underlying calls fail.
 */
int sys_set_tlb_mod_entry(u_int envid, u_int func) {
	struct Env *env;

	/* Step 1: Convert the envid to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.12: Your code here. (1/2) */
	try(envid2env(envid, &env, 1));

	/* Step 2: Set its 'env_user_tlb_mod_entry' to 'func'. */
	/* Exercise 4.12: Your code here. (2/2) */
	env->env_user_tlb_mod_entry = func;
	return 0;
}

/* Overview:
 *   Check 'va' is illegal or not, according to include/mmu.h
 */
// 判断是否为正常用户空间虚拟地址，非法则为真
// inline表示这是内联函数，不会修改栈帧
// 内联函数指的是这个函数不会被编译为一个函数，而是直接内联展开在调用者函数内
static inline int is_illegal_va(u_long va) {
	return va < UTEMP || va >= UTOP;
}

static inline int is_illegal_va_range(u_long va, u_int len) {
	if (len == 0) {
		return 0;
	}
	return va + len < va || va < UTEMP || va + len > UTOP;
}

/* Overview:
 *   Allocate a physical page and map 'va' to it with 'perm' in the address space of 'envid'.
 *   If 'va' is already mapped, that original page is sliently unmapped.
 *   'envid2env' should be used with 'checkperm' set, like in most syscalls, to ensure the target is
 * either the caller or its child.
 *
 * Post-Condition:
 *   Return 0 on success.
 *   Return -E_BAD_ENV: 'checkperm' of 'envid2env' fails for 'envid'.
 *   Return -E_INVAL:   'va' is illegal (should be checked using 'is_illegal_va').
 *   Return the original error: underlying calls fail (you can use 'try' macro).
 *
 * Hint:
 *   You may want to use the following functions:
 *   'envid2env', 'page_alloc', 'page_insert', 'try' (macro)
 */
int sys_mem_alloc(u_int envid, u_int va, u_int perm) {
	struct Env *env;
	struct Page *pp;

	/* Step 1: Check if 'va' is a legal user virtual address using 'is_illegal_va'. */
	/* Exercise 4.4: Your code here. (1/3) */
	if (is_illegal_va(va)) {
		return -E_INVAL;
	}

	/* Step 2: Convert the envid to its corresponding 'struct Env *' using 'envid2env'. */
	/* Hint: **Always** validate the permission in syscalls! */
	/* Exercise 4.4: Your code here. (2/3) */
	if (envid2env(envid, &env, 1) != 0) {
		return -E_BAD_ENV;
	}

	/* Step 3: Allocate a physical page using 'page_alloc'. */
	/* Exercise 4.4: Your code here. (3/3) */
	try(page_alloc(&pp));

	/* Step 4: Map the allocated page at 'va' with permission 'perm' using 'page_insert'. */
	return page_insert(env->env_pgdir, env->env_asid, pp, va, perm);
}

/* Overview:
 *   Find the physical page mapped at 'srcva' in the address space of env 'srcid', and map 'dstid''s
 *   'dstva' to it with 'perm'.
 *
 * Post-Condition:
 *   Return 0 on success.
 *   Return -E_BAD_ENV: 'checkperm' of 'envid2env' fails for 'srcid' or 'dstid'.
 *   Return -E_INVAL: 'srcva' or 'dstva' is illegal, or 'srcva' is unmapped in 'srcid'.
 *   Return the original error: underlying calls fail.
 *
 * Hint:
 *   You may want to use the following functions:
 *   'envid2env', 'page_lookup', 'page_insert'
 */
int sys_mem_map(u_int srcid, u_int srcva, u_int dstid, u_int dstva, u_int perm) {
	struct Env *srcenv;
	struct Env *dstenv;
	struct Page *pp;

	/* Step 1: Check if 'srcva' and 'dstva' are legal user virtual addresses using
	 * 'is_illegal_va'. */
	/* Exercise 4.5: Your code here. (1/4) */
	if (is_illegal_va(srcva) || is_illegal_va(dstva)) {
		return -E_INVAL;
	}

	/* Step 2: Convert the 'srcid' to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.5: Your code here. (2/4) */
	if (envid2env(srcid, &srcenv, 1) != 0) {
		return -E_BAD_ENV;
	}

	/* Step 3: Convert the 'dstid' to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.5: Your code here. (3/4) */
	if (envid2env(dstid, &dstenv, 1) != 0) {
		return -E_BAD_ENV;
	}

	/* Step 4: Find the physical page mapped at 'srcva' in the address space of 'srcid'. */
	/* Return -E_INVAL if 'srcva' is not mapped. */
	/* Exercise 4.5: Your code here. (4/4) */
	pp = page_lookup(srcenv->env_pgdir, srcva, 0);
	if (pp == NULL) {
		return -E_INVAL;
	}

	/* Step 5: Map the physical page at 'dstva' in the address space of 'dstid'. */
	return page_insert(dstenv->env_pgdir, dstenv->env_asid, pp, dstva, perm);
}

/* Overview:
 *   Unmap the physical page mapped at 'va' in the address space of 'envid'.
 *   If no physical page is mapped there, this function silently succeeds.
 *
 * Post-Condition:
 *   Return 0 on success.
 *   Return -E_BAD_ENV: 'checkperm' of 'envid2env' fails for 'envid'.
 *   Return -E_INVAL:   'va' is illegal.
 *   Return the original error when underlying calls fail.
 */
int sys_mem_unmap(u_int envid, u_int va) {
	struct Env *e;

	/* Step 1: Check if 'va' is a legal user virtual address using 'is_illegal_va'. */
	/* Exercise 4.6: Your code here. (1/2) */
	if (is_illegal_va(va)) {
		return -E_INVAL;
	}

	/* Step 2: Convert the envid to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.6: Your code here. (2/2) */
	if (envid2env(envid, &e, 1) != 0) {
		return -E_BAD_ENV;
	}

	/* Step 3: Unmap the physical page at 'va' in the address space of 'envid'. */
	page_remove(e->env_pgdir, e->env_asid, va);
	return 0;
}

/* Overview:
 *   Allocate a new env as a child of 'curenv'.
 *
 * Post-Condition:
 *   Returns the child's envid on success, and
 *   - The new env's 'env_tf' is copied from the kernel stack, except for $v0 set to 0 to indicate
 *     the return value in child.
 *   - The new env's 'env_status' is set to 'ENV_NOT_RUNNABLE'.
 *   - The new env's 'env_pri' is copied from 'curenv'.
 *   Returns the original error if underlying calls fail.
 *
 * Hint:
 *   This syscall works as an essential step in user-space 'fork' and 'spawn'.
 */
int sys_exofork(void) {
	struct Env *e;

	/* Step 1: Allocate a new env using 'env_alloc'. */
	/* Exercise 4.9: Your code here. (1/4) */
	try(env_alloc(&e, curenv->env_id));

	/* Step 2: Copy the current Trapframe below 'KSTACKTOP' to the new env's 'env_tf'. */
	/* Exercise 4.9: Your code here. (2/4) */
	e->env_tf = *((struct Trapframe *)KSTACKTOP - 1);

	/* Step 3: Set the new env's 'env_tf.regs[2]' to 0 to indicate the return value in child. */
	/* Exercise 4.9: Your code here. (3/4) */
	// 将子进程的返回值设为0
	e->env_tf.regs[2] = 0;

	/* Step 4: Set up the new env's 'env_status' and 'env_pri'.  */
	/* Exercise 4.9: Your code here. (4/4) */
	e->env_status = ENV_NOT_RUNNABLE;
	e->env_pri = curenv->env_pri;
	e->cwd = curenv->cwd;
	strcpy(e->r_path, curenv->r_path);
	return e->env_id; // 父进程的返回值为子进程的envid
}

/* Overview:
 *   Set 'envid''s 'env_status' to 'status' and update 'env_sched_list'.
 *
 * Post-Condition:
 *   Returns 0 on success.
 *   Returns -E_INVAL if 'status' is neither 'ENV_RUNNABLE' nor 'ENV_NOT_RUNNABLE'.
 *   Returns the original error if underlying calls fail.
 *
 * Hint:
 *   The invariant that 'env_sched_list' contains and only contains all runnable envs should be
 *   maintained.
 */
int sys_set_env_status(u_int envid, u_int status) {
	struct Env *env;

	/* Step 1: Check if 'status' is valid. */
	/* Exercise 4.14: Your code here. (1/3) */
	if (status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE && status != ENV_FREE) {
		return -E_INVAL;
	}

	/* Step 2: Convert the envid to its corresponding 'struct Env *' using 'envid2env'. */
	/* Exercise 4.14: Your code here. (2/3) */
	try(envid2env(envid, &env, 1));

	/* Step 3: Update 'env_sched_list' if the 'env_status' of 'env' is being changed. */
	/* Exercise 4.14: Your code here. (3/3) */
	if (env->env_status != status) {
		if (status == ENV_RUNNABLE) {
			TAILQ_INSERT_TAIL(&env_sched_list, env, env_sched_link);
		} else if (status == ENV_NOT_RUNNABLE) {
			TAILQ_REMOVE(&env_sched_list, env, env_sched_link);
		} else {
			LIST_REMOVE(env, env_dying_link);
			LIST_INSERT_HEAD(&env_free_list, env, env_link);
			DEBUGF("[%d] env %d is free\n", curenv ? curenv->env_id : 0, env->env_id);
		}
	}

	/* Step 4: Set the 'env_status' of 'env'. */
	env->env_status = status;

	/* Step 5: Use 'schedule' with 'yield' set if ths 'env' is 'curenv'. */
	if (env == curenv) {
		schedule(1);
	}
	return 0;
}

/* Overview:
 *  Set envid's trap frame to 'tf'.
 *
 * Post-Condition:
 *  The target env's context is set to 'tf'.
 *  Returns 0 on success (except when the 'envid' is the current env, so no value could be
 * returned).
 *  Returns -E_INVAL if the environment cannot be manipulated or 'tf' is invalid.
 *  Returns the original error if other underlying calls fail.
 */
int sys_set_trapframe(u_int envid, struct Trapframe *tf) {
	if (is_illegal_va_range((u_long)tf, sizeof *tf)) {
		return -E_INVAL;
	}
	struct Env *env;
	try(envid2env(envid, &env, 1));
	if (env == curenv) {
		*((struct Trapframe *)KSTACKTOP - 1) = *tf;
		// return `tf->regs[2]` instead of 0, because return value overrides regs[2] on
		// current trapframe.
		return tf->regs[2];
	} else {
		env->env_tf = *tf;
		return 0;
	}
}

/* Overview:
 * 	Kernel panic with message `msg`.
 *
 * Post-Condition:
 * 	This function will halt the system.
 */
void sys_panic(char *msg) {
	panic("%s", TRUP(msg));
}

/* Overview:
 *   Wait for a message (a value, together with a page if 'dstva' is not 0) from other envs.
 *   'curenv' is blocked until a message is sent.
 *
 * Post-Condition:
 *   Return 0 on success.
 *   Return -E_INVAL: 'dstva' is neither 0 nor a legal address.
 */
// 用于接收消息
int sys_ipc_recv(u_int dstva) {
	/* Step 1: Check if 'dstva' is either zero or a legal address. */
	if (dstva != 0 && is_illegal_va(dstva)) {
		return -E_INVAL;
	}

	/* Step 2: Set 'curenv->env_ipc_recving' to 1. */
	/* Exercise 4.8: Your code here. (1/8) */
	// 表明该进程准备接收发送方的消息
	curenv->env_ipc_recving = 1;

	/* Step 3: Set the value of 'curenv->env_ipc_dstva'. */
	/* Exercise 4.8: Your code here. (2/8) */
	// 表明该进程要将接收到的页面与stva完成映射
	curenv->env_ipc_dstva = dstva;

	/* Step 4: Set the status of 'curenv' to 'ENV_NOT_RUNNABLE' and remove it from
	 * 'env_sched_list'. */
	/* Exercise 4.8: Your code here. (3/8) */
	// 阻塞当前进程，踢出调度队列
	curenv->env_status = ENV_NOT_RUNNABLE;
	TAILQ_REMOVE(&env_sched_list, curenv, env_sched_link);

	/* Step 5: Give up the CPU and block until a message is received. */
	((struct Trapframe *)KSTACKTOP - 1)->regs[2] = 0;
	schedule(1);
}

/* Overview:
 *   Try to send a 'value' (together with a page if 'srcva' is not 0) to the target env 'envid'.
 *
 * Post-Condition:
 *   Return 0 on success, and the target env is updated as follows:
 *   - 'env_ipc_recving' is set to 0 to block future sends.
 *   - 'env_ipc_from' is set to the sender's envid.
 *   - 'env_ipc_value' is set to the 'value'.
 *   - 'env_status' is set to 'ENV_RUNNABLE' again to recover from 'ipc_recv'.
 *   - if 'srcva' is not NULL, map 'env_ipc_dstva' to the same page mapped at 'srcva' in 'curenv'
 *     with 'perm'.
 *
 *   Return -E_IPC_NOT_RECV if the target has not been waiting for an IPC message with
 *   'sys_ipc_recv'.
 *   Return the original error when underlying calls fail.
 */
int sys_ipc_try_send(u_int envid, u_int value, u_int srcva, u_int perm) {
	struct Env *e;
	struct Page *p;

	/* Step 1: Check if 'srcva' is either zero or a legal address. */
	/* Exercise 4.8: Your code here. (4/8) */
	if (is_illegal_va(srcva) && srcva != 0) {
		return -E_INVAL;
	}

	/* Step 2: Convert 'envid' to 'struct Env *e'. */
	/* This is the only syscall where the 'envid2env' should be used with 'checkperm' UNSET,
	 * because the target env is not restricted to 'curenv''s children. */
	/* Exercise 4.8: Your code here. (5/8) */
	if (envid2env(envid, &e, 0) != 0) {
		return -E_BAD_ENV;
	}

	/* Step 3: Check if the target is waiting for a message. */
	/* Exercise 4.8: Your code here. (6/8) */
	if (e->env_ipc_recving != 1) {
		return -E_IPC_NOT_RECV;
	}

	/* Step 4: Set the target's ipc fields. */
	e->env_ipc_value = value;
	e->env_ipc_from = curenv->env_id;
	e->env_ipc_perm = PTE_V | perm;
	e->env_ipc_recving = 0;

	/* Step 5: Set the target's status to 'ENV_RUNNABLE' again and insert it to the tail of
	 * 'env_sched_list'. */
	/* Exercise 4.8: Your code here. (7/8) */
	e->env_status = ENV_RUNNABLE;
	TAILQ_INSERT_TAIL(&env_sched_list, e, env_sched_link);

	/* Step 6: If 'srcva' is not zero, map the page at 'srcva' in 'curenv' to 'e->env_ipc_dstva'
	 * in 'e'. */
	/* Return -E_INVAL if 'srcva' is not zero and not mapped in 'curenv'. */
	if (srcva != 0) {
		/* Exercise 4.8: Your code here. (8/8) */
		p = page_lookup(curenv->env_pgdir, srcva, 0);
		if (p == NULL) {
			return -E_INVAL;
		}
		try(page_insert(e->env_pgdir, e->env_asid, p, e->env_ipc_dstva, perm));
	}
	return 0;
}

// XXX: kernel does busy waiting here, blocking all envs
int sys_cgetc(void) {
	int ch;
	while ((ch = scancharc()) == 0) {
	}
	return ch;
}

/* Overview:
 *  This function is used to write data at 'va' with length 'len' to a device physical address
 *  'pa'. Remember to check the validity of 'va' and 'pa' (see Hint below);
 *
 *  'va' is the starting address of source data, 'len' is the
 *  length of data (in bytes), 'pa' is the physical address of
 *  the device (maybe with a offset).
 *
 * Pre-Condition:
 *  'len' must be 1, 2 or 4, otherwise return -E_INVAL.
 *
 * Post-Condition:
 *  Data within [va, va+len) is copied to the physical address 'pa'.
 *  Return 0 on success.
 *  Return -E_INVAL on bad address.
 *
 * Hint:
 *  You can use 'is_illegal_va_range' to validate 'va'.
 *  You may use the unmapped and uncached segment in kernel address space (KSEG1)
 *  to perform MMIO by assigning a corresponding-lengthed data to the address,
 *  or you can just simply use the io function defined in 'include/io.h',
 *  such as 'iowrite32', 'iowrite16' and 'iowrite8'.
 *
 *  All valid device and their physical address ranges:
 *	* ---------------------------------*
 *	|   device   | start addr | length |
 *	* -----------+------------+--------*
 *	|  console   | 0x180003f8 | 0x20   |
 *	|  IDE disk  | 0x180001f0 | 0x8    |
 *	* ---------------------------------*
 */
// 通过系统调用实现向设备写入数据
// va指的是用户虚拟地址，pa指的是设备的物理地址，len指的是读写的长度，单位是字节
// 从虚拟地址va处读取长度为len的数据，写入到对应的pa中
// va指的是data_addr，pa指的是device_addr
// device_addr(pa)位于kseg1，不需要经过cache，也不经过MMU映射，由硬件直接完成地址转换
int sys_write_dev(u_int va, u_int pa, u_int len)
{
	/* Exercise 5.1: Your code here. (1/2) */
	// 判断数据所在虚拟地址是否合法
	if (is_illegal_va_range(va, len))
	{
		return -E_INVAL;
	}
	// 数据长度必须为1个字节或者2个字节或者4个字节
	if (len != 1 && len != 2 && len != 4)
	{
		return -E_INVAL;
	}
	// 检查设备物理地址的合法性和有效性，不能越界
	if ((0x180003f8 <= pa && pa + len <= 0x180003f8 + 0x20) ||
		(0x180001f0 <= pa && pa + len <= 0x180001f0 + 0x8))
	{
		// 根据数据长度来向设备写入相应的字节
		if (len == 1)
		{
			iowrite8(*(u_char *)va, pa);
		}
		else if (len == 2)
		{
			iowrite16(*(u_short *)va, pa);
		}
		else if (len == 4)
		{
			iowrite32(*(u_int *)va, pa);
		}
		return 0;
	}
	return -E_INVAL;
}

/* Overview:
 *  This function is used to read data from a device physical address.
 *
 * Pre-Condition:
 *  'len' must be 1, 2 or 4, otherwise return -E_INVAL.
 *
 * Post-Condition:
 *  Data at 'pa' is copied from device to [va, va+len).
 *  Return 0 on success.
 *  Return -E_INVAL on bad address.
 *
 * Hint:
 *  You can use 'is_illegal_va_range' to validate 'va'.
 *  You can use function 'ioread32', 'ioread16' and 'ioread8' to read data from device.
 */
// 通过系统调用从设备读入数据
// va、pa和len定义同sys_write_dev函数
int sys_read_dev(u_int va, u_int pa, u_int len)
{
	/* Exercise 5.1: Your code here. (2/2) */
	// 判断数据所在地址是否合法
	if (is_illegal_va_range(va, len))
	{
		return -E_INVAL;
	}
	// 要读入的数据长度必须为1个字节或者2个字节或者4个字节
	if (len != 1 && len != 2 && len != 4)
	{
		return -E_INVAL;
	}
	// 判断设备物理地址的合法性和有效性
	if ((0x180003f8 <= pa && pa + len <= 0x180003f8 + 0x20) ||
		(0x180001f0 <= pa && pa + len <= 0x180001f0 + 0x8))
	{
		// 根据数据长度从设备读取相应的字节数
		if (len == 1)
		{
			*(u_char *)va = ioread8(pa);
		}
		else if (len == 2)
		{
			*(u_short *)va = ioread16(pa);
		}
		else if (len == 4)
		{
			*(u_int *)va = ioread32(pa);
		}
		return 0;
	}
	return -E_INVAL;
}

int syscall_set_exit_status(int status) {
	curenv->exit_status = status;
	return 0;
}

int sys_set_variable_set(void *vset) {
	if (is_illegal_va_range((u_long)vset, sizeof(struct VariableSet))) {
		return -E_INVAL;
	}
	curenv->variable_set = vset;
	return 0;
}

int set_rPath(char *cwd_name, const char *path) {
	char temp[MAXPATHLEN * 2];

	if (*path != '/') {
		int len = strlen(cwd_name);
		strcpy(temp, cwd_name);
		temp[len] = '/';
		strcpy(temp + len + 1, path);
	} else {
		strcpy(temp, path);
	}

	simplify_path(temp);
	if (strlen(temp) >= MAXPATHLEN) {
		return -E_BAD_PATH;
	}

	strcpy(cwd_name, temp);
	return 0;
}

int sys_chdir(u_int envid, struct File *f, const char *path) {
	struct Env *e;
	if (f == NULL) {
		return -E_INVAL;
	}
	if (f->f_type != FTYPE_DIR) {
		return -E_NOT_DIR;
	}
	try(envid2env(envid, &e, 0));
	try(set_rPath(e->r_path, path));
	e->cwd = f;
	return 0;
}

// 系统调用号
void *syscall_table[MAX_SYSNO] = {
    [SYS_putchar] = sys_putchar,
    [SYS_print_cons] = sys_print_cons,
    [SYS_getenvid] = sys_getenvid,
    [SYS_yield] = sys_yield,
    [SYS_env_destroy] = sys_env_destroy,
    [SYS_set_tlb_mod_entry] = sys_set_tlb_mod_entry,
    [SYS_mem_alloc] = sys_mem_alloc,
    [SYS_mem_map] = sys_mem_map,
    [SYS_mem_unmap] = sys_mem_unmap,
    [SYS_exofork] = sys_exofork,
    [SYS_set_env_status] = sys_set_env_status,
    [SYS_set_trapframe] = sys_set_trapframe,
    [SYS_panic] = sys_panic,
    [SYS_ipc_try_send] = sys_ipc_try_send,
    [SYS_ipc_recv] = sys_ipc_recv,
    [SYS_cgetc] = sys_cgetc,
    [SYS_write_dev] = sys_write_dev,
    [SYS_read_dev] = sys_read_dev,
	[SYS_chdir] = sys_chdir,
	[SYS_set_variable_set] = sys_set_variable_set,
	[SYS_set_exit_status] = syscall_set_exit_status,
};

/* Overview:
 *   Call the function in 'syscall_table' indexed at 'sysno' with arguments from user context and
 * stack.
 *
 * Hint:
 *   Use sysno from $a0 to dispatch the syscall.系统调用号存储在$a0中，用以确定特定调用函数
 *   The possible arguments are stored at $a1, $a2, $a3, [$sp + 16 bytes], [$sp + 20 bytes] in
 *   order.其余5个参数分别位于$a1,$a2,$a3,$sp+16,$sp+20中
 *   Number of arguments cannot exceed 5.除了系统调用号，被调用的函数参数个数不超过5
 */
void do_syscall(struct Trapframe *tf) {
	int (*func)(u_int, u_int, u_int, u_int, u_int);
	int sysno = tf->regs[4]; //获取系统调用号
	if (sysno < 0 || sysno >= MAX_SYSNO) {
		tf->regs[2] = -E_NO_SYS;
		return;
	}

	/* Step 1: Add the EPC in 'tf' by a word (size of an instruction). */
	/* Exercise 4.2: Your code here. (1/4) */
	tf->cp0_epc += 4;//epc指向syscall的下一条指令，防止从syscall返回后仍然执行syscall而陷入循环

	/* Step 2: Use 'sysno' to get 'func' from 'syscall_table'. */
	/* Exercise 4.2: Your code here. (2/4) */
	func = syscall_table[sysno]; // 根据系统调用号获取对应的调用函数

	/* Step 3: First 3 args are stored in $a1, $a2, $a3. */
	u_int arg1 = tf->regs[5]; 
	u_int arg2 = tf->regs[6];
	u_int arg3 = tf->regs[7];

	/* Step 4: Last 2 args are stored in stack at [$sp + 16 bytes], [$sp + 20 bytes]. */
	u_int arg4, arg5;
	/* Exercise 4.2: Your code here. (3/4) */
	arg4 = *(u_int *)(tf->regs[29] + 16);
	arg5 = *(u_int *)(tf->regs[29] + 20);
	/* Step 5: Invoke 'func' with retrieved arguments and store its return value to $v0 in 'tf'.
	 */
	/* Exercise 4.2: Your code here. (4/4) */
	tf->regs[2] = func(arg1, arg2, arg3, arg4, arg5); // 调用该系统调用函数，将返回值保存在$v0中
}

void simplify_path(char *path) {
    char *stack[1024];
    int top = 0;
    int is_absolute = (path[0] == '/');
    int dotdot_count = 0;
    char *token = path;

    // Skip leading slashes
    while(*token == '/') {
        token++;
    }

    while(*token) {
        char *start = token;

        // Find the next slash or end of string
        while(*token && *token != '/') token++;

        int length = token - start;
        if(length == 0) {
            // skip
        }else if(length == 1 && *start == '.') {
            // Do nothing for '.'
        } else if(length == 2 && start[0] == '.' && start[1] == '.') {
            if(dotdot_count < top) {
                -- top;
            }else if(!is_absolute) {
                stack[top++] = start;
                if(*token == '/') {
                    *token = '\0';
                    token++;
                }
                dotdot_count++;
            }
        } else {
            // Push valid segment onto stack
            stack[top++] = start;
            if(*token == '/') {
                *token = '\0';
                token++;
            }
        }

        // Skip slashes
        while(*token == '/') {
            token++;
        }
    }

    // Construct the simplified path
    char *result = path;
    if(is_absolute) {
        *result++ = '/';
    }
    for(int i = 0; i < top; i++) {
        while(*stack[i]) {
            *result++ = *stack[i]++;
        }
        if(i < top - 1) {
            *result++ = '/';
        }
    }
    
    *result = '\0';
}