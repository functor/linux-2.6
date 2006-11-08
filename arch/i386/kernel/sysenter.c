/*
 * linux/arch/i386/kernel/sysenter.c
 *
 * (C) Copyright 2002 Linus Torvalds
 *
 * This file contains the needed initializations to support sysenter.
 */

#include <linux/init.h>
#include <linux/smp.h>
#include <linux/thread_info.h>
#include <linux/sched.h>
#include <linux/gfp.h>
#include <linux/string.h>
#include <linux/elf.h>
#include <linux/mm.h>
#include <linux/mman.h>

#include <asm/a.out.h>
#include <asm/cpufeature.h>
#include <asm/msr.h>
#include <asm/pgtable.h>
#include <asm/unistd.h>

#ifdef CONFIG_XEN
#include <xen/interface/callback.h>
#endif

extern asmlinkage void sysenter_entry(void);

void enable_sep_cpu(void)
{
#ifndef CONFIG_X86_NO_TSS
	int cpu = get_cpu();
	struct tss_struct *tss = &per_cpu(init_tss, cpu);

	if (!boot_cpu_has(X86_FEATURE_SEP)) {
		put_cpu();
		return;
	}

	tss->ss1 = __KERNEL_CS;
	tss->esp1 = sizeof(struct tss_struct) + (unsigned long) tss;
	wrmsr(MSR_IA32_SYSENTER_CS, __KERNEL_CS, 0);
	wrmsr(MSR_IA32_SYSENTER_ESP, tss->esp1, 0);
	wrmsr(MSR_IA32_SYSENTER_EIP, (unsigned long) sysenter_entry, 0);
	put_cpu();	
#endif
}

/*
 * These symbols are defined by vsyscall.o to mark the bounds
 * of the ELF DSO images included therein.
 */
extern const char vsyscall_int80_start, vsyscall_int80_end;
extern const char vsyscall_sysenter_start, vsyscall_sysenter_end;
static struct page *sysenter_pages[2];

int __init sysenter_setup(void)
{
	void *page = (void *)get_zeroed_page(GFP_ATOMIC);

	sysenter_pages[0] = virt_to_page(page);

#ifdef CONFIG_XEN
	if (boot_cpu_has(X86_FEATURE_SEP)) {
		struct callback_register sysenter = {
			.type = CALLBACKTYPE_sysenter,
			.address = { __KERNEL_CS, (unsigned long)sysenter_entry },
		};

		if (HYPERVISOR_callback_op(CALLBACKOP_register, &sysenter) < 0)
			clear_bit(X86_FEATURE_SEP, boot_cpu_data.x86_capability);
	}
#endif

	if (boot_cpu_has(X86_FEATURE_SEP)) {
		memcpy(page,
		       &vsyscall_sysenter_start,
		       &vsyscall_sysenter_end - &vsyscall_sysenter_start);
		return 0;
	}

	memcpy(page,
	       &vsyscall_int80_start,
	       &vsyscall_int80_end - &vsyscall_int80_start);

	return 0;
}

extern void SYSENTER_RETURN_OFFSET;

unsigned int vdso_enabled = 1;

/*
 * This is called from binfmt_elf, we create the special vma for the
 * vDSO and insert it into the mm struct tree.
 */
int arch_setup_additional_pages(struct linux_binprm *bprm,
	int executable_stack, unsigned long start_code,
	unsigned long interp_map_address)
{
	struct thread_info *ti = current_thread_info();
	unsigned long addr = 0, len;
	unsigned flags = MAP_PRIVATE;
	int err;

	current->mm->context.vdso = NULL;
	if (unlikely(!vdso_enabled) || unlikely(!sysenter_pages[0]))
		return 0;

	/*
	 * Map the vDSO (it will be randomized):
	 */
	down_write(&current->mm->mmap_sem);
	len = PAGE_SIZE > ELF_EXEC_PAGESIZE ? PAGE_SIZE : ELF_EXEC_PAGESIZE;
	if (0==exec_shield) { /* off; %cs limit off */
		addr = STACK_TOP;  /* minimal interference with anybody */
		flags = MAP_PRIVATE | MAP_FIXED;
	}
	else if ((3<<2) & exec_shield) { /* vdso just below .text */
		addr = (((2<<2) & exec_shield) && interp_map_address) ?
			interp_map_address : start_code;
		/* 1MB for vm86; 64K for vm86 himem */
		if ((0x110000 + len) <= addr) {
			addr = (PAGE_MASK & addr) - len;
		}
		else { /* start_code is too low */
			addr = 0;
		}
	}
	addr = get_unmapped_area_prot(NULL, addr, len, 0,
				      flags, PROT_READ | PROT_EXEC);
	if (unlikely(addr & ~PAGE_MASK)) {
		up_write(&current->mm->mmap_sem);
		return addr;
	}
	err = install_special_mapping(current->mm, addr, len,
				      VM_DONTEXPAND | VM_READ | VM_EXEC |
				      VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC,
				      PAGE_READONLY_EXEC,
				      sysenter_pages);
	if (likely(err == 0)) {
		current->mm->context.vdso = (void *)addr;
		ti->sysenter_return = &SYSENTER_RETURN_OFFSET + addr;
	}
	up_write(&current->mm->mmap_sem);
	return err;
}

#ifndef CONFIG_XEN
int in_gate_area_no_task(unsigned long addr)
{
	return 0;
}

int in_gate_area(struct task_struct *task, unsigned long addr)
{
	return 0;
}

struct vm_area_struct *get_gate_vma(struct task_struct *tsk)
{
	return NULL;
}
#endif
