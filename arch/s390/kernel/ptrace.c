/*
 *  arch/s390/kernel/ptrace.c
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com),
 *               Martin Schwidefsky (schwidefsky@de.ibm.com)
 *
 *  Based on PowerPC version 
 *    Copyright (C) 1995-1996 Gary Thomas (gdt@linuxppc.org)
 *
 *  Derived from "arch/m68k/kernel/ptrace.c"
 *  Copyright (C) 1994 by Hamish Macdonald
 *  Taken from linux/kernel/ptrace.c and modified for M680x0.
 *  linux/kernel/ptrace.c is by Ross Biro 1/23/92, edited by Linus Torvalds
 *
 * Modified by Cort Dougan (cort@cs.nmt.edu) 
 *
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License.  See the file README.legal in the main directory of
 * this archive for more details.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/tracehook.h>
#include <linux/module.h>
#include <linux/user.h>
#include <linux/security.h>
#include <linux/audit.h>
#include <linux/signal.h>

#include <asm/segment.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <asm/elf.h>

#ifdef CONFIG_COMPAT
#include "compat_ptrace.h"
#endif

static void
FixPerRegisters(struct task_struct *task)
{
	struct pt_regs *regs;
	per_struct *per_info;

	regs = task_pt_regs(task);
	per_info = (per_struct *) &task->thread.per_info;
	per_info->control_regs.bits.em_instruction_fetch =
		per_info->single_step | per_info->instruction_fetch;
	
	if (per_info->single_step) {
		per_info->control_regs.bits.starting_addr = 0;
#ifdef CONFIG_COMPAT
		if (test_thread_flag(TIF_31BIT))
			per_info->control_regs.bits.ending_addr = 0x7fffffffUL;
		else
#endif
			per_info->control_regs.bits.ending_addr = PSW_ADDR_INSN;
	} else {
		per_info->control_regs.bits.starting_addr =
			per_info->starting_addr;
		per_info->control_regs.bits.ending_addr =
			per_info->ending_addr;
	}
	/*
	 * if any of the control reg tracing bits are on 
	 * we switch on per in the psw
	 */
	if (per_info->control_regs.words.cr[0] & PER_EM_MASK)
		regs->psw.mask |= PSW_MASK_PER;
	else
		regs->psw.mask &= ~PSW_MASK_PER;

	if (per_info->control_regs.bits.em_storage_alteration)
		per_info->control_regs.bits.storage_alt_space_ctl = 1;
	else
		per_info->control_regs.bits.storage_alt_space_ctl = 0;

	if (task == current)
		/*
		 * These registers are loaded in __switch_to on
		 * context switch.  We must load them now if
		 * touching the current thread.
		 */
		__ctl_load(per_info->control_regs.words.cr, 9, 11);
}

void
tracehook_enable_single_step(struct task_struct *task)
{
	task->thread.per_info.single_step = 1;
	FixPerRegisters(task);
}

void
tracehook_disable_single_step(struct task_struct *task)
{
	task->thread.per_info.single_step = 0;
	FixPerRegisters(task);
	clear_tsk_thread_flag(task, TIF_SINGLE_STEP);
}

int
tracehook_single_step_enabled(struct task_struct *task)
{
	return task->thread.per_info.single_step;
}


static int
genregs_get(struct task_struct *target,
	    const struct utrace_regset *regset,
	    unsigned int pos, unsigned int count,
	    void *kbuf, void __user *ubuf)
{
	struct pt_regs *regs = task_pt_regs(target);
	unsigned long pswmask;
	int ret;

	/* Remove per bit from user psw. */
	pswmask = regs->psw.mask & ~PSW_MASK_PER;
	ret = utrace_regset_copyout(&pos, &count, &kbuf, &ubuf,
				    &pswmask, PT_PSWMASK, PT_PSWADDR);

	/* The rest of the PSW and the GPRs are directly on the stack. */
	if (ret == 0)
		ret = utrace_regset_copyout(&pos, &count, &kbuf, &ubuf,
					    &regs->psw.addr, PT_PSWADDR,
					    PT_ACR0);

	/* The ACRs are kept in the thread_struct.  */
	if (ret == 0 && count > 0 && pos < PT_ORIGGPR2) {
		if (target == current)
			save_access_regs(target->thread.acrs);

		ret = utrace_regset_copyout(&pos, &count, &kbuf, &ubuf,
					    target->thread.acrs,
					    PT_ACR0, PT_ORIGGPR2);
	}

	/* Finally, the ORIG_GPR2 value.  */
	if (ret == 0)
		ret = utrace_regset_copyout(&pos, &count, &kbuf, &ubuf,
					    &regs->orig_gpr2, PT_ORIGGPR2, -1);

	return ret;
}

static int
genregs_set(struct task_struct *target,
	    const struct utrace_regset *regset,
	    unsigned int pos, unsigned int count,
	    const void *kbuf, const void __user *ubuf)
{
	struct pt_regs *regs = task_pt_regs(target);
	int ret = 0;

	/* Check for an invalid PSW mask.  */
	if (count > 0 && pos == PT_PSWMASK) {
		unsigned long pswmask = regs->psw.mask;
		ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
					   &pswmask, PT_PSWMASK, PT_PSWADDR);
		if (pswmask != PSW_MASK_MERGE(PSW_USER_BITS, pswmask)
#ifdef CONFIG_COMPAT
		    && pswmask != PSW_MASK_MERGE(PSW_USER32_BITS, pswmask)
#endif
			)
			/* Invalid psw mask. */
			return -EINVAL;
		regs->psw.mask = pswmask;
		FixPerRegisters(target);
	}

	/* The rest of the PSW and the GPRs are directly on the stack. */
	if (ret == 0) {
		ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
					   &regs->psw.addr, PT_PSWADDR,
					   PT_ACR0);
