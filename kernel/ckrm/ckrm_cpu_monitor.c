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

// #define CONFIG_CKRM_SUPPORT_MAXLIMITS

#define CPU_MONITOR_INTERVAL (HZ) /*how often do we adjust the shares*/

#define CKRM_CPU_DEMAND_RUN 0
#define CKRM_CPU_DEMAND_SLEEP 1
//sample task cpu demand every 32ms
#define CPU_DEMAND_TASK_RECALC  ( 32*1000*1000LL)
#define CPU_DEMAND_CLASS_RECALC (256*1000*1000LL)
#define CPU_DEMAND_TP_CLASS 0
#define CPU_DEMAND_TP_TASK 1

static void update_ckrm_idle(unsigned long surplus);

void cpu_demand_check_sleep(struct ckrm_cpu_class_stat *stat, int cpu);
int alloc_surplus(struct ckrm_core_class *root_core);
extern struct ckrm_cpu_class *ckrm_get_cpu_class(struct ckrm_core_class *core);

/*interface to share definition*/
static inline int get_my_grt(struct ckrm_cpu_class *cls)
{
	return cls->shares.unused_guarantee;
}

static inline int get_soft_limit(struct ckrm_cpu_class *cls)
{
	return cls->shares.my_limit;
}

static inline int get_mysoft_limit(struct ckrm_cpu_class *cls)
{
	return cls->shares.total_guarantee;
}

static inline int get_hard_limit(struct ckrm_cpu_class *cls)
{
	return cls->shares.total_guarantee;
}

static inline int get_myhard_limit(struct ckrm_cpu_class *cls)
{
	return cls->shares.total_guarantee;
}

static inline void set_eshare(struct ckrm_cpu_class_stat *stat,
				       int new_share)
{
	if (!new_share)
		new_share = 1;

	BUG_ON(new_share < 0);
	stat->eshare = new_share;
}

static inline void set_meshare(struct ckrm_cpu_class_stat *stat,
					    int new_share)
{
	if (!new_share)
		new_share = 1;

	BUG_ON(new_share < 0);
	stat->meshare = new_share;
}

/**
 *get_self_cpu_demand - get cpu demand of the class itself (excluding children)
 *
 * self_cpu_demand = sum(cpu demand of all local queues) 
 */
static inline unsigned long get_self_cpu_demand(struct ckrm_cpu_class_stat *stat)
{
	int cpu_demand = 0;
	int i;
	int cpuonline = 0;

	for_each_online_cpu(i) {
		cpu_demand_check_sleep(stat,i);
		cpu_demand += stat->local_stats[i].cpu_demand;
		cpuonline ++;
	}

	return (cpu_demand/cpuonline);
}

/*
 * my max demand = min(cpu_demand, my effective hard limit)
 */
static inline unsigned long get_mmax_demand(struct ckrm_cpu_class_stat* stat) 
{
	unsigned long mmax_demand = get_self_cpu_demand(stat);
	if (mmax_demand > stat->mehl)
		mmax_demand = stat->mehl;

	return mmax_demand;
}

static inline void cpu_demand_stat_init(struct ckrm_cpu_demand_stat* local_stat, int type)
{
	unsigned long long now = sched_clock();

	local_stat->run = 0;
	local_stat->total = 0;
	local_stat->last_sleep = now;
	switch (type) {
	case CPU_DEMAND_TP_CLASS:
		local_stat->recalc_interval = CPU_DEMAND_CLASS_RECALC;
		local_stat->cpu_demand = 0; 
		break;
	case CPU_DEMAND_TP_TASK:
		local_stat->recalc_interval = CPU_DEMAND_TASK_RECALC;
		//for task, the init cpu_demand is copied from its parent
		break;
	default:
		BUG();
	}
}

void ckrm_cpu_stat_init(struct ckrm_cpu_class_stat *stat, int eshares)
{
	int i;

	stat->stat_lock = SPIN_LOCK_UNLOCKED;
	stat->total_ns = 0;
	stat->max_demand = 0;

	for (i=0; i<NR_CPUS; i++) {
		cpu_demand_stat_init(&stat->local_stats[i],CPU_DEMAND_TP_CLASS);
	}

	stat->egrt = 0;
	stat->megrt = 0;
	stat->ehl = CKRM_SHARE_MAX; /*default: no limit*/
	stat->mehl = CKRM_SHARE_MAX; /*default: no limit */

	stat->eshare = eshares;
	stat->meshare = eshares;

	stat->has_savings = 0;  
	stat->demand_per_share = 0;

}

