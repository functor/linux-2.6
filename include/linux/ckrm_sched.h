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

#define CC_BUG_ON_DO(cond,action)  do { if (cond)  action; BUG_ON(cond); } while(0)
#define CC_BUG_ON(cond)            BUG_ON(cond)

#include <linux/sched.h>
#include <linux/ckrm_rc.h>
#include <linux/ckrm_classqueue.h>

//update every second
#define CVT_UPDATE_TICK     (1*HZ/1 ?: 1)
#define CLASS_BONUS_RATE 22	// shift from ns to increase class bonus
#define PRIORITY_BONUS_RATE 0	// ??  Hubertus

#define BITMAP_SIZE ((((MAX_PRIO+1+7)/8)+sizeof(long)-1)/sizeof(long))
struct prio_array {
	int nr_active;
	unsigned long bitmap[BITMAP_SIZE];
	struct list_head queue[MAX_PRIO];
};

struct ckrm_local_runqueue {
	cq_node_t classqueue_linkobj;	/*links in classqueue */
	struct ckrm_cpu_class *cpu_class;	// class it belongs to
	struct classqueue_struct *classqueue;	// classqueue it belongs tow
	CVT_t uncounted_cvt;
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
	CVT_t local_cvt;	// snapshot of local_cvt, update on every loadbalance
	unsigned long magic;	//for debugging
};

/**
 * @last_sleep: the last time it sleeps, last_sleep = 0 when not sleeping
 */
struct ckrm_cpu_class_local_stat {
	unsigned long long run;
	unsigned long long total;
	unsigned long long last_sleep;
	unsigned long cpu_demand; /*estimated cpu demand */
};

/**
 * ckrm_cpu_class_stat - cpu usage statistics maintained for each class
 * 
 */
struct ckrm_cpu_class_stat {
	spinlock_t stat_lock;

	unsigned long long total_ns;	/*how much nano-secs it has consumed */

	struct ckrm_cpu_class_local_stat local_stats[NR_CPUS];
	unsigned long cpu_demand;

	/*temp stat used by cpu monitor */
	int effective_guarantee;
	int effective_limit;
	int glut;		//true or false
	/*
	 * effective_share: for both default class and its children
	 * self_effective_share: just for the default class
	 */
	int effective_share;
	int self_effective_share;
};

typedef struct ckrm_cpu_class_stat ckrm_stat_t;

/*
 * manages the class status
 * there should be only one instance of this object for each class in the whole system  
 */
struct ckrm_cpu_class {
	struct ckrm_core_class *core;
	struct ckrm_core_class *parent;
	struct ckrm_shares shares;
	spinlock_t cnt_lock;	// always grab parent's lock first and then child's
	CVT_t global_cvt;	// total cummulative virtual time
	struct ckrm_cpu_class_stat stat;
	struct list_head links;	// for linking up in cpu classes
	struct ckrm_local_runqueue local_queues[NR_CPUS];	// runqueues 
};

#if CONFIG_CKRM_CPU_SCHEDULE
#define rq_active(p,rq)   (get_task_class_queue(p)->active)
#define rq_expired(p,rq)  (get_task_class_queue(p)->expired)
#else
#define rq_active(p,rq)   (rq->active)
#define rq_expired(p,rq)  (rq->expired)
#endif

//#define cpu_class_weight(cls) (cls->shares.my_guarantee)
#define cpu_class_weight(cls) (cls->stat.self_effective_share)

#define bpt_queue(cpu) (& (cpu_rq(cpu)->classqueue) )
CVT_t get_min_cvt(int cpu);

struct classqueue_struct *get_cpu_classqueue(int cpu);

extern struct ckrm_cpu_class default_cpu_class_obj;
#define default_cpu_class (&default_cpu_class_obj)

#define local_queue_nr_running(local_queue) \
             (local_queue->active->nr_active + local_queue->expired->nr_active)

static inline struct ckrm_local_runqueue *
get_ckrm_local_runqueue(struct ckrm_cpu_class*cls, int cpu)
{
	return &(cls->local_queues[cpu]);
}

static inline struct ckrm_local_runqueue *get_task_class_queue(struct task_struct *p)
{
	return &(p->cpu_class->local_queues[task_cpu(p)]);
}

#define task_list_entry(list)  list_entry(list,struct task_struct,run_list)
#define class_list_entry(list) list_entry(list,struct ckrm_local_runqueue,classqueue_linkobj)

