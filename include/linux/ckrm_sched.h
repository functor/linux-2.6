/* include/linux/ckrm_sched.h - Supports CKRM scheduling
 *
 * Copyright (C) Haoqiang Zheng,  IBM Corp. 2004
 * Copyright (C) Hubertus Franke,  IBM Corp. 2004
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

/*
 * Overview:
 * ---------
 *
 * Please read Documentation/ckrm/cpu_sched for a general overview of
 * how the O(1) CKRM scheduler.
 *
 * ckrm_sched.h provides the definition for the per class local runqueue.
 *
 */
   
#ifndef _CKRM_SCHED_H
#define _CKRM_SCHED_H

#include <linux/sched.h>
#include <linux/ckrm_rc.h>
#include <linux/ckrm_classqueue.h>

#define BITMAP_SIZE ((((MAX_PRIO+1+7)/8)+sizeof(long)-1)/sizeof(long))

struct prio_array {
	unsigned int nr_active;
	unsigned long bitmap[BITMAP_SIZE];
	struct list_head queue[MAX_PRIO];
};


#ifndef CONFIG_CKRM_CPU_SCHEDULE

#define rq_active(p,rq)   (rq->active)
#define rq_expired(p,rq)  (rq->expired)
static inline void init_ckrm_sched_res(void) {}
static inline int ckrm_cpu_monitor_init(void) {return 0;}

#else

#define rq_active(p,rq)   (get_task_lrq(p)->active)
#define rq_expired(p,rq)  (get_task_lrq(p)->expired)

enum ckrm_sched_mode {
	CKRM_SCHED_MODE_DISABLED, /* always use default linux scheduling     */
				  /* effectively disables the ckrm scheduler */
	CKRM_SCHED_MODE_ENABLED  /* always uses ckrm scheduling behavior    */
};

extern unsigned int ckrm_sched_mode;	  /* true internal sched_mode (DIS/EN ABLED) */

int __init init_ckrm_sched_res(void);

typedef unsigned long long CVT_t;	// cummulative virtual time

struct ckrm_runqueue {
	cq_node_t classqueue_linkobj;	/*links in classqueue */
	struct ckrm_cpu_class *cpu_class;	// class it belongs to
	struct classqueue_struct *classqueue;	// classqueue it belongs tow
	unsigned long long uncounted_ns;

	prio_array_t *active, *expired, arrays[2];
	/*
	   set to 0 on init, become null or array switch
	   set to jiffies whenever an non-interactive job expires
	   reset to jiffies if expires
	 */
	unsigned long expired_timestamp;
        int best_expired_prio;

	/* 
	 * highest priority of tasks in active
	 * initialized to be MAX_PRIO
	 * updated on enqueue, dequeue
	 */
	int top_priority;
	CVT_t local_cvt;

	unsigned long lrq_load;

	/* Three different weights are distinguished:
	 * local_weight, skewed_weight, over_weight:
	 *
	 * - local_weight:  main weight to drive CVT progression
	 * - over_weight:   weight to reduce savings when over its guarantee
	 * - skewed_weight: weight to use when local_weight to small
	 *                  avoids starvation problems.
	 */
	int local_weight;   
	int over_weight;
	int skewed_weight;

	/*
	 * unused CPU time accumulated while the class 
	 * is inactive goes to savings
	 * 
	 * initialized to be 0
	 * a class can't accumulate more than SAVING_THRESHOLD of savings
	 */
	CVT_t savings;

	unsigned long magic;	//for debugging
} ____cacheline_aligned_in_smp;

#define CKRM_LRQ_MAGIC (0xACDC0702)

typedef struct ckrm_runqueue ckrm_lrq_t;

#define ckrm_cpu_disabled() (ckrm_sched_mode == CKRM_SCHED_MODE_DISABLED)   
#define ckrm_cpu_enabled()  (ckrm_sched_mode == CKRM_SCHED_MODE_ENABLED)   

/**
 * ckrm_cpu_class_stat - cpu usage statistics maintained for each class
 * 
 */
struct ckrm_cpu_class_stat {
	spinlock_t stat_lock;

	unsigned long long total_ns;	/*how much nano-secs it has consumed */

	struct ckrm_cpu_demand_stat local_stats[NR_CPUS];

	/* 
	 * 
	 */
	unsigned long max_demand; /* the maximun a class can consume */
	int egrt,megrt; /*effective guarantee*/
	int ehl,mehl; /*effective hard limit, my effective hard limit*/

