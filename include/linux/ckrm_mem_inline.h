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

#ifndef _LINUX_CKRM_MEM_INLINE_H_
#define _LINUX_CKRM_MEM_INLINE_H_

#include <linux/rmap.h>
#include <linux/mmzone.h>
#include <linux/ckrm_mem.h>


#ifdef CONFIG_CKRM_RES_MEM

#define INACTIVE	0
#define ACTIVE		1

static inline struct ckrm_mem_res *
ckrm_get_mem_class(struct task_struct *tsk)
{
	return ckrm_get_res_class(tsk->taskclass, mem_rcbs.resid,
		struct ckrm_mem_res);
}

#define ckrm_shrink_list_empty()	list_empty(&ckrm_shrink_list)

static inline void
ckrm_set_shrink(struct ckrm_zone *cz)
{
	set_bit(CLS_SHRINK_BIT, &cz->shrink_flag);
}

static inline int
ckrm_test_set_shrink(struct ckrm_zone *cz)
{
	return test_and_set_bit(CLS_SHRINK_BIT, &cz->shrink_flag);
}

static inline void 
ckrm_clear_shrink(struct ckrm_zone *cz)
{
	clear_bit(CLS_SHRINK_BIT, &cz->shrink_flag);
}

/*
 * Currently, a shared page that is shared by multiple classes is charged
 * to a class with max available guarantee. Simply replace this function
 * for other policies.
 */
static inline int
ckrm_mem_share_compare(struct ckrm_mem_res *a, struct ckrm_mem_res *b)
{
	if (a == NULL)
		return -(b != NULL);
	if (b == NULL)
		return 0;
	if (a->pg_guar == b->pg_guar)
		return 0;
	if (a->pg_guar == CKRM_SHARE_DONTCARE)
		return 1;
	if (b->pg_guar == CKRM_SHARE_DONTCARE)
		return -1;
	return (a->pg_unused - b->pg_unused);
}

static inline void
incr_use_count(struct ckrm_mem_res *cls, int borrow)
{
	extern int ckrm_mem_shrink_at;
	if (unlikely(!cls))
		return;
	BUG_ON(!ckrm_memclass_valid(cls));
	atomic_inc(&cls->pg_total);

	if (borrow)
		cls->pg_lent++;
	if ((cls->pg_guar == CKRM_SHARE_DONTCARE) ||
			(atomic_read(&cls->pg_total) > cls->pg_unused)) {
		struct ckrm_mem_res *parcls = ckrm_get_res_class(cls->parent,
				mem_rcbs.resid, struct ckrm_mem_res);
		if (parcls) {
			incr_use_count(parcls, 1);
			cls->pg_borrowed++;
		}
	} else {
		atomic_inc(&ckrm_mem_real_count);
	}
	if (unlikely((cls->pg_limit != CKRM_SHARE_DONTCARE) &&
			(atomic_read(&cls->pg_total) >=
			((ckrm_mem_shrink_at * cls->pg_limit) / 100)) &&
			((cls->flags & MEM_AT_LIMIT) != MEM_AT_LIMIT))) {
		ckrm_at_limit(cls);
	}
	return;
}

static inline void
decr_use_count(struct ckrm_mem_res *cls, int borrowed)
{
	if (unlikely(!cls))
		return;
	BUG_ON(!ckrm_memclass_valid(cls));
	atomic_dec(&cls->pg_total);
	if (borrowed)
		cls->pg_lent--;
	if (cls->pg_borrowed > 0) {
		struct ckrm_mem_res *parcls = ckrm_get_res_class(cls->parent,
				mem_rcbs.resid, struct ckrm_mem_res);
		if (parcls) {
			decr_use_count(parcls, 1);
			cls->pg_borrowed--;
			return;
		}
	}
	atomic_dec(&ckrm_mem_real_count);
}

static inline void
ckrm_set_page_class(struct page *page, struct ckrm_mem_res *cls)
{
	if (unlikely(cls == NULL)) {
		cls = ckrm_mem_root_class;
	}
	if (likely(cls != NULL)) {
		struct ckrm_zone *czone = &cls->ckrm_zone[page_zonenum(page)];
		if (unlikely(page->ckrm_zone)) {
			kref_put(&cls->nr_users, memclass_release);
		}
		page->ckrm_zone = czone;
		kref_get(&cls->nr_users);
	} else {
		page->ckrm_zone = NULL;
	}
}

