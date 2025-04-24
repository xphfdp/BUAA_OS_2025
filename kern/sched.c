#include <env.h>
#include <pmap.h>
#include <printk.h>

/* Overview:
 *   Implement a round-robin scheduling to select a runnable env and schedule it using 'env_run'.
 *
 * Post-Condition:
 *   If 'yield' is set (non-zero), 'curenv' should not be scheduled again unless it is the only
 *   runnable env.
 *
 * Hints:
 *   1. The variable 'count' used for counting slices should be defined as 'static'.
 *   2. Use variable 'env_sched_list', which contains and only contains all runnable envs.
 *   3. You shouldn't use any 'return' statement because this function is 'noreturn'.
 */
/*
 * 最早截止期优先EDF
 * 任务的绝对截止时间越早，其优先级越高，优先级最高的任务最先被调度（动态优先级）。
 * 如果两个任务的优先级一样，当调度它们时，EDF算法将随机选择一个调度。
 * 所有任务都是周期性的（存在周期T），必须在限定的时限内完成（存在截至周期D）
 */
void schedule(int yield) {
	static int clock = -1; //当前时间片，从0开始计数
	clock++;
	struct Env *last_rr = NULL;
	struct Env *iter1 = NULL;
	struct Env *iter2 = NULL;
	struct Env *edf_result = NULL;

	LIST_FOREACH(iter1, &env_edf_sched_list, env_edf_sched_link) {
		if (clock == iter1->env_period_deadline) {
			iter1->env_period_deadline += iter1->env_edf_period;
			iter1->env_runtime_left = iter1->env_edf_runtime;
		}
	}
	
	u_int max_deadline = 111111111;
	u_int min_id = 11111111;
	LIST_FOREACH(iter2, &env_edf_sched_list, env_edf_sched_link) {
		if (iter2->env_runtime_left > 0 && iter2->env_period_deadline <= max_deadline) {
			if (iter2->env_period_deadline == max_deadline) {
				if (iter2->env_id < min_id) {
					edf_result = iter2;
					max_deadline = iter2->env_period_deadline;
					min_id = iter2->env_id;
				} else {
					continue;
				}
			} else {
				edf_result = iter2;
				max_deadline = iter2->env_period_deadline;
			}
		}
	}
	if (!LIST_EMPTY(&env_edf_sched_list) && edf_result != NULL) 
	{	
		edf_result->env_runtime_left--;
		env_run(edf_result);
	}

	static int count = 0; // remaining time slices of current env，进程剩余的时间片
	struct Env *e = last_rr; // 当前运行的进程


	/* Exercise 3.12: Your code here. */
	if (yield != 0 || count == 0 || e == NULL || e->env_status != ENV_RUNNABLE) {
		if (e != NULL) {
			TAILQ_REMOVE(&env_sched_list, e, env_sched_link);
			if (e->env_status == ENV_RUNNABLE) {
				TAILQ_INSERT_TAIL(&env_sched_list, e, env_sched_link);
			}
		}
		if (TAILQ_EMPTY(&env_sched_list)) {
			panic("no runnable env");
		}
		e = TAILQ_FIRST(&env_sched_list);
		count = e->env_pri;
	}
	count--;
	last_rr = e;
	env_run(e);
//	if (edf_result == NULL) {
//		last_rr = e;
//		count--;
//		env_run(e);
//	} else {
//		edf_result->env_runtime_left--;
//		env_run(edf_result);
//	}
}
