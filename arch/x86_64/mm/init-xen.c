/*
 *  linux/arch/x86_64/mm/init.c
 *
 *  Copyright (C) 1995  Linus Torvalds
 *  Copyright (C) 2000  Pavel Machek <pavel@suse.cz>
 *  Copyright (C) 2002,2003 Andi Kleen <ak@suse.de>
 *
 *  Jun Nakajima <jun.nakajima@intel.com>
 *	Modified for Xen.
 */

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/mman.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/pagemap.h>
#include <linux/bootmem.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/poison.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/memory_hotplug.h>

#include <asm/processor.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/dma.h>
#include <asm/fixmap.h>
#include <asm/e820.h>
#include <asm/apic.h>
#include <asm/tlb.h>
#include <asm/mmu_context.h>
#include <asm/proto.h>
#include <asm/smp.h>
#include <asm/sections.h>

#include <xen/features.h>

#ifndef Dprintk
#define Dprintk(x...)
#endif

struct dma_mapping_ops* dma_ops;
EXPORT_SYMBOL(dma_ops);

extern unsigned long *contiguous_bitmap;

static unsigned long dma_reserve __initdata;

DEFINE_PER_CPU(struct mmu_gather, mmu_gathers);
extern unsigned long start_pfn;

/*
 * Use this until direct mapping is established, i.e. before __va() is 
 * available in init_memory_mapping().
 */

#define addr_to_page(addr, page)				\
	(addr) &= PHYSICAL_PAGE_MASK;				\
	(page) = ((unsigned long *) ((unsigned long)		\
	(((mfn_to_pfn((addr) >> PAGE_SHIFT)) << PAGE_SHIFT) +	\
	__START_KERNEL_map)))

static void early_make_page_readonly(void *va, unsigned int feature)
{
	unsigned long addr, _va = (unsigned long)va;
	pte_t pte, *ptep;
	unsigned long *page = (unsigned long *) init_level4_pgt;

	if (xen_feature(feature))
		return;

	addr = (unsigned long) page[pgd_index(_va)];
	addr_to_page(addr, page);

	addr = page[pud_index(_va)];
	addr_to_page(addr, page);

	addr = page[pmd_index(_va)];
	addr_to_page(addr, page);

	ptep = (pte_t *) &page[pte_index(_va)];

	pte.pte = ptep->pte & ~_PAGE_RW;
	if (HYPERVISOR_update_va_mapping(_va, pte, 0))
		BUG();
}

void make_page_readonly(void *va, unsigned int feature)
{
	pgd_t *pgd; pud_t *pud; pmd_t *pmd; pte_t pte, *ptep;
	unsigned long addr = (unsigned long) va;

	if (xen_feature(feature))
		return;

	pgd = pgd_offset_k(addr);
	pud = pud_offset(pgd, addr);
	pmd = pmd_offset(pud, addr);
	ptep = pte_offset_kernel(pmd, addr);

	pte.pte = ptep->pte & ~_PAGE_RW;
	if (HYPERVISOR_update_va_mapping(addr, pte, 0))
		xen_l1_entry_update(ptep, pte); /* fallback */

	if ((addr >= VMALLOC_START) && (addr < VMALLOC_END))
		make_page_readonly(__va(pte_pfn(pte) << PAGE_SHIFT), feature);
}

void make_page_writable(void *va, unsigned int feature)
{
	pgd_t *pgd; pud_t *pud; pmd_t *pmd; pte_t pte, *ptep;
	unsigned long addr = (unsigned long) va;

	if (xen_feature(feature))
		return;

	pgd = pgd_offset_k(addr);
	pud = pud_offset(pgd, addr);
	pmd = pmd_offset(pud, addr);
	ptep = pte_offset_kernel(pmd, addr);

	pte.pte = ptep->pte | _PAGE_RW;
	if (HYPERVISOR_update_va_mapping(addr, pte, 0))
		xen_l1_entry_update(ptep, pte); /* fallback */

	if ((addr >= VMALLOC_START) && (addr < VMALLOC_END))
		make_page_writable(__va(pte_pfn(pte) << PAGE_SHIFT), feature);
}

void make_pages_readonly(void *va, unsigned nr, unsigned int feature)
{
	if (xen_feature(feature))
		return;

	while (nr-- != 0) {
		make_page_readonly(va, feature);
		va = (void*)((unsigned long)va + PAGE_SIZE);
	}
}

void make_pages_writable(void *va, unsigned nr, unsigned int feature)
{
	if (xen_feature(feature))
		return;

	while (nr-- != 0) {
		make_page_writable(va, feature);
		va = (void*)((unsigned long)va + PAGE_SIZE);
	}
}

/*
 * NOTE: pagetable_init alloc all the fixmap pagetables contiguous on the
 * physical space so we can cache the place of the first one and move
 * around without checking the pgd every time.
 */

