/* linux/drivers/block/ckrm_io.c : Block I/O Resource Controller for CKRM
 *
 * Copyright (C) Shailabh Nagar, IBM Corp. 2004
 * 
 * 
 * Provides best-effort block I/O bandwidth control for CKRM 
 * This file provides the CKRM API. The underlying scheduler is a 
 * modified Complete-Fair Queueing (CFQ) iosched.
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
 * 29 July 2004
 *          Third complete rewrite for CKRM's current API
 *
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/errno.h>
#include <asm/div64.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/fs.h>

#include <linux/ckrm_tc.h>
#include <linux/ckrm-io.h>

/* sectorate == 512 byte sectors served in CFQ_EPOCH ns*/

/* CKI_ROOTSECTORATE needs to be made configurable from outside */
#define CKI_ROOTSECTORATE	100000
#define CKI_MINSECTORATE	100

#define CKI_IOUSAGE_UNIT	512

typedef struct ckrm_io_stats{
	struct timeval       epochstart ; /* all measurements relative to this 
					     start time */
	unsigned long        blksz;  /* size of bandwidth unit */
	atomic_t             blkrd;  /* read units submitted to DD */
	atomic_t             blkwr; /* write units submitted to DD */

	int nskip;			/* # times q skipped 	*/
	unsigned long navsec;		/* avg sectors serviced */
	int timedout;			/* # times gap > epoch 	*/
	u64 sec[2];			/* sectors serviced in 
					   prev & curr epochs 	*/
} cki_stats_t;          /* per class I/O statistics */

/* Note
 * Currently local unit == CFQ I/O priority directly.
 * CFQ ionice values have an implied bandwidth share so they
 * can be added, subdivided etc. as long as the initial allocation
 * of the systemwide default's total is set to the highest CFQ ionice
 * value (== 100% of disk bandwidth)
 */

typedef struct ckrm_io_class {

	struct ckrm_core_class *core;
	struct ckrm_core_class *parent;
	
	struct ckrm_shares shares;
	spinlock_t	shares_lock; /* protect share changes */
	
	/* Absolute shares of this class
	 * in local units. 
	 */

	cfqlim_t cfqpriv;	/* Data common with cfq priolvl's */ 	


	int cnt_guarantee; /* Allocation as parent */
	int cnt_unused;    /* Allocation to default subclass */
	int cnt_limit;

	/* Statistics, for class and default subclass */
	cki_stats_t stats; 
	cki_stats_t mystats;

} cki_icls_t;


/* Internal functions */
static inline void cki_reset_stats(cki_stats_t *usg);
static inline void init_icls_one(cki_icls_t *icls);
static void cki_recalc_propagate(cki_icls_t *res, cki_icls_t *parres);

/* External functions e.g. interface to ioscheduler */
void *cki_tsk_icls (struct task_struct *tsk);
int cki_tsk_ioprio (struct task_struct *tsk);

extern void cki_cfq_set(icls_tsk_t tskicls, icls_ioprio_t tskioprio, icls_tsk_t tskcfqpriv);

/* CKRM Resource Controller API functions */
static void * cki_alloc(struct ckrm_core_class *this,
			struct ckrm_core_class * parent);
static void cki_free(void *res);
static int cki_setshare(void *res, struct ckrm_shares * shares);
static int cki_getshare(void *res, struct ckrm_shares * shares);
static int cki_getstats(void *res, struct seq_file *);
static int cki_resetstats(void *res);
static int cki_showconfig(void *res, struct seq_file *sfile);
static int cki_setconfig(void *res, const char *cfgstr);
static void cki_chgcls(void *tsk, void *oldres, void *newres);


struct ckrm_res_ctlr cki_rcbs;

static inline void cki_reset_stats(cki_stats_t *stats)
{
	if (stats) {
		atomic_set(&stats->blkrd,0);
		atomic_set(&stats->blkwr,0);
	}
}

