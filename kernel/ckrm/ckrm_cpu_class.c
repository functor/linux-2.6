/* kernel/ckrm/ckrm_cpu_class.c - CPU Class resource controller for CKRM
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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/errno.h>
#include <linux/sched.h>
#include <linux/ckrm.h>
#include <linux/ckrm_rc.h>
#include <linux/ckrm_tc.h>
#include <linux/ckrm_sched.h>
#include <linux/ckrm_classqueue.h>
#include <linux/seq_file.h>
#include <linux/parser.h>

#define CPU_CTRL_NAME  "cpu"

struct ckrm_res_ctlr cpu_rcbs;

#define CKRM_CPU_USAGE_DETAIL_MAX 3
static int usage_detail = 3;  /* 0: show usage 
			       * 1: show settings
			       * 2: show effectives
			       * 3: show per runqueue stats
			       */

static int ckrm_cpu_set_mode(enum ckrm_sched_mode mode);

/*
 * update effective share setting after:
 * -- remove class
 * -- change class share
 * we don't need to call update_effectives() when add new class since 
 * the defaults grt of new class is 0
 * CAUTION: might need a lock here
 */
static inline void update_class_effectives(void) 
{
	//	update_effectives();
	ckrm_cpu_monitor(0);
}

/**
 * insert_cpu_class - insert a class to active_cpu_class list
 *
 * insert the class in decreasing order of class weight
 */
static inline void insert_cpu_class(struct ckrm_cpu_class *cls)
{
	list_add(&cls->links,&active_cpu_classes);
}

/*
 *  initialize a class object and its local queues
 */

CVT_t get_min_cvt_locking(int cpu);
ckrm_lrq_t *rq_get_dflt_lrq(int cpu);

static void init_cpu_class_lrq(struct ckrm_cpu_class *cls, 
			       int cpu, int isdflt)
{
	int j,k;
	ckrm_lrq_t *queue = cls->local_queues[cpu];

	queue->active   = queue->arrays;
	queue->expired  = queue->arrays+1;
	
	for (j = 0; j < 2; j++) {
		prio_array_t *array = queue->arrays + j;
		for (k = 0; k < MAX_PRIO; k++) {
			INIT_LIST_HEAD(array->queue + k);
			__clear_bit(k, array->bitmap);
		}
		// delimiter for bitsearch
		__set_bit(MAX_PRIO, array->bitmap);
		array->nr_active = 0;
	}
	
	queue->expired_timestamp = 0;
	queue->best_expired_prio = MAX_PRIO;
	
	queue->cpu_class = cls;
	queue->classqueue = get_cpu_classqueue(cpu);
	queue->top_priority = MAX_PRIO;
	cq_node_init(&queue->classqueue_linkobj);
	queue->local_cvt = isdflt ? 0 : get_min_cvt_locking(cpu);
	queue->lrq_load = 0;
	queue->local_weight = cpu_class_weight(cls);
	if (queue->local_weight == 0)
		queue->local_weight = 1;
	queue->over_weight = 0;
	queue->skewed_weight = CKRM_MAX_WEIGHT/2; /*otherwise class might starve on start*/
	queue->uncounted_ns = 0;
	queue->savings = 0;
	queue->magic = CKRM_LRQ_MAGIC;
}

void init_cpu_class(struct ckrm_cpu_class *cls,ckrm_shares_t* shares) 
{
	int i;      
	int isdflt;
	struct ckrm_cpu_class *dfltcls;

	dfltcls = get_default_cpu_class();

	isdflt = (cls==dfltcls);

	cls->shares = *shares;
	cls->cnt_lock = SPIN_LOCK_UNLOCKED;
	ckrm_cpu_stat_init(&cls->stat,isdflt ? CKRM_SHARE_MAX : 1);
	ckrm_usage_init(&cls->usage);
	cls->magic = CKRM_CPU_CLASS_MAGIC;

	memset(cls->local_queues,0,NR_CPUS*sizeof(ckrm_lrq_t*));
	
	if (isdflt) {
		for (i=0; i< NR_CPUS; i++) {
			cls->local_queues[i] = rq_get_dflt_lrq(i);
			init_cpu_class_lrq(cls,i,1);
		}
	} else {
		for_each_cpu(i) {
			cls->local_queues[i] = kmalloc(sizeof(ckrm_lrq_t),
						       GFP_KERNEL);
			BUG_ON(cls->local_queues[i]==NULL);
			init_cpu_class_lrq(cls,i,0);
		}
	}

	write_lock(&class_list_lock);
	insert_cpu_class(cls);
	write_unlock(&class_list_lock);
}

