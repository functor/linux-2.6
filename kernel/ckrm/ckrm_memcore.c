/* ckrm_memcore.c - Memory Resource Manager for CKRM
 *
 * Copyright (C) Jiantao Kong, IBM Corp. 2003
 *           (C) Chandra Seetharaman, IBM Corp. 2004
 *
 * Provides a Memory Resource controller for CKRM
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
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/cache.h>
#include <linux/percpu.h>
#include <linux/pagevec.h>
#include <linux/parser.h>
#include <linux/ckrm_mem_inline.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/errno.h>

#define MEM_RES_NAME "mem"

#define CKRM_MEM_MAX_HIERARCHY 2 /* allows only upto 2 levels - 0, 1 & 2 */

/* all 1-level memory_share_class are chained together */
LIST_HEAD(ckrm_memclass_list);
spinlock_t ckrm_mem_lock; /* protects list above */
unsigned int ckrm_tot_lru_pages; /* # of pages in the system */
int ckrm_nr_mem_classes = 0;
struct ckrm_mem_res *ckrm_mem_root_class;
atomic_t ckrm_mem_real_count = ATOMIC_INIT(0);

EXPORT_SYMBOL_GPL(ckrm_memclass_list);
EXPORT_SYMBOL_GPL(ckrm_mem_lock);
EXPORT_SYMBOL_GPL(ckrm_tot_lru_pages);
EXPORT_SYMBOL_GPL(ckrm_nr_mem_classes);
EXPORT_SYMBOL_GPL(ckrm_mem_root_class);
EXPORT_SYMBOL_GPL(ckrm_mem_real_count);

void
memclass_release(struct kref *kref)
{
	struct ckrm_mem_res *cls = container_of(kref, 
				struct ckrm_mem_res, nr_users);
	kfree(cls);
}
EXPORT_SYMBOL_GPL(memclass_release);

static void
set_ckrm_tot_pages(void)
{
	struct zone *zone;
	int tot_lru_pages = 0;

	for_each_zone(zone) {
		tot_lru_pages += zone->nr_active;
		tot_lru_pages += zone->nr_inactive;
		tot_lru_pages += zone->free_pages;
	}
	ckrm_tot_lru_pages = tot_lru_pages;
}

/* Initialize rescls values
 * May be called on each rcfs unmount or as part of error recovery
 * to make share values sane.
 * Does not traverse hierarchy reinitializing children.
 */
static void
mem_res_initcls_one(struct ckrm_mem_res *res)
{
	int zindex = 0;
	struct zone *zone;

	memset(res, 0, sizeof(struct ckrm_mem_res));

	res->shares.my_guarantee     = CKRM_SHARE_DONTCARE;
	res->shares.my_limit         = CKRM_SHARE_DONTCARE;
	res->shares.total_guarantee  = CKRM_SHARE_DFLT_TOTAL_GUARANTEE;
	res->shares.max_limit        = CKRM_SHARE_DFLT_MAX_LIMIT;
	res->shares.unused_guarantee = CKRM_SHARE_DFLT_TOTAL_GUARANTEE;
	res->shares.cur_max_limit    = 0;

	res->pg_guar = CKRM_SHARE_DONTCARE;
	res->pg_limit = CKRM_SHARE_DONTCARE;

	INIT_LIST_HEAD(&res->mcls_list);
	INIT_LIST_HEAD(&res->shrink_list);

	for_each_zone(zone) {
		INIT_LIST_HEAD(&res->ckrm_zone[zindex].active_list);
		INIT_LIST_HEAD(&res->ckrm_zone[zindex].inactive_list);
		INIT_LIST_HEAD(&res->ckrm_zone[zindex].victim_list);
		res->ckrm_zone[zindex].nr_active = 0;
		res->ckrm_zone[zindex].nr_inactive = 0;
		res->ckrm_zone[zindex].zone = zone;
		res->ckrm_zone[zindex].memcls = res;
		zindex++;
	}

	res->pg_unused = 0;
	res->nr_dontcare = 1; /* for default class */
	kref_init(&res->nr_users);
}