#if 0  // keep handy for debugging if necessary
void ckrm_cpu_class_dump(struct ckrm_cpu_class *clsptr,int num)
{
	struct ckrm_cpu_class_stat* stat = &clsptr->stat;
	printk("%d> %p[%d] mg=%d lim=%d tg=%d maxlim=%d ug=%d\n",num,
		clsptr, (clsptr == get_default_cpu_class()),
	        clsptr->shares.my_guarantee, 
		clsptr->shares.my_limit, 
		clsptr->shares.total_guarantee,
	        clsptr->shares.max_limit, 
		clsptr->shares.unused_guarantee);
	printk("      egrt=%d megrt=%d ehl=%d mehl=%d esh=%d mesh=%d\n",
		stat->egrt,stat->megrt,stat->ehl,stat->mehl,
		stat->eshare,stat->meshare);
}
#endif

/**********************************************/
/*          surplus allocation                */
/**********************************************/

/*
 * surplus = egrt - demand
 * if surplus < 0, surplus = 0 
 */
static inline int get_node_surplus(struct ckrm_cpu_class *cls)
{
	int surplus = cls->stat.egrt - cls->stat.max_demand;

	if (surplus < 0)
		surplus = 0;

	return surplus;
}

/*
 * consume savings in advance because this class give surplus to others
 * this is a quick hack, should be integrated with balance_savings()
 */
static inline void consumed_surplus_savings(struct ckrm_cpu_class *clsptr, 
					    int savings_consumed) 
{
	long long total_savings;
	ckrm_lrq_t* lrq;
	int i;
	int cpu_online = 0;
	
	total_savings = 0;
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(clsptr,i);
		total_savings += lrq->savings;
		cpu_online ++;
	}
	
	total_savings -= savings_consumed;
	if (total_savings < 0)
		total_savings = 0;

	//get the average savings
	do_div(total_savings,cpu_online);	
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(clsptr,i);
		lrq->savings = total_savings;
	}
}

static inline int get_my_node_surplus(struct ckrm_cpu_class *cls)
{
	int surplus = cls->stat.megrt - get_mmax_demand(&cls->stat);
	int savings_consumed;

	if (surplus < 0)
		surplus = 0;

	/*
	 * a quick hack about the hierarchy savings distribution 
	 * may not be the right way to do
	 *
	 * since this node give its surplus to other nodes, 
	 * it's savings should be consumed
	 * suppose CPU_MONITOR_INTERVAL = (HZ) 
	 * savings_consumed is roughly how much savings will be consumed for the next second
	 */
	if (surplus) {
		savings_consumed = surplus * HZ * (NSEC_PER_MS >> CKRM_SHARE_SHIFT);
		consumed_surplus_savings(cls, savings_consumed) ;
	}

	return surplus;
}

/*
 * all the class in the queue consume the surplus in order
 * each class consume the amount propotional to its egrt
 */
static int consume_surplus_in_order(struct list_head* queue,
					   struct ckrm_cpu_class *p_cls,
					   int total_surplus)
{
	int total_grt = 0;
	struct ckrm_cpu_class *clsptr;	

	/*
	 * get total_grt of the classes in the queue
	 * total_grt can be maintained instead of re-calcuated each time
	 */
	list_for_each_entry(clsptr,queue,surplus_queue) {
		if (unlikely(clsptr == p_cls))
			total_grt += clsptr->stat.megrt;
		else
			total_grt += clsptr->stat.egrt;
	}

	if (! total_grt)
		goto consume_out;
	
	//allocate in order
	list_for_each_entry(clsptr,queue,surplus_queue) {		
		int surplus_per_share;
		int consumed, my_grt;

		BUG_ON(! total_grt);
		surplus_per_share = 
			(total_surplus << CKRM_SHARE_SHIFT) / total_grt;

		if (surplus_per_share <= 0)
			break;

		if (unlikely(clsptr == p_cls))  //self_node consuming
			my_grt =  clsptr->stat.megrt;
		else
			my_grt = clsptr->stat.egrt;

		BUG_ON(clsptr->stat.demand_per_share <= 0);

		if (clsptr->stat.demand_per_share < surplus_per_share)
			surplus_per_share = clsptr->stat.demand_per_share;

		consumed = surplus_per_share * my_grt;
		consumed >>= CKRM_SHARE_SHIFT;
		total_surplus -= consumed;
		BUG_ON(total_surplus < 0);
		total_grt -= my_grt;

		if (unlikely(clsptr == p_cls))
			set_meshare(&clsptr->stat,clsptr->stat.meshare + consumed);			
		else
			set_eshare(&clsptr->stat,clsptr->stat.eshare + consumed);
	}	
 consume_out:	
	if (total_surplus <= 1) //if total_suplus too small, no need to allocate again
		total_surplus = 0;
	return total_surplus;
}

/*
 * link all the children of parent and the parent itself using their surplus_queue field
 * link the whole queue using src_queue
 * if anything wrong return -1
 */