void show_mem(void)
{
	long i, total = 0, reserved = 0;
	long shared = 0, cached = 0;
	pg_data_t *pgdat;
	struct page *page;

	printk(KERN_INFO "Mem-info:\n");
	show_free_areas();
	printk(KERN_INFO "Free swap:       %6ldkB\n", nr_swap_pages<<(PAGE_SHIFT-10));

	for_each_online_pgdat(pgdat) {
               for (i = 0; i < pgdat->node_spanned_pages; ++i) {
			page = pfn_to_page(pgdat->node_start_pfn + i);
			total++;
			if (PageReserved(page))
				reserved++;
			else if (PageSwapCache(page))
				cached++;
			else if (page_count(page))
				shared += page_count(page) - 1;
               }
	}
	printk(KERN_INFO "%lu pages of RAM\n", total);
	printk(KERN_INFO "%lu reserved pages\n",reserved);
	printk(KERN_INFO "%lu pages shared\n",shared);
	printk(KERN_INFO "%lu pages swap cached\n",cached);
}

int after_bootmem;

static __init void *spp_getpage(void)
{ 
	void *ptr;
	if (after_bootmem)
		ptr = (void *) get_zeroed_page(GFP_ATOMIC); 
	else
		ptr = alloc_bootmem_pages(PAGE_SIZE);
	if (!ptr || ((unsigned long)ptr & ~PAGE_MASK))
		panic("set_pte_phys: cannot allocate page data %s\n", after_bootmem?"after bootmem":"");

	Dprintk("spp_getpage %p\n", ptr);
	return ptr;
} 

#define pgd_offset_u(address) (pgd_t *)(init_level4_user_pgt + pgd_index(address))

static inline pud_t *pud_offset_u(unsigned long address)
{
	pud_t *pud = level3_user_pgt;

	return pud + pud_index(address);
}

static __init void set_pte_phys(unsigned long vaddr,
			 unsigned long phys, pgprot_t prot, int user_mode)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte, new_pte;

	Dprintk("set_pte_phys %lx to %lx\n", vaddr, phys);

	pgd = (user_mode ? pgd_offset_u(vaddr) : pgd_offset_k(vaddr));
	if (pgd_none(*pgd)) {
		printk("PGD FIXMAP MISSING, it should be setup in head.S!\n");
		return;
	}
	pud = (user_mode ? pud_offset_u(vaddr) : pud_offset(pgd, vaddr));
	if (pud_none(*pud)) {
		pmd = (pmd_t *) spp_getpage(); 
		make_page_readonly(pmd, XENFEAT_writable_page_tables);
		set_pud(pud, __pud(__pa(pmd) | _KERNPG_TABLE | _PAGE_USER));
		if (pmd != pmd_offset(pud, 0)) {
			printk("PAGETABLE BUG #01! %p <-> %p\n", pmd, pmd_offset(pud,0));
			return;
		}
	}
	pmd = pmd_offset(pud, vaddr);
	if (pmd_none(*pmd)) {
		pte = (pte_t *) spp_getpage();
		make_page_readonly(pte, XENFEAT_writable_page_tables);
		set_pmd(pmd, __pmd(__pa(pte) | _KERNPG_TABLE | _PAGE_USER));
		if (pte != pte_offset_kernel(pmd, 0)) {
			printk("PAGETABLE BUG #02!\n");
			return;
		}
	}
	new_pte = pfn_pte(phys >> PAGE_SHIFT, prot);

	pte = pte_offset_kernel(pmd, vaddr);
	if (!pte_none(*pte) &&
	    pte_val(*pte) != (pte_val(new_pte) & __supported_pte_mask))
		pte_ERROR(*pte);
	set_pte(pte, new_pte);

	/*
	 * It's enough to flush this one mapping.
	 * (PGE mappings get flushed as well)
	 */
	__flush_tlb_one(vaddr);
}

static void set_pte_phys_ma(unsigned long vaddr,
			 unsigned long phys, pgprot_t prot)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte, new_pte;

	Dprintk("set_pte_phys %lx to %lx\n", vaddr, phys);

	pgd = pgd_offset_k(vaddr);
	if (pgd_none(*pgd)) {
		printk("PGD FIXMAP MISSING, it should be setup in head.S!\n");
		return;
	}
	pud = pud_offset(pgd, vaddr);
	if (pud_none(*pud)) {

		pmd = (pmd_t *) spp_getpage(); 
		make_page_readonly(pmd, XENFEAT_writable_page_tables);

		set_pud(pud, __pud(__pa(pmd) | _KERNPG_TABLE | _PAGE_USER));

		if (pmd != pmd_offset(pud, 0)) {
			printk("PAGETABLE BUG #01! %p <-> %p\n", pmd, pmd_offset(pud,0));
			return;
		}
	}
	pmd = pmd_offset(pud, vaddr);

	if (pmd_none(*pmd)) {
		pte = (pte_t *) spp_getpage();
		make_page_readonly(pte, XENFEAT_writable_page_tables);

		set_pmd(pmd, __pmd(__pa(pte) | _KERNPG_TABLE | _PAGE_USER));
		if (pte != pte_offset_kernel(pmd, 0)) {
			printk("PAGETABLE BUG #02!\n");
			return;
		}
	}

	new_pte = pfn_pte_ma(phys >> PAGE_SHIFT, prot);
	pte = pte_offset_kernel(pmd, vaddr);

	/* 
	 * Note that the pte page is already RO, thus we want to use
	 * xen_l1_entry_update(), not set_pte().
	 */
	xen_l1_entry_update(pte, 
			    pfn_pte_ma(phys >> PAGE_SHIFT, prot));

	/*
	 * It's enough to flush this one mapping.
	 * (PGE mappings get flushed as well)
	 */
	__flush_tlb_one(vaddr);
}