#ifndef CONFIG_64BIT
		/* I'd like to reject addresses without the
		   high order bit but older gdb's rely on it */
		regs->psw.addr |= PSW_ADDR_AMODE;
#endif
	}

	/* The ACRs are kept in the thread_struct.  */
	if (ret == 0 && count > 0 && pos < PT_ORIGGPR2) {
		if (target == current
		    && (pos != PT_ACR0 || count < sizeof(target->thread.acrs)))
			save_access_regs(target->thread.acrs);

		ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
					   target->thread.acrs,
					   PT_ACR0, PT_ORIGGPR2);
		if (ret == 0 && target == current)
			restore_access_regs(target->thread.acrs);
	}

	/* Finally, the ORIG_GPR2 value.  */
	if (ret == 0)
		ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
					   &regs->orig_gpr2, PT_ORIGGPR2, -1);

	return ret;
}

static int
fpregs_get(struct task_struct *target,
	   const struct utrace_regset *regset,
	   unsigned int pos, unsigned int count,
	   void *kbuf, void __user *ubuf)
{
	if (target == current)
		save_fp_regs(&target->thread.fp_regs);

	return utrace_regset_copyout(&pos, &count, &kbuf, &ubuf,
				     &target->thread.fp_regs, 0, -1);
}

static int
fpregs_set(struct task_struct *target,
	    const struct utrace_regset *regset,
	    unsigned int pos, unsigned int count,
	    const void *kbuf, const void __user *ubuf)
{
	int ret = 0;

	if (target == current && (pos != 0 || count != sizeof(s390_fp_regs)))
		save_fp_regs(&target->thread.fp_regs);

	/* If setting FPC, must validate it first. */
	if (count > 0 && pos == 0) {
		unsigned long fpc;
		ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
					   &fpc, 0, sizeof(fpc));
		if (ret)
			return ret;

		if ((fpc & ~((unsigned long) FPC_VALID_MASK
			     << (BITS_PER_LONG - 32))) != 0)
			return -EINVAL;

		memcpy(&target->thread.fp_regs, &fpc, sizeof(fpc));
	}

	if (ret == 0)
		ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
					   &target->thread.fp_regs, 0, -1);

	if (ret == 0 && target == current)
		restore_fp_regs(&target->thread.fp_regs);

	return ret;
}

static int
per_info_get(struct task_struct *target,
	     const struct utrace_regset *regset,
	     unsigned int pos, unsigned int count,
	     void *kbuf, void __user *ubuf)
{
	return utrace_regset_copyout(&pos, &count, &kbuf, &ubuf,
				     &target->thread.per_info, 0, -1);
}

static int
per_info_set(struct task_struct *target,
	     const struct utrace_regset *regset,
	     unsigned int pos, unsigned int count,
	     const void *kbuf, const void __user *ubuf)
{
	int ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
				       &target->thread.per_info, 0, -1);

	FixPerRegisters(target);

	return ret;
}


/*
 * These are our native regset flavors.
 */
static const struct utrace_regset native_regsets[] = {
	{
		.size = sizeof(long), .align = sizeof(long),
		.n = sizeof(s390_regs) / sizeof(long),
		.get = genregs_get, .set = genregs_set
	},
	{
		.size = sizeof(long), .align = sizeof(long),
		.n = sizeof(s390_fp_regs) / sizeof(long),
		.get = fpregs_get, .set = fpregs_set
	},
	{
		.size = sizeof(long), .align = sizeof(long),
		.n = sizeof(per_struct) / sizeof(long),
		.get = per_info_get, .set = per_info_set
	},
};

const struct utrace_regset_view utrace_s390_native_view = {
	.name = UTS_MACHINE, .e_machine = ELF_ARCH,
	.regsets = native_regsets,
	.n = sizeof native_regsets / sizeof native_regsets[0],
};
EXPORT_SYMBOL_GPL(utrace_s390_native_view);


#ifdef CONFIG_COMPAT
static int
s390_genregs_get(struct task_struct *target,
		 const struct utrace_regset *regset,
		 unsigned int pos, unsigned int count,
		 void *kbuf, void __user *ubuf)
{
	struct pt_regs *regs = task_pt_regs(target);
	int ret = 0;

	/* Fake a 31 bit psw mask. */
	if (count > 0 && pos == PT_PSWMASK / 2) {
		u32 pswmask = PSW32_MASK_MERGE(PSW32_USER_BITS,
					       (u32) (regs->psw.mask >> 32));
		ret = utrace_regset_copyout(&pos, &count, &kbuf, &ubuf,
					    &pswmask, PT_PSWMASK / 2,
					    PT_PSWADDR / 2);
	}

	/* Fake a 31 bit psw address. */
	if (ret == 0 && count > 0 && pos == PT_PSWADDR / 2) {
		u32 pswaddr = (u32) regs->psw.addr | PSW32_ADDR_AMODE31;
		ret = utrace_regset_copyout(&pos, &count, &kbuf, &ubuf,
					    &pswaddr, PT_PSWADDR / 2,
					    PT_GPR0 / 2);
	}

	/* The GPRs are directly on the stack.  Just truncate them.  */
	while (ret == 0 && count > 0 && pos < PT_ACR0 / 2) {
		u32 value = regs->gprs[(pos - PT_GPR0 / 2) / sizeof(u32)];
		if (kbuf) {
			*(u32 *) kbuf = value;
			kbuf += sizeof(u32);
		}
		else if (put_user(value, (u32 __user *) ubuf))
			ret = -EFAULT;
		else
			ubuf += sizeof(u32);
		pos += sizeof(u32);
		count -= sizeof(u32);
	}

	/* The ACRs are kept in the thread_struct.  */
	if (ret == 0 && count > 0 && pos < PT_ACR0 / 2 + NUM_ACRS * ACR_SIZE) {
		if (target == current)
			save_access_regs(target->thread.acrs);

		ret = utrace_regset_copyout(&pos, &count, &kbuf, &ubuf,
					    target->thread.acrs,
					    PT_ACR0 / 2,
					    PT_ACR0 / 2 + NUM_ACRS * ACR_SIZE);
	}

	/* Finally, the ORIG_GPR2 value.  */
	if (count > 0) {
		if (kbuf)
			*(u32 *) kbuf = regs->orig_gpr2;
		else if (put_user((u32) regs->orig_gpr2,
				  (u32 __user *) ubuf))
			return -EFAULT;
	}

	return 0;
}