static int get_class_surplus_queue(struct ckrm_core_class *parent,
				   struct list_head* src_queue)
{
	struct ckrm_core_class *child_core = NULL;
	struct ckrm_cpu_class *p_cls,*c_cls;
	int ret = -1;

	p_cls = ckrm_get_cpu_class(parent);
	if (! p_cls)
		goto link_out;

	INIT_LIST_HEAD(src_queue);

	//add the parent node itself
	list_add(&p_cls->surplus_queue,src_queue);
	do {
		child_core = ckrm_get_next_child(parent, child_core);
		if (child_core) {
			c_cls = ckrm_get_cpu_class(child_core);				
			if (! c_cls)
				goto link_out;
			list_add(&c_cls->surplus_queue,src_queue);
		}
	} while (child_core);

	ret = 0;

 link_out:
	return ret;
}

/*
 * insert the class to queue based on stat->demand_per_share
 * status: tested
 */
static void insert_surplus_queue(struct list_head* queue, struct ckrm_cpu_class *clsptr)
{
	struct ckrm_cpu_class *cur_cls = NULL;	
	int end_of_queue = 1;

	list_for_each_entry(cur_cls,queue,surplus_queue) {
		if (cur_cls->stat.demand_per_share >= clsptr->stat.demand_per_share) {
			end_of_queue = 0;
			break;
		}
	}

	//insert the clsptr
	if (! cur_cls || end_of_queue)
		list_add_tail(&clsptr->surplus_queue,queue);
	else
		list_add_tail(&clsptr->surplus_queue,&cur_cls->surplus_queue);
}

/*
 * copy all classes in src_queue to dst_queue,
 * reorder the classes based on their normalized demand 
 * if a class already saturate (eshare >= demand), also remove it from src_queue
 * return the total guarantee of the selected classes
 *
 * @src_queue: source queue
 * @dst_queue: destination queue
 * @check_sl: check soft limit
 * @check_savings: only class has savings should be considered
 */

static unsigned long reorder_surplus_queue(struct list_head* src_queue, 
					   struct list_head* dst_queue, 
					   int check_sl, int check_savings, 
					   struct ckrm_cpu_class *p_cls) 
{
	struct ckrm_cpu_class *clsptr, *tmp;	

	INIT_LIST_HEAD(dst_queue);

	list_for_each_entry_safe(clsptr,tmp,src_queue,surplus_queue) {
		struct ckrm_cpu_class_stat* stat = &clsptr->stat;
		int inc_limit;
		int max_demand, eshare, esl,grt;

		if (unlikely(clsptr == p_cls)) {
			max_demand = get_mmax_demand(stat);
			eshare  = stat->meshare;
			esl = get_mysoft_limit(clsptr);
			grt = stat->megrt;
		} else {
			max_demand = stat->max_demand;
			eshare = stat->eshare;
			esl = get_soft_limit(clsptr);
			grt = stat->egrt;
		}

		//hard limit and demand limit
		inc_limit = max_demand - eshare;
		
		//no additional share needed
		if (inc_limit <= 0 || ! grt) {
			list_del(&clsptr->surplus_queue);
			continue;
		}
			
		//or no more savings
		if (check_savings && ! stat->has_savings)
			continue;
		
		//check soft limit
		if (check_sl) {
			int soft_limit;

			soft_limit = p_cls->stat.eshare * esl
				/ p_cls->shares.total_guarantee;

			if (soft_limit < max_demand)
				inc_limit = soft_limit - eshare;
			if ( inc_limit <= 0)   /* can turn negative */
				continue;
		}

		BUG_ON(! grt);
		//get the stat->demand_per_share
		stat->demand_per_share = 
			(inc_limit << CKRM_SHARE_SHIFT) / grt;	

		list_del_init(&clsptr->surplus_queue);
		//insert the class to the queue
		insert_surplus_queue(dst_queue,clsptr);
	}
	return 0;
}

/*
 * get all the surplus that should be reallocated to the children
 */
static inline int get_total_surplus(struct ckrm_cpu_class *p_cls,
				    struct ckrm_core_class *parent) 
{
	struct ckrm_cpu_class *c_cls;
	int total_surplus;
	struct ckrm_core_class *child_core = NULL;

	//additional share assigned to this sub node from parent
	total_surplus = p_cls->stat.eshare - p_cls->stat.egrt;
	BUG_ON(total_surplus < 0);

	//surplus of this node
	total_surplus += get_my_node_surplus(p_cls);
	do {
		child_core = ckrm_get_next_child(parent, child_core);
		if (child_core) {
			c_cls = ckrm_get_cpu_class(child_core);				
			if (! c_cls) {
				total_surplus = 0;
				break;
			}

			total_surplus += get_node_surplus(c_cls);			
		}
	} while (child_core);

