/* ckrm_cpu_monitor.c - Hierarchical CKRM CPU Resource Monitor
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

/* Changes
 * 
 * 23 June 2004: Created
 * 
 */
#include <linux/module.h>
#include <linux/init.h>
#include <asm/errno.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/ckrm.h>
#include <linux/ckrm_rc.h>
#include <linux/ckrm_tc.h>
#include <asm/div64.h>
#include <linux/ckrm_sched.h>

#define CPU_MONITOR_INTERVAL (4*HZ) /*how often do we adjust the shares*/
#define CKRM_SHARE_ACCURACY 7
#define CKRM_SHARE_MAX (1<<CKRM_SHARE_ACCURACY)

extern struct ckrm_cpu_class *ckrm_get_cpu_class(struct ckrm_core_class *core);

void ckrm_cpu_stat_init(struct ckrm_cpu_class_stat *stat)
{
	int i;
	struct ckrm_cpu_class_local_stat* local_stat;
	unsigned long long now = sched_clock();

	stat->stat_lock = SPIN_LOCK_UNLOCKED;
	stat->total_ns = 0;
	stat->cpu_demand = 0;

	for (i=0; i< NR_CPUS; i++) {
		local_stat = &stat->local_stats[i];
		local_stat->run = 0;
		local_stat->total = 0;
		local_stat->last_sleep = now;
		local_stat->cpu_demand = 0;		
	}

	stat->effective_guarantee = 0;
	stat->effective_limit = 0;
	stat->glut = 0;
	stat->effective_share = 100;
	stat->self_effective_share = 100;
}
/**********************************************/
/*          cpu demand                        */
/**********************************************/

/*
 * How CPU demand is calculated:
 * consider class local runqueue (clr) first
 * at any time, a clr can at the following three states
 * -- run: a task belonning to this class is running on this cpu
 * -- wait: at least one of its task is running, but the class is not running
 * -- sleep: none of the task of this class is runnable
 *
 * cpu_demand(t1,t2) = r(t1,t2)/(r(t1,t2)+s(t1,t2))
 * 
 * the cpu_demand of a class = 
 *    sum of cpu_demand of all the class local runqueues
 */

/**
 * update_cpu_demand - update a state change
 * 
 * should be called whenever the state of a local queue changes
 * -- when deschedule : report how much run
 * -- when enqueue: report how much sleep
 *
 * to deal with excessive long run/sleep state
 * -- whenever the the ckrm_cpu_monitor is called, check if the class is in sleep state, if yes, then update sleep record
 */
#define CKRM_CPU_DEMAND_RUN 0
#define CKRM_CPU_DEMAND_SLEEP 1
//how often should we recalculate the cpu demand, in ns
#define CPU_DEMAND_CAL_THRESHOLD (1000000000LL)
static inline void update_local_cpu_demand(struct ckrm_cpu_class_local_stat* local_stat,int state, unsigned long long len)
{	
	local_stat->total += len;
	if (state == CKRM_CPU_DEMAND_RUN)
		local_stat->run += len;

	if (local_stat->total >= CPU_DEMAND_CAL_THRESHOLD) {
		local_stat->total >>= CKRM_SHARE_ACCURACY;
		if (local_stat->total > 0xFFFFFFFF)
			local_stat->total = 0xFFFFFFFF;

		do_div(local_stat->run,(unsigned long)local_stat->total);
		local_stat->cpu_demand +=local_stat->run;
		local_stat->cpu_demand >>= 1;
		local_stat->total = 0;
		local_stat->run = 0;
	}
}

static inline void cpu_demand_update_run(struct ckrm_cpu_class_local_stat* local_stat, unsigned long long len)
{
	update_local_cpu_demand(local_stat,CKRM_CPU_DEMAND_RUN,len);
}

static inline void cpu_demand_update_sleep(struct ckrm_cpu_class_local_stat* local_stat, unsigned long long len)
{
	update_local_cpu_demand(local_stat,CKRM_CPU_DEMAND_SLEEP,len);
}

#define CPU_DEMAND_ENQUEUE 0
#define CPU_DEMAND_DEQUEUE 1
#define CPU_DEMAND_DESCHEDULE 2