static inline void init_icls_stats(cki_icls_t *icls)
{
	struct timeval tv;

	do_gettimeofday(&tv);
	icls->stats.epochstart = icls->mystats.epochstart = tv;
	icls->stats.blksz = icls->mystats.blksz = CKI_IOUSAGE_UNIT;
	cki_reset_stats(&icls->stats);
	cki_reset_stats(&icls->mystats);
}	

/* Initialize icls to default values 
 * No other classes touched, locks not reinitialized.
 */

static inline void init_icls_one(cki_icls_t *icls)
{
	/* Zero initial guarantee for scalable creation of
	   multiple classes */

	/* Try out a new set */
	
	icls->shares.my_guarantee = CKRM_SHARE_DONTCARE;
	icls->shares.my_limit = CKRM_SHARE_DONTCARE;
	icls->shares.total_guarantee = CKRM_SHARE_DFLT_TOTAL_GUARANTEE;
	icls->shares.max_limit = CKRM_SHARE_DFLT_MAX_LIMIT;
	icls->shares.unused_guarantee = icls->shares.total_guarantee;
	icls->shares.cur_max_limit = 0;

	icls->cnt_guarantee = CKRM_SHARE_DONTCARE;
	icls->cnt_unused = CKRM_SHARE_DONTCARE;
	icls->cnt_limit = CKRM_SHARE_DONTCARE;
	
	init_icls_stats(icls);
}

/* Recalculate absolute shares from relative
 * Caller should hold a lock on icls
 */

static void cki_recalc_propagate(cki_icls_t *res, cki_icls_t *parres)
{

	ckrm_core_class_t *child = NULL;
	cki_icls_t *childres;
	int resid = cki_rcbs.resid;
	u64 temp;

	if (parres) {
		struct ckrm_shares *par = &parres->shares;
		struct ckrm_shares *self = &res->shares;


		if (parres->cnt_guarantee == CKRM_SHARE_DONTCARE) {
			res->cnt_guarantee = CKRM_SHARE_DONTCARE;
		} else if (par->total_guarantee) {
			temp = (u64) self->my_guarantee * 
				parres->cnt_guarantee;
			do_div(temp, par->total_guarantee);
			res->cnt_guarantee = (int) temp;
		} else {
			res->cnt_guarantee = 0;
		}


		if (parres->cnt_limit == CKRM_SHARE_DONTCARE) {
			res->cnt_limit = CKRM_SHARE_DONTCARE;
			atomic_set(&res->cfqpriv.sectorate,CKI_MINSECTORATE);
		} else {
			if (par->max_limit) {
				temp = (u64) self->my_limit * 
					parres->cnt_limit;
				do_div(temp, par->max_limit);
				res->cnt_limit = (int) temp;
			} else {
				res->cnt_limit = 0;
			}
			atomic_set(&res->cfqpriv.sectorate,res->cnt_limit);
		}
		
		if (res->cnt_guarantee == CKRM_SHARE_DONTCARE) {
			res->cnt_unused = CKRM_SHARE_DONTCARE;
		} else {
			if (self->total_guarantee) {
				temp = (u64) self->unused_guarantee * 
					res->cnt_guarantee;
				do_div(temp, self->total_guarantee);
				res->cnt_unused = (int) temp;
			} else {
				res->cnt_unused = 0;
			}

		}
		
	}
	// propagate to children
	ckrm_lock_hier(res->core);
	while ((child = ckrm_get_next_child(res->core,child)) != NULL){
		childres = ckrm_get_res_class(child, resid, 
					      cki_icls_t);
		
		spin_lock(&childres->shares_lock);
		cki_recalc_propagate(childres, res);
		spin_unlock(&childres->shares_lock);
	}
	ckrm_unlock_hier(res->core);
}

void *cki_tsk_icls(struct task_struct *tsk)
{
	return (void *) ckrm_get_res_class(class_core(tsk->taskclass),
					   cki_rcbs.resid, cki_icls_t);
}

int cki_tsk_ioprio(struct task_struct *tsk)
{
	/* Don't use I/O priorities for now */
	return IOPRIO_NORM;
}

