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
#include <linux/ckrm_events.h>
#include <linux/ckrm_rc.h>
#include <linux/ckrm_tc.h>
#include <linux/ckrm_sched.h>
#include <linux/ckrm_classqueue.h>
#include <linux/seq_file.h>

struct ckrm_res_ctlr cpu_rcbs;

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
void init_cpu_class(struct ckrm_cpu_class *cls,ckrm_shares_t* shares) 
{
	int i,j,k;      
	prio_array_t *array; 	
	ckrm_lrq_t* queue;

	cls->shares = *shares;
	cls->cnt_lock = SPIN_LOCK_UNLOCKED;
	ckrm_cpu_stat_init(&cls->stat);
	ckrm_usage_init(&cls->usage);
	cls->magic = CKRM_CPU_CLASS_MAGIC;

	for (i = 0 ; i < NR_CPUS ; i++) {
		queue = &cls->local_queues[i];
	queue->active   = queue->arrays;
	queue->expired  = queue->arrays+1;
	
	for (j = 0; j < 2; j++) {
			array = queue->arrays + j;
		for (k = 0; k < MAX_PRIO; k++) {
			INIT_LIST_HEAD(array->queue + k);
			__clear_bit(k, array->bitmap);
		}
		// delimiter for bitsearch
		__set_bit(MAX_PRIO, array->bitmap);
		array->nr_active = 0;
	}
	
	queue->expired_timestamp = 0;
	
	queue->cpu_class = cls;
		queue->classqueue = get_cpu_classqueue(i);
	queue->top_priority = MAX_PRIO;
	cq_node_init(&queue->classqueue_linkobj);
		queue->local_cvt = 0;
	queue->lrq_load = 0;
	queue->local_weight = cpu_class_weight(cls);
	queue->uncounted_ns = 0;
	queue->savings = 0;
		queue->magic = 0x43FF43D7;
	}

	// add to class list
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
		return cls;
	else
		return NULL;
}


void* ckrm_alloc_cpu_class(struct ckrm_core_class *core, struct ckrm_core_class *parent) 
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

/*
 * hzheng: this is not a stable implementation
 *         need to check race condition issue here
 */		
static void ckrm_free_cpu_class(void *my_res) 
{			
	struct ckrm_cpu_class *cls = my_res, *parres, *childres;
	ckrm_core_class_t *child = NULL;
	int maxlimit;
	ckrm_lrq_t* queue;
	int i;

	if (!cls) 
		return;

	/*the default class can't be freed*/
	if (cls == get_default_cpu_class()) 
		return;
#if 1
#warning "ACB: Remove freed class from any classqueues [PL #4233]"
	for (i = 0 ; i < NR_CPUS ; i++) {
	  queue = &cls->local_queues[i];
	  if (cls_in_classqueue(&queue->classqueue_linkobj))
	    classqueue_dequeue(queue->classqueue,
			       &queue->classqueue_linkobj);
	}
#endif

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

	kfree(cls);

	//call ckrm_cpu_monitor after class removed
	ckrm_cpu_monitor(0);
}				

/*
 *  the system will adjust to the new share automatically  
 */			
int ckrm_cpu_set_share(void *my_res, struct ckrm_shares *new_share) 
{	
        struct ckrm_cpu_class *parres, *cls = my_res;
        struct ckrm_shares *cur = &cls->shares, *par;
        int rc = -EINVAL;

        if (!cls)
		return rc;

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
	if (cur->my_limit == CKRM_SHARE_DONTCARE)
		cur->my_limit = cur->max_limit;


	spin_unlock(&cls->cnt_lock);
	if (cls->parent) {
		spin_unlock(&parres->cnt_lock);
	}

	//call ckrm_cpu_monitor after changes are changed
	ckrm_cpu_monitor(0);

	return rc;
}							
			
static int ckrm_cpu_get_share(void *my_res,
			      struct ckrm_shares *shares)
{			
	struct ckrm_cpu_class *cls = my_res;

        if (!cls)
		return -EINVAL;
	*shares = cls->shares;
	return 0;
}				

int ckrm_cpu_get_stats(void *my_res, struct seq_file * sfile)
{
	struct ckrm_cpu_class *cls = my_res;
	struct ckrm_cpu_class_stat* stat = &cls->stat;
	ckrm_lrq_t* lrq;
	int i;

	if (!cls) 
		return -EINVAL;

	seq_printf(sfile, "-------- CPU Class Status Start---------\n");
	seq_printf(sfile, "Share:\n\tgrt= %d limit= %d total_grt= %d max_limit= %d\n",
		   cls->shares.my_guarantee,
		   cls->shares.my_limit,
		   cls->shares.total_guarantee,
		   cls->shares.max_limit);
	seq_printf(sfile, "\tunused_grt= %d cur_max_limit= %d\n",
		   cls->shares.unused_guarantee,
		   cls->shares.cur_max_limit);

	seq_printf(sfile, "Effective:\n\tegrt= %d\n",stat->egrt);
	seq_printf(sfile, "\tmegrt= %d\n",stat->megrt);
	seq_printf(sfile, "\tehl= %d\n",stat->ehl);
	seq_printf(sfile, "\tmehl= %d\n",stat->mehl);
	seq_printf(sfile, "\teshare= %d\n",stat->eshare);
	seq_printf(sfile, "\tmeshare= %d\n",cpu_class_weight(cls));
	seq_printf(sfile, "\tmax_demand= %lu\n",stat->max_demand);
	seq_printf(sfile, "\ttotal_ns= %llu\n",stat->total_ns);
	seq_printf(sfile, "\tusage(2,10,60)= %d %d %d\n",
		   get_ckrm_usage(cls,2*HZ),
		   get_ckrm_usage(cls,10*HZ),
		   get_ckrm_usage(cls,60*HZ)
		   );
	for_each_online_cpu(i) {
		lrq = get_ckrm_lrq(cls,i);		
		seq_printf(sfile, "\tlrq %d demand= %lu weight= %d lrq_load= %lu cvt= %llu sav= %llu\n",i,stat->local_stats[i].cpu_demand,local_class_weight(lrq),lrq->lrq_load,lrq->local_cvt,lrq->savings);
	}

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

	_ckrm_cpu_change_class(tsk,newcls);
}							

/*dummy function, not used*/
static int ckrm_cpu_show_config(void *my_res, struct seq_file *sfile)
{
	struct ckrm_cpu_class *cls = my_res;

	if (!cls) 
		return -EINVAL;

	seq_printf(sfile, "cls=%s,parameter=somevalue\n","ckrm_cpu class");
	return 0;
}

/*dummy function, not used*/
static int ckrm_cpu_set_config(void *my_res, const char *cfgstr)
{
	struct ckrm_cpu_class *cls = my_res;

	if (!cls) 
		return -EINVAL;
	printk(KERN_DEBUG "ckrm_cpu config='%s'\n",cfgstr);
	return 0;
}
	
struct ckrm_res_ctlr cpu_rcbs = {
	.res_name          = "cpu",
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
		classqueue_init(get_cpu_classqueue(i)); 

	/*
	 * hzheng: initialize the default cpu class
	 *  required for E14/E15 since ckrm_init is called after sched_init
	 */
	ckrm_alloc_cpu_class(NULL,NULL);
	}


EXPORT_SYMBOL(ckrm_get_cpu_class);
