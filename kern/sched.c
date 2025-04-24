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
//	if (!LIST_EMPTY(&env_edf_sched_list) && edf_result != NULL) 
//	{	
//		edf_result->env_runtime_left--;
//		env_run(edf_result);
//	}

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
//	count--;
//	last_rr = e;
//	env_run(e);
	if (LIST_EMPTY(&env_edf_sched_list) || edf_result == NULL) {
		last_rr = e;
		count--;
		env_run(e);
	} else {
		edf_result->env_runtime_left--;
		env_run(edf_result);
	}
}

/*多用户调度算法，每次都会运行已经使用过的时间片最少的用户的进程（相同时取id小者）*/
// void schedule(int yield) {
// 	static int count = 0; // remaining time slices of current env
// 	struct Env *e = curenv;
// 	static int user_time[5];
// 	static int able[5];   // 额外声明了一个数组，当列表中存在该用户进程块时就置 1 ，否则保持 0
// 	for (int i = 0; i < 5; i++) { // 数组初始化
// 	  able[i] = 0;
// 	}
  
// 	if (yield != 0  count == 0  e == NULL  e->env_status != ENV_RUNNABLE) {
// 	  if (e != NULL) {
// 		TAILQ_REMOVE(&env_sched_list, e, env_sched_link);
// 	  }
// 	  if (e != NULL && e->env_status == ENV_RUNNABLE) {
// 		TAILQ_INSERT_TAIL(&env_sched_list, e, env_sched_link);
// 		// 记得 + env_pri，有笨比上来直接丢了这一句，根本调度不起来
// 		user_time[e->env_user] += e->env_pri;    
// 	  }
// 	  if (TAILQ_EMPTY(&env_sched_list)) {
// 		panic("schedule: no runnable envs");
// 	  }
// 	  e = TAILQ_FIRST(&env_sched_list);
// 	  count = e->env_pri;
// 	  struct Env *en = NULL;
		
// 	  // 使用了 TAILQ_FOREACH 宏进行循环包装，循环查询有哪个用户在队列里，更新 able 数组
// 	  TAILQ_FOREACH(en, &env_sched_list, env_sched_link) {
// 		if (able[en->env_user] == 0) {
// 		  able[en->env_user] = 1;
// 		}
// 	  }
  
// 	  // 循环查看哪个用户使用的时间片最少（user_time 最小）
// 	  int user = -1;
// 	  u_int times = 111111111;
// 	  for (int j = 0; j < 5; j++) {
// 		if (user_time[j] < times && able[j] == 1) {
// 		  user = j;
// 		  times = user_time[j];
// 		}
// 	  }
  
// 	  // 再循环调度链表，取出第一个目标用户的进程块，准备调度
// 	  TAILQ_FOREACH(en, &env_sched_list, env_sched_link) {
// 		if (en->env_user == user) {
// 		  e = en;               // 更换调度块
// 		  count = e->env_pri;   // 重置时间片 count
// 		  break;
// 		}
// 	  }
// 	}
// 	count--;
// 	env_run(e);
// }


// // 假设全局当前时间，需在系统时钟中断或其他机制中更新
// extern uint64_t current_time;

// // EDF调度函数
// void schedule(int yield) {
//     static int count = 0; // 当前任务剩余时间片
//     struct Env *e = curenv; // 当前运行的进程

//     // 如果当前任务需要切换（yield、非RUNNABLE、时间片用尽或无任务）
//     if (yield != 0 || count == 0 || e == NULL || e->env_status != ENV_RUNNABLE) {
//         // 从调度队列移除当前任务
//         if (e != NULL) {
//             TAILQ_REMOVE(&env_sched_list, e, env_sched_link);
//             // 如果任务仍可运行，更新其截止时间并重新插入
//             if (e->env_status == ENV_RUNNABLE) {
//                 // 更新截止时间：假设为下一个周期的截止时间
//                 e->deadline += e->period;
//                 // 按截止时间插入队列（需要自定义插入逻辑）
//                 struct Env *iter;
//                 TAILQ_FOREACH(iter, &env_sched_list, env_sched_link) {
//                     if (iter->deadline > e->deadline) {
//                         TAILQ_INSERT_BEFORE(iter, e, env_sched_link);
//                         break;
//                     }
//                 }
//                 // 如果未插入（截止时间最晚），插入队尾
//                 if (!TAILQ_LINKED(e, env_sched_link)) {
//                     TAILQ_INSERT_TAIL(&env_sched_list, e, env_sched_link);
//                 }
//             }
//         }

//         // 检查调度队列是否为空
//         if (TAILQ_EMPTY(&env_sched_list)) {
//             panic("no runnable env");
//         }

//         // 选择截止时间最早的任务
//         e = TAILQ_FIRST(&env_sched_list);
//         // 检查是否错过截止时间
//         if (e->deadline < current_time) {
//             // 可选择记录日志或跳过任务，这里简单跳过并重新调度
//             TAILQ_REMOVE(&env_sched_list, e, env_sched_link);
//             if (e->env_status == ENV_RUNNABLE) {
//                 e->deadline += e->period; // 更新到下一个周期
//                 // 重新插入队列
//                 struct Env *iter;
//                 TAILQ_FOREACH(iter, &env_sched_list, env_sched_link) {
//                     if (iter->deadline > e->deadline) {
//                         TAILQ_INSERT_BEFORE(iter, e, env_sched_link);
//                         break;
//                     }
//                 }
//                 if (!TAILQ_LINKED(e, env_sched_link)) {
//                     TAILQ_INSERT_TAIL(&env_sched_list, e, env_sched_link);
//                 }
//             }
//             // 递归调用以选择新任务
//             schedule(0);
//             return;
//         }

//         // 设置时间片
//         count = e->env_pri;
//     }

//     count--;
//     env_run(e);
// }