void *cki_tsk_cfqpriv(struct task_struct *tsk)
{
	cki_icls_t *icls = ckrm_get_res_class(class_core(tsk->taskclass),
					   cki_rcbs.resid, cki_icls_t);
	return (void *)&(icls->cfqpriv);
}


static void *cki_alloc(struct ckrm_core_class *core,
			 struct ckrm_core_class *parent)
{
	cki_icls_t *icls;
	
	icls = kmalloc(sizeof(cki_icls_t), GFP_ATOMIC);
	if (!icls) {
		printk(KERN_ERR "cki_res_alloc failed GFP_ATOMIC\n");
		return NULL;
	}

	memset(icls, 0, sizeof(cki_icls_t));
	icls->core = core;
	icls->parent = parent;
	icls->shares_lock = SPIN_LOCK_UNLOCKED;

	init_icls_one(icls);

	if (parent == NULL) {
		icls->cnt_guarantee =  CKI_ROOTSECTORATE;
		icls->cnt_unused = CKI_ROOTSECTORATE;
		icls->cnt_limit = CKI_ROOTSECTORATE;
		atomic_set(&(icls->cfqpriv.sectorate),icls->cnt_limit);
	}
	try_module_get(THIS_MODULE);
	return icls;
}

static void cki_free(void *res)
{
	cki_icls_t *icls = res, *parres, *childres;
	ckrm_core_class_t *child = NULL;
	int maxlimit, resid = cki_rcbs.resid;

	
	if (!res)
		return;

	/* Deallocate CFQ queues */

	/* Currently CFQ queues are deallocated when empty. Since no task 
	 * should belong to this icls, no new requests will get added to the
	 * CFQ queue. 
	 * 
	 * When CFQ switches to persistent queues, call its "put" function
	 * so it gets deallocated after the last pending request is serviced.
	 *
	 */

	parres = ckrm_get_res_class(icls->parent, resid, cki_icls_t);
	if (!parres) {
		printk(KERN_ERR "cki_free: error getting "
		       "resclass from core \n");
		return;
	}

	/* Update parent's shares */
	spin_lock(&parres->shares_lock);

	child_guarantee_changed(&parres->shares, icls->shares.my_guarantee, 0);
	parres->cnt_unused += icls->cnt_guarantee;

	// run thru parent's children and get the new max_limit of the parent
	ckrm_lock_hier(parres->core);
	maxlimit = 0;
	while ((child = ckrm_get_next_child(parres->core, child)) != NULL) {
		childres = ckrm_get_res_class(child, resid, cki_icls_t);
		if (maxlimit < childres->shares.my_limit) {
			maxlimit = childres->shares.my_limit;
		}
	}
	ckrm_unlock_hier(parres->core);
	if (parres->shares.cur_max_limit < maxlimit) {
		parres->shares.cur_max_limit = maxlimit;
	}
	spin_unlock(&parres->shares_lock);

	kfree(res);
	module_put(THIS_MODULE);
	return;
}


static int cki_setshare(void *res, struct ckrm_shares *new)
{
	cki_icls_t *icls = res, *parres;
	struct ckrm_shares *cur, *par;
	int rc = -EINVAL, resid = cki_rcbs.resid;

	if (!icls) 
		return rc;

	cur = &icls->shares; 
	if (icls->parent) {
		parres =
		    ckrm_get_res_class(icls->parent, resid, cki_icls_t);
		if (!parres) {
			pr_debug("cki_setshare: invalid resclass\n");
			return -EINVAL;
		}
		spin_lock(&parres->shares_lock);
		spin_lock(&icls->shares_lock);
		par = &parres->shares;
	} else {
		spin_lock(&icls->shares_lock);
		parres = NULL;
		par = NULL;
	}

	rc = set_shares(new, cur, par);

	if ((!rc) && parres) {
		if (parres->cnt_guarantee == CKRM_SHARE_DONTCARE) {
			parres->cnt_unused = CKRM_SHARE_DONTCARE;
		} else if (par->total_guarantee) {
			u64 temp = (u64) par->unused_guarantee * 
				parres->cnt_guarantee;
			do_div(temp, par->total_guarantee);
			parres->cnt_unused = (int) temp;
		} else {
			parres->cnt_unused = 0;
		}
		cki_recalc_propagate(res, parres);
	}
	spin_unlock(&icls->shares_lock);
	if (icls->parent) {
		spin_unlock(&parres->shares_lock);
	}
	return rc;
}

