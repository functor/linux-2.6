/* ckrm_mem.c - Memory Resource Manager for CKRM
 *
 * Copyright (C) Chandra Seetharaman, IBM Corp. 2004
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

/* Code Description: TBD
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <asm/errno.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/cache.h>
#include <linux/percpu.h>
#include <linux/pagevec.h>

#include <linux/ckrm_mem_inline.h>

#include <asm/uaccess.h>
#include <asm/pgtable.h>

#define MEM_NAME "mem"

#define CKRM_MEM_MAX_HIERARCHY 2 // allows only upto 2 levels - 0, 1 & 2

/* all 1-level memory_share_class are chained together */
static LIST_HEAD(ckrm_memclass_list);
LIST_HEAD(ckrm_shrink_list);
EXPORT_SYMBOL(ckrm_shrink_list);
spinlock_t ckrm_mem_lock = SPIN_LOCK_UNLOCKED; // protects both lists above
EXPORT_SYMBOL(ckrm_mem_lock);
unsigned int ckrm_tot_lru_pages; // total # of pages in the system
							 // currently doesn't handle memory add/remove
EXPORT_SYMBOL(ckrm_tot_lru_pages);

static ckrm_mem_res_t *ckrm_mem_root_class;
atomic_t ckrm_mem_real_count = ATOMIC_INIT(0);
EXPORT_SYMBOL(ckrm_mem_real_count);
static void ckrm_mem_evaluate_all_pages(void);

/* Initialize rescls values
 * May be called on each rcfs unmount or as part of error recovery
 * to make share values sane.
 * Does not traverse hierarchy reinitializing children.
 */

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

static void
mem_res_initcls_one(void *my_res)
{
	ckrm_mem_res_t *res = my_res;

	memset(res, 0, sizeof(ckrm_mem_res_t));

	res->shares.my_guarantee     = CKRM_SHARE_DONTCARE;
	res->shares.my_limit         = CKRM_SHARE_DONTCARE;
	res->shares.total_guarantee  = CKRM_SHARE_DFLT_TOTAL_GUARANTEE;
	res->shares.max_limit        = CKRM_SHARE_DFLT_MAX_LIMIT;
	res->shares.unused_guarantee = CKRM_SHARE_DFLT_TOTAL_GUARANTEE;
	res->shares.cur_max_limit    = 0;

	res->pg_guar = CKRM_SHARE_DONTCARE;
	res->pg_limit = CKRM_SHARE_DONTCARE;
	res->pg_unused = 0;
}

static void *
mem_res_alloc(struct ckrm_core_class *core, struct ckrm_core_class *parent)
{
	ckrm_mem_res_t *res, *parres;

	if (mem_rcbs.resid == -1) {
		return NULL;
	}

	parres = ckrm_get_res_class(parent, mem_rcbs.resid, ckrm_mem_res_t);
	if (parres && (parres->hier == CKRM_MEM_MAX_HIERARCHY)) {
		// allows only upto CKRM_MEM_MAX_HIERARCHY
		return NULL;
	}

	if (unlikely((parent == NULL) && (ckrm_mem_root_class != NULL))) {
		printk(KERN_ERR "MEM_RC: Only one root class is allowed\n");
		return NULL;
	}
		
	if (unlikely((parent != NULL) && (ckrm_mem_root_class == NULL))) {
		printk(KERN_ERR "MEM_RC: creating child class without root class\n");
		return NULL;
	}
		
	res = kmalloc(sizeof(ckrm_mem_res_t), GFP_ATOMIC);
	
	if (res) {
		mem_res_initcls_one(res);
		res->core = core;
		res->parent = parent;
		spin_lock(&ckrm_mem_lock);
		list_add(&res->mcls_list, &ckrm_memclass_list);
		spin_unlock(&ckrm_mem_lock);
		if (parent == NULL) {
			// I am part of the root class. So, set the max to 
			// number of pages available
			res->pg_guar = ckrm_tot_lru_pages;
			res->pg_unused = ckrm_tot_lru_pages;
			res->pg_limit = ckrm_tot_lru_pages;
			res->hier = 0;
			ckrm_mem_root_class = res;
		} else {
			res->hier = parres->hier + 1;
		}
		mem_class_get(res);
	}
	else
		printk(KERN_ERR "mem_res_alloc: failed GFP_ATOMIC alloc\n");
	return res;
}