static inline void set_default_share(ckrm_shares_t *shares)
{
	shares->my_guarantee     = 0;
	shares->total_guarantee  = CKRM_SHARE_DFLT_TOTAL_GUARANTEE;
	shares->unused_guarantee = CKRM_SHARE_DFLT_TOTAL_GUARANTEE;
	shares->my_limit         = CKRM_SHARE_DFLT_MAX_LIMIT;
	shares->max_limit        = CKRM_SHARE_DFLT_MAX_LIMIT;
	shares->cur_max_limit    = 0;
}

struct ckrm_cpu_class * ckrm_get_cpu_class(struct ckrm_core_class *core)
{
	struct ckrm_cpu_class * cls;
	cls = ckrm_get_res_class(core, cpu_rcbs.resid, struct ckrm_cpu_class);
  	if (valid_cpu_class(cls))
		return (ckrm_cpu_enabled() ? cls : get_default_cpu_class());
	else
		return NULL;
}

void* ckrm_alloc_cpu_class(struct ckrm_core_class *core, 
			   struct ckrm_core_class *parent) 
{		
	struct ckrm_cpu_class *cls;

	if (! parent) /*root class*/
		cls =  get_default_cpu_class();
	else
		cls = (struct ckrm_cpu_class *) kmalloc(sizeof(struct ckrm_cpu_class),GFP_ATOMIC);

	if (cls) {
		ckrm_shares_t shares;		
		if ((! parent) && (core)) { 
			/*
			 * the default class is already initialized
			 * so only update the core structure
			 */
			cls->core = core;			
		} else {
			set_default_share(&shares);
			init_cpu_class(cls,&shares);
			cls->core = core;
			cls->parent = parent;			
		}
	} else
		printk(KERN_ERR"alloc_cpu_class failed\n");

	return cls;
}		

void ckrm_cpu_class_queue_delete_sync(struct ckrm_cpu_class *clsptr);

static void ckrm_free_cpu_class(void *my_res) 
{			
	struct ckrm_cpu_class *cls = my_res, *parres, *childres;
	ckrm_core_class_t *child = NULL;
	int maxlimit;
	int i;

	if (!cls) 
		return;

	/*the default class can't be freed*/
	if (cls == get_default_cpu_class()) 
		return;

	// Assuming there will be no children when this function is called
	parres = ckrm_get_cpu_class(cls->parent);

	// return child's limit/guarantee to parent node
	spin_lock(&parres->cnt_lock);
	child_guarantee_changed(&parres->shares, cls->shares.my_guarantee, 0);
	// run thru parent's children and get the new max_limit of the parent
	ckrm_lock_hier(parres->core);
	maxlimit = 0;
	while ((child = ckrm_get_next_child(parres->core, child)) != NULL) {
		childres = ckrm_get_cpu_class(child);
		if (maxlimit < childres->shares.my_limit) {
			maxlimit = childres->shares.my_limit;
		}
	}
	ckrm_unlock_hier(parres->core);
	if (parres->shares.cur_max_limit < maxlimit) {
		parres->shares.cur_max_limit = maxlimit;
	}

	spin_unlock(&parres->cnt_lock);

	write_lock(&class_list_lock);
	list_del(&cls->links);
	write_unlock(&class_list_lock);

	ckrm_cpu_class_queue_delete_sync(cls);

	for_each_cpu(i) {
		ckrm_lrq_t *lrq = get_ckrm_lrq(cls,i);
		if (!lrq) continue;
		lrq->magic = -99;
		kfree(lrq);
	}
	kfree(cls);

	//call ckrm_cpu_monitor after class is removed
	if (ckrm_cpu_enabled())
		update_class_effectives();
}				

/*
 *  the system will adjust to the new share automatically  
 */			
