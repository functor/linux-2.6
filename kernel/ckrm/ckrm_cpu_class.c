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


struct ckrm_res_ctlr cpu_rcbs;

/*
 *  initialize a class object and its local queues
 */
 static void init_cpu_class(struct ckrm_cpu_class *cls,ckrm_shares_t* shares) 
{
	int i,j,k;      
	prio_array_t *array; 	
	struct ckrm_local_runqueue* queue;

	for (i = 0 ; i < NR_CPUS ; i++) {
		queue = &cls->local_queues[i];
		queue->active  = queue->arrays;
		queue->expired = queue->arrays+1;
		
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
		queue->uncounted_cvt = 0;
		queue->uncounted_ns = 0;
		queue->magic = 0x43FF43D7;
	}

	cls->shares = *shares;
	cls->global_cvt = 0;
	cls->cnt_lock = SPIN_LOCK_UNLOCKED;
	ckrm_cpu_stat_init(&cls->stat);

	// add to class list
	write_lock(&class_list_lock);
	list_add(&cls->links,&active_cpu_classes);
	write_unlock(&class_list_lock);
}

static inline void set_default_share(ckrm_shares_t *shares)
{
	shares->my_guarantee     = 0;
	shares->my_limit         = CKRM_SHARE_DFLT_MAX_LIMIT;
	shares->total_guarantee  = CKRM_SHARE_DFLT_TOTAL_GUARANTEE;
	shares->max_limit        = CKRM_SHARE_DFLT_MAX_LIMIT;
	shares->unused_guarantee = CKRM_SHARE_DFLT_TOTAL_GUARANTEE;
	shares->cur_max_limit    = CKRM_SHARE_DFLT_MAX_LIMIT;
}

struct ckrm_cpu_class * ckrm_get_cpu_class(struct ckrm_core_class *core) {
	return ckrm_get_res_class(core, cpu_rcbs.resid, struct ckrm_cpu_class);
}


void* ckrm_alloc_cpu_class(struct ckrm_core_class *core, struct ckrm_core_class *parent) 
{		
	struct ckrm_cpu_class *cls;

	if (! parent) /*root class*/
		cls =  default_cpu_class;
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
		printk("alloc_cpu_class failed GFP_ATOMIC\n");

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

	if (!cls) 
		return;

	/*the default class can't be freed*/
	if (cls == default_cpu_class) 
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

	kfree(cls);
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

	rc = set_shares(new_share, cur, par);

	spin_unlock(&cls->cnt_lock);
	if (cls->parent) {
		spin_unlock(&parres->cnt_lock);
	}
	return rc;
}							
			
/*
 * translate the global_CVT to ticks
 */
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

	if (!cls) 
		return -EINVAL;

	seq_printf(sfile, "-------- CPU Class Status Start---------\n");
	seq_printf(sfile, "  gua= %d limit= %d\n",
		   cls->shares.my_guarantee,
		   cls->shares.my_limit);
	seq_printf(sfile, "  total_gua= %d limit= %d\n",
		   cls->shares.total_guarantee,
		   cls->shares.max_limit);
	seq_printf(sfile, "  used_gua= %d cur_limit= %d\n",
		   cls->shares.unused_guarantee,
		   cls->shares.cur_max_limit);

	seq_printf(sfile, "  Share= %d\n",cpu_class_weight(cls));
	seq_printf(sfile, "  cvt= %llu\n",cls->local_queues[0].local_cvt);
	seq_printf(sfile, "  total_ns= %llu\n",cls->stat.total_ns);
	seq_printf(sfile, "  prio= %d\n",cls->local_queues[0].classqueue_linkobj.prio);
	seq_printf(sfile, "  index= %d\n",cls->local_queues[0].classqueue_linkobj.index);
	seq_printf(sfile, "  run= %llu\n",cls->stat.local_stats[0].run);
	seq_printf(sfile, "  total= %llu\n",cls->stat.local_stats[0].total);
	seq_printf(sfile, "  cpu_demand= %lu\n",cls->stat.cpu_demand);

	seq_printf(sfile, "  effective_guarantee= %d\n",cls->stat.effective_guarantee);
	seq_printf(sfile, "  effective_limit= %d\n",cls->stat.effective_limit);
	seq_printf(sfile, "  effective_share= %d\n",cls->stat.effective_share);
	seq_printf(sfile, "-------- CPU Class Status END ---------\n");


	return 0;
}

/*
 * task will remain in the same cpu but on a different local runqueue
 */
static void ckrm_cpu_change_class(void *task, void *old, void *new)
{		
	struct task_struct *tsk = task;			   
	struct ckrm_cpu_class *newcls = new;
	unsigned long flags;
	struct runqueue *rq;
	prio_array_t *array;

	/*sanity checking*/
	if (!task || ! old || !new)
		return; 

	rq = task_rq_lock(tsk,&flags); 
	array = tsk->array;
	if (array) {
		dequeue_task(tsk,array);
		tsk->cpu_class = newcls;
		enqueue_task(tsk,rq_active(tsk,rq));
	} else {
		tsk->cpu_class = newcls;
	}
	task_rq_unlock(rq,&flags);
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
	printk("ckrm_cpu config='%s'\n",cfgstr);
	return 0;
}
	
struct ckrm_res_ctlr cpu_rcbs = {
	.res_name          = "CKRM CPU Class",
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
		printk("........init_ckrm_sched_res , resid= %d\n",resid);
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
 *         required for E14 since ckrm_init is called after sched_init
 */
	ckrm_alloc_cpu_class(NULL,NULL);
}


EXPORT_SYMBOL(ckrm_get_cpu_class);