static int
s390_genregs_set(struct task_struct *target,
		 const struct utrace_regset *regset,
		 unsigned int pos, unsigned int count,
		 const void *kbuf, const void __user *ubuf)
{
	struct pt_regs *regs = task_pt_regs(target);
	int ret = 0;

	/* Check for an invalid PSW mask.  */
	if (count > 0 && pos == PT_PSWMASK / 2) {
		u32 pswmask;
		ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
					   &pswmask, PT_PSWMASK / 2,
					   PT_PSWADDR / 2);
		if (ret)
			return ret;

		if (pswmask != PSW_MASK_MERGE(PSW_USER32_BITS, pswmask))
			/* Invalid psw mask. */
			return -EINVAL;

		/* Build a 64 bit psw mask from 31 bit mask. */
		regs->psw.mask = PSW_MASK_MERGE(PSW_USER32_BITS,
						(u64) pswmask << 32);
		FixPerRegisters(target);
	}

	/* Build a 64 bit psw address from 31 bit address. */
	if (count > 0 && pos == PT_PSWADDR / 2) {
		u32 pswaddr;
		ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
					   &pswaddr, PT_PSWADDR / 2,
					   PT_GPR0 / 2);
		if (ret == 0)
			/* Build a 64 bit psw mask from 31 bit mask. */
			regs->psw.addr = pswaddr & PSW32_ADDR_INSN;
	}

	/* The GPRs are directly onto the stack. */
	while (ret == 0 && count > 0 && pos < PT_ACR0 / 2) {
		u32 value;

		if (kbuf) {
			value = *(const u32 *) kbuf;
			kbuf += sizeof(u32);
		}
		else if (get_user(value, (const u32 __user *) ubuf))
			return -EFAULT;
		else
			ubuf += sizeof(u32);
		pos += sizeof(u32);
		count -= sizeof(u32);

		regs->gprs[(pos - PT_GPR0 / 2) / sizeof(u32)] = value;
	}

	/* The ACRs are kept in the thread_struct.  */
	if (count > 0 && pos < PT_ORIGGPR2 / 2) {
		if (target == current
		    && (pos != PT_ACR0 / 2
			|| count < sizeof(target->thread.acrs)))
			save_access_regs(target->thread.acrs);

		ret = utrace_regset_copyin(&pos, &count, &kbuf, &ubuf,
					   target->thread.acrs,
					   PT_ACR0 / 2,
					   PT_ACR0 / 2 + NUM_ACRS * ACR_SIZE);

		if (ret == 0 && target == current)
			restore_access_regs(target->thread.acrs);
	}

	/* Finally, the ORIG_GPR2 value.  */
	if (ret == 0 && count > 0) {
		u32 value;
		if (kbuf)
			value = *(const u32 *) kbuf;
		else if (get_user(value, (const u32 __user *) ubuf))
			return -EFAULT;
		regs->orig_gpr2 = value;
	}

	return ret;
}


/*
 * This is magic. See per_struct and per_struct32.
 * By incident the offsets in per_struct are exactly
 * twice the offsets in per_struct32 for all fields.
 * The 8 byte fields need special handling though,
 * because the second half (bytes 4-7) is needed and
 * not the first half.
 */
static unsigned int
offset_from_per32(unsigned int offset)
{
	BUILD_BUG_ON(offsetof(per_struct32, control_regs) != 0);
	if (offset - offsetof(per_struct32, control_regs) < 3*sizeof(u32)
	    || (offset >= offsetof(per_struct32, starting_addr) &&
		offset <= offsetof(per_struct32, ending_addr))
	    || offset == offsetof(per_struct32, lowcore.words.address))
		offset = offset*2 + 4;
	else
		offset = offset*2;
	return offset;
}

static int
s390_per_info_get(struct task_struct *target,
		  const struct utrace_regset *regset,
		  unsigned int pos, unsigned int count,
		  void *kbuf, void __user *ubuf)
{
	while (count > 0) {
		u32 val = *(u32 *) ((char *) &target->thread.per_info
				    + offset_from_per32 (pos));
		if (kbuf) {
			*(u32 *) kbuf = val;
			kbuf += sizeof(u32);
		}
		else if (put_user(val, (u32 __user *) ubuf))
			return -EFAULT;
		else
			ubuf += sizeof(u32);
		pos += sizeof(u32);
		count -= sizeof(u32);
	}
	return 0;
}