/**
 * cpu_demand_event - and cpu_demand event occured
 * @event: one of the following three events:
 *   CPU_DEMAND_ENQUEUE: local class enqueue
 *   CPU_DEMAND_DEQUEUE: local class dequeue
 *   CPU_DEMAND_DESCHEDULE: one task belong a certain local class deschedule
 * @len: valid only for CPU_DEMAND_DESCHEDULE, how long the task has been run
 */
void cpu_demand_event(struct ckrm_cpu_class_local_stat* local_stat, int event, unsigned long long len) 
{	
	switch (event) {
	case CPU_DEMAND_ENQUEUE: 
		len = sched_clock() - local_stat->last_sleep;
		local_stat->last_sleep = 0;
		cpu_demand_update_sleep(local_stat,len);
		break;
	case CPU_DEMAND_DEQUEUE:
		local_stat->last_sleep = sched_clock();
		break;
	case CPU_DEMAND_DESCHEDULE:
		cpu_demand_update_run(local_stat,len);		
		break;
	default:
		BUG();
	}
}

/** 
 * check all the class local queue
 * if local queueu is not in runqueue, then it's in sleep state
 * if compare to last sleep, 
 */
static inline void cpu_demand_check_sleep(struct ckrm_cpu_class_stat *stat, int cpu)
{
	struct ckrm_cpu_class_local_stat * local_stat = &stat->local_stats[cpu];
	unsigned long long sleep,now;
	if (local_stat->last_sleep) {
		now = sched_clock();
		sleep = now - local_stat->last_sleep;
		local_stat->last_sleep = now;
		cpu_demand_update_sleep(local_stat,sleep);
	}
}

/**
 *get_self_cpu_demand - get cpu demand of the class itself (excluding children)
 *
 * self_cpu_demand = sum(cpu demand of all local queues) 
 */
static unsigned long get_self_cpu_demand(struct ckrm_cpu_class_stat
						*stat)
{
	int cpu_demand = 0;
	int i;

	for_each_online_cpu(i) {
		cpu_demand_check_sleep(stat,i);
		cpu_demand += stat->local_stats[i].cpu_demand;
	}

	if (cpu_demand > CKRM_SHARE_MAX)
		cpu_demand = CKRM_SHARE_MAX;
	return cpu_demand;
}

/*
 * update effective cpu demand for each class
 * assume the root_core->parent == NULL
 */
static void update_cpu_demand(struct ckrm_core_class *root_core)
{
	struct ckrm_core_class *cur_core, *child_core;
	struct ckrm_cpu_class *cls;

	cur_core = root_core;
	child_core = NULL;
	/*
	 * iterate the tree
	 * update cpu_demand of each node
	 */
      repeat:
	if (!cur_core)
		return;

	cls = ckrm_get_cpu_class(cur_core);
	if (!child_core)	//first child
		cls->stat.cpu_demand = get_self_cpu_demand(&cls->stat);
	else {
		cls->stat.cpu_demand +=
		    ckrm_get_cpu_class(child_core)->stat.cpu_demand;
		if (cls->stat.cpu_demand > CKRM_SHARE_MAX)
			cls->stat.cpu_demand = CKRM_SHARE_MAX;
	}

	//next child
	child_core = ckrm_get_next_child(cur_core, child_core);
	if (child_core) {
		//go down
		cur_core = child_core;
		child_core = NULL;
		goto repeat;
	} else {		//no more child, go back
		child_core = cur_core;
		cur_core = child_core->hnode.parent;
	}
	goto repeat;
}

/**********************************************/
/*          effective guarantee & limit       */
/**********************************************/
static inline void set_effective_share(struct ckrm_cpu_class_stat *stat,
				       int new_share)
{
	if (!new_share)
		new_share = 1;
	stat->effective_share = new_share;
}

static inline void set_self_effective_share(struct ckrm_cpu_class_stat *stat,
					    int new_share)
{
	if (!new_share)
		new_share = 1;
	stat->self_effective_share = new_share;
}