	return total_surplus;
}
/**
 * alloc_surplus_node: re-allocate the shares for a single level
 * @parent: parent node
 * return the remaining surplus
 *
 * The surplus reallocation policy is like below.
 * -- the classes that have eshare >= demand don't need any additional share. 
 *     So they don't participate the surplus allocation.
 * -- all the other classes received share in this order:
 * 1. has savings, not over soft limit
 * 2. has savings, but over soft limit
 * 3. no savings, not over soft limit
 * 4. no savings, over soft limit
 * 
 * In each of the 4 levels above, classes get surplus propotionally to its guarantee
 */
static int alloc_surplus_node(struct ckrm_core_class *parent)
{
	struct ckrm_cpu_class *p_cls;
	int total_surplus;
	int ret = -1;
	struct list_head src_queue, dst_queue;

	p_cls = ckrm_get_cpu_class(parent);
	if (! p_cls) //safty check
		goto realloc_out;

	ret = 0;
	total_surplus = get_total_surplus(p_cls,parent);

	if (! total_surplus) //no surplus to be allocated 
		goto realloc_out;

	/* 
	 * first round, allocated to tasks with savings, check_sl
	 */
	get_class_surplus_queue(parent,&src_queue);
	reorder_surplus_queue(&src_queue, &dst_queue, 1, 1,p_cls);
	if (! list_empty(&dst_queue)) {
		total_surplus = consume_surplus_in_order(&dst_queue,p_cls,total_surplus);
		if (! total_surplus)
			goto realloc_out;
	}

	/* 
	 * second round, check savings, but no check_sl
	 */
	//merge the src_queue and dst_queue and reorder
	list_splice(&dst_queue, &src_queue);
	reorder_surplus_queue(&src_queue, &dst_queue, 0, 1,p_cls);
	if (! list_empty(&dst_queue)) {
		total_surplus = consume_surplus_in_order(&dst_queue,p_cls,total_surplus);
		if (! total_surplus)
			goto realloc_out;
	}

	/* 
	 * third round, no check savings, but check_sl
	 */
	//merge the src_queue and dst_queue and reorder
	list_splice(&dst_queue, &src_queue);
	reorder_surplus_queue(&src_queue, &dst_queue, 1, 0,p_cls);
	if (! list_empty(&dst_queue)) {
		total_surplus = consume_surplus_in_order(&dst_queue,p_cls,total_surplus);
		if (! total_surplus)
			goto realloc_out;
	}
	/* 
	 * fourth round, no check savings, no check_sl
	 */
	//merge the src_queue and dst_queue and reorder
	list_splice(&dst_queue, &src_queue);
	reorder_surplus_queue(&src_queue, &dst_queue, 0, 0,p_cls);
	if (! list_empty(&dst_queue))
		total_surplus = consume_surplus_in_order(&dst_queue,p_cls,total_surplus);	
	
 realloc_out:
	return ret;
}

/*
 * return true if the class total savings > MIN_SAVINGS 
 */
static int balance_local_savings(struct ckrm_cpu_class *clsptr, int cpu_online)
{
	unsigned long long total_savings;
	ckrm_lrq_t* lrq;
	int i;
#define CLASS_MIN_SAVINGS (10 * NSEC_PER_MS)
	
	total_savings = 0;
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(clsptr,i);
		total_savings += lrq->savings;
	}

	if (total_savings < CLASS_MIN_SAVINGS)
		return 0;

	//get the average savings
	do_div(total_savings,cpu_online);	
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(clsptr,i);
		lrq->savings = total_savings;
	}

	/*
	 * hzheng: this is another quick hack
	 * only say I have savings when this node has more demand
	 * ignoring the requirement of child classes
	 */
	if (clsptr->stat.megrt < get_mmax_demand(&clsptr->stat))
		return 1;
	else
		return 0;
}

/*
 * check savings status
 * set has_savings field if the class or its sub class has savings
 */
static void check_savings_status(struct ckrm_core_class *root_core)
{
	struct ckrm_cpu_class *clsptr;
	int cpu_online;

	cpu_online = cpus_weight(cpu_online_map);	

	//class status: demand, share,total_ns prio, index
	list_for_each_entry(clsptr,&active_cpu_classes,links) 
		clsptr->stat.has_savings = balance_local_savings(clsptr,cpu_online);
}

/**
 * alloc_surplus - reallocate unused shares
 *
 * class A's usused share should be allocated to its siblings
 * the re-allocation goes downward from the top
 */
