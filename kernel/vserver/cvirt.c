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
	struct timeval bias;

	jiffies_to_timeval(vxi->cvirt.bias_jiffies - INITIAL_JIFFIES, &bias);

	set_normalized_timespec(uptime,
		uptime->tv_sec - bias.tv_sec,
		uptime->tv_nsec - bias.tv_usec*1000);
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