	/*
	 * eshare: for both default class and its children
	 * meshare: just for the default class
	 */
	int eshare;
	int meshare;

	/* a boolean indicates if the class has savings or not */
	int has_savings; 

	/*
	 * a temporary value used by reorder_surplus_queue 
	 */
	int demand_per_share;
};

#define CKRM_CPU_CLASS_MAGIC 0x7af2abe3

#define USAGE_SAMPLE_FREQ  (HZ)  //sample every 1 seconds
#define USAGE_MAX_HISTORY  (60)  // keep the last 60 usage samples
#define NS_PER_SAMPLE      (USAGE_SAMPLE_FREQ*(NSEC_PER_SEC/HZ))

struct ckrm_usage {
	unsigned long samples[USAGE_MAX_HISTORY]; //record usages 
	unsigned long sample_pointer;  // pointer for the sliding window
	unsigned long long last_ns;    // ns for last sample
	long long last_sample_jiffies; // in number of jiffies
};

/*
 * CPU controller object allocated for each CLASS
 */
struct ckrm_cpu_class {
	struct ckrm_core_class *core;
	struct ckrm_core_class *parent;
	struct ckrm_shares shares;
	spinlock_t cnt_lock;	// always grab parent's lock first and then child's
	struct ckrm_cpu_class_stat stat;
	struct list_head links;	// for linking up in cpu classes
	struct list_head surplus_queue;	//used for surplus allocation
	ckrm_lrq_t* local_queues[NR_CPUS];	// runqueues 
	struct ckrm_usage usage;
	unsigned long magic;	//for debugging
#ifdef __SIMULATOR__
	int class_id;
#endif
};

#define cpu_class_weight(cls)   (SHARE_TO_WEIGHT(cls->stat.meshare))
#define local_class_weight(lrq) (lrq->local_weight)

static inline int valid_cpu_class(struct ckrm_cpu_class * cls)
{
	return (cls && cls->magic == CKRM_CPU_CLASS_MAGIC);
}

struct classqueue_struct *get_cpu_classqueue(int cpu);
struct ckrm_cpu_class * get_default_cpu_class(void);


static inline void ckrm_usage_init(struct ckrm_usage* usage)
{
	int i;

	for (i=0; i < USAGE_MAX_HISTORY; i++)
		usage->samples[i] = 0;
	usage->sample_pointer = 0;
	usage->last_ns = 0;
	usage->last_sample_jiffies = 0;
}

/*
 * this function can be called at any frequency
 * it's self-contained
 */
static inline void ckrm_sample_usage(struct ckrm_cpu_class* clsptr)
{
	struct ckrm_usage* usage = &clsptr->usage;
	unsigned long long cur_sample;
	int duration = jiffies - usage->last_sample_jiffies;

	//jiffies wasn't start from 0
	//so it need to be properly handled
	if (unlikely(!usage->last_sample_jiffies)) 
		usage->last_sample_jiffies = jiffies;

	//called too frequenctly
	if (duration < USAGE_SAMPLE_FREQ)
		return;

	usage->last_sample_jiffies = jiffies;

	cur_sample = clsptr->stat.total_ns - usage->last_ns; 
	usage->last_ns = clsptr->stat.total_ns;

	//scale it based on the sample duration
	cur_sample *= ((USAGE_SAMPLE_FREQ<< 15)/duration);
	cur_sample >>= 15;
	usage->samples[usage->sample_pointer] = cur_sample;
	//	printk("sample = %llu jiffies=%lu \n",cur_sample, jiffies);

	usage->sample_pointer ++;
	if (usage->sample_pointer >= USAGE_MAX_HISTORY)
		usage->sample_pointer = 0;
}

#define lrq_nr_running(lrq) \
             (lrq->active->nr_active + lrq->expired->nr_active)

static inline ckrm_lrq_t *get_ckrm_lrq(struct ckrm_cpu_class*cls, int cpu)
{
	return cls->local_queues[cpu];
}

static inline ckrm_lrq_t *get_task_lrq(struct task_struct *p)
{
	return p->cpu_class->local_queues[task_cpu(p)];
}

#define task_list_entry(list)  list_entry(list,struct task_struct,run_list)
#define class_list_entry(list) list_entry(list,struct ckrm_runqueue,classqueue_linkobj)

/* some additional interfaces exported from sched.c */
struct runqueue;
extern rwlock_t class_list_lock;
extern struct list_head active_cpu_classes;
unsigned int task_timeslice(task_t *p);
void _ckrm_cpu_change_class(task_t *task, struct ckrm_cpu_class *newcls);

