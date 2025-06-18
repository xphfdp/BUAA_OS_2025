#ifndef _ENV_H_
#define _ENV_H_

#include <mmu.h>
#include <queue.h>
#include <trap.h>
#include <types.h>

#define LOG2NENV 10
#define NENV (1 << LOG2NENV) //进程的最大数量(1024)
#define ENVX(envid) ((envid) & (NENV - 1)) // 根据envid获取envs下标

// All possible values of 'env_status' in 'struct Env'，表示进程的状态.
#define ENV_FREE 0
#define ENV_RUNNABLE 1
#define ENV_NOT_RUNNABLE 2

// Control block of an environment (process).
struct Env {
	struct Trapframe env_tf;	 // saved context (registers) before switching
								/*当发生进程调度，或陷入内核时，会将当时的进程上下文环境保存在env_tf变量中*/
								// 上下文环境指的是各种寄存器的值，比如CP0中的status寄存器
	LIST_ENTRY(Env) env_link;	 // intrusive entry in 'env_free_list'
								/*类似于pp_link，用来构造空闲进程链表env_free_list*/
	u_int env_id;			 // unique environment identifier，进程的id，独一无二
	u_int env_asid;			 // ASID of this env，表示进程的ASID，用于TLB中，是进程虚拟地址空间的标识
	u_int env_parent_id;	 // env_id of this env's parent，记录父进程的进程id，由此关联可形成一棵进程树
	u_int env_status;		 // status of this env
							/*表示当前进程的状态*/
							// ENV_FREE:表示该PCB处于空闲状态，没有被任何进程使用，即该进程控制块处于进程空闲链表中
							// ENV_NOT_RUNNABLE:表明进程处于阻塞状态，处于该状态的进程需要在一定条件下变成就绪状态从而被CPU调度
							// ENV_RUNNABLE:该进程处于执行状态或就绪状态，即其可能是正在运行的，也可能正在等待被调度（就绪状态或者运行状态）
	Pde *env_pgdir;			 // page directory，保存该进程页目录的内核虚拟地址
	TAILQ_ENTRY(Env) env_sched_link; // intrusive entry in 'env_sched_list'，用来构造调度队列env_sched_list
	u_int env_pri;			 // schedule priority，表示该进程的优先级，在MOS中表示该进程运行的时间片长度

	// Lab 4 IPC
	u_int env_ipc_value;   // the value sent to us，发送传递的具体数值
	u_int env_ipc_from;    // envid of the sender，发送方的进程id
	u_int env_ipc_recving; // whether this env is blocked receiving，该进程是否可以接受数据，为1则等待接收数据中，反之不可接受数据
	u_int env_ipc_dstva;   // va at which the received page should be mapped，接收到的页面需要与自身的哪个虚拟页面完成映射
	u_int env_ipc_perm;    // perm in which the received page should be mapped，传递的页面的权限位设置

	// Lab 4 fault handling
	u_int env_user_tlb_mod_entry; // userspace TLB Mod handler，处理写入PTE_D无效的页面时的异常

	// Lab 6 scheduler counts
	u_int env_runs; // number of times we've been env_run'ed
};

LIST_HEAD(Env_list, Env);
TAILQ_HEAD(Env_sched_list, Env);
extern struct Env *curenv;		     // the current env
extern struct Env_sched_list env_sched_list; // runnable env list

void env_init(void);
int env_alloc(struct Env **e, u_int parent_id);
void env_free(struct Env *);
struct Env *env_create(const void *binary, size_t size, int priority);
void env_destroy(struct Env *e);

int envid2env(u_int envid, struct Env **penv, int checkperm);
void env_run(struct Env *e) __attribute__((noreturn));

void env_check(void);
void envid2env_check(void);

// 创建优先级为y的进程
#define ENV_CREATE_PRIORITY(x, y)                                                                  \
	({                                                                                         \
		extern u_char binary_##x##_start[];                                                \
		extern u_int binary_##x##_size;                                                    \
		env_create(binary_##x##_start, (u_int)binary_##x##_size, y);                       \
	})

// 创建优先级为1的进程
#define ENV_CREATE(x)                                                                              \
	({                                                                                         \
		extern u_char binary_##x##_start[];                                                \
		extern u_int binary_##x##_size;                                                    \
		env_create(binary_##x##_start, (u_int)binary_##x##_size, 1);                       \
	})

#endif // !_ENV_H_