/* some additional interfaces exported from sched.c */
struct runqueue;
void dequeue_task(struct task_struct *p, prio_array_t * array);
void enqueue_task(struct task_struct *p, prio_array_t * array);
struct runqueue *task_rq_lock(task_t * p, unsigned long *flags);
void task_rq_unlock(struct runqueue *rq, unsigned long *flags);
extern spinlock_t cvt_lock;
extern rwlock_t class_list_lock;
extern struct list_head active_cpu_classes;

/*functions exported by ckrm_cpu_class.c*/
int __init init_ckrm_sched_res(void);
void init_cpu_classes(void);

/*functions exported by ckrm_cpu_monitor.c*/
void ckrm_cpu_monitor(void);
void ckrm_cpu_stat_init(struct ckrm_cpu_class_stat *stat);
#define CPU_DEMAND_ENQUEUE 0
#define CPU_DEMAND_DEQUEUE 1
#define CPU_DEMAND_DESCHEDULE 2
void cpu_demand_event(struct ckrm_cpu_class_local_stat* local_stat, int event, unsigned long long len);

#define get_task_local_stat(p) (&(p)->cpu_class->stat.local_stats[task_cpu(p)])
#define get_rq_local_stat(lrq,cpu) (&(lrq)->cpu_class->stat.local_stats[cpu])

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
static inline int get_effective_prio(struct ckrm_local_runqueue * lcq)
{
	int prio;

	// cumulative usage
	prio = lcq->local_cvt >> CLASS_BONUS_RATE;
	// queue urgency
	prio += lcq->top_priority >> PRIORITY_BONUS_RATE;

	return prio;
}

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
 *   -- update_global_cvt
 */
static inline void update_class_priority(struct ckrm_local_runqueue *local_rq)
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
static inline void set_top_priority(struct ckrm_local_runqueue *class_queue,
				    int new_priority)
{
	class_queue->top_priority = new_priority;
	update_class_priority(class_queue);
}

static inline void class_enqueue_task(struct task_struct *p,
				      prio_array_t * array)
{
	struct ckrm_local_runqueue *queue;
	int effective_prio;

	queue = get_task_class_queue(p);

	if (! cls_in_classqueue(&queue->classqueue_linkobj)) {
		cpu_demand_event(get_task_local_stat(p),CPU_DEMAND_ENQUEUE,0);
		/*make sure the cvt of this class is up to date*/
		queue->local_cvt = get_min_cvt(task_cpu(p));
		effective_prio = get_effective_prio(queue);
		classqueue_enqueue(queue->classqueue, &queue->classqueue_linkobj, effective_prio);
	} 
	
	if ((p->prio < queue->top_priority) && (array == queue->active))
		set_top_priority(queue, p->prio);	

}

static inline void class_dequeue_task(struct task_struct *p,
				      prio_array_t * array)
{
	struct ckrm_local_runqueue *queue = get_task_class_queue(p);

	if ((array == queue->active) && (p->prio == queue->top_priority)
	    && list_empty(&(array->queue[p->prio])))
		set_top_priority(queue,
				 find_next_bit(array->bitmap, MAX_PRIO,
					       p->prio));
}

/*
 *  called after a task is switched out. Update the local cvt accounting 
 *  we need to stick with long instead of long long due to nonexistent 64-bit division
 */
static inline void update_local_cvt(struct task_struct *p, unsigned long nsec)
{
	struct ckrm_local_runqueue *class_queue = get_task_class_queue(p);
	struct ckrm_cpu_class *cls = class_queue->cpu_class;

	unsigned long cvt_inc = nsec / cpu_class_weight(cls);

	class_queue->local_cvt += cvt_inc;
	class_queue->uncounted_cvt += cvt_inc;

	class_queue->uncounted_ns += nsec;
	update_class_priority(class_queue);
}

/*
 * called during loadbalancing 
 * to charge the class with locally accumulated cvt
 */
void update_global_cvts(int this_cpu);

/**
 * 
 */
static inline int class_preempts_curr(struct task_struct * p, struct task_struct* curr)
{
	struct cq_node_struct* node1 = &(get_task_class_queue(p)->classqueue_linkobj);
	struct cq_node_struct* node2 = &(get_task_class_queue(curr)->classqueue_linkobj);

	return (class_compare_prio(node1,node2) < 0);
}
#endif