static void
set_impl_guar_children(struct ckrm_mem_res *parres)
{
	struct ckrm_core_class *child = NULL;
	struct ckrm_mem_res *cres;
	int nr_dontcare = 1; // for defaultclass
	int guar, impl_guar;
	int resid = mem_rcbs.resid;

	ckrm_lock_hier(parres->core);
	while ((child = ckrm_get_next_child(parres->core, child)) != NULL) {
		cres = ckrm_get_res_class(child, resid, struct ckrm_mem_res);
		// treat NULL cres as don't care as that child is just being
		// created.
		// FIXME: need a better way to handle this case.
		if (!cres || cres->pg_guar == CKRM_SHARE_DONTCARE) {
			nr_dontcare++;
		}
	}

	parres->nr_dontcare = nr_dontcare;
	guar = (parres->pg_guar == CKRM_SHARE_DONTCARE) ?
			parres->impl_guar : parres->pg_unused;
	impl_guar = guar / parres->nr_dontcare;

	while ((child = ckrm_get_next_child(parres->core, child)) != NULL) {
		cres = ckrm_get_res_class(child, resid, struct ckrm_mem_res);
		if (cres && cres->pg_guar == CKRM_SHARE_DONTCARE) {
			cres->impl_guar = impl_guar;
			set_impl_guar_children(cres);
		}
	}
	ckrm_unlock_hier(parres->core);

}

static void *
mem_res_alloc(struct ckrm_core_class *core, struct ckrm_core_class *parent)
{
	struct ckrm_mem_res *res, *pres;

	BUG_ON(mem_rcbs.resid == -1);

	pres = ckrm_get_res_class(parent, mem_rcbs.resid, struct ckrm_mem_res);
	if (pres && (pres->hier == CKRM_MEM_MAX_HIERARCHY)) {
		printk(KERN_ERR "MEM_RC: only allows hieararchy of %d\n",
						CKRM_MEM_MAX_HIERARCHY);
		return NULL;
	}

	if ((parent == NULL) && (ckrm_mem_root_class != NULL)) {
		printk(KERN_ERR "MEM_RC: Only one root class is allowed\n");
		return NULL;
	}

	if ((parent != NULL) && (ckrm_mem_root_class == NULL)) {
		printk(KERN_ERR "MEM_RC: child class with no root class!!");
		return NULL;
	}

	res = kmalloc(sizeof(struct ckrm_mem_res), GFP_ATOMIC);

	if (res) {
		mem_res_initcls_one(res);
		res->core = core;
		res->parent = parent;
		spin_lock(&ckrm_mem_lock);
		list_add(&res->mcls_list, &ckrm_memclass_list);
		spin_unlock(&ckrm_mem_lock);
		if (parent == NULL) {
			/* I am the root class. So, set the max to *
			 * number of pages available in the system */
			res->pg_guar = ckrm_tot_lru_pages;
			res->pg_unused = ckrm_tot_lru_pages;
			res->pg_limit = ckrm_tot_lru_pages;
			res->hier = 0;
			ckrm_mem_root_class = res;
		} else {
			int guar;
			res->hier = pres->hier + 1;
			set_impl_guar_children(pres);
			guar = (pres->pg_guar == CKRM_SHARE_DONTCARE) ?
				pres->impl_guar : pres->pg_unused;
			res->impl_guar = guar / pres->nr_dontcare;
		}
		ckrm_nr_mem_classes++;
	} else
		printk(KERN_ERR "MEM_RC: alloc: GFP_ATOMIC failed\n");
	return res;
}

/*
 * It is the caller's responsibility to make sure that the parent only
 * has chilren that are to be accounted. i.e if a new child is added
 * this function should be called after it has been added, and if a
 * child is deleted this should be called after the child is removed.
 */
static void
child_maxlimit_changed_local(struct ckrm_mem_res *parres)
{
	int maxlimit = 0;
	struct ckrm_mem_res *childres;
	struct ckrm_core_class *child = NULL;

	/* run thru parent's children and get new max_limit of parent */
	ckrm_lock_hier(parres->core);
	while ((child = ckrm_get_next_child(parres->core, child)) != NULL) {
		childres = ckrm_get_res_class(child, mem_rcbs.resid,
				struct ckrm_mem_res);
		if (maxlimit < childres->shares.my_limit) {
			maxlimit = childres->shares.my_limit;
		}
	}
	ckrm_unlock_hier(parres->core);
	parres->shares.cur_max_limit = maxlimit;
}

