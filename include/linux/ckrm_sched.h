/* include/linux/ckrm_sched.h - Supports CKRM scheduling
 *
 * Copyright (C) Haoqiang Zheng,  IBM Corp. 2004
 * Copyright (C) Hubertus Franke,  IBM Corp. 2004
 * 
 * Latest version, more details at http://ckrm.sf.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifdef CONFIG_CKRM_CPU_SCHEDULE
#define rq_active(p,rq)   (get_task_lrq(p)->active)
#define rq_expired(p,rq)  (get_task_lrq(p)->expired)
int __init init_ckrm_sched_res(void);
#else
#define rq_active(p,rq)   (rq->active)
#define rq_expired(p,rq)  (rq->expired)
static inline void init_ckrm_sched_res(void) {}
static inline int ckrm_cpu_monitor_init(void) {return 0;}
#endif //CONFIG_CKRM_CPU_SCHEDULE

#ifdef CONFIG_CKRM_CPU_SCHEDULE
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

	/* 
	 * highest priority of tasks in active
	 * initialized to be MAX_PRIO
	 * updated on enqueue, dequeue
	 */
	int top_priority;
	CVT_t local_cvt;

	unsigned long lrq_load;
	int local_weight; 


	/*
	 * unused CPU time accumulated while thoe class 
	 * is inactive goes to savings
	 * 
	 * initialized to be 0
	 * a class can't accumulate more than SAVING_THRESHOLD of savings
	 * savings are kept in normalized form (like cvt)
	 * so when task share change the savings should be scaled accordingly
	 */
	unsigned long long savings;

	unsigned long magic;	//for debugging
};

typedef struct ckrm_runqueue ckrm_lrq_t;

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
};

#define CKRM_CPU_CLASS_MAGIC 0x7af2abe3

#define USAGE_SAMPLE_FREQ HZ  //sample every 1 seconds
#define NS_PER_SAMPLE (USAGE_SAMPLE_FREQ*(NSEC_PER_SEC/HZ))
#define USAGE_WINDOW_SIZE 60  //keep the last 60 sample

struct ckrm_usage {
	unsigned long samples[USAGE_WINDOW_SIZE]; //record usages 
	unsigned long sample_pointer; //pointer for the sliding window
	unsigned long long last_ns; //ns for last sample
	long long last_sample_jiffies; //in number of jiffies
};

/*
 * manages the class status
 * there should be only one instance of this object for each class in the whole system  
 */
struct ckrm_cpu_class {
	struct ckrm_core_class *core;
	struct ckrm_core_class *parent;
	struct ckrm_shares shares;
	spinlock_t cnt_lock;	// always grab parent's lock first and then child's
	struct ckrm_cpu_class_stat stat;
	struct list_head links;	// for linking up in cpu classes
	ckrm_lrq_t local_queues[NR_CPUS];	// runqueues 
	struct ckrm_usage usage;
	unsigned long magic;	//for debugging
};

#define cpu_class_weight(cls) (cls->stat.meshare)
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

	for (i=0; i < USAGE_WINDOW_SIZE; i++)
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
	if (usage->sample_pointer >= USAGE_WINDOW_SIZE)
		usage->sample_pointer = 0;
}

//duration is specified in number of jiffies
//return the usage in percentage
static inline int get_ckrm_usage(struct ckrm_cpu_class* clsptr, int duration)
{
	int nr_samples = duration/USAGE_SAMPLE_FREQ?:1;
	struct ckrm_usage* usage = &clsptr->usage;
	unsigned long long total = 0;
	int i, idx;

	if (nr_samples > USAGE_WINDOW_SIZE)
		nr_samples = USAGE_WINDOW_SIZE;

	idx = usage->sample_pointer;	
	for (i = 0; i< nr_samples; i++) {
		if (! idx)
			idx = USAGE_WINDOW_SIZE;
		idx --;
		total += usage->samples[idx];
	}
        total *= 100;
        do_div(total,nr_samples);
        do_div(total,NS_PER_SAMPLE);
	do_div(total,cpus_weight(cpu_online_map));
        return total;
}