static int
s390_per_info_set(struct task_struct *target,
		  const struct utrace_regset *regset,
		  unsigned int pos, unsigned int count,
		  const void *kbuf, const void __user *ubuf)
{
	while (count > 0) {
		u32 val;

		if (kbuf) {
			val = *(const u32 *) kbuf;
			kbuf += sizeof(u32);
		}
		else if (get_user(val, (const u32 __user *) ubuf))
			return -EFAULT;
		else
			ubuf += sizeof(u32);
		pos += sizeof(u32);
		count -= sizeof(u32);

		*(u32 *) ((char *) &target->thread.per_info
			  + offset_from_per32 (pos)) = val;
	}
	return 0;
}


static const struct utrace_regset s390_compat_regsets[] = {
	{
		.size = sizeof(u32), .align = sizeof(u32),
		.n = sizeof(s390_regs) / sizeof(long),
		.get = s390_genregs_get, .set = s390_genregs_set
	},
	{
		.size = sizeof(u32), .align = sizeof(u32),
		.n = sizeof(s390_fp_regs) / sizeof(u32),
		.get = fpregs_get, .set = fpregs_set
	},
	{
		.size = sizeof(u32), .align = sizeof(u32),
		.n = sizeof(per_struct) / sizeof(u32),
		.get = s390_per_info_get, .set = s390_per_info_set
	},
};

const struct utrace_regset_view utrace_s390_compat_view = {
	.name = "s390", .e_machine = EM_S390,
	.regsets = s390_compat_regsets,
	.n = sizeof s390_compat_regsets / sizeof s390_compat_regsets[0],
};
EXPORT_SYMBOL_GPL(utrace_s390_compat_view);
#endif	/* CONFIG_COMPAT */


#ifdef CONFIG_PTRACE
static const struct ptrace_layout_segment s390_uarea[] = {
	{PT_PSWMASK, PT_FPC, 0, 0},
	{PT_FPC, PT_CR_9, 1, 0},
	{PT_CR_9, PT_IEEE_IP, 2, 0},
	{PT_IEEE_IP, sizeof(struct user), -1, -1},
	{0, 0, -1, 0}
};

fastcall int arch_ptrace(long *request, struct task_struct *child,
			 struct utrace_attached_engine *engine,
			 unsigned long addr, unsigned long data, long *val)
{
	ptrace_area parea;
	unsigned long tmp;
	int copied;

	switch (*request) {
	case PTRACE_PEEKUSR:
		return ptrace_peekusr(child, engine, s390_uarea, addr, data);
	case PTRACE_POKEUSR:
		return ptrace_pokeusr(child, engine, s390_uarea, addr, data);

	case PTRACE_PEEKUSR_AREA:
	case PTRACE_POKEUSR_AREA:
		if (copy_from_user(&parea, (ptrace_area __user *) addr,
				   sizeof(parea)))
			return -EFAULT;
		if ((parea.kernel_addr | parea.len) & (sizeof(data) - 1))
			return -EIO;
		return ptrace_layout_access(child, engine,
					    utrace_native_view(current),
					    s390_uarea,
					    parea.kernel_addr, parea.len,
					    (void __user *) parea.process_addr,
					    NULL,
					    *request == PTRACE_POKEUSR_AREA);

	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		/* Remove high order bit from address (only for 31 bit). */
		addr &= PSW_ADDR_INSN;
		/* read word at location addr. */
		copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
		if (copied != sizeof(tmp))
			return -EIO;
		return put_user(tmp, (unsigned long __user *) data);

	case PTRACE_POKETEXT:
	case PTRACE_POKEDATA:
		/* Remove high order bit from address (only for 31 bit). */
		addr &= PSW_ADDR_INSN;
		/* write the word at location addr. */
		copied = access_process_vm(child, addr, &data, sizeof(data),1);
		if (copied != sizeof(data))
			return -EIO;
		return 0;
	}

	return -ENOSYS;
}

#ifdef CONFIG_COMPAT
static const struct ptrace_layout_segment s390_compat_uarea[] = {
	{PT_PSWMASK / 2, PT_FPC / 2, 0, 0},
	{PT_FPC / 2, PT_CR_9 / 2, 1, 0},
	{PT_CR_9 / 2, PT_IEEE_IP / 2, 2, 0},
	{PT_IEEE_IP / 2, sizeof(struct user32), -1, -1},
	{0, 0, -1, 0}
};

fastcall int arch_compat_ptrace(compat_long_t *request,
				struct task_struct *child,
				struct utrace_attached_engine *engine,
				compat_ulong_t addr, compat_ulong_t data,
				compat_long_t *val)
{
	ptrace_area_emu31 parea;

	switch (*request) {
	case PTRACE_PEEKUSR:
		return ptrace_compat_peekusr(child, engine, s390_compat_uarea,
					     addr, data);
	case PTRACE_POKEUSR:
		return ptrace_compat_pokeusr(child, engine, s390_compat_uarea,
					     addr, data);
	case PTRACE_PEEKUSR_AREA:
	case PTRACE_POKEUSR_AREA:
		if (copy_from_user(&parea, ((ptrace_area_emu31 __user *)
					    (unsigned long) addr),
				   sizeof(parea)))
			return -EFAULT;
		if ((parea.kernel_addr | parea.len) & (sizeof(data) - 1))
			return -EIO;
		return ptrace_layout_access(child, engine,
					    utrace_native_view(current),
					    s390_compat_uarea,
					    parea.kernel_addr, parea.len,
					    (void __user *)
					    (unsigned long) parea.process_addr,
					    NULL,
					    *request == PTRACE_POKEUSR_AREA);
	}

	return -ENOSYS;
}
#endif	/* CONFIG_COMPAT */
#endif	/* CONFIG_PTRACE */


#if 0				/* XXX */

#ifndef CONFIG_64BIT
# define __ADDR_MASK 3
#else
# define __ADDR_MASK 7
#endif