static inline void update_child_effective(struct ckrm_core_class *parent)
{
	struct ckrm_cpu_class *p_cls = ckrm_get_cpu_class(parent);
	struct ckrm_core_class *child_core = ckrm_get_next_child(parent, NULL);

	while (child_core) {
		struct ckrm_cpu_class *c_cls = ckrm_get_cpu_class(child_core);

		c_cls->stat.effective_guarantee =
		    p_cls->stat.effective_guarantee *
		    c_cls->shares.my_guarantee / p_cls->shares.total_guarantee;
		c_cls->stat.effective_limit =
		    p_cls->stat.effective_guarantee * c_cls->shares.my_limit /
		    p_cls->shares.total_guarantee;

		child_core = ckrm_get_next_child(parent, child_core);
	};

}

/*
 * update effective guarantee and effective limit
 * -- effective share = parent->effective->share * share/parent->total_share
 * -- effective limit = parent->effective->share * limit/parent->total_share
 * should be called only when class structure changed
 */
static void update_effective_guarantee_limit(struct ckrm_core_class *root_core)
{
	struct ckrm_core_class *cur_core, *child_core = NULL;
	struct ckrm_cpu_class *cls;

	cur_core = root_core;
	cls = ckrm_get_cpu_class(cur_core);
	cls->stat.effective_guarantee = CKRM_SHARE_MAX;
	cls->stat.effective_limit = cls->stat.effective_guarantee;

      repeat:
	//check exit
	if (!cur_core)
		return;

	//visit this node
	update_child_effective(cur_core);
	//next child
	child_core = ckrm_get_next_child(cur_core, child_core);
	if (child_core) {
		//go down
		cur_core = child_core;
		child_core = NULL;
		goto repeat;
	} else {		//no more child, go back
		child_core = cur_core;
		cur_core = child_core->hnode.parent;
	}
	goto repeat;
}

/**********************************************/
/*          surplus allocation                */
/**********************************************/

/*
 * surplus = my_effective_share - demand
 * if surplus < 0, surplus = 0 
 */
static inline int get_node_surplus(struct ckrm_cpu_class *cls)
{
	int surplus = cls->stat.effective_guarantee - cls->stat.cpu_demand;

	if (surplus < 0)
		surplus = 0;

	return surplus;
}

/*
 * consume the surplus
 * return how much consumed
 * set glut when necessary
 */
static inline int node_surplus_consume(int old_surplus,
				       struct ckrm_core_class *child_core,
				       struct ckrm_cpu_class *p_cls)
{
	int consumed = 0;
	int inc_limit;

	struct ckrm_cpu_class *c_cls = ckrm_get_cpu_class(child_core);

	if (c_cls->stat.glut)
		goto out;

	//check demand
	if (c_cls->stat.effective_share >= c_cls->stat.cpu_demand) {
		c_cls->stat.glut = 1;
		goto out;
	}

	consumed =
	    old_surplus * c_cls->shares.my_guarantee /
	    p_cls->shares.total_guarantee;

	//check limit
	inc_limit = c_cls->stat.effective_limit - c_cls->stat.effective_share;
	if (inc_limit <= consumed) {
		c_cls->stat.glut = 1;
		consumed = inc_limit;
	}

	c_cls->stat.effective_share += consumed;
      out:
	return consumed;
}

/*
 * re-allocate the shares for all the childs under this node
 * task:
 *  1. get total surplus
 *  2. allocate surplus
 *  3. set the effective_share of each node
 */
static void alloc_surplus_node(struct ckrm_core_class *parent)
{
	int total_surplus = 0, old_surplus = 0;
	struct ckrm_cpu_class *p_cls = ckrm_get_cpu_class(parent);
	struct ckrm_core_class *child_core = NULL;
	int self_share;

	/*
	 * calculate surplus 
	 * total_surplus = sum(child_surplus)
	 * reset glut flag
	 * initialize effective_share
	 */
	do {
		child_core = ckrm_get_next_child(parent, child_core);
		if (child_core) {
			struct ckrm_cpu_class *c_cls =
			    ckrm_get_cpu_class(child_core);
			ckrm_stat_t *stat = &c_cls->stat;

			total_surplus += get_node_surplus(c_cls);
			stat->glut = 0;
			set_effective_share(stat, stat->effective_guarantee);
		}
	} while (child_core);

	/*distribute the surplus */
	child_core = NULL;
	do {
		if (!child_core)	//keep the surplus of last round
			old_surplus = total_surplus;

		child_core = ckrm_get_next_child(parent, child_core);
		if (child_core) {
			total_surplus -=
			    node_surplus_consume(old_surplus, child_core,
						 p_cls);
		}
		//start a new round if something is allocated in the last round
	} while (child_core || (total_surplus != old_surplus));

	//any remaining surplus goes to the default class
	self_share = p_cls->stat.effective_share *
	    p_cls->shares.unused_guarantee / p_cls->shares.total_guarantee;
	self_share += total_surplus;

	set_self_effective_share(&p_cls->stat, self_share);
}