int alloc_surplus(struct ckrm_core_class *root_core)
{
	struct ckrm_core_class *cur_core, *child_core;
	//	struct ckrm_cpu_class *cls;
	int ret = -1;

	check_savings_status(root_core);

	/*initialize*/
	cur_core = root_core;
	child_core = NULL;
	//	cls = ckrm_get_cpu_class(cur_core);

	/*the ckrm idle tasks get all what's remaining*/
	/*hzheng: uncomment the following like for hard limit support */
	//	update_ckrm_idle(CKRM_SHARE_MAX - cls->stat.max_demand);
	
 repeat:
	//check exit
	if (!cur_core)
		return 0;

	//visit this node only once
	if (! child_core) 
		if ( alloc_surplus_node(cur_core) < 0 )
			return ret;

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
 * update_cpu_demand_stat - 
 * 
 * should be called whenever the state of a task/task local queue changes
 * -- when deschedule : report how much run
 * -- when enqueue: report how much sleep
 *
 * how often should we recalculate the cpu demand
 * the number is in ns
 */
static inline void update_cpu_demand_stat(struct ckrm_cpu_demand_stat* local_stat,
					  int state, unsigned long long len)
{	
	local_stat->total += len;
	if (state == CKRM_CPU_DEMAND_RUN)
		local_stat->run += len;

	if (local_stat->total >= local_stat->recalc_interval) {
		local_stat->total >>= CKRM_SHARE_SHIFT;
		if (unlikely(local_stat->run > ULONG_MAX))
			local_stat->run = ULONG_MAX;

		if (unlikely(local_stat->total > ULONG_MAX))
			local_stat->total = ULONG_MAX;
			
		do_div(local_stat->run,(unsigned long)local_stat->total);

		if (unlikely(local_stat->total > ULONG_MAX)) {
			//happens after very long sleep
			local_stat->cpu_demand = local_stat->run;
		} else { 
			local_stat->cpu_demand = 
                            (local_stat->cpu_demand + local_stat->run) >> 1;
		}
		local_stat->total = 0;
		local_stat->run = 0;
	}
}

/**
 * cpu_demand_event - and cpu_demand event occured
 * @event: one of the following three events:
 *   CPU_DEMAND_ENQUEUE: local class enqueue
 *   CPU_DEMAND_DEQUEUE: local class dequeue
 *   CPU_DEMAND_DESCHEDULE: one task belong a certain local class deschedule
 * @len: valid only for CPU_DEMAND_DESCHEDULE, how long the task has been run
 */
void cpu_demand_event(struct ckrm_cpu_demand_stat* local_stat, int event, unsigned long long len) 
{	
	switch (event) {
	case CPU_DEMAND_ENQUEUE: 
		len = sched_clock() - local_stat->last_sleep;
		local_stat->last_sleep = 0;
		update_cpu_demand_stat(local_stat,CKRM_CPU_DEMAND_SLEEP,len);
		break;
	case CPU_DEMAND_DEQUEUE:
		if (! local_stat->last_sleep) {
			local_stat->last_sleep = sched_clock();
		}
		break;
	case CPU_DEMAND_DESCHEDULE:
		update_cpu_demand_stat(local_stat,CKRM_CPU_DEMAND_RUN,len);
		break;
	case CPU_DEMAND_INIT: //for task init only
		cpu_demand_stat_init(local_stat,CPU_DEMAND_TP_TASK);
		break;
	default:
		BUG();
	}
}

/** 
 * check all the class local queue
 * 
 * to deal with excessive long run/sleep state
 * -- whenever the the ckrm_cpu_monitor is called, check if the class is in sleep state, if yes, then update sleep record
 */
void cpu_demand_check_sleep(struct ckrm_cpu_class_stat *stat, int cpu)
{
	struct ckrm_cpu_demand_stat * local_stat = &stat->local_stats[cpu];
	unsigned long long sleep,now;
	if (local_stat->last_sleep) {
		now = sched_clock();
		sleep = now - local_stat->last_sleep;
		local_stat->last_sleep = now;
		update_cpu_demand_stat(local_stat,CKRM_CPU_DEMAND_SLEEP,sleep);
	}
}

/**
 * update_max_demand: update effective cpu demand for each class
 * return -1 on error
 * 
 * Assume: the root_core->parent == NULL
 */
static int update_max_demand(struct ckrm_core_class *root_core)
{
	struct ckrm_core_class *cur_core, *child_core;
	struct ckrm_cpu_class *cls,*c_cls;
	int ret = -1;

	cur_core = root_core;
	child_core = NULL;
	
 repeat:
	if (!cur_core) { //normal exit
		ret = 0;
		goto out;
	}

	cls = ckrm_get_cpu_class(cur_core);
	if (! cls) //invalid c_cls, abort
		goto out;

	if (!child_core)	//first child
		cls->stat.max_demand = get_mmax_demand(&cls->stat);
	else {
		c_cls = ckrm_get_cpu_class(child_core);
		if (c_cls)
			cls->stat.max_demand += c_cls->stat.max_demand;
		else //invalid c_cls, abort
			goto out;
	}

	//check class hard limit
	if (cls->stat.max_demand > cls->stat.ehl)
		cls->stat.max_demand = cls->stat.ehl;

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
 out:
	return ret;
}

/**********************************************/
/*          effective guarantee & limit       */
/**********************************************/
/**
 *update_child_effective - update egrt, ehl, mehl for all children of parent
 *@parent: the parent node
 *return -1 if anything wrong
 *
 */
static int update_child_effective(struct ckrm_core_class *parent)
{
	struct ckrm_cpu_class *p_cls = ckrm_get_cpu_class(parent);
	struct ckrm_core_class *child_core;	
	int ret = -1;

	if (! p_cls)
		return ret;

	child_core = ckrm_get_next_child(parent, NULL);
	while (child_core) {
		struct ckrm_cpu_class *c_cls = ckrm_get_cpu_class(child_core);
		if (! c_cls)
			return ret;

		c_cls->stat.egrt =
		    p_cls->stat.egrt *
		    c_cls->shares.my_guarantee / p_cls->shares.total_guarantee;

		c_cls->stat.megrt = c_cls->stat.egrt * get_my_grt(c_cls)
			/ c_cls->shares.total_guarantee;
		
		c_cls->stat.ehl =
		    p_cls->stat.ehl *
		    get_hard_limit(c_cls) / p_cls->shares.total_guarantee;

		c_cls->stat.mehl =
		    c_cls->stat.ehl *
		    get_myhard_limit(c_cls) / c_cls->shares.total_guarantee;

		set_eshare(&c_cls->stat,c_cls->stat.egrt);
		set_meshare(&c_cls->stat,c_cls->stat.megrt);


		child_core = ckrm_get_next_child(parent, child_core);
	};
	return 0;
}

/**
 * update_effectives: update egrt, ehl, mehl for the whole tree
 * should be called only when class structure changed
 *
 * return -1 if anything wrong happened (eg: the structure changed during the process)
 */
int update_effectives(void)
{
	struct ckrm_core_class *root_core = get_default_cpu_class()->core;
	struct ckrm_core_class *cur_core, *child_core;
	struct ckrm_cpu_class *cls;
	int ret = -1;

	cur_core = root_core;
	child_core = NULL;
	cls = ckrm_get_cpu_class(cur_core);

	//initialize the effectives for root 
	cls->stat.egrt = CKRM_SHARE_MAX; /*egrt of the root is always 100% */
	cls->stat.megrt = cls->stat.egrt * get_my_grt(cls)
		/ cls->shares.total_guarantee;
	cls->stat.ehl = CKRM_SHARE_MAX * get_hard_limit(cls)
		/ cls->shares.total_guarantee;
	cls->stat.mehl = cls->stat.ehl * get_myhard_limit(cls)
		/ cls->shares.total_guarantee;
	set_eshare(&cls->stat,cls->stat.egrt);
	set_meshare(&cls->stat,cls->stat.megrt);

 repeat:
	//check exit
	if (!cur_core)
		return 0;

	//visit this node only once
	if (! child_core)
		if (update_child_effective(cur_core) < 0)
			return ret; //invalid cur_core node
	
	//next child
	child_core = ckrm_get_next_child(cur_core, child_core);

	if (child_core) {
		//go down to the next hier
		cur_core = child_core;
		child_core = NULL;
	} else { //no more child, go back
		child_core = cur_core;
		cur_core = child_core->hnode.parent;
	}
	goto repeat;
}

/**********************************************/
/*           CKRM Idle Tasks                  */
/**********************************************/

#ifdef CONFIG_CKRM_SUPPORT_MAXLIMITS

struct ckrm_cpu_class ckrm_idle_class_obj, *ckrm_idle_class;
struct task_struct* ckrm_idle_tasks[NR_CPUS];

/*how many ckrm idle tasks should I wakeup*/
static inline int get_nr_idle(unsigned long surplus)
{
	int cpu_online = cpus_weight(cpu_online_map);	
	int nr_idle = 0; 
	
	nr_idle = surplus * cpu_online;
	nr_idle >>= CKRM_SHARE_SHIFT;

	if (surplus) 
		nr_idle ++;

	if (nr_idle > cpu_online)  
		nr_idle = cpu_online;

	return nr_idle;
}

/**
 * update_ckrm_idle: update the status of the idle class according 
 *                   to the new surplus
 * surplus: new system surplus
 *
 * Task:
 * -- update share of the idle class 
 * -- wakeup idle tasks according to surplus
 */
void update_ckrm_idle(unsigned long surplus)
{
	int nr_idle = get_nr_idle(surplus);
	int i;
	struct task_struct* idle_task;

	set_eshare(&ckrm_idle_class->stat,surplus);
	set_meshare(&ckrm_idle_class->stat,surplus);
	/*wake up nr_idle idle tasks*/
	for_each_online_cpu(i) {
		idle_task = ckrm_idle_tasks[i];
		if (unlikely(idle_task->cpu_class != ckrm_idle_class)) {
			ckrm_cpu_change_class(idle_task,
					      idle_task->cpu_class,
					      ckrm_idle_class);
		}
		if (! idle_task)
			continue;
		if (i < nr_idle) {
			//activate it
			wake_up_process(idle_task);
		} else {
			//deactivate it
			idle_task->state = TASK_INTERRUPTIBLE;
			set_tsk_need_resched(idle_task);
		}
	}
}

static int ckrm_cpu_idled(void *nothing)
{
	set_user_nice(current,19);
	daemonize("ckrm_idle_task");

	//deactivate it, it will be waked up by ckrm_cpu_monitor
	current->state = TASK_INTERRUPTIBLE;
	schedule();		

	/*similar to cpu_idle */
	while (1) {
		while (!need_resched()) {
			ckrm_cpu_monitor(1);
			if (current_cpu_data.hlt_works_ok) {
				local_irq_disable();
				if (!need_resched()) {
					set_tsk_need_resched(current);
					safe_halt();
				} else
					local_irq_enable();
			}
		}
		schedule();		
	}
	return 0;
}

/**
 * ckrm_start_ckrm_idle: 
 *  create the ckrm_idle_class and starts the idle tasks
 *
 */
void ckrm_start_ckrm_idle(void)
{
	int i;
	int ret;
	ckrm_shares_t shares;
	
	ckrm_idle_class = &ckrm_idle_class_obj; 
	memset(ckrm_idle_class,0,sizeof(shares));
	/*don't care about the shares */
	init_cpu_class(ckrm_idle_class,&shares);
	printk(KERN_INFO"ckrm idle class %x created\n",(int)ckrm_idle_class);
	
	for_each_online_cpu(i) {
		ret = kernel_thread(ckrm_cpu_idled, 0, CLONE_KERNEL);
		
		/*warn on error, but the system should still work without it*/
		if (ret < 0)
			printk(KERN_ERR"Warn: can't start ckrm idle tasks\n");
		else {
			ckrm_idle_tasks[i] = find_task_by_pid(ret);
			if (!ckrm_idle_tasks[i])
				printk(KERN_ERR"Warn: can't find ckrm idle tasks %d\n",ret);
		}
	}
}

void ckrm_stop_ckrm_idle(void)
{
	BUG_ON(1);   // not yet implemented
}

#else

static inline void ckrm_start_ckrm_idle(void) { };
static inline void ckrm_stop_ckrm_idle(void) { };
static inline void update_ckrm_idle(unsigned long surplus) { };

#endif


/**********************************************/
/*          Local Weight                      */
/**********************************************/
/**
 * adjust_class_local_weight: adjust the local weight for each cpu
 *
 * lrq->weight = lpr->pressure * class->weight / total_pressure
 */
static void adjust_lrq_weight(struct ckrm_cpu_class *clsptr, int cpu_online)
{
	unsigned long total_pressure = 0;
	ckrm_lrq_t* lrq;
	int i;
	unsigned long class_weight;
	unsigned long long lw;	
	struct ckrm_cpu_class_stat *stat;
	unsigned long oweight;
	unsigned long skewed_limit;
	/*
	 * if a local queue gets less than 1/SKEWED_SHARE_RATIO of the eshare
	 * then we set the skewed_share 
	 */
#define SKEWED_SHARE_RATIO 8
#define SKEWED_WEIGHT_MIN 3
	
	/* get total pressure of the class, if there is not pressure (.. class is
	 * idle, then leave the weights as is
	 */
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(clsptr,i);
		total_pressure += lrq->lrq_load;
	}

	if (! total_pressure)
		return;
	
	stat = &clsptr->stat;

	class_weight = cpu_class_weight(clsptr) * cpu_online;

	/* calculate or skewed limit weight */
	skewed_limit = SHARE_TO_WEIGHT(stat->meshare/SKEWED_SHARE_RATIO);
	if (skewed_limit < SKEWED_WEIGHT_MIN)
		skewed_limit = SKEWED_WEIGHT_MIN;

	/* calculate over_weight */	
	BUG_ON(stat->meshare < stat->megrt);
	oweight = ((stat->meshare - stat->megrt) << CKRM_SHARE_SHIFT) / stat->meshare;
	oweight = SHARE_TO_WEIGHT(oweight);

	/*
	 * update weight for each cpu, minimun is 1
	 */
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(clsptr,i);
		lrq->over_weight = oweight;
		if (! lrq->lrq_load) {
			/* give idle class a high share to boost 
			 * interactiveness 
			 */
			lw = cpu_class_weight(clsptr); 
			if (unlikely(lw==0))
				lw = 1;
		} else {
			lw = lrq->lrq_load;
			lw *= class_weight;
			do_div(lw,total_pressure);
			if (unlikely(lw==0))
				lw = 1;
			else if (unlikely(lw > CKRM_MAX_WEIGHT))
				lw = CKRM_MAX_WEIGHT;
		}	
		BUG_ON(lw > CKRM_MAX_WEIGHT);

		/* 
		 * set is_skewed and local_weight in proper order
		 * to avoid race condition
		 */
		lrq->local_weight = lw;
		if (lw < skewed_limit) 
			lrq->skewed_weight = skewed_limit;
		else
			lrq->skewed_weight = 0;
		BUG_ON((local_class_weight(lrq) == 1) && (! lrq->skewed_weight));
	}
}

