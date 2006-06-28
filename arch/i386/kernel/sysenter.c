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
#include <linux/mman.h>

#include <asm/cpufeature.h>
#include <asm/msr.h>
#include <asm/pgtable.h>
#include <asm/unistd.h>

extern asmlinkage void sysenter_entry(void);

void enable_sep_cpu(void *info)
{
	int cpu = get_cpu();
	struct tss_struct *tss = &per_cpu(init_tss, cpu);

	tss->ss1 = __KERNEL_CS;
	tss->esp1 = sizeof(struct tss_struct) + (unsigned long) tss;
	wrmsr(MSR_IA32_SYSENTER_CS, __KERNEL_CS, 0);
	wrmsr(MSR_IA32_SYSENTER_ESP, tss->esp1, 0);
	wrmsr(MSR_IA32_SYSENTER_EIP, (unsigned long) sysenter_entry, 0);
	put_cpu();	
}

/*
 * These symbols are defined by vsyscall.o to mark the bounds
 * of the ELF DSO images included therein.
 */
extern const char vsyscall_int80_start, vsyscall_int80_end;
extern const char vsyscall_sysenter_start, vsyscall_sysenter_end;

struct page *sysenter_page;

static int __init sysenter_setup(void)
{
	void *page = (void *)get_zeroed_page(GFP_ATOMIC);

	sysenter_page = virt_to_page(page);

	if (1 || (!boot_cpu_has(X86_FEATURE_SEP))) {
		memcpy(page,
		       &vsyscall_int80_start,
		       &vsyscall_int80_end - &vsyscall_int80_start);
		return 0;
	}

	memcpy(page,
	       &vsyscall_sysenter_start,
	       &vsyscall_sysenter_end - &vsyscall_sysenter_start);

	on_each_cpu(enable_sep_cpu, NULL, 1, 1);
	return 0;
}

__initcall(sysenter_setup);

extern void SYSENTER_RETURN_OFFSET;

unsigned int vdso_enabled = 1;

/*
 * This is called from binfmt_elf, we create the special vma for the
 * vDSO and insert it into the mm struct tree.
 */
int arch_setup_additional_pages(struct linux_binprm *bprm,
				int executable_stack)
{
	struct thread_info *ti = current_thread_info();
	unsigned long addr, len;
	int err;

	current->mm->context.vdso = NULL;
	if (unlikely(!vdso_enabled) || unlikely(!sysenter_page))
		return 0;

	/*
	 * Map the vDSO (it will be randomized):
	 */
	down_write(&current->mm->mmap_sem);
	len = PAGE_SIZE > ELF_EXEC_PAGESIZE ? PAGE_SIZE : ELF_EXEC_PAGESIZE;
	addr = get_unmapped_area_prot(NULL, 0, len, 0,
				      MAP_PRIVATE, PROT_READ | PROT_EXEC);
	if (unlikely(addr & ~PAGE_MASK)) {
		up_write(&current->mm->mmap_sem);
		return addr;
	}
	get_page(sysenter_page);
	err = install_special_mapping(current->mm, addr, len,
				      VM_DONTEXPAND | VM_READ | VM_EXEC |
				      VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC,
				      PAGE_READONLY_EXEC,
				      &sysenter_page, 1);
	if (likely(err == 0)) {
		current->mm->context.vdso = (void *)addr;
		ti->sysenter_return = &SYSENTER_RETURN_OFFSET + addr;
	}
	up_write(&current->mm->mmap_sem);
	return err;
}

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

static int __init vdso_setup(char *str)
{
        vdso_enabled = simple_strtoul(str, NULL, 0);
        return 1;
}
__setup("vdso=", vdso_setup);

