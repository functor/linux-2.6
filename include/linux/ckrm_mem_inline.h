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

#define ckrm_shrink_list_empty() list_empty(&ckrm_shrink_list)

static inline struct ckrm_mem_res *
ckrm_get_mem_class(struct task_struct *tsk)
{
	return ckrm_get_res_class(tsk->taskclass, mem_rcbs.resid,
		struct ckrm_mem_res);
}

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

static inline void
set_page_ckrmzone( struct page *page, struct ckrm_zone *cz)
{
	page->ckrm_zone = cz;
}

static inline struct ckrm_zone *
page_ckrmzone(struct page *page)
{
	return page->ckrm_zone;
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
		return 1;
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
	struct ckrm_mem_res *parcls = ckrm_get_res_class(cls->parent,
				mem_rcbs.resid, struct ckrm_mem_res);

	if (!cls)
		return;

	atomic_inc(&cls->pg_total);
	if (borrow)
		cls->pg_lent++;

	parcls = ckrm_get_res_class(cls->parent,
				mem_rcbs.resid, struct ckrm_mem_res);
	if (parcls && ((cls->pg_guar == CKRM_SHARE_DONTCARE) ||
			(atomic_read(&cls->pg_total) > cls->pg_unused))) {
		incr_use_count(parcls, 1);
		cls->pg_borrowed++;
	} else
		atomic_inc(&ckrm_mem_real_count);

	if ((cls->pg_limit != CKRM_SHARE_DONTCARE) &&
			(atomic_read(&cls->pg_total) >=
			((ckrm_mem_shrink_at * cls->pg_limit) / 100)) &&
			((cls->flags & CLS_AT_LIMIT) != CLS_AT_LIMIT)) {
		ckrm_shrink_atlimit(cls);
	}
	return;
}

static inline void
decr_use_count(struct ckrm_mem_res *cls, int borrowed)
{
	if (!cls)
		return;
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
	struct ckrm_zone *new_czone, *old_czone;

	if (!cls) {
		if (!ckrm_mem_root_class) {
			set_page_ckrmzone(page, NULL);
			return;
		}
		cls = ckrm_mem_root_class;
	}
	new_czone = &cls->ckrm_zone[page_zonenum(page)];
	old_czone = page_ckrmzone(page);
	
	if (old_czone)
		kref_put(&old_czone->memcls->nr_users, memclass_release);

	set_page_ckrmzone(page, new_czone);
	kref_get(&cls->nr_users);
	incr_use_count(cls, 0);
	SetPageCkrmAccount(page);
}