/*
 * assume called with class_list_lock read lock held
 */

void adjust_local_weight(void)
{
	static spinlock_t lock = SPIN_LOCK_UNLOCKED; 
	struct ckrm_cpu_class *clsptr;
	int cpu_online;

	//do nothing if someone already holding the lock
	if (! spin_trylock(&lock))
		return;

	cpu_online = cpus_weight(cpu_online_map);	

	//class status: demand, share,total_ns prio, index
	list_for_each_entry(clsptr,&active_cpu_classes,links) {
		adjust_lrq_weight(clsptr,cpu_online);
	}

	spin_unlock(&lock);
}

/**********************************************/
/*          Main                              */
/**********************************************/
/**
 *ckrm_cpu_monitor - adjust relative shares of the classes based on their progress
 *@check_min: if check_min is set, the call can't be within 100ms of last call
 *
 * this function is called every CPU_MONITOR_INTERVAL
 * it computes the cpu demand of each class
 * and re-allocate the un-used shares to other classes
 */
void ckrm_cpu_monitor(int check_min)
{
	static spinlock_t lock = SPIN_LOCK_UNLOCKED; 
	static unsigned long long last_check = 0;
	struct ckrm_core_class *root_core = get_default_cpu_class()->core;
	unsigned long long now;	
	int loc;

#define MIN_CPU_MONITOR_INTERVAL (100*1000*1000)  /* 100 MSEC */

	if (ckrm_cpu_disabled() || !root_core)
		return;

	//do nothing if someone already holding the lock
	if (! spin_trylock(&lock))
		return;

	read_lock(&class_list_lock);

	now = sched_clock();

	//consecutive check should be at least 100ms apart
	if (check_min && (now - last_check < MIN_CPU_MONITOR_INTERVAL))
		goto outunlock_np;

	last_check = now;

	if (update_effectives() != 0) {
		loc = 0;
		goto outunlock;
	}
	
	if (update_max_demand(root_core) != 0) {
		loc = 1;
		goto outunlock;
	}
	
	if (alloc_surplus(root_core) != 0) {
		loc = 2;
		goto outunlock;
	}
	
	adjust_local_weight();

 outunlock_np:
	read_unlock(&class_list_lock);
	spin_unlock(&lock);
	return;

 outunlock:	
	printk("ckrm_cpu_monitor(%d) exits prematurely cause=%d\n",check_min,loc);
	goto outunlock_np;
}