#define SET_FIXMAP_KERNEL 0
#define SET_FIXMAP_USER   1

/* NOTE: this is meant to be run only at boot */
void __init 
__set_fixmap (enum fixed_addresses idx, unsigned long phys, pgprot_t prot)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		printk("Invalid __set_fixmap\n");
		return;
	}
	switch (idx) {
	case VSYSCALL_FIRST_PAGE:
		set_pte_phys(address, phys, prot, SET_FIXMAP_KERNEL);
		break;
	default:
		set_pte_phys_ma(address, phys, prot);
		break;
	}
}

/*
 * At this point it only supports vsyscall area.
 */
void __set_fixmap_user (enum fixed_addresses idx, unsigned long phys, pgprot_t prot)
{
	unsigned long address = __fix_to_virt(idx);

	if (idx >= __end_of_fixed_addresses) {
		printk("Invalid __set_fixmap\n");
		return;
	}

	set_pte_phys(address, phys, prot, SET_FIXMAP_USER); 
}

unsigned long __initdata table_start, table_end; 

#ifndef CONFIG_XEN
extern pmd_t temp_boot_pmds[]; 

static  struct temp_map { 
	pmd_t *pmd;
	void  *address; 
	int    allocated; 
} temp_mappings[] __initdata = { 
	{ &temp_boot_pmds[0], (void *)(40UL * 1024 * 1024) },
	{ &temp_boot_pmds[1], (void *)(42UL * 1024 * 1024) }, 
	{}
}; 
#endif /* !CONFIG_XEN */

unsigned long get_machine_pfn(unsigned long addr)
{
	pud_t* pud = pud_offset_k(NULL, addr);
	pmd_t* pmd = pmd_offset(pud, addr);
	pte_t *pte = pte_offset_kernel(pmd, addr);

	return pte_mfn(*pte);
} 

static __meminit void *alloc_static_page(unsigned long *phys)
{
	unsigned long va = (start_pfn << PAGE_SHIFT) + __START_KERNEL_map;

	if (after_bootmem) {
		void *adr = (void *)get_zeroed_page(GFP_ATOMIC);
		*phys = __pa(adr);
		return adr;
	}

	*phys = start_pfn << PAGE_SHIFT;
	start_pfn++;
	memset((void *)va, 0, PAGE_SIZE);
	return (void *)va;
} 

#define PTE_SIZE PAGE_SIZE

static inline void __set_pte(pte_t *dst, pte_t val)
{
	*dst = val;
}

static inline int make_readonly(unsigned long paddr)
{
	int readonly = 0;

	/* Make new page tables read-only. */
	if (!xen_feature(XENFEAT_writable_page_tables)
	    && (paddr >= (table_start << PAGE_SHIFT))
	    && (paddr < (table_end << PAGE_SHIFT)))
		readonly = 1;
	/* Make old page tables read-only. */
	if (!xen_feature(XENFEAT_writable_page_tables)
	    && (paddr >= (xen_start_info->pt_base - __START_KERNEL_map))
	    && (paddr < (start_pfn << PAGE_SHIFT)))
		readonly = 1;

	/*
	 * No need for writable mapping of kernel image. This also ensures that
	 * page and descriptor tables embedded inside don't have writable
	 * mappings. 
	 */
	if ((paddr >= __pa_symbol(&_text)) && (paddr < __pa_symbol(&_end)))
		readonly = 1;

	return readonly;
}

#ifndef CONFIG_XEN
/* Must run before zap_low_mappings */
__init void *early_ioremap(unsigned long addr, unsigned long size)
{
	unsigned long map = round_down(addr, LARGE_PAGE_SIZE); 

	/* actually usually some more */
	if (size >= LARGE_PAGE_SIZE) { 
		return NULL;
	}
	set_pmd(temp_mappings[0].pmd,  __pmd(map | _KERNPG_TABLE | _PAGE_PSE));
	map += LARGE_PAGE_SIZE;
	set_pmd(temp_mappings[1].pmd,  __pmd(map | _KERNPG_TABLE | _PAGE_PSE));
	__flush_tlb();
	return temp_mappings[0].address + (addr & (LARGE_PAGE_SIZE-1));
}

/* To avoid virtual aliases later */
__init void early_iounmap(void *addr, unsigned long size)
{
	if ((void *)round_down((unsigned long)addr, LARGE_PAGE_SIZE) != temp_mappings[0].address)
		printk("early_iounmap: bad address %p\n", addr);
	set_pmd(temp_mappings[0].pmd, __pmd(0));
	set_pmd(temp_mappings[1].pmd, __pmd(0));
	__flush_tlb();
}
#endif /* !CONFIG_XEN */