void init_cpu_classes(void);
void init_cpu_class(struct ckrm_cpu_class *cls,ckrm_shares_t* shares);
void ckrm_cpu_change_class(void *task, void *old, void *new);

#define CPU_DEMAND_ENQUEUE 0
#define CPU_DEMAND_DEQUEUE 1
#define CPU_DEMAND_DESCHEDULE 2
#define CPU_DEMAND_INIT 3

/*functions exported by ckrm_cpu_monitor.c*/
int update_effectives(void);
void ckrm_cpu_monitor(int check_min);
int ckrm_cpu_monitor_init(void);
void ckrm_cpu_stat_init(struct ckrm_cpu_class_stat *stat, int eshares);
void cpu_demand_event(struct ckrm_cpu_demand_stat* local_stat, int event, unsigned long long len);
void adjust_local_weight(void);

#define get_task_lrq_stat(p) (&(p)->cpu_class->stat.local_stats[task_cpu(p)])
#define get_cls_local_stat(cls,cpu) (&(cls)->stat.local_stats[cpu])
#define get_rq_local_stat(lrq,cpu) (get_cls_local_stat((lrq)->cpu_class,cpu))

/********************************************************************
 * Parameters that determine how quickly CVT's progress and how
 * priority can impact a LRQ's runqueue position. See also
 * get_effective_prio(). These parameters need to adjusted
 * in accordance to the following example and understanding.
 * 
 * CLASS_QUANTIZER:
 * 
 * A class with 50% share, can execute 500 ms / per sec ~ 2^29 ns.
 * It's share will be set to 512 = 2^9. The globl CLASSQUEUE_SIZE is set to 2^7.
 * With CLASS_QUANTIZER=16, the local_cvt of this class will increase
 * by 2^29/2^9 = 2^20 = 1024K.
 * Setting CLASS_QUANTIZER to 16, 2^(20-16) = 16 slots / per second.
  * Do the same math, a class with any share value, will cover 16 slots / per second. 
 * So 2^8 total slots is good track for 8 seconds of system execution
 *
 * PRIORITY_QUANTIZER:
 *
 * How much can top priorities of class impact slot bonus.
 * There are 40 nice priorities, range from -20 to 19, with default nice = 0
 * "2" will allow upto 5 slots improvement 
 * when certain task within the class  has a nice value of -20 
 * in the RQ thus for 50% class it can perform ~300 msec starvation.
 *
 *******************************************************************/

/*
 * The class priority is biasd toward classes with high priority tasks. 
 * But we need to prevent this bias from starving other classes.
 * If a class has nice value of -20, how much it can starve the default class?
 * priority bonus =  (120-100) >> PRIORITY_QUANTIZER, 
 * if PRIORITY_QUANTIZER = 2, then it's 5 steps ahead
 * A class without bonus thus can't get to run until: 
 * bonus * CKRM_MAX_WEIGHT * CVT_INC_PERSHARE = (120-100) >> PRIORITY_QUANTIZER
 *  (1 << CKRM_WEIGHT_SHIFT)
 *  (1 << CLASS_QUANTIZER) 
*/

/* 
 * CKRM_WEIGHT_SHIFT and CLASS_QUANTIZER control how much a class with 
 * high priority task can starve a normal priority class, so it should
 * be constant CLASS_QUANTIZER should not be too small otherwise we 
 * don't have enough bins in the classqueue.
 * The ideal value of CLASS_QUANTIZER is 20, but a little smaller is acceptable
 */

#define CLASS_QUANTIZER     (18)// shift from ns to increase class bonus
#define PRIORITY_QUANTIZER  (2) // how much a high prio task can borrow
#define CKRM_WEIGHT_SHIFT   (8) // 1/2^x == finest weight granularity
#define CKRM_MAX_WEIGHT     (1<<CKRM_WEIGHT_SHIFT)  // - " -

/* SHARES:
 * shares are set in a hierarchical path. Since specified share settings 
 * of a class (c) are relative to the parent (p) and its totals
 * the shares can get very small, dependent on how many classes are 
 * specified.
 */
 
#define CKRM_SHARE_SHIFT (13)  
#define CKRM_SHARE_MAX   (1 << CKRM_SHARE_SHIFT)

#define SHARE_TO_WEIGHT(x) ((x) >> (CKRM_SHARE_SHIFT - CKRM_WEIGHT_SHIFT))
#define WEIGHT_TO_SHARE(x) ((x) << (CKRM_SHARE_SHIFT - CKRM_WEIGHT_SHIFT))

