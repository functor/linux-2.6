/* include/linux/ckrm_mem.h : memory control for CKRM
 *
 * Copyright (C) Jiantao Kong, IBM Corp. 2003
 *           (C) Shailabh Nagar, IBM Corp. 2003
 *           (C) Chandra Seetharaman, IBM Corp. 2004
 *
 *
 * Memory control functions of the CKRM kernel API
 *
 * Latest version, more details at http://ckrm.sf.net
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef _LINUX_CKRM_MEM_H
#define _LINUX_CKRM_MEM_H

#ifdef CONFIG_CKRM_RES_MEM

#include <linux/list.h>
#include <linux/ckrm_rc.h>
#include <linux/kref.h>

struct ckrm_zone {
	struct list_head active_list;
	struct list_head inactive_list;

	unsigned long nr_active;	// # of pages in the active list
	unsigned long nr_inactive;	// # of pages in the inactive list
	unsigned long active_over;
	unsigned long inactive_over;

	unsigned long shrink_active;
	unsigned long shrink_inactive;
	long shrink_weight;
	unsigned long shrink_flag;

	struct list_head victim_list;	// list of ckrm_zones chosen for shrinking
	struct zone *zone;
	struct ckrm_mem_res *memcls;
};

struct ckrm_mem_res {
	unsigned long flags;
	struct ckrm_core_class *core;	// the core i am part of...
	struct ckrm_core_class *parent;	// parent of the core i am part of....
	struct ckrm_shares shares;
	struct list_head mcls_list;	// list of all 1-level classes
	struct list_head shrink_list;	// list of classes need to be shrunk
	struct kref nr_users;		// # of references to this class/data structure
	atomic_t pg_total;		// # of pages used by this class
	int pg_guar;			// # of pages this class is guaranteed
	int pg_limit;			// max # of pages this class can get
	int pg_borrowed;		// # of pages this class borrowed from its parent
	int pg_lent;			// # of pages this class lent to its children
	int pg_unused;			// # of pages left to this class (after giving the
					// guarantees to children. need to borrow from parent if
					// more than this is needed.
	int impl_guar;			// implicit guarantee for class with don't care guar
	int nr_dontcare;		// # of children with don't care guarantee
	struct ckrm_zone ckrm_zone[MAX_NR_ZONES];
	int shrink_count;
	unsigned long last_shrink;
	int over_limit_failures;
	int shrink_pages;		// # of pages to free in this class
	int hier;			// hiearchy, root = 0
};

extern atomic_t ckrm_mem_real_count;
extern unsigned int ckrm_tot_lru_pages;
extern int ckrm_nr_mem_classes;
extern struct list_head ckrm_shrink_list;
extern struct list_head ckrm_memclass_list;
extern spinlock_t ckrm_mem_lock;
extern struct ckrm_res_ctlr mem_rcbs;
extern struct ckrm_mem_res *ckrm_mem_root_class;

#define page_ckrmzone(page)	((page)->ckrm_zone)

#define CLS_SHRINK_BIT	(1)

// used in flags. set when a class is more than 90% of its maxlimit
#define MEM_AT_LIMIT	1

extern void ckrm_init_mm_to_task(struct mm_struct *, struct task_struct *);
extern void ckrm_mem_evaluate_mm(struct mm_struct *, struct ckrm_mem_res *);
extern void ckrm_at_limit(struct ckrm_mem_res *);
extern int ckrm_memclass_valid(struct ckrm_mem_res *);
extern int ckrm_mem_get_shrink_to(void);
extern void check_memclass(struct ckrm_mem_res *, char *);
extern void memclass_release(struct kref *);

#else

#define ckrm_init_mm_to_current(a)			do {} while (0)
#define ckrm_mem_evaluate_mm(a)				do {} while (0)
#define ckrm_init_mm_to_task(a,b)			do {} while (0)

#endif // CONFIG_CKRM_RES_MEM

#endif //_LINUX_CKRM_MEM_H