static inline void
ckrm_set_pages_class(struct page *pages, int numpages, struct ckrm_mem_res *cls)
{
	int i;
	for (i = 0; i < numpages; pages++, i++) {
		ckrm_set_page_class(pages, cls);
	}
}

static inline void
ckrm_clear_page_class(struct page *page)
{
	if (likely(page->ckrm_zone != NULL)) {
		if (CkrmAccount(page)) {
			decr_use_count(page->ckrm_zone->memcls, 0);
			ClearCkrmAccount(page);
		}
		kref_put(&page->ckrm_zone->memcls->nr_users, memclass_release);
		page->ckrm_zone = NULL;
	}
}

static inline void
ckrm_change_page_class(struct page *page, struct ckrm_mem_res *newcls)
{
	struct ckrm_zone *old_czone = page->ckrm_zone, *new_czone;
	struct ckrm_mem_res *oldcls;

	if (unlikely(!old_czone || !newcls)) {
		BUG_ON(CkrmAccount(page));
		return;
	}
	BUG_ON(!CkrmAccount(page));

	oldcls = old_czone->memcls;
	if (oldcls == NULL || (oldcls == newcls))
		return;

	kref_put(&oldcls->nr_users, memclass_release);
	decr_use_count(oldcls, 0);

	page->ckrm_zone = new_czone = &newcls->ckrm_zone[page_zonenum(page)];

	kref_get(&newcls->nr_users);
	incr_use_count(newcls, 0);

	list_del(&page->lru);
	if (PageActive(page)) {
		old_czone->nr_active--;
		new_czone->nr_active++;
		list_add(&page->lru, &new_czone->active_list);
	} else {
		old_czone->nr_inactive--;
		new_czone->nr_inactive++;
		list_add(&page->lru, &new_czone->inactive_list);
	}
}

static inline void
ckrm_mem_inc_active(struct page *page)
{
	struct ckrm_mem_res *cls = ckrm_get_mem_class(current) ?: ckrm_mem_root_class;

	if (cls == NULL)
		return;
	BUG_ON(CkrmAccount(page));
	BUG_ON(page->ckrm_zone != NULL);

	ckrm_set_page_class(page, cls);
	incr_use_count(cls, 0);
	SetCkrmAccount(page);
	BUG_ON(page->ckrm_zone == NULL);
	page->ckrm_zone->nr_active++;
	list_add(&page->lru, &page->ckrm_zone->active_list);
}

static inline void
ckrm_mem_dec_active(struct page *page)
{
	if (page->ckrm_zone == NULL)
		return;
	BUG_ON(page->ckrm_zone->memcls == NULL);
	BUG_ON(!CkrmAccount(page));

	list_del(&page->lru);
	page->ckrm_zone->nr_active--;
	ckrm_clear_page_class(page);
}


static inline void
ckrm_mem_inc_inactive(struct page *page)
{
	struct ckrm_mem_res *cls = ckrm_get_mem_class(current) ?: ckrm_mem_root_class;

	if (cls == NULL)
		return;
	BUG_ON(CkrmAccount(page));
	BUG_ON(page->ckrm_zone != NULL);

	ckrm_set_page_class(page, cls);
	incr_use_count(cls, 0);
	SetCkrmAccount(page);
	BUG_ON(page->ckrm_zone == NULL);
	page->ckrm_zone->nr_inactive++;
	list_add(&page->lru, &page->ckrm_zone->inactive_list);
}

static inline void
ckrm_mem_dec_inactive(struct page *page)
{
	if (page->ckrm_zone == NULL)
		return;
	BUG_ON(page->ckrm_zone->memcls == NULL);
	BUG_ON(!CkrmAccount(page));

	page->ckrm_zone->nr_inactive--;
	list_del(&page->lru);
	ckrm_clear_page_class(page);
}

static inline int
ckrm_class_limit_ok(struct ckrm_mem_res *cls)
{
	int ret;
	extern int ckrm_mem_fail_over;

	if ((mem_rcbs.resid == -1) || !cls) {
		return 1;
	}
	if (cls->pg_limit == CKRM_SHARE_DONTCARE) {
		struct ckrm_mem_res *parcls = ckrm_get_res_class(cls->parent,
					mem_rcbs.resid, struct ckrm_mem_res);
		ret = (parcls ? ckrm_class_limit_ok(parcls) : 0);
	} else {
		ret = (atomic_read(&cls->pg_total) <=
			((ckrm_mem_fail_over * cls->pg_limit) / 100));
	}

	if (ret == 0) {
		// if we are failing... just nudge the back end
		ckrm_at_limit(cls);
	}
	return ret;
}