/*
 * Read the word at offset addr from the user area of a process. The
 * trouble here is that the information is littered over different
 * locations. The process registers are found on the kernel stack,
 * the floating point stuff and the trace settings are stored in
 * the task structure. In addition the different structures in
 * struct user contain pad bytes that should be read as zeroes.
 * Lovely...
 */
static int
peek_user(struct task_struct *child, addr_t addr, addr_t data)
{
	struct user *dummy = NULL;
	addr_t offset, tmp, mask;

	/*
	 * Stupid gdb peeks/pokes the access registers in 64 bit with
	 * an alignment of 4. Programmers from hell...
	 */
	mask = __ADDR_MASK;
#ifdef CONFIG_64BIT
	if (addr >= (addr_t) &dummy->regs.acrs &&
	    addr < (addr_t) &dummy->regs.orig_gpr2)
		mask = 3;
#endif
	if ((addr & mask) || addr > sizeof(struct user) - __ADDR_MASK)
		return -EIO;

	if (addr < (addr_t) &dummy->regs.acrs) {
		/*
		 * psw and gprs are stored on the stack
		 */
		tmp = *(addr_t *)((addr_t) &task_pt_regs(child)->psw + addr);
		if (addr == (addr_t) &dummy->regs.psw.mask)
			/* Remove per bit from user psw. */
			tmp &= ~PSW_MASK_PER;

	} else if (addr < (addr_t) &dummy->regs.orig_gpr2) {
		/*
		 * access registers are stored in the thread structure
		 */
		offset = addr - (addr_t) &dummy->regs.acrs;
#ifdef CONFIG_64BIT
		/*
		 * Very special case: old & broken 64 bit gdb reading
		 * from acrs[15]. Result is a 64 bit value. Read the
		 * 32 bit acrs[15] value and shift it by 32. Sick...
		 */
		if (addr == (addr_t) &dummy->regs.acrs[15])
			tmp = ((unsigned long) child->thread.acrs[15]) << 32;
		else
#endif
		tmp = *(addr_t *)((addr_t) &child->thread.acrs + offset);

	} else if (addr == (addr_t) &dummy->regs.orig_gpr2) {
		/*
		 * orig_gpr2 is stored on the kernel stack
		 */
		tmp = (addr_t) task_pt_regs(child)->orig_gpr2;

	} else if (addr < (addr_t) (&dummy->regs.fp_regs + 1)) {
		/* 
		 * floating point regs. are stored in the thread structure
		 */
		offset = addr - (addr_t) &dummy->regs.fp_regs;
		tmp = *(addr_t *)((addr_t) &child->thread.fp_regs + offset);
		if (addr == (addr_t) &dummy->regs.fp_regs.fpc)
			tmp &= (unsigned long) FPC_VALID_MASK
				<< (BITS_PER_LONG - 32);

	} else if (addr < (addr_t) (&dummy->regs.per_info + 1)) {
		/*
		 * per_info is found in the thread structure
		 */
		offset = addr - (addr_t) &dummy->regs.per_info;
		tmp = *(addr_t *)((addr_t) &child->thread.per_info + offset);

	} else
		tmp = 0;

	return put_user(tmp, (addr_t __user *) data);
}

/*
 * Write a word to the user area of a process at location addr. This
 * operation does have an additional problem compared to peek_user.
 * Stores to the program status word and on the floating point
 * control register needs to get checked for validity.
 */
static int
poke_user(struct task_struct *child, addr_t addr, addr_t data)
{
	struct user *dummy = NULL;
	addr_t offset, mask;

	/*
	 * Stupid gdb peeks/pokes the access registers in 64 bit with
	 * an alignment of 4. Programmers from hell indeed...
	 */
	mask = __ADDR_MASK;
#ifdef CONFIG_64BIT
	if (addr >= (addr_t) &dummy->regs.acrs &&
	    addr < (addr_t) &dummy->regs.orig_gpr2)
		mask = 3;
#endif
	if ((addr & mask) || addr > sizeof(struct user) - __ADDR_MASK)
		return -EIO;

	if (addr < (addr_t) &dummy->regs.acrs) {
		/*
		 * psw and gprs are stored on the stack
		 */
		if (addr == (addr_t) &dummy->regs.psw.mask &&
#ifdef CONFIG_COMPAT
		    data != PSW_MASK_MERGE(PSW_USER32_BITS, data) &&
#endif
		    data != PSW_MASK_MERGE(PSW_USER_BITS, data))
			/* Invalid psw mask. */
			return -EINVAL;
#ifndef CONFIG_64BIT
		if (addr == (addr_t) &dummy->regs.psw.addr)
			/* I'd like to reject addresses without the
			   high order bit but older gdb's rely on it */
			data |= PSW_ADDR_AMODE;
#endif
		*(addr_t *)((addr_t) &task_pt_regs(child)->psw + addr) = data;

	} else if (addr < (addr_t) (&dummy->regs.orig_gpr2)) {
		/*
		 * access registers are stored in the thread structure
		 */
		offset = addr - (addr_t) &dummy->regs.acrs;
#ifdef CONFIG_64BIT
		/*
		 * Very special case: old & broken 64 bit gdb writing
		 * to acrs[15] with a 64 bit value. Ignore the lower
		 * half of the value and write the upper 32 bit to
		 * acrs[15]. Sick...
		 */
		if (addr == (addr_t) &dummy->regs.acrs[15])
			child->thread.acrs[15] = (unsigned int) (data >> 32);
		else
#endif
		*(addr_t *)((addr_t) &child->thread.acrs + offset) = data;

	} else if (addr == (addr_t) &dummy->regs.orig_gpr2) {
		/*
		 * orig_gpr2 is stored on the kernel stack
		 */
		task_pt_regs(child)->orig_gpr2 = data;

	} else if (addr < (addr_t) (&dummy->regs.fp_regs + 1)) {
		/*
		 * floating point regs. are stored in the thread structure
		 */
		if (addr == (addr_t) &dummy->regs.fp_regs.fpc &&
		    (data & ~((unsigned long) FPC_VALID_MASK
			      << (BITS_PER_LONG - 32))) != 0)
			return -EINVAL;
		offset = addr - (addr_t) &dummy->regs.fp_regs;
		*(addr_t *)((addr_t) &child->thread.fp_regs + offset) = data;

	} else if (addr < (addr_t) (&dummy->regs.per_info + 1)) {
		/*
		 * per_info is found in the thread structure 
		 */
		offset = addr - (addr_t) &dummy->regs.per_info;
		*(addr_t *)((addr_t) &child->thread.per_info + offset) = data;

	}

	FixPerRegisters(child);
	return 0;
}