static void __meminit
phys_pmd_init(pmd_t *pmd_page, unsigned long address, unsigned long end)
{
	int i = pmd_index(address);
	int k;

	for (; i < PTRS_PER_PMD; i++) {
		unsigned long pte_phys;
		pte_t *pte, *pte_save;
		pmd_t *pmd = pmd_page + pmd_index(address);

		if (address >= end) {
			if (!after_bootmem)
				for (; i < PTRS_PER_PMD; i++, pmd++)
					set_pmd(pmd, __pmd(0));
			break;
		}

		if (pmd_val(*pmd))
			continue;

		pte = alloc_static_page(&pte_phys);
		pte_save = pte;
		for (k = 0; k < PTRS_PER_PTE; pte++, k++, address += PTE_SIZE) {
			if ((address >= end) ||
			    ((address >> PAGE_SHIFT) >=
			     xen_start_info->nr_pages)) { 
				__set_pte(pte, __pte(0)); 
				continue;
			}
			if (make_readonly(address)) {
				__set_pte(pte, 
					  __pte(address | (_KERNPG_TABLE & ~_PAGE_RW)));
				continue;
			}
			__set_pte(pte, __pte(address | _KERNPG_TABLE));
		}
		pte = pte_save;
		early_make_page_readonly(pte, XENFEAT_writable_page_tables);
		set_pmd(pmd, __pmd(pte_phys | _KERNPG_TABLE));
	}
}

static void __meminit
phys_pmd_update(pud_t *pud, unsigned long address, unsigned long end)
{
	pmd_t *pmd = pmd_offset(pud,0);
	spin_lock(&init_mm.page_table_lock);
	phys_pmd_init(pmd, address, end);
	spin_unlock(&init_mm.page_table_lock);
	__flush_tlb_all();
}

static void __meminit phys_pud_init(pud_t *pud_page, unsigned long addr, unsigned long end)
{ 
	int i = pud_index(addr);


	for (; i < PTRS_PER_PUD; i++, addr = (addr & PUD_MASK) + PUD_SIZE ) {
		unsigned long pmd_phys;
		pud_t *pud = pud_page + pud_index(addr);
		pmd_t *pmd;

		if (addr >= end)
			break;

		if (pud_val(*pud)) {
			phys_pmd_update(pud, addr, end);
			continue;
		}

		pmd = alloc_static_page(&pmd_phys);
		early_make_page_readonly(pmd, XENFEAT_writable_page_tables);
		spin_lock(&init_mm.page_table_lock);
		set_pud(pud, __pud(pmd_phys | _KERNPG_TABLE));
		phys_pmd_init(pmd, addr, end);
		spin_unlock(&init_mm.page_table_lock);
	}
	__flush_tlb();
} 

void __init xen_init_pt(void)
{
	unsigned long addr, *page;

	memset((void *)init_level4_pgt,   0, PAGE_SIZE);
	memset((void *)level3_kernel_pgt, 0, PAGE_SIZE);
	memset((void *)level2_kernel_pgt, 0, PAGE_SIZE);

	/* Find the initial pte page that was built for us. */
	page = (unsigned long *)xen_start_info->pt_base;
	addr = page[pgd_index(__START_KERNEL_map)];
	addr_to_page(addr, page);
	addr = page[pud_index(__START_KERNEL_map)];
	addr_to_page(addr, page);

	/* Construct mapping of initial pte page in our own directories. */
	init_level4_pgt[pgd_index(__START_KERNEL_map)] = 
		mk_kernel_pgd(__pa_symbol(level3_kernel_pgt));
	level3_kernel_pgt[pud_index(__START_KERNEL_map)] = 
		__pud(__pa_symbol(level2_kernel_pgt) |
		      _KERNPG_TABLE);
	memcpy((void *)level2_kernel_pgt, page, PAGE_SIZE);

	early_make_page_readonly(init_level4_pgt,
				 XENFEAT_writable_page_tables);
	early_make_page_readonly(init_level4_user_pgt,
				 XENFEAT_writable_page_tables);
	early_make_page_readonly(level3_kernel_pgt,
				 XENFEAT_writable_page_tables);
	early_make_page_readonly(level3_user_pgt,
				 XENFEAT_writable_page_tables);
	early_make_page_readonly(level2_kernel_pgt,
				 XENFEAT_writable_page_tables);

	xen_pgd_pin(__pa_symbol(init_level4_pgt));
	xen_pgd_pin(__pa_symbol(init_level4_user_pgt));

	set_pgd((pgd_t *)(init_level4_user_pgt + 511), 
		mk_kernel_pgd(__pa_symbol(level3_user_pgt)));
}