int ckrm_cpu_set_share(void *my_res, struct ckrm_shares *new_share) 
{	
        struct ckrm_cpu_class *parres, *cls = my_res;
        struct ckrm_shares *cur = &cls->shares, *par;
        int rc = -EINVAL;

	if (ckrm_cpu_disabled())
		return -ENOSYS;
        if (!cls)
		return rc;
	if (new_share->total_guarantee > CKRM_SHARE_MAX)
		return -E2BIG;

        if (cls->parent) {
                parres = ckrm_get_cpu_class(cls->parent);
                spin_lock(&parres->cnt_lock);
                spin_lock(&cls->cnt_lock);
                par = &parres->shares;
        } else {
                spin_lock(&cls->cnt_lock);
                par = NULL;
                parres = NULL;
        }

	/*
	 * hzheng: CKRM_SHARE_DONTCARE should be handled
	 */
	if (new_share->my_guarantee == CKRM_SHARE_DONTCARE)
		new_share->my_guarantee = 0;

	rc = set_shares(new_share, cur, par);
	if (!rc && cur->my_limit == CKRM_SHARE_DONTCARE)
		cur->my_limit = cur->max_limit;


	spin_unlock(&cls->cnt_lock);
	if (cls->parent) {
		spin_unlock(&parres->cnt_lock);
	}

	//call ckrm_cpu_monitor after changes are changed
	update_class_effectives();

	return rc;
}							
			
static int ckrm_cpu_get_share(void *my_res,
			      struct ckrm_shares *shares)
{			
	struct ckrm_cpu_class *cls = my_res;

	if (ckrm_cpu_disabled())
		return -ENOSYS;
        if (!cls)
		return -EINVAL;

	*shares = cls->shares;
	return 0;
}				

/*
 *   get_ckrm_usage():
 *     obtain a sequence of <num> usage informations
 *     returns number of usages reported.
 *
 *     report IN:  specifies the sequence of jiffies for which to report
 *                 must be ordered (smallest first)
 *            OUT: returns the usage in each field
 *
 */


int ckrm_cpu_get_usage(struct ckrm_cpu_class* clsptr, 
		       int num, ulong report[])
{
	struct ckrm_usage* usage = &clsptr->usage;
	unsigned long long total = 0;
	int i, idx, cur, num_ofs;

	num_ofs = cur = i = 0;
	idx = usage->sample_pointer;	

	for ( num_ofs = 0; num_ofs < num ; num_ofs++ ) {
		int nr_samples;
		int duration = report[num_ofs];	
		unsigned long long totval = 0;

		nr_samples = duration/USAGE_SAMPLE_FREQ?:1;
		
		if (nr_samples > USAGE_MAX_HISTORY)
			nr_samples = USAGE_MAX_HISTORY;

		for ( ; i< nr_samples; i++) {
			if (! idx)
				idx = USAGE_MAX_HISTORY;
			idx --;
			total += usage->samples[idx];
		}
		totval = total * 1000;
		do_div(totval,NS_PER_SAMPLE);
		do_div(totval,nr_samples * cpus_weight(cpu_online_map));
		report[num_ofs] = totval;
	}

        return num;
}

