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

#define CPU_MONITOR_INTERVAL (HZ) /*how often do we adjust the shares*/
#define CKRM_SHARE_MAX (1<<CKRM_SHARE_ACCURACY)

#define CKRM_CPU_DEMAND_RUN 0
#define CKRM_CPU_DEMAND_SLEEP 1
//sample task cpu demand every 64ms
#define CPU_DEMAND_TASK_RECALC  (64000000LL)
#define CPU_DEMAND_CLASS_RECALC (256000000LL)
#define CPU_DEMAND_TP_CLASS 0
#define CPU_DEMAND_TP_TASK 1

extern struct ckrm_cpu_class *ckrm_get_cpu_class(struct ckrm_core_class *core);
void update_ckrm_idle(unsigned long surplus);

/*interface to share definition*/
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

void ckrm_cpu_stat_init(struct ckrm_cpu_class_stat *stat)
{
	int i;

	stat->stat_lock = SPIN_LOCK_UNLOCKED;
	stat->total_ns = 0;
	stat->max_demand = 0;

	for (i=0; i< NR_CPUS; i++) {
		cpu_demand_stat_init(&stat->local_stats[i],CPU_DEMAND_TP_CLASS);
	}

	stat->egrt = 0;
	stat->megrt = 0;
	stat->ehl = CKRM_SHARE_MAX; /*default: no limit*/
	stat->mehl = CKRM_SHARE_MAX; /*default: no limit */

	stat->eshare = CKRM_SHARE_MAX;
	stat->meshare = CKRM_SHARE_MAX;
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
static inline void update_cpu_demand_stat(struct ckrm_cpu_demand_stat* local_stat,int state, unsigned long long len)
{	
	local_stat->total += len;
	if (state == CKRM_CPU_DEMAND_RUN)
		local_stat->run += len;

	if (local_stat->total >= local_stat->recalc_interval) {
		local_stat->total >>= CKRM_SHARE_ACCURACY;
		if (unlikely(local_stat->run > 0xFFFFFFFF))
			local_stat->run = 0xFFFFFFFF;

		if (local_stat->total > 0xFFFFFFFF) 
			local_stat->total = 0xFFFFFFFF;
			
		do_div(local_stat->run,(unsigned long)local_stat->total);

		if (local_stat->total > 0xFFFFFFFF) //happens after very long sleep
			local_stat->cpu_demand = local_stat->run;
		else {
			local_stat->cpu_demand += local_stat->run;
			local_stat->cpu_demand >>= 1;
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
static inline void cpu_demand_check_sleep(struct ckrm_cpu_class_stat *stat, int cpu)
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

		c_cls->stat.megrt = c_cls->stat.egrt * c_cls->shares.unused_guarantee
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
static int update_effectives(struct ckrm_core_class *root_core)
{
	struct ckrm_core_class *cur_core, *child_core;
	struct ckrm_cpu_class *cls;
	int ret = -1;

	cur_core = root_core;
	child_core = NULL;
	cls = ckrm_get_cpu_class(cur_core);

	//initialize the effectives for root 
	cls->stat.egrt = CKRM_SHARE_MAX; /*egrt of the root is always 100% */
	cls->stat.megrt = cls->stat.egrt * cls->shares.unused_guarantee
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

static inline int get_my_node_surplus(struct ckrm_cpu_class *cls)
{
	int surplus = cls->stat.megrt - get_mmax_demand(&cls->stat);

	if (surplus < 0)
		surplus = 0;

	return surplus;
}

/**
 * consume_surplus: decides how much surplus a node can consume
 * @ckeck_sl: if check_sl is set, then check soft_limitx
 * return how much consumed
 *
 * implements all the CKRM Scheduling Requirement
 * assume c_cls is valid
 */
static inline int consume_surplus(int surplus,
				       struct ckrm_cpu_class *c_cls,
				       struct ckrm_cpu_class *p_cls,
				       int check_sl
				       )
{
	int consumed = 0;
	int inc_limit;
	int total_grt = p_cls->shares.total_guarantee;

 	BUG_ON(surplus < 0);

	/*can't consume more than demand or hard limit*/
	if (c_cls->stat.eshare >= c_cls->stat.max_demand)
		goto out;

	//the surplus allocation is propotional to grt
	consumed =
		surplus * c_cls->shares.my_guarantee / total_grt;

	if (! consumed) //no more share
		goto out;

	//hard limit and demand limit
	inc_limit = c_cls->stat.max_demand - c_cls->stat.eshare;

	if (check_sl) {
		int esl = p_cls->stat.eshare * get_soft_limit(c_cls)
			/total_grt;
		if (esl < c_cls->stat.max_demand)
			inc_limit = esl - c_cls->stat.eshare;
	}

	if (consumed > inc_limit)
		consumed = inc_limit;

        BUG_ON(consumed < 0);
 out:		
	return consumed;
}

/*
 * how much a node can consume for itself?
 */
static inline int consume_self_surplus(int surplus,
				       struct ckrm_cpu_class *p_cls,
				       int check_sl
				       )
{
	int consumed = 0;
	int inc_limit;
	int total_grt = p_cls->shares.total_guarantee;
	int max_demand = get_mmax_demand(&p_cls->stat);

 	BUG_ON(surplus < 0);

	/*can't consume more than demand or hard limit*/
	if (p_cls->stat.meshare >= max_demand)
		goto out;

	//the surplus allocation is propotional to grt
	consumed =
		surplus * p_cls->shares.unused_guarantee / total_grt;

	if (! consumed) //no more share
		goto out;

	//hard limit and demand limit
	inc_limit = max_demand - p_cls->stat.meshare;

	if (check_sl) {
		int mesl = p_cls->stat.eshare * get_mysoft_limit(p_cls)
			/total_grt;
		if (mesl < max_demand)
			inc_limit = mesl - p_cls->stat.meshare;
	}

	if (consumed > inc_limit)
		consumed = inc_limit;

        BUG_ON(consumed < 0);
 out:		
	return consumed;
}


/*
 * allocate surplus to all its children and also its default class
 */
static int alloc_surplus_single_round(
				      int surplus,
				      struct ckrm_core_class *parent,
				      struct ckrm_cpu_class *p_cls,
				      int check_sl)
{
	struct ckrm_cpu_class *c_cls;
	struct ckrm_core_class *child_core = NULL;
	int total_consumed = 0,consumed;

	//first allocate to the default class
	consumed  =
		consume_self_surplus(surplus,p_cls,check_sl);

	if (consumed > 0) {
		set_meshare(&p_cls->stat,p_cls->stat.meshare + consumed);
		total_consumed += consumed;
	}

	do {
		child_core = ckrm_get_next_child(parent, child_core);
		if (child_core)  {
			c_cls = ckrm_get_cpu_class(child_core);
			if (! c_cls)
				return -1;

			consumed    =
				consume_surplus(surplus, c_cls,
						     p_cls,check_sl);
			if (consumed > 0) {
				set_eshare(&c_cls->stat,c_cls->stat.eshare + consumed);
				total_consumed += consumed;
			}
		}
	} while (child_core);

	return total_consumed;
}

/**
 * alloc_surplus_node: re-allocate the shares for children under parent
 * @parent: parent node
 * return the remaining surplus
 *
 * task:
 *  1. get total surplus
 *  2. allocate surplus
 *  3. set the effective_share of each node
 */
static int alloc_surplus_node(struct ckrm_core_class *parent)
{
	struct ckrm_cpu_class *p_cls,*c_cls;
	int total_surplus,consumed;
	int check_sl;
	int ret = -1;
	struct ckrm_core_class *child_core = NULL;

	p_cls = ckrm_get_cpu_class(parent);
	if (! p_cls)
		goto realloc_out;

	/*
	 * get total surplus
	 */
	total_surplus = p_cls->stat.eshare - p_cls->stat.egrt;
	BUG_ON(total_surplus < 0);
	total_surplus += get_my_node_surplus(p_cls);

	do {
		child_core = ckrm_get_next_child(parent, child_core);
		if (child_core) {
			c_cls = ckrm_get_cpu_class(child_core);				
			if (! c_cls)
				goto realloc_out;

			total_surplus += get_node_surplus(c_cls);
		}
	} while (child_core);


	if (! total_surplus) {
		ret = 0;
		goto realloc_out;
	}

	/* 
	 * distributing the surplus 
	 * first with the check_sl enabled
	 * once all the tasks has research the soft limit, disable check_sl and try again
	 */
	
	check_sl = 1;
	do {
		consumed = alloc_surplus_single_round(total_surplus,parent,p_cls,check_sl);
		if (consumed < 0) //something is wrong
			goto realloc_out;

		if (! consumed)
			check_sl = 0;
		else
			total_surplus -= consumed;

	} while ((total_surplus > 0) && (consumed || check_sl) );

	ret = 0;
	
 realloc_out:
	return ret;
}

/**
 * alloc_surplus - reallocate unused shares
 *
 * class A's usused share should be allocated to its siblings
 * the re-allocation goes downward from the top
 */
static int alloc_surplus(struct ckrm_core_class *root_core)
{
	struct ckrm_core_class *cur_core, *child_core;
	//	struct ckrm_cpu_class *cls;
	int ret = -1;

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
/*           CKRM Idle Tasks                  */
/**********************************************/
struct ckrm_cpu_class ckrm_idle_class_obj, *ckrm_idle_class;
struct task_struct* ckrm_idle_tasks[NR_CPUS];

/*how many ckrm idle tasks should I wakeup*/
static inline int get_nr_idle(unsigned long surplus)
{
	int cpu_online = cpus_weight(cpu_online_map);	
	int nr_idle = 0; 
	
	nr_idle = surplus * cpu_online;
	nr_idle >>= CKRM_SHARE_ACCURACY;

	if (surplus) 
		nr_idle ++;

	if (nr_idle > cpu_online)  
		nr_idle = cpu_online;

	return nr_idle;
}

/**
 * update_ckrm_idle: update the status of the idle class according to the new surplus
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

	//deactivate it, it will be awakened by ckrm_cpu_monitor
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

	//get total pressure
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(clsptr,i);
		total_pressure += lrq->lrq_load;
	}

#if 1
#warning "ACB taking out suspicious early return"
#else
	if (! total_pressure)
		return;
#endif
	
	class_weight = cpu_class_weight(clsptr) * cpu_online;

	/*
	 * update weight for each cpu, minimun is 1
	 */
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(clsptr,i);
		if (! lrq->lrq_load)
			/*give idle class a high share to boost interactiveness */
			lw = cpu_class_weight(clsptr); 
		else {
			lw = lrq->lrq_load * class_weight;
			do_div(lw,total_pressure);
			if (!lw)
				lw = 1;
			else if (lw > CKRM_SHARE_MAX)
				lw = CKRM_SHARE_MAX;
		}
		
		lrq->local_weight = lw;
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
#define MIN_CPU_MONITOR_INTERVAL 100000000UL

	if (!root_core)
		return;

	//do nothing if someone already holding the lock
	if (! spin_trylock(&lock))
		return;

	read_lock(&class_list_lock);

	now = sched_clock();

	//consecutive check should be at least 100ms apart
	if (check_min && ((now - last_check) < MIN_CPU_MONITOR_INTERVAL))
		goto outunlock;

	last_check = now;

	if (update_effectives(root_core) != 0)
		goto outunlock;
	
	if (update_max_demand(root_core) != 0)
		goto outunlock;
	
#ifndef ALLOC_SURPLUS_SUPPORT
#warning "MEF taking out alloc_surplus"
#else
	if (alloc_surplus(root_core) != 0)
		goto outunlock;
#endif
	
	adjust_local_weight();

 outunlock:	
	read_unlock(&class_list_lock);
	spin_unlock(&lock);
}

/*****************************************************/
/*            Supporting Functions                   */
/*****************************************************/
static pid_t cpu_monitor_pid = -1;
static int thread_exit = 0;

static int ckrm_cpu_monitord(void *nothing)
{
	daemonize("ckrm_cpu_ctrld");
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
	printk(KERN_DEBUG "cpu_monitord exit\n");
	return 0;
}

void ckrm_start_monitor(void)
{
	cpu_monitor_pid = kernel_thread(ckrm_cpu_monitord, 0, CLONE_KERNEL);
	if (cpu_monitor_pid < 0) {
		printk(KERN_DEBUG "ckrm_cpu_monitord for failed\n");
	}
}

void ckrm_kill_monitor(void)
{
	printk(KERN_DEBUG "killing process %d\n", cpu_monitor_pid);
	if (cpu_monitor_pid > 0) {
		thread_exit = 1;
		while (thread_exit != 2) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule_timeout(CPU_MONITOR_INTERVAL);
		}
	}
}

int ckrm_cpu_monitor_init(void)
{
	ckrm_start_monitor();
	/*hzheng: uncomment the following like for hard limit support */
	//	ckrm_start_ckrm_idle();
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
