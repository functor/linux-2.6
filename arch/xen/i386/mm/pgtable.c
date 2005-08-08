/*
 *  linux/arch/i386/mm/pgtable.c
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/smp.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/spinlock.h>

#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/fixmap.h>
#include <asm/e820.h>
#include <asm/tlb.h>
#include <asm/tlbflush.h>
#include <asm/io.h>

#include <asm-xen/foreign_page.h>

void show_mem(void)
{
	int total = 0, reserved = 0;
	int shared = 0, cached = 0;
	int highmem = 0;
	struct page *page;
	pg_data_t *pgdat;
	unsigned long i;

	printk("Mem-info:\n");
	show_free_areas();
	printk("Free swap:       %6ldkB\n", nr_swap_pages<<(PAGE_SHIFT-10));
	for_each_pgdat(pgdat) {
		for (i = 0; i < pgdat->node_spanned_pages; ++i) {
			page = pgdat->node_mem_map + i;
			total++;
			if (PageHighMem(page))
				highmem++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (page_count(page))
				shared += page_count(page) - 1;
		}
	}
	printk("%d pages of RAM\n", total);
	printk("%d pages of HIGHMEM\n",highmem);
	printk("%d reserved pages\n",reserved);
	printk("%d pages shared\n",shared);
	printk("%d pages swap cached\n",cached);
}

/*
 * Associate a virtual page frame with a given physical page frame 
 * and protection flags for that frame.
 */ 
static void set_pte_pfn(unsigned long vaddr, unsigned long pfn, pgprot_t flags)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = swapper_pg_dir + pgd_index(vaddr);
	if (pgd_none(*pgd)) {
		BUG();
		return;
	}
	pud = pud_offset(pgd, vaddr);
	if (pud_none(*pud)) {
		BUG();
		return;
	}
	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		BUG();
		return;
	}
	pte = pte_offset_kernel(pmd, vaddr);
	/* <pfn,flags> stored as-is, to permit clearing entries */
	set_pte(pte, pfn_pte(pfn, flags));

	/*
	 * It's enough to flush this one mapping.
	 * (PGE mappings get flushed as well)
	 */
	__flush_tlb_one(vaddr);
}

/*
 * Associate a virtual page frame with a given physical page frame 
 * and protection flags for that frame.
 */ 
static void set_pte_pfn_ma(unsigned long vaddr, unsigned long pfn,
			   pgprot_t flags)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	pgd = swapper_pg_dir + pgd_index(vaddr);
	if (pgd_none(*pgd)) {
		BUG();
		return;
	}
	pud = pud_offset(pgd, vaddr);
	if (pud_none(*pud)) {
		BUG();
		return;
	}
	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		BUG();
		return;
	}
	pte = pte_offset_kernel(pmd, vaddr);
	/* <pfn,flags> stored as-is, to permit clearing entries */
	set_pte(pte, pfn_pte_ma(pfn, flags));

	/*
	 * It's enough to flush this one mapping.
	 * (PGE mappings get flushed as well)
	 */
	__flush_tlb_one(vaddr);
}

/*
 * Associate a large virtual page frame with a given physical page frame 
 * and protection flags for that frame. pfn is for the base of the page,
 * vaddr is what the page gets mapped to - both must be properly aligned. 
 * The pmd must already be instantiated. Assumes PAE mode.
 */ 
void set_pmd_pfn(unsigned long vaddr, unsigned long pfn, pgprot_t flags)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	if (vaddr & (PMD_SIZE-1)) {		/* vaddr is misaligned */
		printk ("set_pmd_pfn: vaddr misaligned\n");
		return; /* BUG(); */
	}
	if (pfn & (PTRS_PER_PTE-1)) {		/* pfn is misaligned */
		printk ("set_pmd_pfn: pfn misaligned\n");
		return; /* BUG(); */
	}
	pgd = swapper_pg_dir + pgd_index(vaddr);
	if (pgd_none(*pgd)) {
		printk ("set_pmd_pfn: pgd_none\n");
		return; /* BUG(); */
	}
	pud = pud_offset(pgd, vaddr);
	pmd = pmd_offset(pud, vaddr);
	set_pmd(pmd, pfn_pmd(pfn, flags));
	/*
	 * It's enough to flush this one mapping.
	 * (PGE mappings get flushed as well)
	 */
	__flush_tlb_one(vaddr);
}

void __set_fixmap (enum fixed_addresses idx, unsigned long phys, pgprot_t flags)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		BUG();
		return;
	}
	set_pte_pfn(address, phys >> PAGE_SHIFT, flags);
}