int ckrm_cpu_get_stats(void *my_res, struct seq_file * sfile)
{
	struct ckrm_cpu_class *cls = my_res;
	struct ckrm_cpu_class_stat* stat = &cls->stat;
	ckrm_lrq_t* lrq;
	int i;
	ulong usage[3] = { 2*HZ, 10*HZ, 60*HZ };

	if (!cls || ckrm_cpu_disabled()) 
		return -EINVAL;

	ckrm_cpu_get_usage(cls,3,usage);

	/* this will after full stabilization become the only cpu usage stats
	 */

	seq_printf(sfile, "cpu-usage(2,10,60)= %lu %lu %lu\n",
		   usage[0],usage[1],usage[2]);

	if (usage_detail < 1) 
		return 0;

	/* the extended statistics we can decide whether we want to make the 
	 * additional statistics available over config options
	 * eitherway they should be reported in a more concised form
	 * during stabilization, this is OK
	 */

	seq_printf(sfile, "-------- CPU Class Status Start---------\n");
	seq_printf(sfile, "Share:\n\tgrt= %d limit= %d total_grt= %d max_limit= %d\n",
		   cls->shares.my_guarantee,
		   cls->shares.my_limit,
		   cls->shares.total_guarantee,
		   cls->shares.max_limit);
	seq_printf(sfile, "\tunused_grt= %d cur_max_limit= %d\n",
		   cls->shares.unused_guarantee,
		   cls->shares.cur_max_limit);

	if (usage_detail < 2) 
		goto out;

	seq_printf(sfile, "Effective:\n\tegrt= %d\n",stat->egrt);
	seq_printf(sfile, "\tmegrt= %d\n",stat->megrt);
	seq_printf(sfile, "\tehl= %d\n",stat->ehl);
	seq_printf(sfile, "\tmehl= %d\n",stat->mehl);
	seq_printf(sfile, "\teshare= %d\n",stat->eshare);
	seq_printf(sfile, "\tmeshare= %d\n",stat->meshare);
	seq_printf(sfile, "\tmax_demand= %lu\n",stat->max_demand);
	seq_printf(sfile, "\ttotal_ns= %llu\n",stat->total_ns);
	seq_printf(sfile, "\tusage(2,10,60)= %lu %lu %lu\n",
		   usage[0],usage[1],usage[2]);

	if (usage_detail < 3) 
		goto out;

	/* provide per run queue information */
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(cls,i);		
		seq_printf(sfile, "\tlrq %d demand= %lu weight= %d "
			   "lrq_load= %lu cvt= %llu sav= %llu\n",
			   i,stat->local_stats[i].cpu_demand,
			   local_class_weight(lrq),lrq->lrq_load,
			   lrq->local_cvt,lrq->savings);
	}

out:
	seq_printf(sfile, "-------- CPU Class Status END ---------\n");
	return 0;
}

/*
 * task will remain in the same cpu but on a different local runqueue
 */
void ckrm_cpu_change_class(void *task, void *old, void *new)
{		
	struct task_struct *tsk = task;			   
	struct ckrm_cpu_class *newcls = new;

	/*sanity checking*/
	if (!task || ! old || !new)
		return; 

	if (ckrm_cpu_disabled())
		newcls = get_default_cpu_class();
	_ckrm_cpu_change_class(tsk,newcls);
}							

enum config_token_t {
	config_usage_detail,   /* define usage level 			  */
	config_disable,        /* always use default linux scheduling     */
			       /* effectively disables the ckrm scheduler */
	config_enable,         /* always uses ckrm scheduling behavior    */
	config_err             /* parsing error */
};

#define CKRM_SCHED_MODE_DISABLED_STR "disabled"
#define CKRM_SCHED_MODE_ENABLED_STR  "enabled"

static char *ckrm_sched_mode_str[] = { 
		CKRM_SCHED_MODE_DISABLED_STR,
		CKRM_SCHED_MODE_ENABLED_STR
};

static match_table_t config_tokens = {
	{ config_disable,      "mode="CKRM_SCHED_MODE_DISABLED_STR },
	{ config_enable,       "mode="CKRM_SCHED_MODE_ENABLED_STR  },
	{ config_usage_detail, "usage_detail=%u" 		   },
	{ config_err,          NULL                                }
};

static int ckrm_cpu_show_config(void *my_res, struct seq_file *sfile)
{
	struct ckrm_cpu_class *cls = my_res;

	if (!cls) 
		return -EINVAL;

	seq_printf(sfile, "res=%s,mode=%s",
		   CPU_CTRL_NAME,ckrm_sched_mode_str[ckrm_sched_mode]);
	if (!ckrm_cpu_disabled())  /* enabled || mixed */
		seq_printf(sfile, ",usage_detail=%u",usage_detail);
	seq_printf(sfile,"\n");
	return 0;
}

