#ifndef _VX_SCHED_DEF_H
#define _VX_SCHED_DEF_H

#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/cpumask.h>
#include <asm/atomic.h>
#include <asm/param.h>


struct _vx_ticks {
	uint64_t user_ticks;		/* token tick events */
	uint64_t sys_ticks;		/* token tick events */
	uint64_t hold_ticks;		/* token ticks paused */
	uint64_t unused[5];		/* cacheline ? */
};

#ifdef CONFIG_VSERVER_ACB_SCHED
enum {
// Different scheduling classes
    SCH_GUARANTEE = 0,
    SCH_BEST_EFFORT = 1,
    SCH_NUM_CLASSES = 2,
// States
    SCH_UNINITIALIZED,
    SCH_INITIALIZED,
};
#endif

/* context sub struct */

struct _vx_sched {
#ifdef CONFIG_VSERVER_ACB_SCHED
        uint64_t ticks[SCH_NUM_CLASSES];
        uint64_t last_ticks[SCH_NUM_CLASSES];
        int      state[SCH_NUM_CLASSES];
#endif
	atomic_t tokens;		/* number of CPU tokens */
	spinlock_t tokens_lock;		/* lock for token bucket */

	int fill_rate;			/* Fill rate: add X tokens... */
	int interval;			/* Divisor:   per Y jiffies   */
	int tokens_min;			/* Limit:     minimum for unhold */
	int tokens_max;			/* Limit:     no more than N tokens */
	uint32_t jiffies;		/* last time accounted */

	int priority_bias;		/* bias offset for priority */
	int vavavoom;			/* last calculated vavavoom */

	cpumask_t cpus_allowed;		/* cpu mask for context */

	struct _vx_ticks cpu[NR_CPUS];
};

#endif	/* _VX_SCHED_DEF_H */