/*
 * It is the caller's responsibility to make sure that the parent only
 * has chilren that are to be accounted. i.e if a new child is added
 * this function should be called after it has been added, and if a
 * child is deleted this should be called after the child is removed.
 */
static void
child_maxlimit_changed_local(ckrm_mem_res_t *parres)
{
	int maxlimit = 0;
	ckrm_mem_res_t *childres;
	ckrm_core_class_t *child = NULL;

	// run thru parent's children and get the new max_limit of the parent
	ckrm_lock_hier(parres->core);
	while ((child = ckrm_get_next_child(parres->core, child)) != NULL) {
		childres = ckrm_get_res_class(child, mem_rcbs.resid,
				ckrm_mem_res_t);
		if (maxlimit < childres->shares.my_limit) {
			maxlimit = childres->shares.my_limit;
		}
	}
	ckrm_unlock_hier(parres->core);
	parres->shares.cur_max_limit = maxlimit;
}

static void
mem_res_free(void *my_res)
{
	ckrm_mem_res_t *res = my_res;
	ckrm_mem_res_t *parres;

	if (!res) 
		return;

	res->shares.my_guarantee = 0;
	res->shares.my_limit = 0;
	res->pg_guar = 0;
	res->pg_limit = 0;
	res->pg_unused = 0;

	parres = ckrm_get_res_class(res->parent, mem_rcbs.resid, ckrm_mem_res_t);
	// return child's limit/guarantee to parent node
	if (parres) {
		child_guarantee_changed(&parres->shares, res->shares.my_guarantee, 0);
		child_maxlimit_changed_local(parres);
	}
	spin_lock(&ckrm_mem_lock);
	list_del(&res->mcls_list);
	spin_unlock(&ckrm_mem_lock);
	mem_class_put(res);
	ckrm_mem_evaluate_all_pages();
	return;
}

/*
 * Recalculate the guarantee and limit in # of pages... and propagate the
 * same to children.
 * Caller is responsible for protecting res and for the integrity of parres
 */