/*
 * Recalculate the guarantee and limit in # of pages... and propagate the
 * same to children.
 * Caller is responsible for protecting res and for the integrity of parres
 */
static void
recalc_and_propagate(struct ckrm_mem_res * res, struct ckrm_mem_res * parres)
{
	struct ckrm_core_class *child = NULL;
	struct ckrm_mem_res *cres;
	int resid = mem_rcbs.resid;
	struct ckrm_shares *self = &res->shares;

	if (parres) {
		struct ckrm_shares *par = &parres->shares;

		/* calculate pg_guar and pg_limit */
		if (parres->pg_guar == CKRM_SHARE_DONTCARE ||
				self->my_guarantee == CKRM_SHARE_DONTCARE) {
			res->pg_guar = CKRM_SHARE_DONTCARE;
		} else if (par->total_guarantee) {
			u64 temp = (u64) self->my_guarantee * parres->pg_guar;
			do_div(temp, par->total_guarantee);
			res->pg_guar = (int) temp;
			res->impl_guar = CKRM_SHARE_DONTCARE;
		} else {
			res->pg_guar = 0;
			res->impl_guar = CKRM_SHARE_DONTCARE;
		}

		if (parres->pg_limit == CKRM_SHARE_DONTCARE ||
				self->my_limit == CKRM_SHARE_DONTCARE) {
			res->pg_limit = CKRM_SHARE_DONTCARE;
		} else if (par->max_limit) {
			u64 temp = (u64) self->my_limit * parres->pg_limit;
			do_div(temp, par->max_limit);
			res->pg_limit = (int) temp;
		} else {
			res->pg_limit = 0;
		}
	}

	/* Calculate unused units */
	if (res->pg_guar == CKRM_SHARE_DONTCARE) {
		res->pg_unused = CKRM_SHARE_DONTCARE;
	} else if (self->total_guarantee) {
		u64 temp = (u64) self->unused_guarantee * res->pg_guar;
		do_div(temp, self->total_guarantee);
		res->pg_unused = (int) temp;
	} else {
		res->pg_unused = 0;
	}

	/* propagate to children */
	ckrm_lock_hier(res->core);
	while ((child = ckrm_get_next_child(res->core, child)) != NULL) {
		cres = ckrm_get_res_class(child, resid, struct ckrm_mem_res);
		recalc_and_propagate(cres, res);
	}
	ckrm_unlock_hier(res->core);
	return;
}

static void
mem_res_free(void *my_res)
{
	struct ckrm_mem_res *res = my_res;
	struct ckrm_mem_res *pres;

	if (!res)
		return;

	ckrm_mem_migrate_all_pages(res, ckrm_mem_root_class);

	pres = ckrm_get_res_class(res->parent, mem_rcbs.resid,
			struct ckrm_mem_res);

	if (pres) {
		child_guarantee_changed(&pres->shares,
				res->shares.my_guarantee, 0);
		child_maxlimit_changed_local(pres);
		recalc_and_propagate(pres, NULL);
		set_impl_guar_children(pres);
	}

	/*
	 * Making it all zero as freeing of data structure could 
	 * happen later.
	 */
	res->shares.my_guarantee = 0;
	res->shares.my_limit = 0;
	res->pg_guar = 0;
	res->pg_limit = 0;
	res->pg_unused = 0;

	spin_lock(&ckrm_mem_lock);
	list_del_init(&res->mcls_list);
	spin_unlock(&ckrm_mem_lock);

	res->core = NULL;
	res->parent = NULL;
	kref_put(&res->nr_users, memclass_release);
	ckrm_nr_mem_classes--;
	return;
}

static int
mem_set_share_values(void *my_res, struct ckrm_shares *shares)
{
	struct ckrm_mem_res *res = my_res;
	struct ckrm_mem_res *parres;
	int rc;

	if (!res)
		return -EINVAL;

	parres = ckrm_get_res_class(res->parent, mem_rcbs.resid,
		struct ckrm_mem_res);

	rc = set_shares(shares, &res->shares, parres ? &parres->shares : NULL);

	if ((rc == 0) && (parres != NULL)) {
		child_maxlimit_changed_local(parres);
		recalc_and_propagate(parres, NULL);
		set_impl_guar_children(parres);
	}

	return rc;
}