// task/mm initializations/cleanup

static inline void
ckrm_task_mm_init(struct task_struct *tsk)
{
	INIT_LIST_HEAD(&tsk->mm_peers);
}

static inline void
ckrm_task_change_mm(struct task_struct *tsk, struct mm_struct *oldmm, struct mm_struct *newmm)
{
	if (oldmm) {
		spin_lock(&oldmm->peertask_lock);
		list_del(&tsk->mm_peers);
		ckrm_mem_evaluate_mm(oldmm, NULL);
		spin_unlock(&oldmm->peertask_lock);
	}
	spin_lock(&newmm->peertask_lock);
	list_add_tail(&tsk->mm_peers, &newmm->tasklist);
	ckrm_mem_evaluate_mm(newmm, NULL);
	spin_unlock(&newmm->peertask_lock);
}

static inline void
ckrm_task_clear_mm(struct task_struct *tsk, struct mm_struct *mm)
{
	spin_lock(&mm->peertask_lock);
	list_del_init(&tsk->mm_peers);
	ckrm_mem_evaluate_mm(mm, NULL);
	spin_unlock(&mm->peertask_lock);
}

static inline void
ckrm_mm_init(struct mm_struct *mm)
{
	INIT_LIST_HEAD(&mm->tasklist);
	mm->peertask_lock = SPIN_LOCK_UNLOCKED;
}

static inline void
ckrm_mm_setclass(struct mm_struct *mm, struct ckrm_mem_res *cls)
{
	mm->memclass = cls;
	kref_get(&cls->nr_users);
}

static inline void
ckrm_mm_clearclass(struct mm_struct *mm)
{
	if (mm->memclass) {
		kref_put(&mm->memclass->nr_users, memclass_release);
		mm->memclass = NULL;
	}
}

static inline void
ckrm_zone_inc_active(struct ckrm_zone *czone, int cnt)
{
	czone->nr_active += cnt;
}

static inline void
ckrm_zone_inc_inactive(struct ckrm_zone *czone, int cnt)
{
	czone->nr_inactive += cnt;
}

static inline void
ckrm_zone_dec_active(struct ckrm_zone *czone, int cnt)
{
	czone->nr_active -= cnt;
}

static inline void
ckrm_zone_dec_inactive(struct ckrm_zone *czone, int cnt)
{
	czone->nr_inactive -= cnt;
}

#else // !CONFIG_CKRM_RES_MEM

#define ckrm_set_page_class(a,b)	do{}while(0)
#define ckrm_set_pages_class(a,b,c)	do{}while(0)
#define ckrm_clear_page_class(a)	do{}while(0)
#define ckrm_clear_pages_class(a,b)	do{}while(0)
#define ckrm_change_page_class(a,b)	do{}while(0)
#define ckrm_change_pages_class(a,b,c)	do{}while(0)
#define ckrm_mem_inc_active(a)		do{}while(0)
#define ckrm_mem_dec_active(a)		do{}while(0)
#define ckrm_mem_inc_inactive(a)	do{}while(0)
#define ckrm_mem_dec_inactive(a)	do{}while(0)
#define ckrm_shrink_list_empty()	(1)
#define ckrm_kick_page(a,b)		(0)
#define ckrm_class_limit_ok(a)		(1)
#define ckrm_task_mm_init(a)		do{}while(0)
#define ckrm_task_clear_mm(a, b)	do{}while(0)
#define ckrm_task_change_mm(a, b, c)	do{}while(0)
#define ckrm_mm_init(a)			do{}while(0)
#define ckrm_mm_setclass(a, b)		do{}while(0)
#define ckrm_mm_clearclass(a)		do{}while(0)
#define ckrm_zone_inc_active(a, b)	do{}while(0)
#define ckrm_zone_inc_inactive(a, b)	do{}while(0)
#define ckrm_zone_dec_active(a, b)	do{}while(0)
#define ckrm_zone_dec_inactive(a, b)	do{}while(0)

#endif // CONFIG_CKRM_RES_MEM

#endif // _LINUX_CKRM_MEM_INLINE_H_
