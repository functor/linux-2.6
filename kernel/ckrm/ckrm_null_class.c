/* kernel/ckrm/ckrm_null_class.c - NULL TaskClass controller for CKRM
 *
 * Copyright (C) Haoqiang Zheng,     IBM Corp. 2004
 *           (C) Hubertus Franke,    IBM Corp. 2004
 *
 * Copyright (C) Marc E. Fiuczynski, Princeton University 2005
 *               Adapted from ckrm_cpu_class.c.
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
#include <linux/ckrm.h>
#include <linux/ckrm_rc.h>
#include <linux/ckrm_tc.h>
#include <linux/ckrm_classqueue.h>
#include <linux/seq_file.h>

#define CKRM_NULL_CLASS_MAGIC 0xdeadbeef

static struct ckrm_res_ctlr null_rcbs;

/*
 * manages the class status
 * there should be only one instance of this object for each class in the whole system  
 */
struct ckrm_null_class {
	struct ckrm_core_class *core;
	struct ckrm_core_class *parent;
	struct ckrm_shares shares;
	spinlock_t cnt_lock;	// always grab parent's lock first and then child's
	unsigned long magic;	//for debugging
};

/*
 *  initialize a class object and its local queues
 */
static void init_null_class(struct ckrm_null_class *cls,ckrm_shares_t* shares) 
{
	cls->shares = *shares;
	cls->cnt_lock = SPIN_LOCK_UNLOCKED;
	cls->magic = CKRM_NULL_CLASS_MAGIC;
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

static inline int valid_null_class(struct ckrm_null_class * cls)
{
	return (cls && cls->magic == CKRM_NULL_CLASS_MAGIC);
}


static struct ckrm_null_class * ckrm_get_null_class(struct ckrm_core_class *core)
{
	struct ckrm_null_class * cls;
	cls = ckrm_get_res_class(core, null_rcbs.resid, struct ckrm_null_class);
  	if (valid_null_class(cls))
		return cls;
	else
		return NULL;
}


static struct ckrm_null_class default_null_class_obj;

static struct ckrm_null_class * get_default_null_class(void) {
	return (&default_null_class_obj);
}


static void* ckrm_alloc_null_class(struct ckrm_core_class *core, struct ckrm_core_class *parent) 
{		
	struct ckrm_null_class *cls;

	if (! parent) /*root class*/
		cls =  get_default_null_class();
	else
		cls = (struct ckrm_null_class *) kmalloc(sizeof(struct ckrm_null_class),GFP_ATOMIC);

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
			init_null_class(cls,&shares);
			cls->core = core;
			cls->parent = parent;			
		}
	} else
		printk(KERN_ERR"alloc_null_class failed\n");

	return cls;
}		

/*
 * hzheng: this is not a stable implementation
 *         need to check race condition issue here
 */		
static void ckrm_free_null_class(void *my_res) 
{			
	struct ckrm_null_class *cls = my_res, *parres, *childres;
	ckrm_core_class_t *child = NULL;
	int maxlimit;

	if (!cls) 
		return;

	/*the default class can't be freed*/
	if (cls == get_default_null_class()) 
		return;

	// Assuming there will be no children when this function is called
	parres = ckrm_get_null_class(cls->parent);

	// return child's limit/guarantee to parent node
	spin_lock(&parres->cnt_lock);
	child_guarantee_changed(&parres->shares, cls->shares.my_guarantee, 0);

	// run thru parent's children and get the new max_limit of the parent
	ckrm_lock_hier(parres->core);
	maxlimit = 0;
	while ((child = ckrm_get_next_child(parres->core, child)) != NULL) {
		childres = ckrm_get_null_class(child);
		if (maxlimit < childres->shares.my_limit) {
			maxlimit = childres->shares.my_limit;
		}
	}
	ckrm_unlock_hier(parres->core);
	if (parres->shares.cur_max_limit < maxlimit) {
		parres->shares.cur_max_limit = maxlimit;
	}

	spin_unlock(&parres->cnt_lock);
	kfree(cls);
}				