static int
mem_get_share_values(void *my_res, struct ckrm_shares *shares)
{
	struct ckrm_mem_res *res = my_res;

	if (!res)
		return -EINVAL;
	printk(KERN_INFO "get_share called for %s resource of class %s\n",
			MEM_RES_NAME, res->core->name);
	*shares = res->shares;
	return 0;
}

static int
mem_get_stats(void *my_res, struct seq_file *sfile)
{
	struct ckrm_mem_res *res = my_res;
	struct zone *zone;
	int active = 0, inactive = 0, fr = 0;

	if (!res)
		return -EINVAL;

	seq_printf(sfile, "--------- Memory Resource stats start ---------\n");
	if (res == ckrm_mem_root_class) {
		int i = 0;
		for_each_zone(zone) {
			active += zone->nr_active;
			inactive += zone->nr_inactive;
			fr += zone->free_pages;
			i++;
		}
		seq_printf(sfile,"System: tot_pages=%d,active=%d,inactive=%d"
				",free=%d\n", ckrm_tot_lru_pages,
				active, inactive, fr);
	}
	seq_printf(sfile, "Number of pages used(including pages lent to"
			" children): %d\n", atomic_read(&res->pg_total));
	seq_printf(sfile, "Number of pages guaranteed: %d\n",
			res->pg_guar);
	seq_printf(sfile, "Maximum limit of pages: %d\n",
			res->pg_limit);
	seq_printf(sfile, "Total number of pages available"
			"(after serving guarantees to children): %d\n",
			res->pg_unused);
	seq_printf(sfile, "Number of pages lent to children: %d\n",
			res->pg_lent);
	seq_printf(sfile, "Number of pages borrowed from the parent: %d\n",
			res->pg_borrowed);
	seq_printf(sfile, "---------- Memory Resource stats end ----------\n");

	return 0;
}

static void
mem_change_resclass(void *tsk, void *old, void *new)
{
	struct mm_struct *mm;
	struct task_struct *task = tsk, *t1;
	struct ckrm_mem_res *prev_mmcls;

	if (!task->mm || (new == old) || (old == (void *) -1))
		return;

	mm = task->active_mm;
	spin_lock(&mm->peertask_lock);
	prev_mmcls = mm->memclass;

	if (new == NULL) {
		list_del_init(&task->mm_peers);
	} else {
		int found = 0;
		list_for_each_entry(t1, &mm->tasklist, mm_peers) {
			if (t1 == task) {
				found++;
				break;
			}
		}
		if (!found) {
			list_del_init(&task->mm_peers);
			list_add_tail(&task->mm_peers, &mm->tasklist);
		}
	}

	spin_unlock(&mm->peertask_lock);
	ckrm_mem_migrate_mm(mm, (struct ckrm_mem_res *) new);
	return;
}

#define MEM_FAIL_OVER "fail_over"
#define MEM_SHRINK_AT "shrink_at"
#define MEM_SHRINK_TO "shrink_to"
#define MEM_SHRINK_COUNT "num_shrinks"
#define MEM_SHRINK_INTERVAL "shrink_interval"

int ckrm_mem_fail_at = 110;
int ckrm_mem_shrink_at = 90;
int ckrm_mem_shrink_to = 80;
int ckrm_mem_shrink_count = 10;
int ckrm_mem_shrink_interval = 10;

EXPORT_SYMBOL_GPL(ckrm_mem_fail_at);
EXPORT_SYMBOL_GPL(ckrm_mem_shrink_at);
EXPORT_SYMBOL_GPL(ckrm_mem_shrink_to);

static int
mem_show_config(void *my_res, struct seq_file *sfile)
{
	struct ckrm_mem_res *res = my_res;

	if (!res)
		return -EINVAL;

	seq_printf(sfile, "res=%s,%s=%d,%s=%d,%s=%d,%s=%d,%s=%d\n",
		MEM_RES_NAME,
		MEM_FAIL_OVER, ckrm_mem_fail_at,
		MEM_SHRINK_AT, ckrm_mem_shrink_at,
		MEM_SHRINK_TO, ckrm_mem_shrink_to,
		MEM_SHRINK_COUNT, ckrm_mem_shrink_count,
		MEM_SHRINK_INTERVAL, ckrm_mem_shrink_interval);

	return 0;
}

