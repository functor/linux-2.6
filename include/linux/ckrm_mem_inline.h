/* include/linux/ckrm_mem_inline.h : memory control for CKRM
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


#ifndef _LINUX_CKRM_MEM_INLINE_H_
#define _LINUX_CKRM_MEM_INLINE_H_

#include <linux/rmap.h>
#include <linux/mmzone.h>
#include <linux/ckrm_mem.h>


#ifdef CONFIG_CKRM_RES_MEM

#define GET_MEM_CLASS(tsk) \
	ckrm_get_res_class(tsk->taskclass, mem_rcbs.resid, ckrm_mem_res_t)

#define ckrm_set_shrink(cls) \
	set_bit(CLS_SHRINK_BIT, (unsigned long *)&(cls)->reclaim_flags)
#define ckrm_test_set_shrink(cls) \
	test_and_set_bit(CLS_SHRINK_BIT, (unsigned long *)&(cls)->reclaim_flags)
#define ckrm_clear_shrink(cls) \
	clear_bit(CLS_SHRINK_BIT, (unsigned long *)&(cls)->reclaim_flags)

#define ckrm_shrink_list_empty()	list_empty(&ckrm_shrink_list)

/*
 * Currently, the class of an address is assigned to the class with max
 * available guarantee. Simply replace this function for other policies.
 */
static inline int
ckrm_mem_share_compare(ckrm_mem_res_t *a, ckrm_mem_res_t *b)
{
	if (a == NULL) 
		return -(b != NULL) ;
	if (b == NULL)
		return 0;
	return (a->pg_unused - b->pg_unused);
}

static inline void
mem_class_get(ckrm_mem_res_t *cls)
{
	if (cls)
		atomic_inc(&((cls)->nr_users));
}

static inline void
mem_class_put(ckrm_mem_res_t *cls)
{
	if (cls && atomic_dec_and_test(&(cls->nr_users)) ) {
		printk("freeing memclass %p of <core:%s>\n", cls, cls->core->name);
		//kfree(cls);
	}	
}

static inline int
incr_use_count(ckrm_mem_res_t *cls, int borrow)
{
	int over_limit;

	atomic_inc(&cls->pg_total);
	over_limit = (atomic_read(&cls->pg_total) > ((9 * cls->pg_limit) / 10));

	if (borrow) 
		cls->pg_lent++;
	if ((cls->pg_guar != CKRM_SHARE_DONTCARE) &&
				(atomic_read(&cls->pg_total) > cls->pg_unused)) {
		ckrm_mem_res_t *parcls = ckrm_get_res_class(cls->parent,
				mem_rcbs.resid, ckrm_mem_res_t);
		if (parcls) {
			over_limit |= incr_use_count(parcls, 1);
			cls->pg_borrowed++;
			return over_limit;
		}
	}
	atomic_inc(&ckrm_mem_real_count);
	return over_limit;
}

static inline void
decr_use_count(ckrm_mem_res_t *cls, int borrowed)
{
	atomic_dec(&cls->pg_total);
	if (borrowed)
		cls->pg_lent--;
	if (cls->pg_borrowed > 0) {
		ckrm_mem_res_t *parcls = ckrm_get_res_class(cls->parent,
				mem_rcbs.resid, ckrm_mem_res_t);
		if (parcls) {
			decr_use_count(parcls, 1);
			cls->pg_borrowed--;
			return;
		}
	}
	atomic_dec(&ckrm_mem_real_count);
}

static inline void
ckrm_set_page_class(struct page *page, ckrm_mem_res_t *cls)
{
	if (mem_rcbs.resid != -1 && cls != NULL) {
		if (unlikely(page->memclass)) {
			mem_class_put(page->memclass);
		}
		page->memclass = cls;
		mem_class_get(cls);
	} else {
		page->memclass = NULL;
	}
}

static inline void
ckrm_set_pages_class(struct page *pages, int numpages, ckrm_mem_res_t *cls)
{
	int i;
	for (i = 0; i < numpages; pages++, i++) {
		ckrm_set_page_class(pages, cls);
	}
}

static inline void
ckrm_clear_page_class(struct page *page)
{
	if (page->memclass != NULL) {
		mem_class_put(page->memclass);
		page->memclass = NULL;
	}
}

static inline void
ckrm_clear_pages_class(struct page *pages, int numpages)
{
	int i;
	for (i = 0; i < numpages; pages++, i++) {
		ckrm_clear_page_class(pages);
	}
}

static inline void
ckrm_change_page_class(struct page *page, ckrm_mem_res_t *cls)
{
	ckrm_clear_page_class(page);
	ckrm_set_page_class(page, cls);
}

static inline void
ckrm_change_pages_class(struct page *pages, int numpages, 
					ckrm_mem_res_t *cls)
{
	int i;
	for (i = 0; i < numpages; pages++, i++) {
		ckrm_change_page_class(pages, cls);
	}
}

static inline void
ckrm_mem_inc_active(struct page *page)
{
	ckrm_mem_res_t *cls = page_class(page);
	BUG_ON(cls == NULL);
	cls->nr_active[page_zonenum(page)]++;
	if (incr_use_count(cls, 0)) {
		ckrm_near_limit(cls);
	}
}

static inline void
ckrm_mem_dec_active(struct page *page)
{
	ckrm_mem_res_t *cls = page_class(page);
	BUG_ON(cls == NULL);
	cls->nr_active[page_zonenum(page)]--;
	decr_use_count(cls, 0);
}

static inline void
ckrm_mem_inc_inactive(struct page *page)
{
	ckrm_mem_res_t *cls = page_class(page);
	BUG_ON(cls == NULL);
	cls->nr_inactive[page_zonenum(page)]++;
	if (incr_use_count(cls, 0) &&
			((cls->flags & MEM_NEAR_LIMIT) != MEM_NEAR_LIMIT)) {
		ckrm_near_limit(cls);
	}
}

static inline void
ckrm_mem_dec_inactive(struct page *page)
{
	ckrm_mem_res_t *cls = page_class(page);
	BUG_ON(cls == NULL);
	cls->nr_inactive[page_zonenum(page)]--;
	decr_use_count(cls, 0);
}

static inline int
ckrm_kick_page(struct page *page, unsigned int bits)
{
	if (page_class(page) == NULL) {
		return bits;
	} else {
		return (page_class(page)->reclaim_flags & bits);
	}
}

static inline int 
ckrm_class_limit_ok(ckrm_mem_res_t *cls)
{
	if ((mem_rcbs.resid == -1) || !cls) {
		return 1;
	}
	return (atomic_read(&cls->pg_total) <= (11 * cls->pg_limit) / 10);
}

#else // !CONFIG_CKRM_RES_MEM

#define ckrm_set_page_class(a,b)		do{}while(0)
#define ckrm_set_pages_class(a,b,c)		do{}while(0)
#define ckrm_clear_page_class(a)		do{}while(0)
#define ckrm_clear_pages_class(a,b)		do{}while(0)
#define ckrm_change_page_class(a,b)		do{}while(0)
#define ckrm_change_pages_class(a,b,c)	do{}while(0)
#define ckrm_mem_inc_active(a)			do{}while(0)
#define ckrm_mem_dec_active(a)			do{}while(0)
#define ckrm_mem_inc_inactive(a)		do{}while(0)
#define ckrm_mem_dec_inactive(a)		do{}while(0)
#define ckrm_shrink_list_empty()		(1)
#define ckrm_kick_page(a,b)				(0)
#define ckrm_class_limit_ok(a)			(1)

#endif // CONFIG_CKRM_RES_MEM

#endif // _LINUX_CKRM_MEM_INLINE_H_
