#include <linux/ckrm_mem_inline.h>

static inline void
add_page_to_active_list(struct zone *zone, struct page *page)
{
	list_add(&page->lru, &zone->active_list);
	zone->nr_active++;
	ckrm_mem_inc_active(page);
}

static inline void
add_page_to_inactive_list(struct zone *zone, struct page *page)
{
	list_add(&page->lru, &zone->inactive_list);
	zone->nr_inactive++;
	ckrm_mem_inc_inactive(page);
}

static inline void
del_page_from_active_list(struct zone *zone, struct page *page)
{
	list_del(&page->lru);
	zone->nr_active--;
	ckrm_mem_dec_active(page);
}

static inline void
del_page_from_inactive_list(struct zone *zone, struct page *page)
{
	list_del(&page->lru);
	zone->nr_inactive--;
	ckrm_mem_dec_inactive(page);
}

static inline void
del_page_from_lru(struct zone *zone, struct page *page)
{
	list_del(&page->lru);
	if (PageActive(page)) {
		ClearPageActive(page);
		zone->nr_active--;
		ckrm_mem_dec_active(page);
	} else {
		zone->nr_inactive--;
		ckrm_mem_dec_inactive(page);
	}
}