static int
do_ptrace_normal(struct task_struct *child, long request, long addr, long data)
{
	unsigned long tmp;
	ptrace_area parea; 
	int copied, ret;

	switch (request) {
	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		/* Remove high order bit from address (only for 31 bit). */
		addr &= PSW_ADDR_INSN;
		/* read word at location addr. */
		copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
		if (copied != sizeof(tmp))
			return -EIO;
		return put_user(tmp, (unsigned long __user *) data);

	case PTRACE_PEEKUSR:
		/* read the word at location addr in the USER area. */
		return peek_user(child, addr, data);

	case PTRACE_POKETEXT:
	case PTRACE_POKEDATA:
		/* Remove high order bit from address (only for 31 bit). */
		addr &= PSW_ADDR_INSN;
		/* write the word at location addr. */
		copied = access_process_vm(child, addr, &data, sizeof(data),1);
		if (copied != sizeof(data))
			return -EIO;
		return 0;

	case PTRACE_POKEUSR:
		/* write the word at location addr in the USER area */
		return poke_user(child, addr, data);

	case PTRACE_PEEKUSR_AREA:
	case PTRACE_POKEUSR_AREA:
		if (copy_from_user(&parea, (void __user *) addr,
							sizeof(parea)))
			return -EFAULT;
		addr = parea.kernel_addr;
		data = parea.process_addr;
		copied = 0;
		while (copied < parea.len) {
			if (request == PTRACE_PEEKUSR_AREA)
				ret = peek_user(child, addr, data);
			else {
				addr_t tmp;
				if (get_user (tmp, (addr_t __user *) data))
					return -EFAULT;
				ret = poke_user(child, addr, tmp);
			}
			if (ret)
				return ret;
			addr += sizeof(unsigned long);
			data += sizeof(unsigned long);
			copied += sizeof(unsigned long);
		}
		return 0;
	}
	return ptrace_request(child, request, addr, data);
}

#ifdef CONFIG_COMPAT
/*
 * Now the fun part starts... a 31 bit program running in the
 * 31 bit emulation tracing another program. PTRACE_PEEKTEXT,
 * PTRACE_PEEKDATA, PTRACE_POKETEXT and PTRACE_POKEDATA are easy
 * to handle, the difference to the 64 bit versions of the requests
 * is that the access is done in multiples of 4 byte instead of
 * 8 bytes (sizeof(unsigned long) on 31/64 bit).
 * The ugly part are PTRACE_PEEKUSR, PTRACE_PEEKUSR_AREA,
 * PTRACE_POKEUSR and PTRACE_POKEUSR_AREA. If the traced program
 * is a 31 bit program too, the content of struct user can be
 * emulated. A 31 bit program peeking into the struct user of
 * a 64 bit program is a no-no.
 */

/*
 * Same as peek_user but for a 31 bit program.
 */
static int
peek_user_emu31(struct task_struct *child, addr_t addr, addr_t data)
{
	struct user32 *dummy32 = NULL;
	per_struct32 *dummy_per32 = NULL;
	addr_t offset;
	__u32 tmp;

	if (!test_thread_flag(TIF_31BIT) ||
	    (addr & 3) || addr > sizeof(struct user) - 3)
		return -EIO;

	if (addr < (addr_t) &dummy32->regs.acrs) {
		/*
		 * psw and gprs are stored on the stack
		 */
		if (addr == (addr_t) &dummy32->regs.psw.mask) {
			/* Fake a 31 bit psw mask. */
			tmp = (__u32)(task_pt_regs(child)->psw.mask >> 32);
			tmp = PSW32_MASK_MERGE(PSW32_USER_BITS, tmp);
		} else if (addr == (addr_t) &dummy32->regs.psw.addr) {
			/* Fake a 31 bit psw address. */
			tmp = (__u32) task_pt_regs(child)->psw.addr |
				PSW32_ADDR_AMODE31;
		} else {
			/* gpr 0-15 */
			tmp = *(__u32 *)((addr_t) &task_pt_regs(child)->psw +
					 addr*2 + 4);
		}
	} else if (addr < (addr_t) (&dummy32->regs.orig_gpr2)) {
		/*
		 * access registers are stored in the thread structure
		 */
		offset = addr - (addr_t) &dummy32->regs.acrs;
		tmp = *(__u32*)((addr_t) &child->thread.acrs + offset);

	} else if (addr == (addr_t) (&dummy32->regs.orig_gpr2)) {
		/*
		 * orig_gpr2 is stored on the kernel stack
		 */
		tmp = *(__u32*)((addr_t) &task_pt_regs(child)->orig_gpr2 + 4);

	} else if (addr < (addr_t) (&dummy32->regs.fp_regs + 1)) {
		/*
		 * floating point regs. are stored in the thread structure 
		 */
	        offset = addr - (addr_t) &dummy32->regs.fp_regs;
		tmp = *(__u32 *)((addr_t) &child->thread.fp_regs + offset);

	} else if (addr < (addr_t) (&dummy32->regs.per_info + 1)) {
		/*
		 * per_info is found in the thread structure
		 */
		offset = addr - (addr_t) &dummy32->regs.per_info;
		/* This is magic. See per_struct and per_struct32. */
		if ((offset >= (addr_t) &dummy_per32->control_regs &&
		     offset < (addr_t) (&dummy_per32->control_regs + 1)) ||
		    (offset >= (addr_t) &dummy_per32->starting_addr &&
		     offset <= (addr_t) &dummy_per32->ending_addr) ||
		    offset == (addr_t) &dummy_per32->lowcore.words.address)
			offset = offset*2 + 4;
		else
			offset = offset*2;
		tmp = *(__u32 *)((addr_t) &child->thread.per_info + offset);

	} else
		tmp = 0;

	return put_user(tmp, (__u32 __user *) data);
}