/* Other constants */

#define NSEC_PER_MS          (1000000)
#define NSEC_PER_JIFFIES     (NSEC_PER_SEC/HZ)

#define MAX_SAVINGS_ABSOLUTE (4LLU*NSEC_PER_SEC)  // 4 seconds
#define CVT_UPDATE_TICK      ((HZ/2)?:1)
#define MAX_SAVINGS          MAX_SAVINGS_ABSOLUTE
#define SAVINGS_LEAK_SPEED   (CVT_UPDATE_TICK/10*NSEC_PER_JIFFIES)

/**
 * get_effective_prio: return the effective priority of a class local queue
 *
 * class priority = progress * a + urgency * b
 * progress = queue cvt
 * urgency = queue top priority
 * a and b are scaling factors  
 * currently, prio increases by 1 if either: top_priority increase by one
 *                                   or, local_cvt increases by 4ms
 */
static inline int get_effective_prio(ckrm_lrq_t * lrq)
{
	int prio;

	prio = lrq->local_cvt >> CLASS_QUANTIZER;  // cumulative usage
#define URGENCY_SUPPORT 1
#ifndef URGENCY_SUPPORT
#warning "ACB removing urgency calculation from get_effective_prio"
#else
	prio += lrq->top_priority >> PRIORITY_QUANTIZER; // queue urgency
#endif

	return prio;
}

CVT_t get_local_cur_cvt(int cpu);

/** 
 * update_class_priority:
 * 
 * called whenever cvt or top_priority changes
 *
 * internal: (calling structure)
 * update_class_priority
 *   -- set_top_priority
 *      -- class_enqueue_task
 *      -- class_dequeue_task
 *      -- rq_get_next_task (queue switch)
 *   -- update_local_cvt
 *      -- schedule
 */
static inline void update_class_priority(ckrm_lrq_t *local_rq)
{
	int effective_prio = get_effective_prio(local_rq);
	classqueue_update_prio(local_rq->classqueue,
			       &local_rq->classqueue_linkobj,
			       effective_prio);
}

/*
 *  set the new top priority and reposition the queue
 *  called when: task enqueue/dequeue and queue switch
 */
static inline void set_top_priority(ckrm_lrq_t *lrq,
				    int new_priority)
{
	lrq->top_priority = new_priority;
	update_class_priority(lrq);
}

/*
 * task_load: how much load this task counts
 */
static inline unsigned long task_load(struct task_struct* p)
{
	return (task_timeslice(p) * p->demand_stat.cpu_demand);
}

/*
 * moved to ckrm_sched.c
 * but may need to make it static inline to improve performance
 */
void update_local_cvt(struct task_struct *p, unsigned long nsec);
                                                                                
static inline int class_preempts_curr(struct task_struct * p, struct task_struct* curr)
{
	struct cq_node_struct* node1 = &(get_task_lrq(p)->classqueue_linkobj);
	struct cq_node_struct* node2 = &(get_task_lrq(curr)->classqueue_linkobj);

	return (class_compare_prio(node1,node2) < 0);
}

/*
 * return a random value with range [0, (val-1)]
 */
static inline int get_ckrm_rand(unsigned long val)
{
	int rand;
	static int last_rand[NR_CPUS];
	int cpu = smp_processor_id();

	rand = last_rand[cpu];
	rand ++;
	if (rand >= val)
		rand = 0; 
	
	last_rand[cpu] = rand;
	return rand;
}

void update_class_cputime(int this_cpu, int idle);

/**********************************************/
/*          PID_LOAD_BALANCING                */
/**********************************************/

#define CPU_PID_CTRL_TICK 32

struct ckrm_load_struct {
	unsigned long load_p; 	/*propotional*/
	unsigned long load_i;   /*integral   */
	long load_d;   /*derivative */
};

typedef struct ckrm_load_struct ckrm_load_t;

static inline void ckrm_load_init(ckrm_load_t* ckrm_load) {
	ckrm_load->load_p = 0;
	ckrm_load->load_i = 0;
	ckrm_load->load_d = 0;
}

void ckrm_load_sample(ckrm_load_t* ckrm_load,int cpu);
long ckrm_get_pressure(ckrm_load_t* ckrm_load, int local_group);
#define rq_ckrm_load(rq) (&((rq)->ckrm_load))


#endif /*CONFIG_CKRM_CPU_SCHEDULE */

#endif