void __init extend_init_mapping(unsigned long tables_space)
{
	unsigned long va = __START_KERNEL_map;
	unsigned long phys, addr, *pte_page;
	pmd_t *pmd;
	pte_t *pte, new_pte;
	unsigned long *page = (unsigned long *)init_level4_pgt;

	addr = page[pgd_index(va)];
	addr_to_page(addr, page);
	addr = page[pud_index(va)];
	addr_to_page(addr, page);

	/* Kill mapping of low 1MB. */
	while (va < (unsigned long)&_text) {
		HYPERVISOR_update_va_mapping(va, __pte_ma(0), 0);
		va += PAGE_SIZE;
	}

	/* Ensure init mappings cover kernel text/data and initial tables. */
	while (va < (__START_KERNEL_map
		     + (start_pfn << PAGE_SHIFT)
		     + tables_space)) {
		pmd = (pmd_t *)&page[pmd_index(va)];
		if (pmd_none(*pmd)) {
			pte_page = alloc_static_page(&phys);
			early_make_page_readonly(
				pte_page, XENFEAT_writable_page_tables);
			set_pmd(pmd, __pmd(phys | _KERNPG_TABLE));
		} else {
			addr = page[pmd_index(va)];
			addr_to_page(addr, pte_page);
		}
		pte = (pte_t *)&pte_page[pte_index(va)];
		if (pte_none(*pte)) {
			new_pte = pfn_pte(
				(va - __START_KERNEL_map) >> PAGE_SHIFT, 
				__pgprot(_KERNPG_TABLE));
			xen_l1_entry_update(pte, new_pte);
		}
		va += PAGE_SIZE;
	}

	/* Finally, blow away any spurious initial mappings. */
	while (1) {
		pmd = (pmd_t *)&page[pmd_index(va)];
		if (pmd_none(*pmd))
			break;
		HYPERVISOR_update_va_mapping(va, __pte_ma(0), 0);
		va += PAGE_SIZE;
	}
}

static void __init find_early_table_space(unsigned long end)
{
	unsigned long puds, pmds, ptes, tables; 

	puds = (end + PUD_SIZE - 1) >> PUD_SHIFT;
	pmds = (end + PMD_SIZE - 1) >> PMD_SHIFT;
	ptes = (end + PTE_SIZE - 1) >> PAGE_SHIFT;

	tables = round_up(puds * 8, PAGE_SIZE) + 
		round_up(pmds * 8, PAGE_SIZE) + 
		round_up(ptes * 8, PAGE_SIZE); 

	extend_init_mapping(tables);

	table_start = start_pfn;
	table_end = table_start + (tables>>PAGE_SHIFT);

	early_printk("kernel direct mapping tables up to %lx @ %lx-%lx\n",
		end, table_start << PAGE_SHIFT,
		     (table_end << PAGE_SHIFT) + tables);
}

/* Setup the direct mapping of the physical memory at PAGE_OFFSET.
   This runs before bootmem is initialized and gets pages directly from the 
   physical memory. To access them they are temporarily mapped. */
void __meminit init_memory_mapping(unsigned long start, unsigned long end)
{ 
	unsigned long next; 

	Dprintk("init_memory_mapping\n");

	/* 
	 * Find space for the kernel direct mapping tables.
	 * Later we should allocate these tables in the local node of the memory
	 * mapped.  Unfortunately this is done currently before the nodes are 
	 * discovered.
	 */
	if (!after_bootmem)
		find_early_table_space(end);

	start = (unsigned long)__va(start);
	end = (unsigned long)__va(end);

	for (; start < end; start = next) {
		unsigned long pud_phys; 
		pgd_t *pgd = pgd_offset_k(start);
		pud_t *pud;

		if (after_bootmem) {
			pud = pud_offset(pgd, start & PGDIR_MASK);
			make_page_readonly(pud, XENFEAT_writable_page_tables);
			pud_phys = __pa(pud);
		} else {
			pud = alloc_static_page(&pud_phys);
			early_make_page_readonly(pud, XENFEAT_writable_page_tables);
		}
		next = start + PGDIR_SIZE;
		if (next > end) 
			next = end; 
		phys_pud_init(pud, __pa(start), __pa(next));
		if (!after_bootmem)
			set_pgd(pgd_offset_k(start), mk_kernel_pgd(pud_phys));
	}

	if (!after_bootmem) {
		BUG_ON(start_pfn != table_end);

		/* Re-vector virtual addresses pointing into the initial
		   mapping to the just-established permanent ones. */
		xen_start_info = __va(__pa(xen_start_info));
		xen_start_info->pt_base = (unsigned long)
			__va(__pa(xen_start_info->pt_base));
		if (!xen_feature(XENFEAT_auto_translated_physmap)) {
			phys_to_machine_mapping =
				__va(__pa(xen_start_info->mfn_list));
			xen_start_info->mfn_list = (unsigned long)
				phys_to_machine_mapping;
		}
		if (xen_start_info->mod_start)
			xen_start_info->mod_start = (unsigned long)
				__va(__pa(xen_start_info->mod_start));

		/* Destroy the Xen-created mappings beyond the kernel image as
		 * well as the temporary mappings created above. Prevents
		 * overlap with modules area (if init mapping is very big).
		 */
		start = PAGE_ALIGN((unsigned long)_end);
		end   = __START_KERNEL_map + (table_end << PAGE_SHIFT);
		for (; start < end; start += PAGE_SIZE)
			WARN_ON(HYPERVISOR_update_va_mapping(
				start, __pte_ma(0), 0));
	}

	__flush_tlb_all();
}

