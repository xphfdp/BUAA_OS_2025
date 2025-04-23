#include <asm/asm.h>
#include <stackframe.h>

/*异常分发程序，用于检测发生了哪种异常*/

/*处理TLB miss异常*/
.section .text.tlb_miss_entry 
tlb_miss_entry:
	j       exc_gen_entry

/*处理其他异常*/
.section .text.exc_gen_entry
exc_gen_entry:
	/*将当前上下文保存到内核的异常栈中*/
	SAVE_ALL
	/*
	* Note: When EXL is set or UM is unset, the processor is in kernel mode.
	* When EXL is set, the value of EPC is not updated when a new exception occurs.
	* To keep the processor in kernel mode and enable exception reentrancy,
	* we unset UM and EXL, and unset IE to globally disable interrupts.
	*/
	/*清除Status寄存器中的UM、EXL、IE位，以保持处理器处于内核态(UM == 0)、关闭中断且允许嵌套异常*/
	mfc0    t0, CP0_STATUS
	and     t0, t0, ~(STATUS_UM | STATUS_EXL | STATUS_IE)
	mtc0    t0, CP0_STATUS
/* Exercise 3.9: Your code here. */
	/*取得Cause寄存器中的2~6位，也就是对应的异常码，这是区别不同异常的重要标志*/
	/*以得到的异常码作为索引在exception_handlers数组中找到对应的中断处理函数*/
	/*跳转到对应的中断处理函数中，从而响应了异常，并将异常交给了对应的异常处理函数去处理*/
	mfc0	t0, CP0_CAUSE
	andi	t0, 0x7c
	lw		t0, exception_handlers(t0)
	jr		t0