static int cki_getshare(void *res, struct ckrm_shares * shares)
{
	cki_icls_t *icls = res;

	if (!icls)
		return -EINVAL;
	*shares = icls->shares;
	return 0;
}

static int cki_getstats(void *res, struct seq_file *sfile)
{
	cki_icls_t *icls = res;

	if (!icls)
		return -EINVAL;

	seq_printf(sfile, "abs limit %d\n",icls->cnt_limit);
	seq_printf(sfile, "skip %d timdout %d avsec %lu rate %ld "
		   " sec0 %ld sec1 %ld\n",
		   icls->cfqpriv.nskip,
		   icls->cfqpriv.timedout,
		   icls->cfqpriv.navsec,
		   atomic_read(&(icls->cfqpriv.sectorate)),
		   (unsigned long)icls->cfqpriv.sec[0],
		   (unsigned long)icls->cfqpriv.sec[1]);

	return 0;
}

static int cki_resetstats(void *res)
{
	cki_icls_t *icls = res;

	if (!res)
		return -EINVAL;
	
	init_icls_stats(icls);
	return 0;
}

static int cki_showconfig(void *res, struct seq_file *sfile)
{
	return -ENOSYS;
}
	
static int cki_setconfig(void *res, const char *cfgstr)
{
	return -ENOSYS;
}

static void cki_chgcls(void *tsk, void *oldres, void *newres)
{
	/* cki_icls_t *oldicls = oldres, *newicls = newres; */
	
	/* Nothing needs to be done 
	 * Future requests from task will go to the new class's CFQ q
	 * Old ones will continue to get satisfied from the original q
	 * 
	 * Once CFQ moves to a persistent queue model and if refcounts on 
	 * icls's CFQ queues are used, a decrement op would be needed here
	 */
	
	return;
}



struct ckrm_res_ctlr cki_rcbs = {
	.res_name = "io",
	.res_hdepth = 1,
	.resid = -1,
	.res_alloc = cki_alloc,
	.res_free = cki_free,
	.set_share_values = cki_setshare,
	.get_share_values = cki_getshare,
	.get_stats = cki_getstats,
	.reset_stats = cki_resetstats,
	.show_config = cki_showconfig,
	.set_config = cki_setconfig,
	.change_resclass = cki_chgcls,
};



int __init cki_init(void)
{
	struct ckrm_classtype *clstype;
	int resid = cki_rcbs.resid;

	clstype = ckrm_find_classtype_by_name("taskclass");
	if (clstype == NULL) {
		printk(KERN_INFO "init_cki: classtype<taskclass> not found\n");
		return -ENOENT;
	}

	if (resid == -1) {
		resid = ckrm_register_res_ctlr(clstype, &cki_rcbs);
		if (resid != -1) {
			cki_rcbs.classtype = clstype;
			cki_cfq_set(cki_tsk_icls,cki_tsk_ioprio,cki_tsk_cfqpriv);
		}
	}
	
	return 0;
}
	
void __exit cki_exit(void)
{
	ckrm_unregister_res_ctlr(&cki_rcbs);
	cki_rcbs.resid = -1;
	cki_rcbs.classtype = NULL; 
	cki_cfq_set(NULL,NULL,NULL);
}

module_init(cki_init)
module_exit(cki_exit)

MODULE_AUTHOR("Shailabh Nagar <nagar@watson.ibm.com>");
MODULE_DESCRIPTION("CKRM Disk I/O Resource Controller");
MODULE_LICENSE("GPL");

