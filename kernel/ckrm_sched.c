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

static inline void check_inactive_class(ckrm_lrq_t * lrq,CVT_t cur_cvt)
{
	CVT_t min_cvt;
	CVT_t bonus;

	//just a safty measure
	if (unlikely(! cur_cvt))
		return; 

	/*
	 * Always leaving a small bonus for inactive classes 
	 * allows them to compete for cycles immediately when the become
	 * active. This should improve interactive behavior
	 */
	bonus = INTERACTIVE_BONUS(lrq);
	//cvt can't be negative
	if (cur_cvt > bonus)
		min_cvt = cur_cvt - bonus;
	else
		min_cvt = 0;
	
	if (lrq->local_cvt < min_cvt) {
		CVT_t lost_cvt;

		lost_cvt = scale_cvt(min_cvt - lrq->local_cvt,lrq);
		lrq->local_cvt = min_cvt;

		/* add what the class lost to its savings*/
		lrq->savings += lost_cvt;
		if (lrq->savings > MAX_SAVINGS)
			lrq->savings = MAX_SAVINGS; 
	} else if (lrq->savings) {
		/*
		 *if a class saving and falling behind
		 * then start to use it saving in a leaking bucket way
		 */
		CVT_t savings_used;

		savings_used = scale_cvt((lrq->local_cvt - min_cvt),lrq);
		if (savings_used > lrq->savings)
			savings_used = lrq->savings;
		
		if (savings_used > SAVINGS_LEAK_SPEED)
			savings_used = SAVINGS_LEAK_SPEED;

		BUG_ON(lrq->savings < savings_used);
		lrq->savings -= savings_used;
		unscale_cvt(savings_used,lrq);
		BUG_ON(lrq->local_cvt < savings_used);
		lrq->local_cvt -= savings_used;
	}		
}

/*
 * return the max_cvt of all the classes
 */
static inline CVT_t get_max_cvt(int this_cpu)
{
        struct ckrm_cpu_class *clsptr;
        ckrm_lrq_t * lrq;
        CVT_t max_cvt;

        max_cvt = 0;

        /*update class time, at the same time get max_cvt */
        list_for_each_entry(clsptr, &active_cpu_classes, links) {
                lrq = get_ckrm_lrq(clsptr, this_cpu);
                if (lrq->local_cvt > max_cvt)
                        max_cvt = lrq->local_cvt;
        }

	return max_cvt;
}

/**
 * update_class_cputime - updates cvt of inactive classes
 * -- an inactive class shouldn't starve others when it comes back
 * -- the cpu time it lost when it's inactive should be accumulated
 * -- its accumulated saving should be compensated (in a leaky bucket fashion)
 * 
 * class_list_lock must have been acquired 
 */
void update_class_cputime(int this_cpu)
{
	struct ckrm_cpu_class *clsptr;
	ckrm_lrq_t * lrq;
	CVT_t cur_cvt;

	/*
	 *  a class's local_cvt must not be significantly smaller than min_cvt 
	 *  of active classes otherwise, it will starve other classes when it 
         *  is reactivated.
	 * 
  	 *  Hence we keep all local_cvt's within a range of the min_cvt off
	 *  all active classes (approximated by the local_cvt of the currently
	 *  running class) and account for how many cycles where thus taken
	 *  from an inactive class building a savings (not to exceed a few seconds)
	 *  for a class to gradually make up upon reactivation, without 
	 *  starvation of other classes.
         *  
	 */
	cur_cvt = get_local_cur_cvt(this_cpu);

	/*
	 * cur_cvt == 0 means the system is now idle
	 * in this case, we use max_cvt as cur_cvt
	 * max_cvt roughly represents the cvt of the class 
	 * that has just finished running
	 *
	 * fairness wouldn't be a problem since we account for whatever lost in savings
	 * if the system is not busy, the system responsiveness is not a problem.
	 * still fine if the sytem is busy, but happened to be idle at this certain point
	 * since bias toward interactive classes (class priority) is a more important way to improve system responsiveness
	 */
	if (unlikely(! cur_cvt))  {
		cur_cvt = get_max_cvt(this_cpu);
		//return;
	}

	/* 
	 *  - check the local cvt of all the classes 
	 *  - update total_ns received by the class
	 *  - do a usage sampling for the whole class
	 */
	list_for_each_entry(clsptr, &active_cpu_classes, links) {
		lrq = get_ckrm_lrq(clsptr, this_cpu);

		spin_lock(&clsptr->stat.stat_lock);
		clsptr->stat.total_ns += lrq->uncounted_ns;
		ckrm_sample_usage(clsptr);
		spin_unlock(&clsptr->stat.stat_lock);
		lrq->uncounted_ns = 0;

		check_inactive_class(lrq,cur_cvt);		
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