#define lrq_nr_running(lrq) \
             (lrq->active->nr_active + lrq->expired->nr_active)

static inline ckrm_lrq_t *
get_ckrm_lrq(struct ckrm_cpu_class*cls, int cpu)
{
	return &(cls->local_queues[cpu]);
}

static inline ckrm_lrq_t *get_task_lrq(struct task_struct *p)
{
	return &(p->cpu_class->local_queues[task_cpu(p)]);
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
void ckrm_cpu_monitor(void);
int ckrm_cpu_monitor_init(void);
void ckrm_cpu_stat_init(struct ckrm_cpu_class_stat *stat);
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
 * A class with 5% share, can execute 50M nsecs / per sec ~ 2^28.
 * It's share will be set to 512 = 2^9. The globl CLASSQUEUE_SIZE is set to 2^7.
 * With CLASS_QUANTIZER=16, the local_cvt of this class will increase
 * by 2^28/2^9 = 2^19 = 512K.
 * Setting CLASS_QUANTIZER to 16, 2^(19-16) = 8 slots / per second.
 * A class with 5% shares, will cover 80 slots / per second.
 *
 * PRIORITY_QUANTIZER:
 *
 * How much can top priorities of class impact slot bonus.
 * There are 40 nice priorities. "2" will allow upto 10 slots improvement
 * in the RQ thus for 50% class it can perform ~1sec starvation.
 *
 *******************************************************************/

#define CLASS_QUANTIZER 16 	//shift from ns to increase class bonus
#define PRIORITY_QUANTIZER 2	//controls how much a high prio task can borrow

#define CKRM_SHARE_ACCURACY 10
#define NSEC_PER_MS 1000000
#define NSEC_PER_JIFFIES (NSEC_PER_SEC/HZ)


#define MAX_SAVINGS_ABSOLUTE (10LLU*NSEC_PER_SEC)  // 10 seconds

#define CVT_UPDATE_TICK     ((HZ/2)?:1)

// ABSOLUTE_CKRM_TUNING determines whether classes can make up
// lost time in absolute time or in relative values

#define ABSOLUTE_CKRM_TUNING         // preferred due to more predictable behavior

#ifdef ABSOLUTE_CKRM_TUNING

#define MAX_SAVINGS        MAX_SAVINGS_ABSOLUTE
//an absolute bonus of 200ms for classes when reactivated
#define INTERACTIVE_BONUS(lrq) ((200*NSEC_PER_MS)/local_class_weight(lrq))
#define SAVINGS_LEAK_SPEED (CVT_UPDATE_TICK/10*NSEC_PER_JIFFIES)

#define scale_cvt(val,lrq)   ((val)*local_class_weight(lrq))
#define unscale_cvt(val,lrq) (do_div(val,local_class_weight(lrq)))

#else

#define MAX_SAVINGS (MAX_SAVINGS_ABSOLUTE >> CKRM_SHARE_ACCURACY) 
/*
 * to improve system responsiveness
 * an inactive class is put a little bit ahead of the current class when it wakes up
 * the amount is set in normalized termis to simplify the calculation
 * for class with 100% share, it can be 2s ahead
 * while for class with 10% share, it can be 200ms ahead
 */
#define INTERACTIVE_BONUS(lrq) (2*NSEC_PER_MS)  

/*
 * normalized savings can't be more than MAX_NORMALIZED_SAVINGS
 * based on the current configuration
 * this means that a class with share 100% will accumulate 10s at most
 * while a class with 1% of the share can only accumulate 100ms
 */

//a class with share 100% can get 100ms every 500ms
//while a class with share 10% can only get 10ms every 500ms
#define SAVINGS_LEAK_SPEED ((CVT_UPDATE_TICK/5*NSEC_PER_JIFFIES) >> CKRM_SHARE_ACCURACY)

#define scale_cvt(val,lrq)   (val)
#define unscale_cvt(val,lrq) (val)

#endif


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
	prio += lrq->top_priority >> PRIORITY_QUANTIZER; // queue urgency

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
 * runqueue load is the local_weight of all the classes on this cpu
 * must be called with class_list_lock held
 */
static inline unsigned long ckrm_cpu_load(int cpu)
{
	struct ckrm_cpu_class *clsptr;
	ckrm_lrq_t* lrq;
	struct ckrm_cpu_demand_stat* l_stat;
	int total_load = 0;
	int load;

	list_for_each_entry(clsptr,&active_cpu_classes,links) {
		lrq =  get_ckrm_lrq(clsptr,cpu);
		l_stat = get_cls_local_stat(clsptr,cpu);
		load = lrq->local_weight;
		if (l_stat->cpu_demand < load)
			load = l_stat->cpu_demand;
		total_load += load;
	}	
	return total_load;
}

static inline void class_enqueue_task(struct task_struct *p,
				      prio_array_t * array)
{
	ckrm_lrq_t *lrq;
	int effective_prio;

	lrq = get_task_lrq(p);

	cpu_demand_event(&p->demand_stat,CPU_DEMAND_ENQUEUE,0);
	lrq->lrq_load += task_load(p);

	if ((p->prio < lrq->top_priority) && (array == lrq->active))
		set_top_priority(lrq, p->prio);	

	if (! cls_in_classqueue(&lrq->classqueue_linkobj)) {
		cpu_demand_event(get_task_lrq_stat(p),CPU_DEMAND_ENQUEUE,0);
		effective_prio = get_effective_prio(lrq);
		classqueue_enqueue(lrq->classqueue, &lrq->classqueue_linkobj, effective_prio);
	} 

}

static inline void class_dequeue_task(struct task_struct *p,
				      prio_array_t * array)
{
	ckrm_lrq_t *lrq = get_task_lrq(p);
	unsigned long load = task_load(p);

	BUG_ON(lrq->lrq_load < load);
	lrq->lrq_load -= load;

	cpu_demand_event(&p->demand_stat,CPU_DEMAND_DEQUEUE,0);

	if ((array == lrq->active) && (p->prio == lrq->top_priority)
	    && list_empty(&(array->queue[p->prio])))
		set_top_priority(lrq,
				 find_next_bit(array->bitmap, MAX_PRIO,
					       p->prio));
}

/*
 *  called after a task is switched out. Update the local cvt accounting 
 *  we need to stick with long instead of long long due to nonexistent 64-bit division
 */
static inline void update_local_cvt(struct task_struct *p, unsigned long nsec)
{
	ckrm_lrq_t * lrq = get_task_lrq(p);

	unsigned long cvt_inc = nsec / local_class_weight(lrq);

	lrq->local_cvt += cvt_inc;
	lrq->uncounted_ns += nsec;

	update_class_priority(lrq);
}

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

void update_class_cputime(int this_cpu);

/**********************************************/
/*          PID_LOAD_BALANCING                */
/**********************************************/
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
long pid_get_pressure(ckrm_load_t* ckrm_load, int local_group);
#define rq_ckrm_load(rq) (&((rq)->ckrm_load))

static inline void ckrm_sched_tick(unsigned long j,int this_cpu,struct ckrm_load_struct* ckrm_load)
{
	read_lock(&class_list_lock);
       
#ifdef CONFIG_SMP
	ckrm_load_sample(ckrm_load,this_cpu);
#endif

	if (! (j % CVT_UPDATE_TICK)) {
		//		printk("ckrm_sched j=%lu\n",j);
		classqueue_update_base(get_cpu_classqueue(this_cpu));
		update_class_cputime(this_cpu);
	}

	read_unlock(&class_list_lock);
}

#endif //CONFIG_CKRM_CPU_SCHEDULE

#endif
