#include <elf.h>
#include <env.h>
#include <lib.h>
#include <mmu.h>
#include <variable.h>

#define debug 0

int init_stack(u_int child, char **argv, u_int *init_sp) {
	int argc, i, r, tot;
	char *strings;
	u_int *args;

	// Count the number of arguments (argc)
	// and the total amount of space needed for strings (tot)
	tot = 0;
	for (argc = 0; argv[argc]; argc++) {
		tot += strlen(argv[argc]) + 1;
	}

	// Make sure everything will fit in the initial stack page
	if (ROUND(tot, 4) + 4 * (argc + 3) > PAGE_SIZE) {
		return -E_NO_MEM;
	}

	// Determine where to place the strings and the args array
	strings = (char *)(UTEMP + PAGE_SIZE) - tot;
	args = (u_int *)(UTEMP + PAGE_SIZE - ROUND(tot, 4) - 4 * (argc + 1));

	if ((r = syscall_mem_alloc(0, (void *)UTEMP, PTE_D)) < 0) {
		return r;
	}

	// Copy the argument strings into the stack page at 'strings'
	char *ctemp, *argv_temp;
	u_int j;
	ctemp = strings;
	for (i = 0; i < argc; i++) {
		argv_temp = argv[i];
		for (j = 0; j < strlen(argv[i]); j++) {
			*ctemp = *argv_temp;
			ctemp++;
			argv_temp++;
		}
		*ctemp = 0;
		ctemp++;
	}

	// Initialize args[0..argc-1] to be pointers to these strings
	// that will be valid addresses for the child environment
	// (for whom this page will be at USTACKTOP-PAGE_SIZE!).
	ctemp = (char *)(USTACKTOP - UTEMP - PAGE_SIZE + (u_int)strings);
	for (i = 0; i < argc; i++) {
		args[i] = (u_int)ctemp;
		ctemp += strlen(argv[i]) + 1;
	}

	// Set args[argc] to 0 to null-terminate the args array.
	ctemp--;
	args[argc] = (u_int)ctemp;

	// Push two more words onto the child's stack below 'args',
	// containing the argc and argv parameters to be passed
	// to the child's main() function.
	u_int *pargv_ptr;
	pargv_ptr = args - 1;
	*pargv_ptr = USTACKTOP - UTEMP - PAGE_SIZE + (u_int)args;
	pargv_ptr--;
	*pargv_ptr = argc;

	// Set *init_sp to the initial stack pointer for the child
	*init_sp = USTACKTOP - UTEMP - PAGE_SIZE + (u_int)pargv_ptr;

	if ((r = syscall_mem_map(0, (void *)UTEMP, child, (void *)(USTACKTOP - PAGE_SIZE), PTE_D)) <
	    0) {
		goto error;
	}
	if ((r = syscall_mem_unmap(0, (void *)UTEMP)) < 0) {
		goto error;
	}

	return 0;

error:
	syscall_mem_unmap(0, (void *)UTEMP);
	return r;
}

static int spawn_mapper(void *data, u_long va, size_t offset, u_int perm, const void *src,
			size_t len) {
	u_int child_id = *(u_int *)data;
	try(syscall_mem_alloc(child_id, (void *)va, perm));
	if (src != NULL) {
		int r = syscall_mem_map(child_id, (void *)va, 0, (void *)UTEMP, perm | PTE_D);
		if (r) {
			syscall_mem_unmap(child_id, (void *)va);
			return r;
		}
		memcpy((void *)(UTEMP + offset), src, len);
		return syscall_mem_unmap(0, (void *)UTEMP);
	}
	return 0;
}

/* Note:
 *   This function involves loading executable code to memory. After the completion of load
 *   procedures, D-cache and I-cache writeback/invalidation MUST be performed to maintain cache
 *   coherence, which MOS has NOT implemented. This may result in unexpected behaviours on real
 *   CPUs! QEMU doesn't simulate caching, allowing the OS to function correctly.
 */