static inline void
ckrm_change_page_class(struct page *page, struct ckrm_mem_res *newcls)
{
	struct ckrm_zone *old_czone = page_ckrmzone(page), *new_czone;
	struct ckrm_mem_res *oldcls;

	if  (!newcls) {
		if (!ckrm_mem_root_class)
			return;
		newcls = ckrm_mem_root_class;
	}

	oldcls = old_czone->memcls;
	if (oldcls == newcls)
		return;

	if (oldcls) {
		kref_put(&oldcls->nr_users, memclass_release);
		decr_use_count(oldcls, 0);
	}

	new_czone = &newcls->ckrm_zone[page_zonenum(page)];
	set_page_ckrmzone(page, new_czone);
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
ckrm_clear_page_class(struct page *page)
{
	struct ckrm_zone *czone = page_ckrmzone(page);
	if (czone != NULL) {
		if (PageCkrmAccount(page)) {
			decr_use_count(czone->memcls, 0);
			ClearPageCkrmAccount(page);
		}
		kref_put(&czone->memcls->nr_users, memclass_release);
		set_page_ckrmzone(page, NULL);
	}
}

static inline void
ckrm_mem_inc_active(struct page *page)
{
	struct ckrm_mem_res *cls = ckrm_get_mem_class(current)
						?: ckrm_mem_root_class;
	struct ckrm_zone *czone;

	if (cls == NULL)
		return;

	ckrm_set_page_class(page, cls);
	czone = page_ckrmzone(page);
	czone->nr_active++;
	list_add(&page->lru, &czone->active_list);
}

static inline void
ckrm_mem_dec_active(struct page *page)
{
	struct ckrm_zone *czone = page_ckrmzone(page);
	if (czone == NULL)
		return;

	list_del(&page->lru);
	czone->nr_active--;
	ckrm_clear_page_class(page);
}


static inline void
ckrm_mem_inc_inactive(struct page *page)
{
	struct ckrm_mem_res *cls = ckrm_get_mem_class(current)
						?: ckrm_mem_root_class;
	struct ckrm_zone *czone;

	if (cls == NULL)
		return;

	ckrm_set_page_class(page, cls);
	czone = page_ckrmzone(page);
	czone->nr_inactive++;
	list_add(&page->lru, &czone->inactive_list);
}

static inline void
ckrm_mem_dec_inactive(struct page *page)
{
	struct ckrm_zone *czone = page_ckrmzone(page);
	if (czone == NULL)
		return;

	czone->nr_inactive--;
	list_del(&page->lru);
	ckrm_clear_page_class(page);
}

static inline void
ckrm_zone_add_active(struct ckrm_zone *czone, int cnt)
{
	czone->nr_active += cnt;
}

static inline void
ckrm_zone_add_inactive(struct ckrm_zone *czone, int cnt)
{
	czone->nr_inactive += cnt;
}

static inline void
ckrm_zone_sub_active(struct ckrm_zone *czone, int cnt)
{
	czone->nr_active -= cnt;
}

static inline void
ckrm_zone_sub_inactive(struct ckrm_zone *czone, int cnt)
{
	czone->nr_inactive -= cnt;
}

static inline int
ckrm_class_limit_ok(struct ckrm_mem_res *cls)
{
	int ret;

	if ((mem_rcbs.resid == -1) || !cls) {
		return 1;
	}
	if (cls->pg_limit == CKRM_SHARE_DONTCARE) {
		struct ckrm_mem_res *parcls = ckrm_get_res_class(cls->parent,
					mem_rcbs.resid, struct ckrm_mem_res);
		ret = (parcls ? ckrm_class_limit_ok(parcls) : 0);
	} else
		ret = (atomic_read(&cls->pg_total) <= cls->pg_limit);

	/* If we are failing, just nudge the back end */
	if (ret == 0)
		ckrm_shrink_atlimit(cls);

	return ret;
}

static inline void
ckrm_page_init(struct page *page)
{
	page->flags &= ~(1 << PG_ckrm_account);
	set_page_ckrmzone(page, NULL);
}


/* task/mm initializations/cleanup */

static inline void
ckrm_task_mm_init(struct task_struct *tsk)
{
	INIT_LIST_HEAD(&tsk->mm_peers);
}

static inline void
ckrm_task_mm_set(struct mm_struct * mm, struct task_struct *task)
{
	spin_lock(&mm->peertask_lock);
	if (!list_empty(&task->mm_peers)) {
		printk(KERN_ERR "MEM_RC: Task list NOT empty!! emptying...\n");
		list_del_init(&task->mm_peers);
	}
	list_add_tail(&task->mm_peers, &mm->tasklist);
	spin_unlock(&mm->peertask_lock);
	if (mm->memclass != ckrm_get_mem_class(task))
		ckrm_mem_migrate_mm(mm, NULL);
	return;
}

static inline void
ckrm_task_mm_change(struct task_struct *tsk,
		struct mm_struct *oldmm, struct mm_struct *newmm)
{
	if (oldmm) {
		spin_lock(&oldmm->peertask_lock);
		list_del(&tsk->mm_peers);
		ckrm_mem_migrate_mm(oldmm, NULL);
		spin_unlock(&oldmm->peertask_lock);
	}
	spin_lock(&newmm->peertask_lock);
	list_add_tail(&tsk->mm_peers, &newmm->tasklist);
	ckrm_mem_migrate_mm(newmm, NULL);
	spin_unlock(&newmm->peertask_lock);
}

static inline void
ckrm_task_mm_clear(struct task_struct *tsk, struct mm_struct *mm)
{
	spin_lock(&mm->peertask_lock);
	list_del_init(&tsk->mm_peers);
	ckrm_mem_migrate_mm(mm, NULL);
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

static inline void ckrm_init_lists(struct zone *zone) 			{}

static inline void ckrm_add_tail_inactive(struct page *page)
{
	 struct ckrm_zone *ckrm_zone = page_ckrmzone(page);
	 list_add_tail(&page->lru, &ckrm_zone->inactive_list);
}

#else

#define ckrm_shrink_list_empty()		(1)

static inline void *
ckrm_get_memclass(struct task_struct *tsk)
{
	return NULL;
}

static inline void ckrm_clear_page_class(struct page *p)		{}

static inline void ckrm_mem_inc_active(struct page *p)			{}
static inline void ckrm_mem_dec_active(struct page *p)			{}
static inline void ckrm_mem_inc_inactive(struct page *p)		{}
static inline void ckrm_mem_dec_inactive(struct page *p)		{}

#define ckrm_zone_add_active(a, b)	do {} while (0)
#define ckrm_zone_add_inactive(a, b)	do {} while (0)
#define ckrm_zone_sub_active(a, b)	do {} while (0)
#define ckrm_zone_sub_inactive(a, b)	do {} while (0)

#define ckrm_class_limit_ok(a)						(1)

static inline void ckrm_page_init(struct page *p)			{}
static inline void ckrm_task_mm_init(struct task_struct *tsk)		{}
static inline void ckrm_task_mm_set(struct mm_struct * mm,
					struct task_struct *task)	{}
static inline void ckrm_task_mm_change(struct task_struct *tsk,
		struct mm_struct *oldmm, struct mm_struct *newmm)	{}
static inline void ckrm_task_mm_clear(struct task_struct *tsk,
						struct mm_struct *mm)	{}

static inline void ckrm_mm_init(struct mm_struct *mm)			{}

/* using #define instead of static inline as the prototype requires   *
 * data structures that is available only with the controller enabled */
#define ckrm_mm_setclass(a, b) 					do {} while(0)

static inline void ckrm_mm_clearclass(struct mm_struct *mm)		{}

static inline void ckrm_init_lists(struct zone *zone)
{
	INIT_LIST_HEAD(&zone->active_list);
	INIT_LIST_HEAD(&zone->inactive_list);
}

static inline void ckrm_add_tail_inactive(struct page *page)
{
	 struct zone *zone = page_zone(page);
	 list_add_tail(&page->lru, &zone->inactive_list);
}
#endif 
#endif /* _LINUX_CKRM_MEM_INLINE_H_ */