typedef int __bitwise memclass_token_t;

enum memclass_token {
	mem_fail_over = (__force memclass_token_t) 1,
	mem_shrink_at = (__force memclass_token_t) 2,
	mem_shrink_to = (__force memclass_token_t) 3,
	mem_shrink_count = (__force memclass_token_t) 4,
	mem_shrink_interval = (__force memclass_token_t) 5,
	mem_err = (__force memclass_token_t) 6
};

static match_table_t mem_tokens = {
	{mem_fail_over, MEM_FAIL_OVER "=%d"},
	{mem_shrink_at, MEM_SHRINK_AT "=%d"},
	{mem_shrink_to, MEM_SHRINK_TO "=%d"},
	{mem_shrink_count, MEM_SHRINK_COUNT "=%d"},
	{mem_shrink_interval, MEM_SHRINK_INTERVAL "=%d"},
	{mem_err, NULL},
};

static int
mem_set_config(void *my_res, const char *cfgstr)
{
	char *p;
	struct ckrm_mem_res *res = my_res;
	int err = 0, val;

	if (!res)
		return -EINVAL;

	while ((p = strsep((char**)&cfgstr, ",")) != NULL) {
		substring_t args[MAX_OPT_ARGS];
		int token;
		if (!*p)
			continue;

		token = match_token(p, mem_tokens, args);
		switch (token) {
		case mem_fail_over:
			if (match_int(args, &val) || (val <= 0)) {
				err = -EINVAL;
			} else {
				ckrm_mem_fail_at = val;
			}
			break;
		case mem_shrink_at:
			if (match_int(args, &val) || (val <= 0)) {
				err = -EINVAL;
			} else {
				ckrm_mem_shrink_at = val;
			}
			break;
		case mem_shrink_to:
			if (match_int(args, &val) || (val < 0) || (val > 100)) {
				err = -EINVAL;
			} else {
				ckrm_mem_shrink_to = val;
			}
			break;
		case mem_shrink_count:
			if (match_int(args, &val) || (val <= 0)) {
				err = -EINVAL;
			} else {
				ckrm_mem_shrink_count = val;
			}
			break;
		case mem_shrink_interval:
			if (match_int(args, &val) || (val <= 0)) {
				err = -EINVAL;
			} else {
				ckrm_mem_shrink_interval = val;
			}
			break;
		default:
			err = -EINVAL;
		}
	}
	return err;
}

static int
mem_reset_stats(void *my_res)
{
	struct ckrm_mem_res *res = my_res;
	printk(KERN_INFO "MEM_RC: reset stats called for class %s\n",
				res->core->name);
	return 0;
}

struct ckrm_res_ctlr mem_rcbs = {
	.res_name          = MEM_RES_NAME,
	.res_hdepth        = CKRM_MEM_MAX_HIERARCHY,
	.resid             = -1,
	.res_alloc         = mem_res_alloc,
	.res_free          = mem_res_free,
	.set_share_values  = mem_set_share_values,
	.get_share_values  = mem_get_share_values,
	.get_stats         = mem_get_stats,
	.change_resclass   = mem_change_resclass,
	.show_config       = mem_show_config,
	.set_config        = mem_set_config,
	.reset_stats       = mem_reset_stats,
};

EXPORT_SYMBOL_GPL(mem_rcbs);

int __init
init_ckrm_mem_res(void)
{
	struct ckrm_classtype *clstype;
	int resid = mem_rcbs.resid;

	set_ckrm_tot_pages();
	spin_lock_init(&ckrm_mem_lock);
	clstype = ckrm_find_classtype_by_name("taskclass");
	if (clstype == NULL) {
		printk(KERN_INFO " Unknown ckrm classtype<taskclass>");
		return -ENOENT;
	}

	if (resid == -1) {
		resid = ckrm_register_res_ctlr(clstype, &mem_rcbs);
		if (resid != -1) {
			mem_rcbs.classtype = clstype;
		}
	}
	return ((resid < 0) ? resid : 0);
}

void __exit
exit_ckrm_mem_res(void)
{
	ckrm_unregister_res_ctlr(&mem_rcbs);
	mem_rcbs.resid = -1;
}

module_init(init_ckrm_mem_res)
module_exit(exit_ckrm_mem_res)
MODULE_LICENSE("GPL");