/*
 * Same as poke_user but for a 31 bit program.
 */
static int
poke_user_emu31(struct task_struct *child, addr_t addr, addr_t data)
{
	struct user32 *dummy32 = NULL;
	per_struct32 *dummy_per32 = NULL;
	addr_t offset;
	__u32 tmp;

	if (!test_thread_flag(TIF_31BIT) ||
	    (addr & 3) || addr > sizeof(struct user32) - 3)
		return -EIO;

	tmp = (__u32) data;

	if (addr < (addr_t) &dummy32->regs.acrs) {
		/*
		 * psw, gprs, acrs and orig_gpr2 are stored on the stack
		 */
		if (addr == (addr_t) &dummy32->regs.psw.mask) {
			/* Build a 64 bit psw mask from 31 bit mask. */
			if (tmp != PSW32_MASK_MERGE(PSW32_USER_BITS, tmp))
				/* Invalid psw mask. */
				return -EINVAL;
			task_pt_regs(child)->psw.mask =
				PSW_MASK_MERGE(PSW_USER32_BITS, (__u64) tmp << 32);
		} else if (addr == (addr_t) &dummy32->regs.psw.addr) {
			/* Build a 64 bit psw address from 31 bit address. */
			task_pt_regs(child)->psw.addr =
				(__u64) tmp & PSW32_ADDR_INSN;
		} else {
			/* gpr 0-15 */
			*(__u32*)((addr_t) &task_pt_regs(child)->psw
				  + addr*2 + 4) = tmp;
		}
	} else if (addr < (addr_t) (&dummy32->regs.orig_gpr2)) {
		/*
		 * access registers are stored in the thread structure
		 */
		offset = addr - (addr_t) &dummy32->regs.acrs;
		*(__u32*)((addr_t) &child->thread.acrs + offset) = tmp;

	} else if (addr == (addr_t) (&dummy32->regs.orig_gpr2)) {
		/*
		 * orig_gpr2 is stored on the kernel stack
		 */
		*(__u32*)((addr_t) &task_pt_regs(child)->orig_gpr2 + 4) = tmp;

	} else if (addr < (addr_t) (&dummy32->regs.fp_regs + 1)) {
		/*
		 * floating point regs. are stored in the thread structure 
		 */
		if (addr == (addr_t) &dummy32->regs.fp_regs.fpc &&
		    (tmp & ~FPC_VALID_MASK) != 0)
			/* Invalid floating point control. */
			return -EINVAL;
	        offset = addr - (addr_t) &dummy32->regs.fp_regs;
		*(__u32 *)((addr_t) &child->thread.fp_regs + offset) = tmp;

	} else if (addr < (addr_t) (&dummy32->regs.per_info + 1)) {
		/*
		 * per_info is found in the thread structure.
		 */
		offset = addr - (addr_t) &dummy32->regs.per_info;
		/*
		 * This is magic. See per_struct and per_struct32.
		 * By incident the offsets in per_struct are exactly
		 * twice the offsets in per_struct32 for all fields.
		 * The 8 byte fields need special handling though,
		 * because the second half (bytes 4-7) is needed and
		 * not the first half.
		 */
		if ((offset >= (addr_t) &dummy_per32->control_regs &&
		     offset < (addr_t) (&dummy_per32->control_regs + 1)) ||
		    (offset >= (addr_t) &dummy_per32->starting_addr &&
		     offset <= (addr_t) &dummy_per32->ending_addr) ||
		    offset == (addr_t) &dummy_per32->lowcore.words.address)
			offset = offset*2 + 4;
		else
			offset = offset*2;
		*(__u32 *)((addr_t) &child->thread.per_info + offset) = tmp;

	}

	FixPerRegisters(child);
	return 0;
}

