/*
 *  linux/kernel/vserver/sched.c
 *
 *  Virtual Server: Scheduler Support
 *
 *  Copyright (C) 2004  Herbert Pötzl
 *
 *  V0.01  adapted Sam Vilains version to 2.6.3
 *  V0.02  removed legacy interface
 *
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/vs_base.h>
#include <linux/vs_context.h>
#include <linux/vserver/context.h>
#include <linux/vserver/sched.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


/*
 * recalculate the context's scheduling tokens
 *
 * ret > 0 : number of tokens available
 * ret = 0 : context is paused
 * ret < 0 : number of jiffies until new tokens arrive
 *
 */
int vx_tokens_recalc(struct vx_info *vxi)
{
	long delta, tokens = 0;

	if (__vx_flags(vxi->vx_flags, VXF_SCHED_PAUSE, 0))
		/* we are paused */
		return 0;

	delta = jiffies - vxi->sched.jiffies;

	if (delta >= vxi->sched.interval) {
		/* lockdown scheduler info */
		spin_lock(&vxi->sched.tokens_lock);

		/* calc integral token part */
		delta = jiffies - vxi->sched.jiffies;
		tokens = delta / vxi->sched.interval;
		delta = tokens * vxi->sched.interval;
		tokens *= vxi->sched.fill_rate;

		atomic_add(tokens, &vxi->sched.tokens);
		vxi->sched.jiffies += delta;
		tokens = atomic_read(&vxi->sched.tokens);
	
		if (tokens > vxi->sched.tokens_max) {
			tokens = vxi->sched.tokens_max;
			atomic_set(&vxi->sched.tokens, tokens);
		}
		spin_unlock(&vxi->sched.tokens_lock);
	} else {
		/* no new tokens */
		if ((tokens = vx_tokens_avail(vxi)) < vxi->sched.tokens_min) {
			/* enough tokens will be available in */
			if (vxi->sched.tokens_min == 0)
				return delta - vxi->sched.interval;
			return delta - vxi->sched.interval *
				vxi->sched.tokens_min / vxi->sched.fill_rate;
		}
	}
	/* we have some tokens left */
	return tokens;
}

/*
 * effective_prio - return the priority that is based on the static
 * priority but is modified by bonuses/penalties.
 *
 * We scale the actual sleep average [0 .... MAX_SLEEP_AVG]
 * into a -4 ... 0 ... +4 bonus/penalty range.
 *
 * Additionally, we scale another amount based on the number of
 * CPU tokens currently held by the context, if the process is
 * part of a context (and the appropriate SCHED flag is set).
 * This ranges from -5 ... 0 ... +15, quadratically.
 *
 * So, the total bonus is -9 .. 0 .. +19
 * We use ~50% of the full 0...39 priority range so that:
 *
 * 1) nice +19 interactive tasks do not preempt nice 0 CPU hogs.
 * 2) nice -20 CPU hogs do not get preempted by nice 0 tasks.
 *    unless that context is far exceeding its CPU allocation.
 *
 * Both properties are important to certain workloads.
 */
int effective_vavavoom(task_t *p, int max_prio)
{
	struct vx_info *vxi = p->vx_info;
	int vavavoom, max;

	/* lots of tokens = lots of vavavoom
	 *      no tokens = no vavavoom      */
	if ((vavavoom = atomic_read(&vxi->sched.tokens)) >= 0) {
		max = vxi->sched.tokens_max;
		vavavoom = max - vavavoom;
		max = max * max;
		vavavoom = max_prio * VAVAVOOM_RATIO / 100
			* (vavavoom*vavavoom - (max >> 2)) / max;
		/*  alternative, geometric mapping
		vavavoom = -( MAX_USER_PRIO*VAVAVOOM_RATIO/100 * vavavoom
			/ vxi->sched.tokens_max -
			MAX_USER_PRIO*VAVAVOOM_RATIO/100/2); */
	} else
		vavavoom = 0;
	/* vavavoom = ( MAX_USER_PRIO*VAVAVOOM_RATIO/100*tokens_left(p) -
		MAX_USER_PRIO*VAVAVOOM_RATIO/100/2); */

	return vavavoom;
}


int vc_set_sched(uint32_t xid, void __user *data)
{
	struct vcmd_set_sched_v2 vc_data;
	struct vx_info *vxi;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;
	
	vxi = locate_vx_info(xid);
	if (!vxi)
		return -EINVAL;

	spin_lock(&vxi->sched.tokens_lock);

	if (vc_data.interval != SCHED_KEEP)
		vxi->sched.interval = vc_data.interval;
	if (vc_data.fill_rate != SCHED_KEEP)
		vxi->sched.fill_rate = vc_data.fill_rate;
	if (vc_data.tokens_min != SCHED_KEEP)
		vxi->sched.tokens_min = vc_data.tokens_min;
	if (vc_data.tokens_max != SCHED_KEEP)
		vxi->sched.tokens_max = vc_data.tokens_max;
	if (vc_data.tokens != SCHED_KEEP)
		atomic_set(&vxi->sched.tokens, vc_data.tokens);

	/* Sanity check the resultant values */
	if (vxi->sched.fill_rate <= 0)
		vxi->sched.fill_rate = 1;
	if (vxi->sched.interval <= 0)
		vxi->sched.interval = HZ;
	if (vxi->sched.tokens_max == 0)
		vxi->sched.tokens_max = 1;
	if (atomic_read(&vxi->sched.tokens) > vxi->sched.tokens_max)
		atomic_set(&vxi->sched.tokens, vxi->sched.tokens_max);
	if (vxi->sched.tokens_min > vxi->sched.tokens_max)
		vxi->sched.tokens_min = vxi->sched.tokens_max;

	spin_unlock(&vxi->sched.tokens_lock);
	put_vx_info(vxi);
	return 0;
}