void __cpuinit zap_low_mappings(int cpu)
{
	/* this is not required for Xen */
#if 0
	swap_low_mappings();
#endif
}

#ifndef CONFIG_NUMA
void __init paging_init(void)
{
	int i;
	unsigned long max_zone_pfns[MAX_NR_ZONES];
	memset(max_zone_pfns, 0, sizeof(max_zone_pfns));
	max_zone_pfns[ZONE_DMA] = end_pfn;
	max_zone_pfns[ZONE_DMA32] = end_pfn;
	max_zone_pfns[ZONE_NORMAL] = end_pfn;

	memory_present(0, 0, end_pfn);
	sparse_init();
	free_area_init_nodes(max_zone_pfns);

	/* Switch to the real shared_info page, and clear the
	 * dummy page. */
	set_fixmap(FIX_SHARED_INFO, xen_start_info->shared_info);
	HYPERVISOR_shared_info = (shared_info_t *)fix_to_virt(FIX_SHARED_INFO);
	memset(empty_zero_page, 0, sizeof(empty_zero_page));

	init_mm.context.pinned = 1;

	/* Setup mapping of lower 1st MB */
	for (i = 0; i < NR_FIX_ISAMAPS; i++)
		if (is_initial_xendomain())
			set_fixmap(FIX_ISAMAP_BEGIN - i, i * PAGE_SIZE);
		else
			__set_fixmap(FIX_ISAMAP_BEGIN - i,
				     virt_to_mfn(empty_zero_page) << PAGE_SHIFT,
				     PAGE_KERNEL_RO);
}
#endif

/* Unmap a kernel mapping if it exists. This is useful to avoid prefetches
   from the CPU leading to inconsistent cache lines. address and size
   must be aligned to 2MB boundaries. 
   Does nothing when the mapping doesn't exist. */
void __init clear_kernel_mapping(unsigned long address, unsigned long size) 
{
	unsigned long end = address + size;

	BUG_ON(address & ~LARGE_PAGE_MASK);
	BUG_ON(size & ~LARGE_PAGE_MASK); 
	
	for (; address < end; address += LARGE_PAGE_SIZE) { 
		pgd_t *pgd = pgd_offset_k(address);
		pud_t *pud;
		pmd_t *pmd;
		if (pgd_none(*pgd))
			continue;
		pud = pud_offset(pgd, address);
		if (pud_none(*pud))
			continue; 
		pmd = pmd_offset(pud, address);
		if (!pmd || pmd_none(*pmd))
			continue; 
		if (0 == (pmd_val(*pmd) & _PAGE_PSE)) { 
			/* Could handle this, but it should not happen currently. */
			printk(KERN_ERR 
	       "clear_kernel_mapping: mapping has been split. will leak memory\n"); 
			pmd_ERROR(*pmd); 
		}
		set_pmd(pmd, __pmd(0)); 		
	}
	__flush_tlb_all();
} 

/*
 * Memory hotplug specific functions
 */
void online_page(struct page *page)
{
	ClearPageReserved(page);
	init_page_count(page);
	__free_page(page);
	totalram_pages++;
	num_physpages++;
}

#ifdef CONFIG_MEMORY_HOTPLUG
/*
 * Memory is added always to NORMAL zone. This means you will never get
 * additional DMA/DMA32 memory.
 */
int arch_add_memory(int nid, u64 start, u64 size)
{
	struct pglist_data *pgdat = NODE_DATA(nid);
	struct zone *zone = pgdat->node_zones + ZONE_NORMAL;
	unsigned long start_pfn = start >> PAGE_SHIFT;
	unsigned long nr_pages = size >> PAGE_SHIFT;
	int ret;

	init_memory_mapping(start, (start + size -1));

	ret = __add_pages(zone, start_pfn, nr_pages);
	if (ret)
		goto error;

	return ret;
error:
	printk("%s: Problem encountered in __add_pages!\n", __func__);
	return ret;
}
EXPORT_SYMBOL_GPL(arch_add_memory);

int remove_memory(u64 start, u64 size)
{
	return -EINVAL;
}
EXPORT_SYMBOL_GPL(remove_memory);

#if !defined(CONFIG_ACPI_NUMA) && defined(CONFIG_NUMA)
int memory_add_physaddr_to_nid(u64 start)
{
	return 0;
}
EXPORT_SYMBOL_GPL(memory_add_physaddr_to_nid);
#endif

#endif /* CONFIG_MEMORY_HOTPLUG */

#ifdef CONFIG_MEMORY_HOTPLUG_RESERVE
/*
 * Memory Hotadd without sparsemem. The mem_maps have been allocated in advance,
 * just online the pages.
 */
