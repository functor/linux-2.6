/* ckrm_memctlr.c - Basic routines for the CKRM memory controller
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

#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/ckrm_mem_inline.h>

static int
ckrm_mem_evaluate_page_anon(struct page* page)
{
	struct ckrm_mem_res* pgcls = page_ckrmzone(page)->memcls;
	struct ckrm_mem_res* maxshareclass = NULL;
	struct anon_vma *anon_vma = (struct anon_vma *) page->mapping;
	struct vm_area_struct *vma;
	struct mm_struct* mm;
	int ret = 0;

	if (!spin_trylock(&anon_vma->lock))
		return 0;
	BUG_ON(list_empty(&anon_vma->head));
	list_for_each_entry(vma, &anon_vma->head, anon_vma_node) {
		mm = vma->vm_mm;
		if (!maxshareclass || ckrm_mem_share_compare(maxshareclass,
				mm->memclass) < 0) {
			maxshareclass = mm->memclass;
		}
	}
	spin_unlock(&anon_vma->lock);

	if (!maxshareclass) {
		maxshareclass = ckrm_mem_root_class;
	}
	if (pgcls != maxshareclass) {
		ckrm_change_page_class(page, maxshareclass);
		ret = 1;
	}
	return ret;
}

static int
ckrm_mem_evaluate_page_file(struct page* page)
{
	struct ckrm_mem_res* pgcls = page_ckrmzone(page)->memcls;
	struct ckrm_mem_res* maxshareclass = NULL;
	struct address_space *mapping = page->mapping;
	struct vm_area_struct *vma = NULL;
	pgoff_t pgoff = page->index << (PAGE_CACHE_SHIFT - PAGE_SHIFT);
	struct prio_tree_iter iter;
	struct mm_struct* mm;
	int ret = 0;

	if (!mapping)
		return 0;

	if (!spin_trylock(&mapping->i_mmap_lock))
		return 0;

	vma_prio_tree_foreach(vma, &iter, &mapping->i_mmap,
					pgoff, pgoff) {
		mm = vma->vm_mm;
		if (!maxshareclass || ckrm_mem_share_compare(maxshareclass,
				mm->memclass)<0)
			maxshareclass = mm->memclass;
	}
	spin_unlock(&mapping->i_mmap_lock);

	if (!maxshareclass) {
		maxshareclass = ckrm_mem_root_class;
	}
	if (pgcls != maxshareclass) {
		ckrm_change_page_class(page, maxshareclass);
		ret = 1;
	}
	return ret;
}

static int
ckrm_mem_evaluate_page(struct page* page)
{
	int ret = 0;
	if (page->mapping) {
		if (PageAnon(page))
			ret = ckrm_mem_evaluate_page_anon(page);
		else
			ret = ckrm_mem_evaluate_page_file(page);
	}
	return ret;
}

void
ckrm_mem_migrate_all_pages(struct ckrm_mem_res* from, struct ckrm_mem_res* def)
{
	int i;
	struct page *page;
	struct zone *zone;
	struct list_head *pos, *next;
	struct ckrm_zone *ckrm_zone;

	for (i = 0; i < MAX_NR_ZONES; i++) {
		ckrm_zone = &from->ckrm_zone[i];
		zone = ckrm_zone->zone;
		spin_lock_irq(&zone->lru_lock);
		pos = ckrm_zone->inactive_list.next;
		while (pos != &ckrm_zone->inactive_list) {
			next = pos->next;
			page = list_entry(pos, struct page, lru);
			if (ckrm_mem_evaluate_page(page))
				ckrm_change_page_class(page, def);
			pos = next;
		}
		pos = ckrm_zone->active_list.next;
		while (pos != &ckrm_zone->active_list) {
			next = pos->next;
			page = list_entry(pos, struct page, lru);
			if (ckrm_mem_evaluate_page(page))
				ckrm_change_page_class(page, def);
			pos = next;
		}
		spin_unlock_irq(&zone->lru_lock);
	}
	return;
}

static inline int
class_migrate_pmd(struct mm_struct* mm, struct vm_area_struct* vma,
		pmd_t* pmdir, unsigned long address, unsigned long end)
{
	pte_t *pte;
	unsigned long pmd_end;

	if (pmd_none(*pmdir))
		return 0;
	BUG_ON(pmd_bad(*pmdir));

	pmd_end = (address+ PMD_SIZE) & PMD_MASK;
	if (end > pmd_end)
		end = pmd_end;

	do {
		pte = pte_offset_map(pmdir, address);
		if (pte_present(*pte)) {
			struct page *page = pte_page(*pte);
			struct ckrm_zone *czone = page_ckrmzone(page);
			if (page->mapping && czone) {
				struct zone *zone = czone->zone;
				spin_lock_irq(&zone->lru_lock);
				ckrm_change_page_class(page, mm->memclass);
				spin_unlock_irq(&zone->lru_lock);
			}
		}
		address += PAGE_SIZE;
		pte_unmap(pte);
		pte++;
	} while(address && (address < end));
	return 0;
}

static inline int
class_migrate_pgd(struct mm_struct* mm, struct vm_area_struct* vma,
		pgd_t* pgdir, unsigned long address, unsigned long end)
{
	pmd_t* pmd;
	unsigned long pgd_end;

	if (pgd_none(*pgdir))
		return 0;
	BUG_ON(pgd_bad(*pgdir));

	pmd = pmd_offset(pgdir, address);
	pgd_end = (address + PGDIR_SIZE) & PGDIR_MASK;

	if (pgd_end && (end > pgd_end))
		end = pgd_end;

	do {
		class_migrate_pmd(mm, vma, pmd, address, end);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address && (address < end));
	return 0;
}

static inline int
class_migrate_vma(struct mm_struct* mm, struct vm_area_struct* vma)
{
	pgd_t* pgdir;
	unsigned long address, end;

	address = vma->vm_start;
	end = vma->vm_end;

	pgdir = pgd_offset(vma->vm_mm, address);
	do {
		class_migrate_pgd(mm, vma, pgdir, address, end);
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		pgdir++;
	} while(address && (address < end));
	return 0;
}

/* this function is called with mm->peertask_lock hold */
void
ckrm_mem_migrate_mm(struct mm_struct* mm, struct ckrm_mem_res *def)
{
	struct task_struct *task;
	struct vm_area_struct *vma;
	struct ckrm_mem_res *maxshareclass = def;

	if (list_empty(&mm->tasklist)) {
		/* We leave the mm->memclass untouched since we believe that one
		 * mm with no task associated will be deleted soon or attach
		 * with another task later.
		 */
		return;
	}

	list_for_each_entry(task, &mm->tasklist, mm_peers) {
		struct ckrm_mem_res* cls = ckrm_get_mem_class(task);
		if (!cls)
			continue;
		if (!maxshareclass ||
				ckrm_mem_share_compare(maxshareclass,cls)<0 )
			maxshareclass = cls;
	}

	if (maxshareclass && (mm->memclass != maxshareclass)) {
		if (mm->memclass) {
			kref_put(&mm->memclass->nr_users, memclass_release);
		}
		mm->memclass = maxshareclass;
		kref_get(&maxshareclass->nr_users);

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

static int
shrink_weight(struct ckrm_zone *czone)
{
	u64 temp;
	struct zone *zone = czone->zone;
	struct ckrm_mem_res *cls = czone->memcls;
	int zone_usage, zone_guar, zone_total, guar, ret, cnt;

	zone_usage = czone->nr_active + czone->nr_inactive;
	czone->active_over = czone->inactive_over = 0;

	if (zone_usage < SWAP_CLUSTER_MAX * 4)
		return 0;

	if (cls->pg_guar == CKRM_SHARE_DONTCARE) {
		// no guarantee for this class. use implicit guarantee
		guar = cls->impl_guar / cls->nr_dontcare;
	} else {
		guar = cls->pg_unused / cls->nr_dontcare;
	}
	zone_total = zone->nr_active + zone->nr_inactive + zone->free_pages;
	temp = (u64) guar * zone_total;
	do_div(temp, ckrm_tot_lru_pages);
	zone_guar = (int) temp;

	ret = ((zone_usage - zone_guar) > SWAP_CLUSTER_MAX) ?
				(zone_usage - zone_guar) : 0;
	if (ret) {
		cnt = czone->nr_active - (2 * zone_guar / 3);
		if (cnt > 0)
			czone->active_over = cnt;
		cnt = czone->active_over + czone->nr_inactive
					- zone_guar / 3;
		if (cnt > 0)
			czone->inactive_over = cnt;
	}
	return ret;
}

/* insert an entry to the list and sort decendently*/
static void
list_add_sort(struct list_head *entry, struct list_head *head)
{
	struct ckrm_zone *czone, *new =
			list_entry(entry, struct ckrm_zone, victim_list);
	struct list_head* pos = head->next;

	while (pos != head) {
		czone = list_entry(pos, struct ckrm_zone, victim_list);
		if (new->shrink_weight > czone->shrink_weight) {
			__list_add(entry, pos->prev, pos);
			return;
		}
		pos = pos->next;
	}
	list_add_tail(entry, head);
	return;	
}

static void
shrink_choose_victims(struct list_head *victims,
		unsigned long nr_active, unsigned long nr_inactive)
{
	unsigned long nr;
	struct ckrm_zone* czone;
	struct list_head *pos, *next;

	pos = victims->next;
	while ((pos != victims) && (nr_active || nr_inactive)) {
		czone = list_entry(pos, struct ckrm_zone, victim_list);
		
		if (nr_active && czone->active_over) {
			nr = min(nr_active, czone->active_over);
			czone->shrink_active += nr;
			czone->active_over -= nr;
			nr_active -= nr;
		}

		if (nr_inactive && czone->inactive_over) {
			nr = min(nr_inactive, czone->inactive_over);
			czone->shrink_inactive += nr;
			czone->inactive_over -= nr;
			nr_inactive -= nr;
		}
		pos = pos->next;
	}

	pos = victims->next;
	while (pos != victims) {
		czone = list_entry(pos, struct ckrm_zone, victim_list);
		next = pos->next;
		if (czone->shrink_active == 0 && czone->shrink_inactive == 0) {
			list_del_init(pos);
			ckrm_clear_shrink(czone);
		}
		pos = next;
	}	
	return;
}

void
shrink_get_victims(struct zone *zone, unsigned long nr_active,
		unsigned long nr_inactive, struct list_head *victims)
{
	struct list_head *pos;
	struct ckrm_mem_res *cls;
	struct ckrm_zone *czone;
	int zoneindex = zone_idx(zone);
	
	if (ckrm_nr_mem_classes <= 1) {
		if (ckrm_mem_root_class) {
			czone = ckrm_mem_root_class->ckrm_zone + zoneindex;
			if (!ckrm_test_set_shrink(czone)) {
				list_add(&czone->victim_list, victims);
				czone->shrink_active = nr_active;
				czone->shrink_inactive = nr_inactive;
			}
		}
		return;
	}
	spin_lock(&ckrm_mem_lock);
	list_for_each_entry(cls, &ckrm_memclass_list, mcls_list) {
		czone = cls->ckrm_zone + zoneindex;
		if (ckrm_test_set_shrink(czone))
			continue;

		czone->shrink_active = 0;
		czone->shrink_inactive = 0;
		czone->shrink_weight = shrink_weight(czone);
		if (czone->shrink_weight) {
			list_add_sort(&czone->victim_list, victims);
		} else {
			ckrm_clear_shrink(czone);
		}
	}
	pos = victims->next;
	while (pos != victims) {
		czone = list_entry(pos, struct ckrm_zone, victim_list);
		pos = pos->next;
	}
	shrink_choose_victims(victims, nr_active, nr_inactive);
	spin_unlock(&ckrm_mem_lock);
	pos = victims->next;
	while (pos != victims) {
		czone = list_entry(pos, struct ckrm_zone, victim_list);
		pos = pos->next;
	}
}

LIST_HEAD(ckrm_shrink_list);
void
ckrm_shrink_atlimit(struct ckrm_mem_res *cls)
{
	struct zone *zone;
	unsigned long now = jiffies;
	int order;

	if (!cls || (cls->pg_limit == CKRM_SHARE_DONTCARE) ||
			((cls->flags & CLS_AT_LIMIT) == CLS_AT_LIMIT)) {
		return;
	}
	if ((cls->last_shrink > now) /* jiffies wrapped around */ ||
		   (cls->last_shrink + (ckrm_mem_shrink_interval * HZ)) < now) {
		cls->last_shrink = now;
		cls->shrink_count = 0;
	}
	cls->shrink_count++;
	if (cls->shrink_count > ckrm_mem_shrink_count) {
		return;
	}
	spin_lock(&ckrm_mem_lock);
	list_add(&cls->shrink_list, &ckrm_shrink_list);
	spin_unlock(&ckrm_mem_lock);
	cls->flags |= CLS_AT_LIMIT;
	for_each_zone(zone) {
		/* This is just a number to get to wakeup kswapd */
		order = atomic_read(&cls->pg_total) -
			((ckrm_mem_shrink_to * cls->pg_limit) / 100);
		wakeup_kswapd(zone);
		break; // only once is enough
	}
}
