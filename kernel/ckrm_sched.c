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

rwlock_t   class_list_lock = RW_LOCK_UNLOCKED;
LIST_HEAD(active_cpu_classes);   // list of active cpu classes; anchor

struct ckrm_cpu_class default_cpu_class_obj;

struct ckrm_cpu_class * get_default_cpu_class(void) {
	return (&default_cpu_class_obj);
}

/*******************************************************/
/*                CVT Management                       */
/*******************************************************/
/**
 * update_class_cputime - update the total cpu time received by a class
 * 
 * class_list_lock must have been acquired 
 */
void update_class_cputime(int this_cpu)
{
	struct ckrm_cpu_class *clsptr;
	ckrm_lrq_t * lrq;
	CVT_t max_cvt, min_cvt;

	max_cvt = 0;

	/*update class time, at the same time get max_cvt */
	list_for_each_entry(clsptr, &active_cpu_classes, links) {
		lrq = get_ckrm_lrq(clsptr, this_cpu);

		spin_lock(&clsptr->stat.stat_lock);
		clsptr->stat.total_ns += lrq->uncounted_ns;
		spin_unlock(&clsptr->stat.stat_lock);

		lrq->uncounted_ns = 0;
		if (lrq->local_cvt > max_cvt)
			max_cvt = lrq->local_cvt;
	}
	min_cvt = max_cvt - CVT_INTERACTIVE_BONUS;
	BUG_ON(min_cvt < 0);

	/*check again, make sure no one get too small cvt*/
	list_for_each_entry(clsptr, &active_cpu_classes, links) {
		lrq = get_ckrm_lrq(clsptr, this_cpu);
		if (lrq->local_cvt < min_cvt)
			lrq->local_cvt = min_cvt;
	}
}

/*******************************************************/
/*                PID load balancing stuff             */
/*******************************************************/
#define PID_SAMPLE_T 32
#define PID_KP 20
#define PID_KI 60
#define PID_KD 20

/**
 * sample pid load periodically
 */
void ckrm_load_sample(ckrm_load_t* pid,int cpu)
{
	long load;
	long err;

	if (jiffies % PID_SAMPLE_T)
		return;

	adjust_local_weight();	

	load = ckrm_cpu_load(cpu);
	err = load - pid->load_p;
	pid->load_d = err;
	pid->load_p = load;
	pid->load_i *= 9;
	pid->load_i += load;
	pid->load_i /= 10;
}

long pid_get_pressure(ckrm_load_t* ckrm_load, int local_group)
{
	long pressure;
	pressure = ckrm_load->load_p * PID_KP;
	pressure += ckrm_load->load_i * PID_KI;
	pressure += ckrm_load->load_d * PID_KD;
	pressure /= 100;
	return pressure;
}