// 根据磁盘文件创建了一个进程
int spawn(char *file_path, char **argv) {
	// 打开磁盘路径对应的文件
	// 如果打开失败则返回错误
	int fd;
	int isShell = 0;
	struct Stat st;
	if ((fd = open(file_path, O_RDONLY)) < 0) {
		int len = strlen(file_path);
		if (len < 2 || (len < MAXPATHLEN - 2 && (file_path[len - 1] != 'b' || file_path[len - 2] != '.'))) {
			char tmp_path[MAXPATHLEN];
			strcpy(tmp_path, file_path);
			tmp_path[len] = '.';
			tmp_path[len+1] = 'b';
			tmp_path[len+2] = '\0';
			if ((fd = open(tmp_path, O_RDONLY)) < 0) {
				return fd;
			}
		} else {
			return fd;
		}
	}

	panic_on(fstat(fd, &st));
	if (strcmp(st.st_name, "sh.b") == 0) {
		isShell = 1;
	}

	// Step 2: Read the ELF header (of type 'Elf32_Ehdr') from the file into 'elfbuf' using
	// 'readn()'.
	// If that fails (where 'readn' returns a different size than expected),
	// set 'r' and 'goto err' to close the file and return the error.
	int r;
	u_char elfbuf[512];
	// 读入文件内容到elfbuf中
	if ((r = readn(fd, elfbuf, sizeof(Elf32_Ehdr))) != sizeof(Elf32_Ehdr)) {
		goto err;
	}
	// 将文件头转换为Elf32_Ehdr结构体的格式
	const Elf32_Ehdr *ehdr = elf_from(elfbuf, sizeof(Elf32_Ehdr));
	if (!ehdr) {
		r = -E_NOT_EXEC;
		goto err;
	}
	// 读取程序入口信息
	u_long entrypoint = ehdr->e_entry;

	// Step 3: Create a child using 'syscall_exofork()' and store its envid in 'child'.
	// If the syscall fails, set 'r' and 'goto err'.
	// 使用系统调用创建了一个子进程
	// 不使用fork是因为会替换子进程的代码和数据，不会再从此处继续执行
	// 如果系统调用失败则返回错误
	u_int child;
	child = syscall_exofork();
	if (child < 0)
	{
		r = child;
		goto err;
	}

	// Step 4: Use 'init_stack(child, argv, &sp)' to initialize the stack of the child.
	// 'goto err1' if that fails.
	// 初始化子进程的栈空间
	u_int sp;
	if ((r = init_stack(child, argv, &sp)) < 0)
	{
		goto err1;
	}

	// Step 5: Load the ELF segments in the file into the child's memory.
	// This is similar to 'load_icode()' in the kernel.
	// 遍历整个ELF头的程序段，将程序段的内容读到内存中
	size_t ph_off;
	ELF_FOREACH_PHDR_OFF (ph_off, ehdr) {
		// Read the program header in the file with offset 'ph_off' and length
		// 'ehdr->e_phentsize' into 'elfbuf'.
		// 'goto err1' on failure.
		// You may want to use 'seek' and 'readn'.
		// 设置文件描述符相应的偏移量并读取文件的内容
		if ((r = seek(fd, ph_off)) < 0) {
			goto err1;
		}
		if ((r = readn(fd, elfbuf, ehdr->e_phentsize)) != ehdr->e_phentsize) {
			goto err1;
		}

		Elf32_Phdr *ph = (Elf32_Phdr *)elfbuf;
		// 如果是需要加载的程序段
		if (ph->p_type == PT_LOAD) {
			void *bin;
			// Read and map the ELF data in the file at 'ph->p_offset' into our memory
			// using 'read_map()'.
			// 'goto err1' if that fails.
			// 先根据程序段相对于文件的偏移得到其在内存中映射到的地址
			r = read_map(fd, ph->p_offset, &bin);
			if (r != 0) {
				goto err1;
			}

			// Load the segment 'ph' into the child's memory using 'elf_load_seg()'.
			// Use 'spawn_mapper' as the callback, and '&child' as its data.
			// 'goto err1' if that fails.
			// 调用elf_load_seg将程序段加载到适当的位置
			r = elf_load_seg(ph, bin, spawn_mapper, &child);
			if (r != 0) {
				goto err1;
			}
		}
	}
	// 关闭文件
	close(fd);

	// 设置栈帧
	// 父子进程共享USTACKTOP地址之下的数据，但不共享程序部分
	struct Trapframe tf = envs[ENVX(child)].env_tf;
	tf.cp0_epc = entrypoint;
	tf.regs[29] = sp;
	if ((r = syscall_set_trapframe(child, &tf)) != 0) {
		goto err2;
	}

	// Pages with 'PTE_LIBRARY' set are shared between the parent and the child.
	// 设置父子进程共享页面
	for (u_int pdeno = 0; pdeno <= PDX(USTACKTOP); pdeno++) {
		if (!(vpd[pdeno] & PTE_V)) {
			continue;
		}
		for (u_int pteno = 0; pteno <= PTX(~0); pteno++) {
			u_int pn = (pdeno << 10) + pteno;
			u_int perm = vpt[pn] & ((1 << PGSHIFT) - 1);
			if ((perm & PTE_V) && (perm & PTE_LIBRARY)) {
				void *va = (void *)(pn << PGSHIFT);
				if ((r = syscall_mem_map(0, va, child, va, perm)) < 0) {
					debugf("spawn: syscall_mem_map %x %x: %d\n", va, child, r);
					goto err2;
				}
			}
		}
	}

	if (isShell && env->variable_set) {
		if((r = syscall_mem_alloc(0, (void *)UTEMP, PTE_D)) < 0) {
			debugf("spawn: syscall_mem_alloc %x: %d\n", child, r);
			goto err2;
		}
		struct VariableSet *vset = (struct VariableSet *)UTEMP;
		vset->exportIdx = 0;  // Reset export index
		copy_vars(vset, env->variable_set);
		// memcpy(vset, env->variable_set, sizeof(struct VariableSet));
		if((r = syscall_mem_map(0, (void *)UTEMP, child, (void *)UTEMP, PTE_D)) < 0) {
			debugf("spawn: syscall_mem_map %x: %d\n", child, r);
			goto err2;
		}
		if((r = syscall_mem_unmap(0, (void *)UTEMP)) < 0) {
			debugf("spawn: syscall_set_variable_set %x: %d\n", child, r);
			goto err2;
		}
		DEBUGF("spawn: copy_vars to child %d\n", child);
	}

	// 设定子进程为运行状态以将其加入进程调度队列，实现子进程的创建
	if ((r = syscall_set_env_status(child, ENV_RUNNABLE)) < 0) {
		debugf("spawn: syscall_set_env_status %x: %d\n", child, r);
		goto err2;
	}
	return child;

// 异常处理程序
// 销毁创建的子进程
err2:
	syscall_env_destroy(child);
	return r;
err1:
	syscall_env_destroy(child);
// 关闭打开的文件
err:
	close(fd);
	return r;
}

// 将磁盘中的文件加载到内存，并以此创建一个新进程
int spawnl(char *file_path, char *args, ...) {
	// Thanks to MIPS calling convention, the layout of arguments on the stack
	// are straightforward.
	// 由于mips的传参机制，可以直接这样传参
	return spawn(file_path, &args);
}
