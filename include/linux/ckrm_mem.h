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

	unsigned long nr_active;
	unsigned long nr_inactive;
	unsigned long active_over;
	unsigned long inactive_over;

	unsigned long shrink_active;
	unsigned long shrink_inactive;
	long shrink_weight;
	unsigned long shrink_flag;
	struct list_head victim_list;	/* list of ckrm_zones chosen for
					 * shrinking. These are over their
					 * 'guarantee'
					 */
	struct zone *zone;
	struct ckrm_mem_res *memcls;
};

struct ckrm_mem_res {
	unsigned long flags;
	struct ckrm_core_class *core;	/* the core i am part of... */
	struct ckrm_core_class *parent;	/* parent of the core i am part of */
	struct ckrm_shares shares;	
	struct list_head mcls_list;	/* list of all 1-level classes */
	struct kref nr_users;		/* ref count */
	atomic_t pg_total;		/* # of pages used by this class */
	int pg_guar;			/* absolute # of guarantee */
	int pg_limit;			/* absolute # of limit */
	int pg_borrowed;		/* # of pages borrowed from parent */
	int pg_lent;			/* # of pages lent to children */
	int pg_unused;			/* # of pages left to this class
					 * (after giving the guarantees to
					 * children. need to borrow from
					 * parent if more than this is needed.
					 */
	int hier;			/* hiearchy level, root = 0 */
	int impl_guar;			/* for classes with don't care guar */
	int nr_dontcare;		/* # of dont care children */

	struct ckrm_zone ckrm_zone[MAX_NR_ZONES];

	struct list_head shrink_list;	/* list of classes that are near
				 	 * limit and need to be shrunk
					 */
	int shrink_count;
	unsigned long last_shrink;
};

#define CLS_SHRINK_BIT		(1)

#define CLS_AT_LIMIT		(1)

extern atomic_t ckrm_mem_real_count;
extern struct ckrm_res_ctlr mem_rcbs;
extern struct ckrm_mem_res *ckrm_mem_root_class;
extern struct list_head ckrm_memclass_list;
extern struct list_head ckrm_shrink_list;
extern spinlock_t ckrm_mem_lock;
extern int ckrm_nr_mem_classes;
extern unsigned int ckrm_tot_lru_pages;
extern int ckrm_mem_shrink_count;
extern int ckrm_mem_shrink_to;
extern int ckrm_mem_shrink_interval ;

extern void ckrm_mem_migrate_mm(struct mm_struct *, struct ckrm_mem_res *);
extern void ckrm_mem_migrate_all_pages(struct ckrm_mem_res *,
						struct ckrm_mem_res *);
extern void memclass_release(struct kref *);
extern void shrink_get_victims(struct zone *, unsigned long ,
				unsigned long, struct list_head *);
extern void ckrm_shrink_atlimit(struct ckrm_mem_res *);
#else

#define ckrm_mem_migrate_mm(a, b)			do {} while (0)
#define ckrm_mem_migrate_all_pages(a, b)		do {} while (0)

#endif /* CONFIG_CKRM_RES_MEM */

#endif /* _LINUX_CKRM_MEM_H */
