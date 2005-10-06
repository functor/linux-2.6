/*
 *  linux/kernel/vserver/cvirt.c
 *
 *  Virtual Server: Context Virtualization
 *
 *  Copyright (C) 2004  Herbert Pötzl
 *
 *  V0.01  broken out from limit.c
 *
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/vs_context.h>
#include <linux/vs_cvirt.h>
#include <linux/vserver/switch.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


void vx_vsi_uptime(struct timespec *uptime, struct timespec *idle)
{
	struct vx_info *vxi = current->vx_info;

	set_normalized_timespec(uptime,
		uptime->tv_sec - vxi->cvirt.bias_uptime.tv_sec,
		uptime->tv_nsec - vxi->cvirt.bias_uptime.tv_nsec);
	if (!idle)
		return;
	set_normalized_timespec(idle,
		idle->tv_sec - vxi->cvirt.bias_idle.tv_sec,
		idle->tv_nsec - vxi->cvirt.bias_idle.tv_nsec);
	return;
}

uint64_t vx_idle_jiffies(void)
{
	return init_task.utime + init_task.stime;
}



static inline uint32_t __update_loadavg(uint32_t load,
	int wsize, int delta, int n)
{
	unsigned long long calc, prev;

	/* just set it to n */
	if (unlikely(delta >= wsize))
		return (n << FSHIFT);

	calc = delta * n;
	calc <<= FSHIFT;
	prev = (wsize - delta);
	prev *= load;
	calc += prev;
	do_div(calc, wsize);
	return calc;
}


void vx_update_load(struct vx_info *vxi)
{
	uint32_t now, last, delta;
	unsigned int nr_running, nr_uninterruptible;
	unsigned int total;

	spin_lock(&vxi->cvirt.load_lock);

	now = jiffies;
	last = vxi->cvirt.load_last;
	delta = now - last;

	if (delta < 5*HZ)
		goto out;

	nr_running = atomic_read(&vxi->cvirt.nr_running);
	nr_uninterruptible = atomic_read(&vxi->cvirt.nr_uninterruptible);
	total = nr_running + nr_uninterruptible;

	vxi->cvirt.load[0] = __update_loadavg(vxi->cvirt.load[0],
		60*HZ, delta, total);
	vxi->cvirt.load[1] = __update_loadavg(vxi->cvirt.load[1],
		5*60*HZ, delta, total);
	vxi->cvirt.load[2] = __update_loadavg(vxi->cvirt.load[2],
		15*60*HZ, delta, total);

	vxi->cvirt.load_last = now;
out:
	atomic_inc(&vxi->cvirt.load_updates);
	spin_unlock(&vxi->cvirt.load_lock);
}