void __set_fixmap_ma (enum fixed_addresses idx, unsigned long phys, pgprot_t flags)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		BUG();
		return;
	}
	set_pte_pfn_ma(address, phys >> PAGE_SHIFT, flags);
}

pte_t *pte_alloc_one_kernel(struct mm_struct *mm, unsigned long address)
{
	pte_t *pte = (pte_t *)__get_free_page(GFP_KERNEL|__GFP_REPEAT|__GFP_ZERO);
	if (pte)
		make_page_readonly(pte);
	return pte;
}

void pte_ctor(void *pte, kmem_cache_t *cache, unsigned long unused)
{
	struct page *page = virt_to_page(pte);
	SetPageForeign(page, pte_free);
	set_page_count(page, 1);

	clear_page(pte);
	make_page_readonly(pte);
	xen_pte_pin(__pa(pte));
}

void pte_dtor(void *pte, kmem_cache_t *cache, unsigned long unused)
{
	struct page *page = virt_to_page(pte);
	ClearPageForeign(page);

	xen_pte_unpin(__pa(pte));
	make_page_writable(pte);
}

struct page *pte_alloc_one(struct mm_struct *mm, unsigned long address)
{
	pte_t *ptep;

#ifdef CONFIG_HIGHPTE
	struct page *pte;

	pte = alloc_pages(GFP_KERNEL|__GFP_HIGHMEM|__GFP_REPEAT|__GFP_ZERO, 0);
	if (pte == NULL)
		return pte;
	if (PageHighMem(pte))
		return pte;
	/* not a highmem page -- free page and grab one from the cache */
	__free_page(pte);
#endif
	ptep = kmem_cache_alloc(pte_cache, GFP_KERNEL);
	if (ptep)
		return virt_to_page(ptep);
	return NULL;
}

void pte_free(struct page *pte)
{
	set_page_count(pte, 1);
#ifdef CONFIG_HIGHPTE
	if (!PageHighMem(pte))
#endif
		kmem_cache_free(pte_cache,
				phys_to_virt(page_to_pseudophys(pte)));
#ifdef CONFIG_HIGHPTE
	else
		__free_page(pte);
#endif
}

void pmd_ctor(void *pmd, kmem_cache_t *cache, unsigned long flags)
{
	memset(pmd, 0, PTRS_PER_PMD*sizeof(pmd_t));
}

/*
 * List of all pgd's needed for non-PAE so it can invalidate entries
 * in both cached and uncached pgd's; not needed for PAE since the
 * kernel pmd is shared. If PAE were not to share the pmd a similar
 * tactic would be needed. This is essentially codepath-based locking
 * against pageattr.c; it is the unique case in which a valid change
 * of kernel pagetables can't be lazily synchronized by vmalloc faults.
 * vmalloc faults work because attached pagetables are never freed.
 * The locking scheme was chosen on the basis of manfred's
 * recommendations and having no core impact whatsoever.
 * -- wli
 */
DEFINE_SPINLOCK(pgd_lock);
struct page *pgd_list;

static inline void pgd_list_add(pgd_t *pgd)
{
	struct page *page = virt_to_page(pgd);
	page->index = (unsigned long)pgd_list;
	if (pgd_list)
		pgd_list->private = (unsigned long)&page->index;
	pgd_list = page;
	page->private = (unsigned long)&pgd_list;
}

static inline void pgd_list_del(pgd_t *pgd)
{
	struct page *next, **pprev, *page = virt_to_page(pgd);
	next = (struct page *)page->index;
	pprev = (struct page **)page->private;
	*pprev = next;
	if (next)
		next->private = (unsigned long)pprev;
}

