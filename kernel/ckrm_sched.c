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

unsigned int ckrm_sched_mode __cacheline_aligned_in_smp = 
#ifdef CONFIG_CKRM_CPU_SCHEDULE_AT_BOOT
			CKRM_SCHED_MODE_ENABLED;
#else
			CKRM_SCHED_MODE_DISABLED;
#endif

static int __init ckrm_cpu_enabled_setup(char *str)
{
	ckrm_sched_mode = CKRM_SCHED_MODE_ENABLED;
	return 1;
}

static int __init ckrm_cpu_disabled_setup(char *str)
{
	ckrm_sched_mode = CKRM_SCHED_MODE_DISABLED;
	return 1;
}

__setup("ckrmcpu",  ckrm_cpu_enabled_setup);
__setup("nockrmcpu",ckrm_cpu_disabled_setup);

struct ckrm_cpu_class * get_default_cpu_class(void) {
	return (&default_cpu_class_obj);
}

/*******************************************************/
/*                CVT Management                       */
/*******************************************************/

//an absolute bonus of 200ms for classes when reactivated
#define INTERACTIVE_BONUS(lrq) ((200*NSEC_PER_MS)/local_class_weight(lrq))

static void check_inactive_class(ckrm_lrq_t * lrq,CVT_t cur_cvt)
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
	if (likely(cur_cvt > bonus))
		min_cvt = cur_cvt - bonus;
	else
		min_cvt = 0;

	if (lrq->local_cvt < min_cvt) {	
		//	if (lrq->local_cvt < min_cvt && ! lrq_nr_running(lrq)) {
		CVT_t lost_cvt;

		if (unlikely(lrq->local_cvt == 0)) {
			lrq->local_cvt = cur_cvt;
			return;
		}
		lost_cvt = min_cvt - lrq->local_cvt;
		lost_cvt *= local_class_weight(lrq);
		lrq->local_cvt = min_cvt;
		BUG_ON(lost_cvt < 0);

		/* add what the class lost to its savings*/
#if 1 /*zhq debugging*/
		lrq->savings += lost_cvt;	       
#endif
		if (lrq->savings > MAX_SAVINGS)
			lrq->savings = MAX_SAVINGS; 
#if 0 /* zhq debugging*/
		printk("lrq= %x savings: %llu lost= %llu\n",(int)lrq,lrq->savings,lost_cvt);
#endif
	}
}

/*
 * return the max_cvt of all the classes
 */
CVT_t get_max_cvt(int this_cpu)
{
        struct ckrm_cpu_class *clsptr;
        ckrm_lrq_t * lrq;
        CVT_t max_cvt;

        max_cvt = 0;

        list_for_each_entry(clsptr, &active_cpu_classes, links) {
                lrq = get_ckrm_lrq(clsptr, this_cpu);
                if (lrq->local_cvt > max_cvt)
                        max_cvt = lrq->local_cvt;
        }

	return max_cvt;
}

CVT_t get_min_cvt(int this_cpu)
{
        struct ckrm_cpu_class *clsptr;
        ckrm_lrq_t * lrq;
        CVT_t max_cvt;

        max_cvt = 0xFFFFFFFFFFFFFLLU;

        list_for_each_entry(clsptr, &active_cpu_classes, links) {
                lrq = get_ckrm_lrq(clsptr, this_cpu);
                if (lrq->local_cvt < max_cvt)
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
void update_class_cputime(int this_cpu, int idle)
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
#define PID_KP 20
#define PID_KI 60
#define PID_KD 20

/*
 * runqueue load is the local_weight of all the classes on this cpu
 * must be called with class_list_lock held
 */
static unsigned long ckrm_cpu_load(int cpu)
{
	struct ckrm_cpu_class *clsptr;
	ckrm_lrq_t* lrq;
	struct ckrm_cpu_demand_stat* l_stat;
	int total_load = 0;
	int load;

	list_for_each_entry(clsptr,&active_cpu_classes,links) {
		lrq =  get_ckrm_lrq(clsptr,cpu);
		l_stat = get_cls_local_stat(clsptr,cpu);

		load = WEIGHT_TO_SHARE(lrq->local_weight);
		
		if (l_stat->cpu_demand < load)
			load = l_stat->cpu_demand;
		total_load += load;
	}	
	return total_load;
}


/**
 * sample pid load periodically
 */

void ckrm_load_sample(ckrm_load_t* pid,int cpu)
{
	long load;
	long err;

	load = ckrm_cpu_load(cpu);
	err = load - pid->load_p;
	pid->load_d = err;
	pid->load_p = load;
	pid->load_i *= 9;
	pid->load_i += load;
	pid->load_i /= 10;
}

long ckrm_get_pressure(ckrm_load_t* ckrm_load, int local_group)
{
	long pressure;
	pressure = ckrm_load->load_p * PID_KP;
	pressure += ckrm_load->load_i * PID_KI;
	pressure += ckrm_load->load_d * PID_KD;
	pressure /= 100;
	return pressure;
}

/*
 *  called after a task is switched out. Update the local cvt accounting 
 *  we need to stick with long instead of long long due to nonexistent 
 *  64-bit division
 */
void update_local_cvt(struct task_struct *p, unsigned long nsec)
{
	ckrm_lrq_t * lrq = get_task_lrq(p);
	unsigned long cvt_inc;

	/*
	 * consume from savings if eshare is larger than egrt
	 */
	if (lrq->savings && lrq->over_weight) {
		unsigned long savings_used;

		savings_used = nsec;
		savings_used >>= CKRM_WEIGHT_SHIFT;
		savings_used *= lrq->over_weight;
		if (savings_used > lrq->savings)
			savings_used = lrq->savings;
		lrq->savings -= savings_used;	
	}

	//BUG_ON(local_class_weight(lrq) == 0);
	cvt_inc = nsec / local_class_weight(lrq); 

	/* 
	 * For a certain processor, CKRM allocates CPU time propotional 
	 * to the class's local_weight. So once a class consumed nsec, 
	 * it will wait for X (nsec) for its next turn.
	 *
	 * X is calculated based on the following fomular
	 *     nsec / local_weight < X / (CKRM_MAX_WEIGHT - local_weight)
	 * if local_weight is small, then approximated as
	 *     nsec / local_weight < X / (CKRM_MAX_WEIGHT)
	 */
#define CVT_STARVATION_LIMIT (200LL*NSEC_PER_MS)
#define CVT_STARVATION_INC_LIMIT (CVT_STARVATION_LIMIT >> CKRM_WEIGHT_SHIFT)

	if (unlikely(lrq->skewed_weight)) {
		unsigned long long starvation_limit = CVT_STARVATION_INC_LIMIT;
		
		starvation_limit *= local_class_weight(lrq);
		if (unlikely(cvt_inc > starvation_limit))	  
			cvt_inc = nsec / lrq->skewed_weight;
	}

	/* now update the CVT accounting */

	lrq->local_cvt += cvt_inc;
	lrq->uncounted_ns += nsec;
	update_class_priority(lrq);
}