static int ckrm_cpu_set_config(void *my_res, const char *cfgstr)
{
	struct ckrm_cpu_class *cls = my_res;
	char *p;
	char **cfgstr_p = (char**)&cfgstr;
	substring_t args[MAX_OPT_ARGS];
	int option,rc;
	enum ckrm_sched_mode new_sched_mode;

	if (!cls) 
		return -EINVAL;

	new_sched_mode = ckrm_sched_mode;	
	rc = 0;

	while ((p = strsep(cfgstr_p, ",")) != NULL) {
		int token;
		if (!*p)
			continue;
		
		token = match_token(p, config_tokens, args);
		switch (token) {
		case config_usage_detail:
			if (ckrm_cpu_disabled() || 
			    (match_int(&args[0], &option)) ||
			    (option > CKRM_CPU_USAGE_DETAIL_MAX))
			{
				return -EINVAL;
			}
			usage_detail = option;
			break;
		case config_disable:
		 	new_sched_mode = CKRM_SCHED_MODE_DISABLED;
			break;
		case config_enable:
		 	new_sched_mode = CKRM_SCHED_MODE_ENABLED;
			break;
		case config_err:
			return -EINVAL;
		}
	}
	rc = ckrm_cpu_set_mode(new_sched_mode);
	return rc;
}
	
struct ckrm_res_ctlr cpu_rcbs = {
	.res_name          = CPU_CTRL_NAME,
	.res_hdepth        = 1,
	.resid             = -1,
	.res_alloc         = ckrm_alloc_cpu_class,
	.res_free          = ckrm_free_cpu_class,
	.set_share_values  = ckrm_cpu_set_share,
	.get_share_values  = ckrm_cpu_get_share,
	.get_stats         = ckrm_cpu_get_stats,
	.show_config       = ckrm_cpu_show_config,
	.set_config        = ckrm_cpu_set_config,
	.change_resclass   = ckrm_cpu_change_class,
};

int __init init_ckrm_sched_res(void)
{
	struct ckrm_classtype *clstype;
	int resid = cpu_rcbs.resid;

	clstype = ckrm_find_classtype_by_name("taskclass");
	if (clstype == NULL) {
		printk(KERN_INFO" Unknown ckrm classtype<taskclass>");
		return -ENOENT;
	}

	if (resid == -1) { /*not registered */
		resid = ckrm_register_res_ctlr(clstype,&cpu_rcbs);
		printk(KERN_DEBUG "........init_ckrm_sched_res , resid= %d\n",resid);
	}
	return 0;
}

/*
 * initialize the class structure
 * add the default class: class 0
 */
void init_cpu_classes(void) 
{
	int i;

	//init classqueues for each processor
	for (i=0; i < NR_CPUS; i++)
		classqueue_init(get_cpu_classqueue(i),ckrm_cpu_enabled()); 

	ckrm_alloc_cpu_class(NULL,NULL);
}

void ckrm_cpu_class_queue_update(int on);
void ckrm_cpu_start_monitor(void);
void ckrm_cpu_kill_monitor(void);

static int ckrm_cpu_set_mode(enum ckrm_sched_mode mode) 
{
        struct task_struct *proc, *tsk;
	struct ckrm_cpu_class *new_cls = NULL;
	int i;

	if (mode == ckrm_sched_mode)
		return 0;

	printk("ckrm_cpu_set_mode from <%s> to <%s> pid=%d\n",
		   ckrm_sched_mode_str[ckrm_sched_mode],
		   ckrm_sched_mode_str[mode], 
		   current->pid);

	if (mode == CKRM_SCHED_MODE_DISABLED) {
		ckrm_cpu_kill_monitor();
		new_cls = get_default_cpu_class();
	} else {
		ckrm_cpu_class_queue_update(1);
	}
                             
	/* run twice through the list to catch everyone,
	 * current and transient once
         */

        read_lock(&tasklist_lock);

	ckrm_sched_mode = mode;
	/* we have to run through the list twice
	 * first catch all existing tasks
	 * and then deal with some potential race condition
	 */
	for ( i=2 ; i-- ; ) {
		/* lock class_list_lock ? */
	
	        do_each_thread(proc, tsk) {
			if (mode == CKRM_SCHED_MODE_ENABLED) {
				new_cls = ckrm_get_res_class(class_core(tsk->taskclass),
							     cpu_rcbs.resid,
							     struct ckrm_cpu_class);
			}	
			_ckrm_cpu_change_class(tsk,new_cls);
	        } while_each_thread(proc, tsk);
	}
        read_unlock(&tasklist_lock);

	if (mode == CKRM_SCHED_MODE_DISABLED) 
		ckrm_cpu_class_queue_update(0);
	else 
		ckrm_cpu_start_monitor();
	return 0;
}

EXPORT_SYMBOL(ckrm_get_cpu_class);