/*
 *  the system will adjust to the new share automatically  
 */			
static int ckrm_null_set_share(void *my_res, struct ckrm_shares *new_share) 
{	
        struct ckrm_null_class *parres, *cls = my_res;
        struct ckrm_shares *cur = &cls->shares, *par;
        int rc = -EINVAL;

        if (!cls)
		return rc;

        if (cls->parent) {
                parres = ckrm_get_null_class(cls->parent);
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

	return rc;
}							
			
static int ckrm_null_get_share(void *my_res,
			      struct ckrm_shares *shares)
{			
	struct ckrm_null_class *cls = my_res;

        if (!cls)
		return -EINVAL;
	*shares = cls->shares;
	return 0;
}				

static int ckrm_null_get_stats(void *my_res, struct seq_file * sfile)
{
	struct ckrm_null_class *cls = my_res;

	if (!cls) 
		return -EINVAL;

	seq_printf(sfile, "-------- Null Class Status Start---------\n");
	seq_printf(sfile, "Share:\n\tgrt= %d limit= %d total_grt= %d max_limit= %d\n",
		   cls->shares.my_guarantee,
		   cls->shares.my_limit,
		   cls->shares.total_guarantee,
		   cls->shares.max_limit);
	seq_printf(sfile, "\tunused_grt= %d cur_max_limit= %d\n",
		   cls->shares.unused_guarantee,
		   cls->shares.cur_max_limit);

	seq_printf(sfile, "-------- Null Class Status END ---------\n");

	return 0;
}

/*
 * task will remain in the same null but on a different local runqueue
 */
static void ckrm_null_change_class(void *task, void *old, void *new)
{		
	/*sanity checking*/
	if (!task || ! old || !new)
		return; 

	/* hook to controller */
}							

/*dummy function, not used*/
static int ckrm_null_show_config(void *my_res, struct seq_file *sfile)
{
	struct ckrm_null_class *cls = my_res;

	if (!cls) 
		return -EINVAL;

	seq_printf(sfile, "cls=%s,parameter=somevalue\n","ckrm_null class");
	return 0;
}

/*dummy function, not used*/
static int ckrm_null_set_config(void *my_res, const char *cfgstr)
{
	struct ckrm_nullclass *cls = my_res;

	if (!cls) 
		return -EINVAL;
	printk(KERN_DEBUG "ckrm_null config='%s'\n",cfgstr);
	return 0;
}
	
static struct ckrm_res_ctlr null_rcbs = {
	.res_name          = "null",
	.res_hdepth        = 1,
	.resid             = -1,
	.res_alloc         = ckrm_alloc_null_class,
	.res_free          = ckrm_free_null_class,
	.set_share_values  = ckrm_null_set_share,
	.get_share_values  = ckrm_null_get_share,
	.get_stats         = ckrm_null_get_stats,
	.show_config       = ckrm_null_show_config,
	.set_config        = ckrm_null_set_config,
	.change_resclass   = ckrm_null_change_class,
};

int __init init_ckrm_null_res(void)
{
	struct ckrm_classtype *clstype;
	int resid = null_rcbs.resid;

	clstype = ckrm_find_classtype_by_name("taskclass");
	if (clstype == NULL) {
		printk(KERN_INFO" Unknown ckrm classtype<taskclass>");
		return -ENOENT;
	}

	/* Initialize default class obj before registering with core */
	ckrm_alloc_null_class(NULL,NULL);

	if (resid == -1) { /*not registered */
		resid = ckrm_register_res_ctlr(clstype,&null_rcbs);
		printk(KERN_DEBUG "........init_ckrm_null_res , resid= %d\n",resid);
	}
	return 0;
}

void __exit exit_ckrm_null_res(void)
{
	ckrm_unregister_res_ctlr(&null_rcbs);
	null_rcbs.resid = -1;
}

module_init(init_ckrm_null_res)
module_exit(exit_ckrm_null_res)