int __add_pages(struct zone *z, unsigned long start_pfn, unsigned long nr_pages)
{
	int err = -EIO;
	unsigned long pfn;
	unsigned long total = 0, mem = 0;
	for (pfn = start_pfn; pfn < start_pfn + nr_pages; pfn++) {
		if (pfn_valid(pfn)) {
			online_page(pfn_to_page(pfn));
			err = 0;
			mem++;
		}
		total++;
	}
	if (!err) {
		z->spanned_pages += total;
		z->present_pages += mem;
		z->zone_pgdat->node_spanned_pages += total;
		z->zone_pgdat->node_present_pages += mem;
	}
	return err;
}
#endif

static inline int page_is_ram (unsigned long pagenr)
{
	return 1;
}
EXPORT_SYMBOL_GPL(page_is_ram);

/*
 * devmem_is_allowed() checks to see if /dev/mem access to a certain address is
 * valid. The argument is a physical page number.
 *
 *
 * On x86-64, access has to be given to the first megabyte of ram because that area
 * contains bios code and data regions used by X and dosemu and similar apps.
 * Access has to be given to non-kernel-ram areas as well, these contain the PCI
 * mmio resources as well as potential bios/acpi data regions.
 */
int devmem_is_allowed(unsigned long pagenr)
{
	if (pagenr <= 256)
		return 1;
	if (!page_is_ram(pagenr))
		return 1;
	return 0;
}


static struct kcore_list kcore_mem, kcore_vmalloc, kcore_kernel, kcore_modules,
			 kcore_vsyscall;

void __init mem_init(void)
{
	long codesize, reservedpages, datasize, initsize;
	unsigned long pfn;

	contiguous_bitmap = alloc_bootmem_low_pages(
		(end_pfn + 2*BITS_PER_LONG) >> 3);
	BUG_ON(!contiguous_bitmap);
	memset(contiguous_bitmap, 0, (end_pfn + 2*BITS_PER_LONG) >> 3);

	pci_iommu_alloc();

	/* clear the zero-page */
	memset(empty_zero_page, 0, PAGE_SIZE);

	reservedpages = 0;

	/* this will put all low memory onto the freelists */
#ifdef CONFIG_NUMA
	totalram_pages = numa_free_all_bootmem();
#else
	totalram_pages = free_all_bootmem();
#endif
	/* XEN: init and count pages outside initial allocation. */
	for (pfn = xen_start_info->nr_pages; pfn < max_pfn; pfn++) {
		ClearPageReserved(&mem_map[pfn]);
		init_page_count(&mem_map[pfn]);
		totalram_pages++;
	}
	reservedpages = end_pfn - totalram_pages -
					absent_pages_in_range(0, end_pfn);


	after_bootmem = 1;

	codesize =  (unsigned long) &_etext - (unsigned long) &_text;
	datasize =  (unsigned long) &_edata - (unsigned long) &_etext;
	initsize =  (unsigned long) &__init_end - (unsigned long) &__init_begin;

	/* Register memory areas for /proc/kcore */
	kclist_add(&kcore_mem, __va(0), max_low_pfn << PAGE_SHIFT); 
	kclist_add(&kcore_vmalloc, (void *)VMALLOC_START, 
		   VMALLOC_END-VMALLOC_START);
	kclist_add(&kcore_kernel, &_stext, _end - _stext);
	kclist_add(&kcore_modules, (void *)MODULES_VADDR, MODULES_LEN);
	kclist_add(&kcore_vsyscall, (void *)VSYSCALL_START, 
				 VSYSCALL_END - VSYSCALL_START);

	printk("Memory: %luk/%luk available (%ldk kernel code, %ldk reserved, %ldk data, %ldk init)\n",
		(unsigned long) nr_free_pages() << (PAGE_SHIFT-10),
		end_pfn << (PAGE_SHIFT-10),
		codesize >> 10,
		reservedpages << (PAGE_SHIFT-10),
		datasize >> 10,
		initsize >> 10);

#ifndef CONFIG_XEN
#ifdef CONFIG_SMP
	/*
	 * Sync boot_level4_pgt mappings with the init_level4_pgt
	 * except for the low identity mappings which are already zapped
	 * in init_level4_pgt. This sync-up is essential for AP's bringup
	 */
	memcpy(boot_level4_pgt+1, init_level4_pgt+1, (PTRS_PER_PGD-1)*sizeof(pgd_t));
#endif
#endif
}

void free_init_pages(char *what, unsigned long begin, unsigned long end)
{
	unsigned long addr;

	if (begin >= end)
		return;

	printk(KERN_INFO "Freeing %s: %ldk freed\n", what, (end - begin) >> 10);
	for (addr = begin; addr < end; addr += PAGE_SIZE) {
		ClearPageReserved(virt_to_page(addr));
		init_page_count(virt_to_page(addr));
		memset((void *)(addr & ~(PAGE_SIZE-1)),
			POISON_FREE_INITMEM, PAGE_SIZE);
		free_page(addr);
		totalram_pages++;
	}
}

