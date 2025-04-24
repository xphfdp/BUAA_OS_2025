#include <asm/asm.h>
#include <mmu.h>
#include <trap.h>
#include <kclock.h>

.text
LEAF(env_pop_tf)
.set reorder
.set at
	mtc0    a1, CP0_ENTRYHI		# 设置EntryHi的ASID域
	move    sp, a0				# 设置sp寄存器，配合RESTORE_ALL宏
	RESET_KCLOCK				# 重制时钟中断
	j       ret_from_exception  # 异常处理完毕，返回执行进程（包括回复进程上下文和eret）
END(env_pop_tf)
