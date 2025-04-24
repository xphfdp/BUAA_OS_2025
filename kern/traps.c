#include <env.h>
#include <pmap.h>
#include <printk.h>
#include <trap.h>

extern void handle_int(void);
extern void handle_tlb(void);
extern void handle_sys(void);
extern void handle_mod(void);
extern void handle_reserved(void);
extern void handle_adel(void);
extern void handle_ades(void);

// 异常向量组
/*
* 0号异常的处理函数为handle_int，表示中断，由时钟中断、控制台中断等中断造成
* 1号异常的处理函数为handle_mod，表示存储异常，进行存储操作时该页被标记为只读 
* 2号异常的处理函数为handle_tlb，表示TLB load异常
* 3号异常的处理函数为handle_tlb，表示TLB store异常
* 8号异常的处理函数为handle_sys，表示系统调用，用户进程通过执行syscall指令陷入内核
*/
void (*exception_handlers[32])(void) = {
    [0 ... 31] = handle_reserved,
    [0] = handle_int,
    [2 ... 3] = handle_tlb,
    [4] = handle_adel,
    [5] = handle_ades,
#if !defined(LAB) || LAB >= 4
    [1] = handle_mod,
    [8] = handle_sys,
#endif
};

/* Overview:
 *   The fallback handler when an unknown exception code is encountered.
 *   'genex.S' wraps this function in 'handle_reserved'.
 */
void do_reserved(struct Trapframe *tf) {
	print_tf(tf);
	panic("Unknown ExcCode %2d", (tf->cp0_cause >> 2) & 0x1f);
}

void do_adel(struct Trapframe *tf) {
	unsigned long tf_regs[32] = tf->regs[32];
	tf_regs[0] = tf_regs[0] & 0;
	tf_regs[0] = tf_regs[1] & 0;
	printk("AdEL handled, new imm is : %04x\n", tf_regs[32] & 0xffff);
}

void do_ades(struct Trapframe *tf) {
	unsigned long tf_regs[32] = tf->regs[32];
	tf_regs[0] = tf_regs[0] & 0;
	tf_regs[1] = tf_regs[1] & 0;
	printk("AdES handled, new imm is : %04x\n", tf_regs[32] & 0xffff);
}
