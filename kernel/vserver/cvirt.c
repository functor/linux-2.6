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
#include <linux/vserver/cvirt.h>
#include <linux/vserver/context.h>
#include <linux/vserver/switch.h>
#include <linux/vs_base.h>
#include <linux/vs_context.h>
#include <linux/vs_cvirt.h>

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

uint64_t vx_idle_jiffies()
{
	return init_task.utime + init_task.stime;
}



static inline uint32_t __update_loadavg(uint32_t load,
	int wsize, int delta, int n)
{
	unsigned long long calc;

	/* just set it to n */
	if (unlikely(delta >= wsize))
		return (n << FSHIFT);

	calc = (delta * n) << FSHIFT;
	calc += (wsize - delta) * load;
	do_div(calc, wsize);
	return calc;
}


void vx_update_load(struct vx_info *vxi)
{
	uint32_t now, last, delta;

	spin_lock(&vxi->cvirt.load_lock);

	now = jiffies;
	last = vxi->cvirt.load_last;
	delta = now - last;

	vxi->cvirt.load[0] = __update_loadavg(vxi->cvirt.load[0],
		60*HZ, delta, atomic_read(&vxi->cvirt.nr_running));
	vxi->cvirt.load[1] = __update_loadavg(vxi->cvirt.load[1],
		5*60*HZ, delta, atomic_read(&vxi->cvirt.nr_running));
	vxi->cvirt.load[2] = __update_loadavg(vxi->cvirt.load[2],
		15*60*HZ, delta, atomic_read(&vxi->cvirt.nr_running));

	vxi->cvirt.load_last = now;
	spin_unlock(&vxi->cvirt.load_lock);
}