/**
 * alloc_surplus - reallocate unused shares
 *
 * class A's usused share should be allocated to its siblings
 */
static void alloc_surplus(struct ckrm_core_class *root_core)
{
	struct ckrm_core_class *cur_core, *child_core = NULL;
	struct ckrm_cpu_class *cls;

	cur_core = root_core;
	cls = ckrm_get_cpu_class(cur_core);
	cls->stat.glut = 0;
	set_effective_share(&cls->stat, cls->stat.effective_guarantee);
      repeat:
	//check exit
	if (!cur_core)
		return;

	//visit this node
	alloc_surplus_node(cur_core);
	//next child
	child_core = ckrm_get_next_child(cur_core, child_core);
	if (child_core) {
		//go down
		cur_core = child_core;
		child_core = NULL;
		goto repeat;
	} else {		//no more child, go back
		child_core = cur_core;
		cur_core = child_core->hnode.parent;
	}
	goto repeat;
}

/**
 *ckrm_cpu_monitor - adjust relative shares of the classes based on their progress
 *
 * this function is called every CPU_MONITOR_INTERVAL
 * it computes the cpu demand of each class
 * and re-allocate the un-used shares to other classes
 */
void ckrm_cpu_monitor(void)
{
	struct ckrm_core_class *root_core = default_cpu_class->core;
	if (!root_core)
		return;

	update_effective_guarantee_limit(root_core);
	update_cpu_demand(root_core);
	alloc_surplus(root_core);
}

/*****************************************************/
/*            Supporting Functions                   */
/*****************************************************/
static pid_t cpu_monitor_pid = -1;
static int thread_exit = 0;

static int ckrm_cpu_monitord(void *nothing)
{
	wait_queue_head_t wait;

	init_waitqueue_head(&wait);

	daemonize("ckrm_cpu_ctrld");
	for (;;) {
		/*sleep for sometime before next try*/
		interruptible_sleep_on_timeout(&wait, CPU_MONITOR_INTERVAL);
		ckrm_cpu_monitor();
		if (thread_exit) {
			break;
		}
	}
	cpu_monitor_pid = -1;
	thread_exit = 2;
	printk("cpu_monitord exit\n");
	return 0;
}

void ckrm_start_monitor(void)
{
	cpu_monitor_pid = kernel_thread(ckrm_cpu_monitord, 0, CLONE_KERNEL);
	if (cpu_monitor_pid < 0) {
		printk("ckrm_cpu_monitord for failed\n");
	}
}

void ckrm_kill_monitor(void)
{
	wait_queue_head_t wait;
	int interval = HZ;
	init_waitqueue_head(&wait);

	printk("killing process %d\n", cpu_monitor_pid);
	if (cpu_monitor_pid > 0) {
		thread_exit = 1;
		while (thread_exit != 2) {
			interruptible_sleep_on_timeout(&wait, interval);
		}
	}
}

int ckrm_cpu_monitor_init(void)
{
	ckrm_start_monitor();
	return 0;
}

void ckrm_cpu_monitor_exit(void)
{
	ckrm_kill_monitor();
}

module_init(ckrm_cpu_monitor_init);
module_exit(ckrm_cpu_monitor_exit);

MODULE_AUTHOR("Haoqiang Zheng <hzheng@cs.columbia.edu>");
MODULE_DESCRIPTION("Hierarchical CKRM CPU Resource Monitor");
MODULE_LICENSE("GPL");
