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

/* Changes
 *
 * 28 Aug 2003
 *        Created.
 */

#ifndef _LINUX_CKRM_MEM_H
#define _LINUX_CKRM_MEM_H

#ifdef CONFIG_CKRM_RES_MEM

#include <linux/list.h>
#include <linux/ckrm_rc.h>

typedef struct ckrm_mem_res {
	unsigned long reclaim_flags; 
	unsigned long flags; 
	struct ckrm_core_class *core; // the core i am part of...
	struct ckrm_core_class *parent; // parent of the core i am part of....
	struct ckrm_shares shares;
	struct list_head mcls_list; // list of all 1-level classes
	struct list_head shrink_list; // list of classes need to be shrunk
	atomic_t nr_users; // # of references to this class/data structure
	atomic_t pg_total;  // # of pages used by this class
	int pg_guar; // # of pages this class is guaranteed
	int pg_limit; // max # of pages this class can get
	int pg_borrowed; // # of pages this class borrowed from its parent
	int pg_lent; // # of pages this class lent to its children
	int pg_unused; // # of pages left to this class (after giving the
				// guarantees to children. need to borrow from parent if
				// more than this is needed.
	int nr_active[MAX_NR_ZONES];
	int nr_inactive[MAX_NR_ZONES];
	int shrink_count;
	unsigned long last_shrink;
	int over_limit_failures;
	int hier; // hiearchy, root = 0
} ckrm_mem_res_t;

extern atomic_t ckrm_mem_real_count;
extern unsigned int ckrm_tot_lru_pages;
extern struct list_head ckrm_shrink_list;
extern spinlock_t ckrm_mem_lock;
extern struct ckrm_res_ctlr mem_rcbs;

#define page_class(page)	((ckrm_mem_res_t*)((page)->memclass))

// used to fill reclaim_flags, used only when memory is low in the system
#define CLS_CLEAR		(0)      // class under its guarantee
#define CLS_OVER_GUAR	(1 << 0) // class is over its guarantee
#define CLS_PARENT_OVER	(1 << 1) // parent is over 120% mark over limit
#define CLS_OVER_75		(1 << 2) // class over 75% mark bet guar(0) & limit(100)
#define CLS_OVER_100	(1 << 3) // class over its limit
#define CLS_OVER_110	(1 << 4) // class over 110% mark over limit
#define CLS_FLAGS_ALL	( CLS_OVER_GUAR | CLS_PARENT_OVER | CLS_OVER_75 | \
					CLS_OVER_100 | CLS_OVER_110 )
#define CLS_SHRINK_BIT	(31)	  // used to both lock and set the bit
#define CLS_SHRINK		(1 << CLS_SHRINK_BIT) // shrink the given class

// used in flags. set when a class is more than 90% of its maxlimit
#define MEM_NEAR_LIMIT 1

extern void ckrm_set_aggressive(ckrm_mem_res_t *);
extern unsigned int ckrm_setup_reclamation(void);
extern void ckrm_teardown_reclamation(void);
extern void ckrm_get_reclaim_bits(unsigned int *, unsigned int *);
extern void ckrm_init_mm_to_task(struct mm_struct *, struct task_struct *);
extern void ckrm_mem_evaluate_mm(struct mm_struct *);
extern void ckrm_mem_evaluate_page_byadd(struct page *, struct mm_struct *);
extern void ckrm_near_limit(ckrm_mem_res_t *);
#define ckrm_get_reclaim_flags(cls)	((cls)->reclaim_flags)

#else

#define ckrm_init_mm_to_current(a)			do {} while (0)
#define ckrm_mem_evaluate_mm(a)				do {} while (0)
#define ckrm_mem_evaluate_page_byadd(a,b)	do {} while (0)
#define page_class(page)					(NULL)
#define ckrm_get_reclaim_flags(a)			(0)
#define ckrm_setup_reclamation()			(0)
#define ckrm_teardown_reclamation()			do {} while (0)
#define ckrm_get_reclaim_bits(a, b)			do { *(a) = 0; *(b)= 0; } while (0)
#define ckrm_init_mm_to_task(a,b)			do {} while (0)

#endif // CONFIG_CKRM_RES_MEM

#endif //_LINUX_CKRM_MEM_H