void free_initmem(void)
{
	memset(__initdata_begin, POISON_FREE_INITDATA,
	       __initdata_end - __initdata_begin);
#ifdef __DO_LATER__
	free_init_pages("unused kernel memory",
			(unsigned long)(&__init_begin),
			(unsigned long)(&__init_end));
#endif
}

#ifdef CONFIG_DEBUG_RODATA

void mark_rodata_ro(void)
{
	unsigned long addr = (unsigned long)__start_rodata;

	for (; addr < (unsigned long)__end_rodata; addr += PAGE_SIZE)
		change_page_attr_addr(addr, 1, PAGE_KERNEL_RO);

	printk ("Write protecting the kernel read-only data: %luk\n",
			(__end_rodata - __start_rodata) >> 10);

	/*
	 * change_page_attr_addr() requires a global_flush_tlb() call after it.
	 * We do this after the printk so that if something went wrong in the
	 * change, the printk gets out at least to give a better debug hint
	 * of who is the culprit.
	 */
	global_flush_tlb();
}
#endif

#ifdef CONFIG_BLK_DEV_INITRD
void free_initrd_mem(unsigned long start, unsigned long end)
{
	free_init_pages("initrd memory", start, end);
}
#endif

void __init reserve_bootmem_generic(unsigned long phys, unsigned len) 
{ 
#ifdef CONFIG_NUMA
	int nid = phys_to_nid(phys);
#endif
	unsigned long pfn = phys >> PAGE_SHIFT;
	if (pfn >= end_pfn) {
		/* This can happen with kdump kernels when accessing firmware
		   tables. */
		if (pfn < end_pfn_map)
			return;
		printk(KERN_ERR "reserve_bootmem: illegal reserve %lx %u\n",
				phys, len);
		return;
	}

	/* Should check here against the e820 map to avoid double free */
#ifdef CONFIG_NUMA
  	reserve_bootmem_node(NODE_DATA(nid), phys, len);
#else       		
	reserve_bootmem(phys, len);    
#endif
	if (phys+len <= MAX_DMA_PFN*PAGE_SIZE) {
		dma_reserve += len / PAGE_SIZE;
		set_dma_reserve(dma_reserve);
	}
}

int kern_addr_valid(unsigned long addr) 
{ 
	unsigned long above = ((long)addr) >> __VIRTUAL_MASK_SHIFT;
       pgd_t *pgd;
       pud_t *pud;
       pmd_t *pmd;
       pte_t *pte;

	if (above != 0 && above != -1UL)
		return 0; 
	
	pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd))
		return 0;

	pud = pud_offset_k(pgd, addr);
	if (pud_none(*pud))
		return 0; 

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return 0;
	if (pmd_large(*pmd))
		return pfn_valid(pmd_pfn(*pmd));

	pte = pte_offset_kernel(pmd, addr);
	if (pte_none(*pte))
		return 0;
	return pfn_valid(pte_pfn(*pte));
}

#ifdef CONFIG_SYSCTL
#include <linux/sysctl.h>

extern int exception_trace, page_fault_trace;

static ctl_table debug_table2[] = {
	{ 99, "exception-trace", &exception_trace, sizeof(int), 0644, NULL,
	  proc_dointvec },
	{ 0, }
}; 

static ctl_table debug_root_table2[] = { 
	{ .ctl_name = CTL_DEBUG, .procname = "debug", .mode = 0555, 
	   .child = debug_table2 }, 
	{ 0 }, 
}; 

static __init int x8664_sysctl_init(void)
{ 
	register_sysctl_table(debug_root_table2, 1);
	return 0;
}
__initcall(x8664_sysctl_init);
#endif

/* A pseudo VMA to allow ptrace access for the vsyscall page.  This only
   covers the 64bit vsyscall page now. 32bit has a real VMA now and does
   not need special handling anymore. */

static struct vm_area_struct gate_vma = {
	.vm_start = VSYSCALL_START,
	.vm_end = VSYSCALL_START + (VSYSCALL_MAPPED_PAGES << PAGE_SHIFT),
	.vm_page_prot = PAGE_READONLY_EXEC,
	.vm_flags = VM_READ | VM_EXEC
};

struct vm_area_struct *get_gate_vma(struct task_struct *tsk)
{
#ifdef CONFIG_IA32_EMULATION
	if (test_tsk_thread_flag(tsk, TIF_IA32))
		return NULL;
#endif
	return &gate_vma;
}

int in_gate_area(struct task_struct *task, unsigned long addr)
{
	struct vm_area_struct *vma = get_gate_vma(task);
	if (!vma)
		return 0;
	return (addr >= vma->vm_start) && (addr < vma->vm_end);
}

/* Use this when you have no reliable task/vma, typically from interrupt
 * context.  It is less reliable than using the task's vma and may give
 * false positives.
 */
int in_gate_area_no_task(unsigned long addr)
{
	return (addr >= VSYSCALL_START) && (addr < VSYSCALL_END);
}