void pgd_ctor(void *pgd, kmem_cache_t *cache, unsigned long unused)
{
	unsigned long flags;

	if (PTRS_PER_PMD == 1)
		spin_lock_irqsave(&pgd_lock, flags);

	memcpy((pgd_t *)pgd + USER_PTRS_PER_PGD,
			swapper_pg_dir + USER_PTRS_PER_PGD,
			(PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));

	if (PTRS_PER_PMD > 1)
		goto out;

	pgd_list_add(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
	memset(pgd, 0, USER_PTRS_PER_PGD*sizeof(pgd_t));
 out:
	make_page_readonly(pgd);
	xen_pgd_pin(__pa(pgd));
}

/* never called when PTRS_PER_PMD > 1 */
void pgd_dtor(void *pgd, kmem_cache_t *cache, unsigned long unused)
{
	unsigned long flags; /* can be called from interrupt context */

	xen_pgd_unpin(__pa(pgd));
	make_page_writable(pgd);

	if (PTRS_PER_PMD > 1)
		return;

	spin_lock_irqsave(&pgd_lock, flags);
	pgd_list_del(pgd);
	spin_unlock_irqrestore(&pgd_lock, flags);
}

pgd_t *pgd_alloc(struct mm_struct *mm)
{
	int i;
	pgd_t *pgd = kmem_cache_alloc(pgd_cache, GFP_KERNEL);

	if (PTRS_PER_PMD == 1 || !pgd)
		return pgd;

	for (i = 0; i < USER_PTRS_PER_PGD; ++i) {
		pmd_t *pmd = kmem_cache_alloc(pmd_cache, GFP_KERNEL);
		if (!pmd)
			goto out_oom;
		set_pgd(&pgd[i], __pgd(1 + __pa(pmd)));
	}
	return pgd;

out_oom:
	for (i--; i >= 0; i--)
		kmem_cache_free(pmd_cache, (void *)__va(pgd_val(pgd[i])-1));
	kmem_cache_free(pgd_cache, pgd);
	return NULL;
}

void pgd_free(pgd_t *pgd)
{
	int i;

	/* in the PAE case user pgd entries are overwritten before usage */
	if (PTRS_PER_PMD > 1)
		for (i = 0; i < USER_PTRS_PER_PGD; ++i)
			kmem_cache_free(pmd_cache, (void *)__va(pgd_val(pgd[i])-1));
	/* in the non-PAE case, clear_page_range() clears user pgd entries */
	kmem_cache_free(pgd_cache, pgd);
}

#ifndef CONFIG_XEN_SHADOW_MODE
void make_lowmem_page_readonly(void *va)
{
	pgd_t *pgd = pgd_offset_k((unsigned long)va);
	pud_t *pud = pud_offset(pgd, (unsigned long)va);
	pmd_t *pmd = pmd_offset(pud, (unsigned long)va);
	pte_t *pte = pte_offset_kernel(pmd, (unsigned long)va);
	set_pte(pte, pte_wrprotect(*pte));
}

void make_lowmem_page_writable(void *va)
{
	pgd_t *pgd = pgd_offset_k((unsigned long)va);
	pud_t *pud = pud_offset(pgd, (unsigned long)va);
	pmd_t *pmd = pmd_offset(pud, (unsigned long)va);
	pte_t *pte = pte_offset_kernel(pmd, (unsigned long)va);
	set_pte(pte, pte_mkwrite(*pte));
}

void make_page_readonly(void *va)
{
	pgd_t *pgd = pgd_offset_k((unsigned long)va);
	pud_t *pud = pud_offset(pgd, (unsigned long)va);
	pmd_t *pmd = pmd_offset(pud, (unsigned long)va);
	pte_t *pte = pte_offset_kernel(pmd, (unsigned long)va);
	set_pte(pte, pte_wrprotect(*pte));
	if ( (unsigned long)va >= (unsigned long)high_memory )
	{
		unsigned long phys;
		phys = machine_to_phys(*(unsigned long *)pte & PAGE_MASK);
#ifdef CONFIG_HIGHMEM
		if ( (phys >> PAGE_SHIFT) < highstart_pfn )
#endif
			make_lowmem_page_readonly(phys_to_virt(phys));
	}
}

void make_page_writable(void *va)
{
	pgd_t *pgd = pgd_offset_k((unsigned long)va);
	pud_t *pud = pud_offset(pgd, (unsigned long)va);
	pmd_t *pmd = pmd_offset(pud, (unsigned long)va);
	pte_t *pte = pte_offset_kernel(pmd, (unsigned long)va);
	set_pte(pte, pte_mkwrite(*pte));
	if ( (unsigned long)va >= (unsigned long)high_memory )
	{
		unsigned long phys;
		phys = machine_to_phys(*(unsigned long *)pte & PAGE_MASK);
#ifdef CONFIG_HIGHMEM
		if ( (phys >> PAGE_SHIFT) < highstart_pfn )
#endif
			make_lowmem_page_writable(phys_to_virt(phys));
	}
}

void make_pages_readonly(void *va, unsigned int nr)
{
	while ( nr-- != 0 )
	{
		make_page_readonly(va);
		va = (void *)((unsigned long)va + PAGE_SIZE);
	}
}

void make_pages_writable(void *va, unsigned int nr)
{
	while ( nr-- != 0 )
	{
		make_page_writable(va);
		va = (void *)((unsigned long)va + PAGE_SIZE);
	}
}
#endif /* CONFIG_XEN_SHADOW_MODE */