/*****************************************************/
/*            Supporting Functions                   */
/*****************************************************/
static pid_t cpu_monitor_pid = -1;
static int thread_exit = 0;

static int ckrm_cpu_monitord(void *nothing)
{
	daemonize("ckrm_cpu_ctrld");
	printk("cpu_monitord started\n");
	thread_exit = 0;
	for (;;) {
		/*sleep for sometime before next try*/
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(CPU_MONITOR_INTERVAL);
		ckrm_cpu_monitor(1);
		if (thread_exit) {
			break;
		}
	}
	cpu_monitor_pid = -1;
	thread_exit = 2;
	printk("cpu_monitord exit\n");
	return 0;
}

void ckrm_cpu_start_monitor(void)
{
	if (cpu_monitor_pid != -1) {
		/* already started ... */
		return;
	}	
	cpu_monitor_pid = kernel_thread(ckrm_cpu_monitord, 0, CLONE_KERNEL);
	if (cpu_monitor_pid < 0) {
		printk("ckrm_cpu_monitord for failed\n");
	}
}

void ckrm_cpu_kill_monitor(void)
{
	printk("killing process %d\n", cpu_monitor_pid);
	if (cpu_monitor_pid > 0) {
		thread_exit = 1;
		while (thread_exit != 2) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(CPU_MONITOR_INTERVAL);
		}
	}
}

static int __init ckrm_cpu_init_monitor(void)
{
	if (ckrm_cpu_enabled()) 
		ckrm_cpu_start_monitor();
	return 0;
}

__initcall(ckrm_cpu_init_monitor);