static void
recalc_and_propagate(ckrm_mem_res_t * res, ckrm_mem_res_t * parres)
{
	ckrm_core_class_t *child = NULL;
	ckrm_mem_res_t *childres;
	int resid = mem_rcbs.resid;
	struct ckrm_shares *self = &res->shares;

	if (parres) {
		struct ckrm_shares *par = &parres->shares;

		// calculate pg_guar and pg_limit
		//
		if (parres->pg_guar == CKRM_SHARE_DONTCARE ||
				self->my_guarantee == CKRM_SHARE_DONTCARE) {
			res->pg_guar = CKRM_SHARE_DONTCARE;
		} else if (par->total_guarantee) {
			u64 temp = (u64) self->my_guarantee * parres->pg_guar;
			do_div(temp, par->total_guarantee);
			res->pg_guar = (int) temp;
		} else {
			res->pg_guar = 0;
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

	// Calculate unused units
	if (res->pg_guar == CKRM_SHARE_DONTCARE) {
		res->pg_unused = CKRM_SHARE_DONTCARE;
	} else if (self->total_guarantee) {
		u64 temp = (u64) self->unused_guarantee * res->pg_guar;
		do_div(temp, self->total_guarantee);
		res->pg_unused = (int) temp;
	} else {
		res->pg_unused = 0;
	}

	// propagate to children
	ckrm_lock_hier(res->core);
	while ((child = ckrm_get_next_child(res->core, child)) != NULL) {
		childres = ckrm_get_res_class(child, resid, ckrm_mem_res_t);
		recalc_and_propagate(childres, res);
	}
	ckrm_unlock_hier(res->core);
	return;
}

static int
mem_set_share_values(void *my_res, struct ckrm_shares *shares)
{
	ckrm_mem_res_t *res = my_res;
	ckrm_mem_res_t *parres;
	int rc = EINVAL;

	if (!res) 
		return -EINVAL;

	parres = ckrm_get_res_class(res->parent, mem_rcbs.resid, ckrm_mem_res_t);

	rc = set_shares(shares, &res->shares, parres ? &parres->shares : NULL);

	if ((rc == 0) && (parres != NULL)) {
		child_maxlimit_changed_local(parres);
		recalc_and_propagate(parres, NULL);
	}
	return rc;
}

static int
mem_get_share_values(void *my_res, struct ckrm_shares *shares)
{
	ckrm_mem_res_t *res = my_res;

	if (!res) 
		return -EINVAL;
	*shares = res->shares;
	return 0;
}

static int  
mem_get_stats(void *my_res, struct seq_file *sfile)
{
	ckrm_mem_res_t *res = my_res;

	if (!res) 
		return -EINVAL;

#if 0
	seq_printf(sfile, "tot %6d;gua %6d;lmt %6d;unu %6d;"
			"lnt %6d;bor %6d;rlt %6d\n", atomic_read(&res->pg_total),
			res->pg_guar, res->pg_limit, res->pg_unused, res->pg_lent,
			res->pg_borrowed, atomic_read(&ckrm_mem_real_count));
#endif


	seq_printf(sfile, "----------- Memory Resource stats start -----------\n");
	seq_printf(sfile, "Number of pages used(including pages lent to children):"
			" %d\n", atomic_read(&res->pg_total));
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
	seq_printf(sfile, "----------- Memory Resource stats end -----------\n");

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
	ckrm_mem_evaluate_mm(mm);
	/*
	printk("chg_cls: task <%s:%d> mm %p oldmm %s newmm %s o %s n %s\n",
		task->comm, task->pid, mm, prev_mmcls ? prev_mmcls->core->name:
		"NULL", mm->memclass ? mm->memclass->core->name : "NULL",
		o ? o->core->name: "NULL", n ? n->core->name: "NULL");	
	*/
	return;
}

// config file is available only at the root level,
// so assuming my_res to be the system level class
static int
mem_set_config(void *my_res, const char *cfgstr)
{
	ckrm_mem_res_t *res = my_res;

	printk(KERN_INFO "%s class of %s is called with config<%s>\n",
			MEM_NAME, res->core->name, cfgstr);
	return 0;
}

static int 
mem_show_config(void *my_res, struct seq_file *sfile)
{
	struct zone *zone;
	ckrm_mem_res_t *res = my_res;
	int active = 0, inactive = 0, fr = 0;

	if (!res)
		return -EINVAL;

	for_each_zone(zone) {
		active += zone->nr_active;
		inactive += zone->nr_inactive;
		fr += zone->free_pages;
	}
	seq_printf(sfile, "res=%s;tot_pages=%d,active=%d,inactive=%d,free=%d\n",
			MEM_NAME, ckrm_tot_lru_pages,active,inactive,fr);


	return 0;
}

static int
mem_reset_stats(void *my_res)
{
	ckrm_mem_res_t *res = my_res;
	printk(KERN_INFO " memclass of %s called for reset\n", res->core->name);
	return 0;
}

struct ckrm_res_ctlr mem_rcbs = {
	.res_name          = MEM_NAME,
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

EXPORT_SYMBOL(mem_rcbs);

int __init
init_ckrm_mem_res(void)
{
	struct ckrm_classtype *clstype;
	int resid = mem_rcbs.resid;

	set_ckrm_tot_pages();
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

static void
set_flags_of_children(ckrm_mem_res_t *parres, unsigned int flag)
{
	ckrm_mem_res_t *childres;
	ckrm_core_class_t *child = NULL;

	parres->reclaim_flags |= flag;
	ckrm_lock_hier(parres->core);
	while ((child = ckrm_get_next_child(parres->core, child)) != NULL) {
		childres = ckrm_get_res_class(child, mem_rcbs.resid,
				ckrm_mem_res_t);
		set_flags_of_children(childres, flag);
	}
	ckrm_unlock_hier(parres->core);
	return;
}

// FIXME: more attention is needed to this function
static unsigned int
set_usage_flags(ckrm_mem_res_t *res)
{
	int tot_usage, cls_usage, range, guar;

	if (res->pg_limit == CKRM_SHARE_DONTCARE) {
			// No limit is set for the class. don't bother it
			res->reclaim_flags = 0;
			return res->reclaim_flags;
	}

	tot_usage = atomic_read(&res->pg_total);
	cls_usage = tot_usage - res->pg_lent;
	guar = (res->pg_guar > 0) ? res->pg_guar : 0;
	range = res->pg_limit - guar;

	if ((tot_usage > (guar + ((110 * range) / 100))) &&
				(res->pg_lent > (guar + ((25 * range) / 100)))) {
		set_flags_of_children(res, CLS_PARENT_OVER);
	}

	if (cls_usage > (guar + ((110 * range) / 100))) {
		res->reclaim_flags |= CLS_OVER_110;
	} else if (cls_usage > (guar + range)) {
		res->reclaim_flags |= CLS_OVER_100;
	} else if (cls_usage > (guar + ((3 * range) / 4))) {
		res->reclaim_flags |= CLS_OVER_75;
	} else if (cls_usage > (guar + (range / 2))) {
		res->reclaim_flags |= CLS_OVER_50;
	} else if (cls_usage > (guar + (range / 4))) {
		res->reclaim_flags |= CLS_OVER_25;
	} else if (cls_usage > guar) {
		res->reclaim_flags |= CLS_OVER_GUAR;
	} else {
		res->reclaim_flags = 0;
	}
	return res->reclaim_flags;
}

/*
 * The functions ckrm_setup_reclamation(), ckrm_teardown_reclamation(),
 * ckrm_get_reclaim_bits() and the macro ckrm_kick_page() along with the 
 * macros CLS_* define how the pages are reclaimed.
 * Keeping this logic thru these interface eliminate the necessity to
 * change the reclaimation code in VM if we want to change the logic.
 */
unsigned int
ckrm_setup_reclamation(void)
{
	ckrm_mem_res_t *res;
	unsigned int ret = 0;

	spin_lock(&ckrm_mem_lock);
	set_ckrm_tot_pages();
	ckrm_mem_root_class->pg_guar = ckrm_tot_lru_pages;
	ckrm_mem_root_class->pg_unused = ckrm_tot_lru_pages;
	ckrm_mem_root_class->pg_limit = ckrm_tot_lru_pages;
	recalc_and_propagate(ckrm_mem_root_class, NULL);
	list_for_each_entry(res, &ckrm_memclass_list, mcls_list) {
		ret |= set_usage_flags(res);
	}
	spin_unlock(&ckrm_mem_lock);
	return ret;
}

void
ckrm_teardown_reclamation(void)
{
	ckrm_mem_res_t *res;
	spin_lock(&ckrm_mem_lock);
	list_for_each_entry(res, &ckrm_memclass_list, mcls_list) {
		res->reclaim_flags = 0;
	}
	spin_unlock(&ckrm_mem_lock);
}

void
ckrm_get_reclaim_bits(unsigned int *flags, unsigned int *extract)
{
	int i, j, mask = 0;

	if (*flags == 0) {
		*extract = 0;
		return;
	}

	if (*flags & CLS_SHRINK) {
		*extract = CLS_SHRINK;
		*flags = 0;
		return;
	}

	i = fls(*flags);
	for (j = i-1; j > 0; j--) {
		mask = (mask<<1) | 1;
	}
	*extract = (CLS_FLAGS_ALL & ~mask);
	*flags &= ~*extract;
	return;
}

void
ckrm_at_limit(ckrm_mem_res_t *cls)
{
#ifndef AT_LIMIT_SUPPORT
#warning "ckrm_at_limit disabled due to problems with memory hog tests"
#else
	struct zone *zone;
	unsigned long now = jiffies;

	if (!cls || (cls->pg_limit == CKRM_SHARE_DONTCARE) || 
			((cls->flags & MEM_AT_LIMIT) == MEM_AT_LIMIT)) {
		return;
	}
	if ((cls->last_shrink + (10 * HZ)) < now) { // 10 seconds since last ?
		cls->last_shrink = now;
		cls->shrink_count = 0;
	}
	cls->shrink_count++;
	if (cls->shrink_count > 10) {
		return;
	}
	spin_lock(&ckrm_mem_lock);
	list_add(&cls->shrink_list, &ckrm_shrink_list);
	spin_unlock(&ckrm_mem_lock);
	cls->flags |= MEM_AT_LIMIT;
	for_each_zone(zone) {
		wakeup_kswapd(zone);
		break; // only once is enough
	}
#endif // AT_LIMIT_SUPPORT
}

static int unmapped = 0, changed = 0, unchanged = 0, maxnull = 0,
anovma = 0, fnovma = 0;
static void
ckrm_mem_evaluate_page_anon(struct page* page)
{
	ckrm_mem_res_t* pgcls = page_class(page);
	ckrm_mem_res_t* maxshareclass = NULL;
	struct anon_vma *anon_vma = (struct anon_vma *) page->mapping;
	struct vm_area_struct *vma;
	struct mm_struct* mm;
	int v = 0;

	spin_lock(&anon_vma->lock);
	BUG_ON(list_empty(&anon_vma->head));
	list_for_each_entry(vma, &anon_vma->head, anon_vma_node) {
		v++;
		mm = vma->vm_mm;
		if (!maxshareclass ||
				ckrm_mem_share_compare(maxshareclass, mm->memclass) < 0) {
			maxshareclass = mm->memclass;
		}
	}
	spin_unlock(&anon_vma->lock);
	if (!v)
		anovma++;

	if (!maxshareclass)
		maxnull++;
	if (maxshareclass && (pgcls != maxshareclass)) {
		ckrm_change_page_class(page, maxshareclass);
		changed++;
	} else 
		unchanged++;
	return;
}

static void
ckrm_mem_evaluate_page_file(struct page* page) 
{
	ckrm_mem_res_t* pgcls = page_class(page);
	ckrm_mem_res_t* maxshareclass = NULL;
	struct address_space *mapping = page->mapping;
	struct vm_area_struct *vma = NULL;
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	struct prio_tree_iter iter;
	struct mm_struct* mm;
	int v = 0;

	if (!mapping)
		return;

	if (!spin_trylock(&mapping->i_mmap_lock))
		return;

	while ((vma = vma_prio_tree_next(vma, &mapping->i_mmap,
					&iter, pgoff, pgoff)) != NULL) {
		v++;
		mm = vma->vm_mm;
		if (!maxshareclass || ckrm_mem_share_compare(maxshareclass,mm->memclass)<0)
			maxshareclass = mm->memclass;
	}
	spin_unlock(&mapping->i_mmap_lock);

	if (!v)
		fnovma++;
	if (!maxshareclass)
		maxnull++;

	if (maxshareclass && pgcls != maxshareclass) {
		ckrm_change_page_class(page, maxshareclass);
		changed++;
	} else 
		unchanged++;
	return;
}

static void
ckrm_mem_evaluate_page(struct page* page) 
{
	if (page->mapping) {
		if (PageAnon(page))
			ckrm_mem_evaluate_page_anon(page);
		else
			ckrm_mem_evaluate_page_file(page);
	} else
		unmapped++;
	return;
}

static void
ckrm_mem_evaluate_all_pages()
{
	struct page *page;
	struct zone *zone;
	int active = 0, inactive = 0, cleared = 0;
	int act_cnt, inact_cnt, idx;
	ckrm_mem_res_t *res;

	spin_lock(&ckrm_mem_lock);
	list_for_each_entry(res, &ckrm_memclass_list, mcls_list) {
		res->tmp_cnt = 0;
	}
	spin_unlock(&ckrm_mem_lock);

	for_each_zone(zone) {
		spin_lock_irq(&zone->lru_lock);
		list_for_each_entry(page, &zone->inactive_list, lru) {
			ckrm_mem_evaluate_page(page);
			active++;
			page_class(page)->tmp_cnt++;
			if (!test_bit(PG_ckrm_account, &page->flags))
				cleared++;
		}
		list_for_each_entry(page, &zone->active_list, lru) {
			ckrm_mem_evaluate_page(page);
			inactive++;
			page_class(page)->tmp_cnt++;
			if (!test_bit(PG_ckrm_account, &page->flags))
				cleared++;
		}
		spin_unlock_irq(&zone->lru_lock);
	}
	printk("all_pages: active %d inactive %d cleared %d\n", 
			active, inactive, cleared);
	spin_lock(&ckrm_mem_lock);
	list_for_each_entry(res, &ckrm_memclass_list, mcls_list) {
		act_cnt = 0; inact_cnt = 0; idx = 0;
		for_each_zone(zone) {
			act_cnt += res->nr_active[idx];
			inact_cnt += res->nr_inactive[idx];
			idx++;
		}
		printk("all_pages: %s: tmp_cnt %d; act_cnt %d inact_cnt %d\n",
			res->core->name, res->tmp_cnt, act_cnt, inact_cnt);
	}
	spin_unlock(&ckrm_mem_lock);

	// check all mm's in the system to see which memclass they are attached
	// to.
	return;
}

static /*inline*/ int
class_migrate_pmd(struct mm_struct* mm, struct vm_area_struct* vma,
		pmd_t* pmdir, unsigned long address, unsigned long end)
{
	pte_t *pte, *orig_pte;
	unsigned long pmd_end;
	
	if (pmd_none(*pmdir))
		return 0;
	BUG_ON(pmd_bad(*pmdir));
	
	orig_pte = pte = pte_offset_map(pmdir,address);
	pmd_end = (address+PMD_SIZE)&PMD_MASK;
	if (end>pmd_end)
		end = pmd_end;
	
	do {
		if (pte_present(*pte)) {
			BUG_ON(mm->memclass == NULL);
			ckrm_change_page_class(pte_page(*pte), mm->memclass);
			// ckrm_mem_evaluate_page(pte_page(*pte));
		}
		address += PAGE_SIZE;
		pte++;
	} while(address && (address<end));
	pte_unmap(orig_pte);
	return 0;
}

static /*inline*/ int
class_migrate_pgd(struct mm_struct* mm, struct vm_area_struct* vma,
		pgd_t* pgdir, unsigned long address, unsigned long end)
{
	pmd_t* pmd;
	unsigned long pgd_end;
	
	if (pgd_none(*pgdir))
		return 0;
	BUG_ON(pgd_bad(*pgdir));
	
	pmd = pmd_offset(pgdir,address);
	pgd_end = (address+PGDIR_SIZE)&PGDIR_MASK;
	
	if (pgd_end && (end>pgd_end))
		end = pgd_end;
	
	do {
		class_migrate_pmd(mm,vma,pmd,address,end);
		address =  (address+PMD_SIZE)&PMD_MASK;
		pmd++;
	} while (address && (address<end));
	return 0;
}

static /*inline*/ int
class_migrate_vma(struct mm_struct* mm, struct vm_area_struct* vma)
{
	pgd_t* pgdir;
	unsigned long address, end;
	
	address = vma->vm_start;
	end = vma->vm_end;
	
	pgdir = pgd_offset(vma->vm_mm, address);
	do {
		class_migrate_pgd(mm,vma,pgdir,address,end);
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		pgdir++;
	} while(address && (address<end));
	return 0;
}

/* this function is called with mm->peertask_lock hold */
void
ckrm_mem_evaluate_mm(struct mm_struct* mm)
{
	struct task_struct *task;
	struct ckrm_mem_res *maxshareclass = NULL;
	struct vm_area_struct *vma;
	
	if (list_empty(&mm->tasklist)) {
		/* We leave the mm->memclass untouched since we believe that one
		 * mm with no task associated will be deleted soon or attach
		 * with another task later.
		 */
		return; 
	}

	list_for_each_entry(task, &mm->tasklist, mm_peers) {
		ckrm_mem_res_t* cls = GET_MEM_CLASS(task);
		if (!cls)
			continue;
		if (!maxshareclass || ckrm_mem_share_compare(maxshareclass,cls)<0 ) 
			maxshareclass = cls;
	}

	if (maxshareclass && (mm->memclass != (void *)maxshareclass)) {
		if (mm->memclass)
			mem_class_put(mm->memclass);
		mm->memclass = maxshareclass;
		mem_class_get(maxshareclass);
		
		/* Go through all VMA to migrate pages */
		down_read(&mm->mmap_sem);
		vma = mm->mmap;
		while(vma) {
			class_migrate_vma(mm, vma);
			vma = vma->vm_next;
		}
		up_read(&mm->mmap_sem);
	}
	return;
}

void
ckrm_init_mm_to_task(struct mm_struct * mm, struct task_struct *task)
{
	spin_lock(&mm->peertask_lock);
	if (!list_empty(&task->mm_peers)) {
		printk(KERN_ERR "CKRM_MEM: Task list should be empty, but is not!!\n");
		list_del_init(&task->mm_peers);
	}
	list_add_tail(&task->mm_peers, &mm->tasklist);
	spin_unlock(&mm->peertask_lock);
	if (mm->memclass != GET_MEM_CLASS(task))
		ckrm_mem_evaluate_mm(mm);
	return;
}

int
ckrm_memclass_valid(ckrm_mem_res_t *cls)
{
	ckrm_mem_res_t *tmp;

	spin_lock(&ckrm_mem_lock);
	list_for_each_entry(tmp, &ckrm_memclass_list, mcls_list) {
		if (tmp == cls) {
			spin_unlock(&ckrm_mem_lock);
			return 1;
		}
	}
	spin_unlock(&ckrm_mem_lock);
	return 0;
}

MODULE_LICENSE("GPL");
