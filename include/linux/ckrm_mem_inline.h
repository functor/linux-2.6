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
	if (a->pg_guar == CKRM_SHARE_DONTCARE)
		return 1;
	if (b->pg_guar == CKRM_SHARE_DONTCARE)
		return -1;
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
		BUG_ON(ckrm_memclass_valid(cls));
		//kfree(cls);
	}	
}

static inline void
incr_use_count(ckrm_mem_res_t *cls, int borrow)
{
	atomic_inc(&cls->pg_total);

	if (borrow) 
		cls->pg_lent++;
	if ((cls->pg_guar == CKRM_SHARE_DONTCARE) ||
				(atomic_read(&cls->pg_total) > cls->pg_unused)) {
		ckrm_mem_res_t *parcls = ckrm_get_res_class(cls->parent,
				mem_rcbs.resid, ckrm_mem_res_t);
		if (parcls) {
			incr_use_count(parcls, 1);
			cls->pg_borrowed++;
		}
	} else {
		atomic_inc(&ckrm_mem_real_count);
	}
	if ((cls->pg_limit != CKRM_SHARE_DONTCARE) && 
			(atomic_read(&cls->pg_total) >= cls->pg_limit) &&
			((cls->flags & MEM_AT_LIMIT) != MEM_AT_LIMIT)) {
		ckrm_at_limit(cls);
	}
	return;
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
ckrm_change_page_class(struct page *page, ckrm_mem_res_t *newcls)
{
	ckrm_mem_res_t *oldcls = page_class(page);

	if (!newcls || oldcls == newcls)
		return;

	ckrm_clear_page_class(page);
	ckrm_set_page_class(page, newcls);
	if (test_bit(PG_ckrm_account, &page->flags)) {
		decr_use_count(oldcls, 0);
		incr_use_count(newcls, 0);
		if (PageActive(page)) {
			oldcls->nr_active[page_zonenum(page)]--;
			newcls->nr_active[page_zonenum(page)]++;
		} else {
			oldcls->nr_inactive[page_zonenum(page)]--;
			newcls->nr_inactive[page_zonenum(page)]++;
		}
	}
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
	ckrm_mem_res_t *cls = page_class(page), *curcls;
	if (!cls) {
		return;
	}
	BUG_ON(test_bit(PG_ckrm_account, &page->flags));
	if (unlikely(cls != (curcls = GET_MEM_CLASS(current)))) {
		cls = curcls;
		ckrm_change_page_class(page, cls);
	}
	cls->nr_active[page_zonenum(page)]++;
	incr_use_count(cls, 0);
	set_bit(PG_ckrm_account, &page->flags);
}

static inline void
ckrm_mem_dec_active(struct page *page)
{
	ckrm_mem_res_t *cls = page_class(page);
	if (!cls) {
		return;
	}
	BUG_ON(!test_bit(PG_ckrm_account, &page->flags));
	cls->nr_active[page_zonenum(page)]--;
	decr_use_count(cls, 0);
	clear_bit(PG_ckrm_account, &page->flags);
}

static inline void
ckrm_mem_inc_inactive(struct page *page)
{
	ckrm_mem_res_t *cls = page_class(page), *curcls;
	if (!cls) {
		return;
	}
	BUG_ON(test_bit(PG_ckrm_account, &page->flags));
	if (unlikely(cls != (curcls = GET_MEM_CLASS(current)))) {
		cls = curcls;
		ckrm_change_page_class(page, cls);
	}
	cls->nr_inactive[page_zonenum(page)]++;
	incr_use_count(cls, 0);
	set_bit(PG_ckrm_account, &page->flags);
}

static inline void
ckrm_mem_dec_inactive(struct page *page)
{
	ckrm_mem_res_t *cls = page_class(page);
	if (!cls) {
		return;
	}
	BUG_ON(!test_bit(PG_ckrm_account, &page->flags));
	cls->nr_inactive[page_zonenum(page)]--;
	decr_use_count(cls, 0);
	clear_bit(PG_ckrm_account, &page->flags);
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
	if (cls->pg_limit == CKRM_SHARE_DONTCARE) {
		ckrm_mem_res_t *parcls = ckrm_get_res_class(cls->parent,
						mem_rcbs.resid, ckrm_mem_res_t);
		return (!parcls ?: ckrm_class_limit_ok(parcls));
	} else {
		return (atomic_read(&cls->pg_total) <= (11 * cls->pg_limit) / 10);
	}
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