static int
do_ptrace_emu31(struct task_struct *child, long request, long addr, long data)
{
	unsigned int tmp;  /* 4 bytes !! */
	ptrace_area_emu31 parea; 
	int copied, ret;

	switch (request) {
	case PTRACE_PEEKTEXT:
	case PTRACE_PEEKDATA:
		/* read word at location addr. */
		copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 0);
		if (copied != sizeof(tmp))
			return -EIO;
		return put_user(tmp, (unsigned int __user *) data);

	case PTRACE_PEEKUSR:
		/* read the word at location addr in the USER area. */
		return peek_user_emu31(child, addr, data);

	case PTRACE_POKETEXT:
	case PTRACE_POKEDATA:
		/* write the word at location addr. */
		tmp = data;
		copied = access_process_vm(child, addr, &tmp, sizeof(tmp), 1);
		if (copied != sizeof(tmp))
			return -EIO;
		return 0;

	case PTRACE_POKEUSR:
		/* write the word at location addr in the USER area */
		return poke_user_emu31(child, addr, data);

	case PTRACE_PEEKUSR_AREA:
	case PTRACE_POKEUSR_AREA:
		if (copy_from_user(&parea, (void __user *) addr,
							sizeof(parea)))
			return -EFAULT;
		addr = parea.kernel_addr;
		data = parea.process_addr;
		copied = 0;
		while (copied < parea.len) {
			if (request == PTRACE_PEEKUSR_AREA)
				ret = peek_user_emu31(child, addr, data);
			else {
				__u32 tmp;
				if (get_user (tmp, (__u32 __user *) data))
					return -EFAULT;
				ret = poke_user_emu31(child, addr, tmp);
			}
			if (ret)
				return ret;
			addr += sizeof(unsigned int);
			data += sizeof(unsigned int);
			copied += sizeof(unsigned int);
		}
		return 0;
#if 0				/* XXX */
	case PTRACE_GETEVENTMSG:
		return put_user((__u32) child->ptrace_message,
				(unsigned int __user *) data);
	case PTRACE_GETSIGINFO:
		if (child->last_siginfo == NULL)
			return -EINVAL;
		return copy_siginfo_to_user32((compat_siginfo_t __user *) data,
					      child->last_siginfo);
	case PTRACE_SETSIGINFO:
		if (child->last_siginfo == NULL)
			return -EINVAL;
		return copy_siginfo_from_user32(child->last_siginfo,
						(compat_siginfo_t __user *) data);
#endif
	}
	return ptrace_request(child, request, addr, data);
}
#endif

#define PT32_IEEE_IP 0x13c

static int
do_ptrace(struct task_struct *child, long request, long addr, long data)
{
	int ret;

	if (request == PTRACE_ATTACH)
		return ptrace_attach(child);

	/*
	 * Special cases to get/store the ieee instructions pointer.
	 */
	if (child == current) {
		if (request == PTRACE_PEEKUSR && addr == PT_IEEE_IP)
			return peek_user(child, addr, data);
		if (request == PTRACE_POKEUSR && addr == PT_IEEE_IP)
			return poke_user(child, addr, data);
#ifdef CONFIG_COMPAT
		if (request == PTRACE_PEEKUSR &&
		    addr == PT32_IEEE_IP && test_thread_flag(TIF_31BIT))
			return peek_user_emu31(child, addr, data);
		if (request == PTRACE_POKEUSR &&
		    addr == PT32_IEEE_IP && test_thread_flag(TIF_31BIT))
			return poke_user_emu31(child, addr, data);
#endif
	}

	ret = ptrace_check_attach(child, request == PTRACE_KILL);
	if (ret < 0)
		return ret;

	switch (request) {
	case PTRACE_SYSCALL:
		/* continue and stop at next (return from) syscall */
	case PTRACE_CONT:
		/* restart after signal. */
		if (!valid_signal(data))
			return -EIO;
		if (request == PTRACE_SYSCALL)
			set_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		else
			clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		child->exit_code = data;
		/* make sure the single step bit is not set. */
		tracehook_disable_single_step(child);
		wake_up_process(child);
		return 0;

	case PTRACE_KILL:
		/*
		 * make the child exit.  Best I can do is send it a sigkill. 
		 * perhaps it should be put in the status that it wants to 
		 * exit.
		 */
		if (child->exit_state == EXIT_ZOMBIE) /* already dead */
			return 0;
		child->exit_code = SIGKILL;
		/* make sure the single step bit is not set. */
		tracehook_disable_single_step(child);
		wake_up_process(child);
		return 0;

	case PTRACE_SINGLESTEP:
		/* set the trap flag. */
		if (!valid_signal(data))
			return -EIO;
		clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
		child->exit_code = data;
		if (data)
			set_tsk_thread_flag(child, TIF_SINGLE_STEP);
		else
			tracehook_enable_single_step(child);
		/* give it a chance to run. */
		wake_up_process(child);
		return 0;

	case PTRACE_DETACH:
		/* detach a process that was attached. */
		return ptrace_detach(child, data);


	/* Do requests that differ for 31/64 bit */
	default:
#ifdef CONFIG_COMPAT
		if (test_thread_flag(TIF_31BIT))
			return do_ptrace_emu31(child, request, addr, data);
#endif
		return do_ptrace_normal(child, request, addr, data);
	}
	/* Not reached.  */
	return -EIO;
}
#endif



asmlinkage void
syscall_trace(struct pt_regs *regs, int entryexit)
{
	if (unlikely(current->audit_context) && entryexit)
		audit_syscall_exit(AUDITSC_RESULT(regs->gprs[2]), regs->gprs[2]);

	if (test_thread_flag(TIF_SYSCALL_TRACE)) {
		tracehook_report_syscall(regs, entryexit);

		/*
		 * If the debugger has set an invalid system call number,
		 * we prepare to skip the system call restart handling.
		 */
		if (!entryexit && regs->gprs[2] >= NR_syscalls)
			regs->trap = -1;
	}

	if (unlikely(current->audit_context) && !entryexit)
		audit_syscall_entry(test_thread_flag(TIF_31BIT)?AUDIT_ARCH_S390:AUDIT_ARCH_S390X,
				    regs->gprs[2], regs->orig_gpr2, regs->gprs[3],
				    regs->gprs[4], regs->gprs[5]);
}
