/* kernel/ckrm_sched.c - Supporting functions for ckrm scheduling
 *
 * Copyright (C) Haoqiang Zheng,  IBM Corp. 2004
 *           (C) Hubertus Franke, IBM Corp. 2004
 * 
 * Latest version, more details at http://ckrm.sf.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ckrm_sched.h>

/*******************************************************/
/*                CVT Management                       */
/*******************************************************/
#define CVT_WINDOW_SIZE (CLASSQUEUE_SIZE << CLASS_BONUS_RATE)
static CVT_t max_CVT = CVT_WINDOW_SIZE;

/*
 *  Also ensure that the classes global cvt is upgraded to the 
 * minimum CVT in the system, as a class might not have run for a while
 */
static void update_global_cvt(struct ckrm_cpu_class *cpu_class, int cpu)
{
	struct ckrm_local_runqueue *class_queue =
	    get_ckrm_local_runqueue(cpu_class, cpu);
	CVT_t min_cvt;
	CVT_t local_cvt_old = class_queue->local_cvt;

	spin_lock(&cvt_lock);
	if (class_queue->uncounted_cvt) {
		cpu_class->global_cvt += class_queue->uncounted_cvt;
		class_queue->uncounted_cvt = 0;
	}
	min_cvt = max_CVT - CVT_WINDOW_SIZE;
	if (cpu_class->global_cvt < min_cvt)
		cpu_class->global_cvt = min_cvt;
	else  if (cpu_class->global_cvt > max_CVT)
		max_CVT = cpu_class->global_cvt;

/* update local cvt from global cvt*/
#if 0
	class_queue->local_cvt = cpu_class->global_cvt;
#endif
	spin_unlock(&cvt_lock);

	if (class_queue->local_cvt != local_cvt_old)
		update_class_priority(class_queue);
}

/*
 * class_list_lock must have been acquired 
 */
void update_global_cvts(int this_cpu)
{
	struct ckrm_cpu_class *clsptr;
	struct ckrm_local_runqueue *class_queue;

	/*for each class*/
	list_for_each_entry(clsptr, &active_cpu_classes, links) {
		update_global_cvt(clsptr, this_cpu);
		class_queue = get_ckrm_local_runqueue(clsptr, this_cpu);
		clsptr->stat.total_ns += class_queue->uncounted_ns;
		class_queue->uncounted_ns = 0;
	}
}
