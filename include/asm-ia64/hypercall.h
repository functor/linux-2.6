/******************************************************************************
 * hypercall.h
 * 
 * Linux-specific hypervisor handling.
 * 
 * Copyright (c) 2002-2004, K A Fraser
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __HYPERCALL_H__
#define __HYPERCALL_H__

#include <linux/string.h> /* memcpy() */

#ifndef __HYPERVISOR_H__
# error "please don't include this file directly"
#endif

/*
 * Assembler stubs for hyper-calls.
 */

#define _hypercall0(type, name)					\
({								\
	long __res;						\
	__asm__ __volatile__ (";;\n"				\
			      "mov r2=%1\n"			\
			      "break 0x1000 ;;\n"		\
			      "mov %0=r8 ;;\n"			\
			      : "=r" (__res)			\
			      : "J" (__HYPERVISOR_##name)	\
			      : "r2","r8",			\
			        "memory" );			\
	(type)__res;						\
})

#define _hypercall1(type, name, a1)				\
({								\
	long __res;						\
	__asm__ __volatile__ (";;\n"				\
			      "mov r14=%2\n"			\
			      "mov r2=%1\n"			\
			      "break 0x1000 ;;\n"		\
			      "mov %0=r8 ;;\n"			\
			      : "=r" (__res)			\
			      : "J" (__HYPERVISOR_##name),	\
				"rI" ((unsigned long)(a1))	\
			      : "r14","r2","r8",		\
				"memory" );			\
	(type)__res;						\
})

#define _hypercall2(type, name, a1, a2)				\
({								\
	long __res;						\
	__asm__ __volatile__ (";;\n"				\
			      "mov r14=%2\n"			\
			      "mov r15=%3\n"			\
			      "mov r2=%1\n"			\
			      "break 0x1000 ;;\n"		\
			      "mov %0=r8 ;;\n"			\
			      : "=r" (__res)			\
			      : "J" (__HYPERVISOR_##name),	\
				"rI" ((unsigned long)(a1)),	\
				"rI" ((unsigned long)(a2))	\
			      : "r14","r15","r2","r8",		\
				"memory" );			\
	(type)__res;						\
})

#define _hypercall3(type, name, a1, a2, a3)			\
({								\
	long __res;						\
	__asm__ __volatile__ (";;\n"                            \
			      "mov r14=%2\n"                    \
			      "mov r15=%3\n"                    \
			      "mov r16=%4\n"                    \
			      "mov r2=%1\n"                     \
			      "break 0x1000 ;;\n"               \
			      "mov %0=r8 ;;\n"                  \
			      : "=r" (__res)                    \
			      : "J" (__HYPERVISOR_##name),      \
				"rI" ((unsigned long)(a1)),     \
				"rI" ((unsigned long)(a2)),     \
				"rI" ((unsigned long)(a3))      \
			      : "r14","r15","r16","r2","r8",	\
			        "memory" );                     \
	(type)__res;                                            \
})

#define _hypercall4(type, name, a1, a2, a3, a4)			\
({								\
	long __res;						\
	__asm__ __volatile__ (";;\n"                            \
			      "mov r14=%2\n"                    \
			      "mov r15=%3\n"                    \
			      "mov r16=%4\n"                    \
			      "mov r17=%5\n"                    \
			      "mov r2=%1\n"                     \
			      "break 0x1000 ;;\n"               \
			      "mov %0=r8 ;;\n"                  \
			      : "=r" (__res)                    \
			      : "J" (__HYPERVISOR_##name),      \
				"rI" ((unsigned long)(a1)),     \
				"rI" ((unsigned long)(a2)),     \
				"rI" ((unsigned long)(a3)),     \
				"rI" ((unsigned long)(a4))      \
			      : "r14","r15","r16","r2","r8",	\
			        "r17","memory" );               \
	(type)__res;                                            \
})

#define _hypercall5(type, name, a1, a2, a3, a4, a5)		\
({								\
	long __res;						\
	__asm__ __volatile__ (";;\n"                            \
			      "mov r14=%2\n"                    \
			      "mov r15=%3\n"                    \
			      "mov r16=%4\n"                    \
			      "mov r17=%5\n"                    \
			      "mov r18=%6\n"                    \
			      "mov r2=%1\n"                     \
			      "break 0x1000 ;;\n"               \
			      "mov %0=r8 ;;\n"                  \
			      : "=r" (__res)                    \
			      : "J" (__HYPERVISOR_##name),      \
				"rI" ((unsigned long)(a1)),     \
				"rI" ((unsigned long)(a2)),     \
				"rI" ((unsigned long)(a3)),     \
				"rI" ((unsigned long)(a4)),     \
				"rI" ((unsigned long)(a5))      \
			      : "r14","r15","r16","r2","r8",	\
			        "r17","r18","memory" );         \
	(type)__res;                                            \
})

static inline int
HYPERVISOR_sched_op_compat(
    int cmd, unsigned long arg)
{
	return _hypercall2(int, sched_op_compat, cmd, arg);
}

static inline int
HYPERVISOR_sched_op(
	int cmd, void *arg)
{
	return _hypercall2(int, sched_op, cmd, arg);
}

static inline long
HYPERVISOR_set_timer_op(
    u64 timeout)
{
    unsigned long timeout_hi = (unsigned long)(timeout>>32);
    unsigned long timeout_lo = (unsigned long)timeout;
    return _hypercall2(long, set_timer_op, timeout_lo, timeout_hi);
}

static inline int
HYPERVISOR_dom0_op(
    dom0_op_t *dom0_op)
{
    dom0_op->interface_version = DOM0_INTERFACE_VERSION;
    return _hypercall1(int, dom0_op, dom0_op);
}

static inline int
HYPERVISOR_multicall(
    void *call_list, int nr_calls)
{
    return _hypercall2(int, multicall, call_list, nr_calls);
}

//XXX xen/ia64 copy_from_guest() is broken.
//    This is a temporal work around until it is fixed.
static inline int
____HYPERVISOR_memory_op(
    unsigned int cmd, void *arg)
{
    return _hypercall2(int, memory_op, cmd, arg);
}

#include <xen/interface/memory.h>
#ifdef CONFIG_VMX_GUEST
# define ia64_xenmem_reservation_op(op, xmr) (0)
#else
int ia64_xenmem_reservation_op(unsigned long op,
		   struct xen_memory_reservation* reservation__);
#endif
static inline int
HYPERVISOR_memory_op(
    unsigned int cmd, void *arg)
{
    switch (cmd) {
    case XENMEM_increase_reservation:
    case XENMEM_decrease_reservation:
    case XENMEM_populate_physmap:
        return ia64_xenmem_reservation_op(cmd, 
                                          (struct xen_memory_reservation*)arg);
    default:
        return ____HYPERVISOR_memory_op(cmd, arg);
    }
    /* NOTREACHED */
}

static inline int
HYPERVISOR_event_channel_op(
    int cmd, void *arg)
{
    int rc = _hypercall2(int, event_channel_op, cmd, arg);
    if (unlikely(rc == -ENOSYS)) {
        struct evtchn_op op;
        op.cmd = cmd;
        memcpy(&op.u, arg, sizeof(op.u));
        rc = _hypercall1(int, event_channel_op_compat, &op);
    }
    return rc;
}

static inline int
HYPERVISOR_acm_op(
	unsigned int cmd, void *arg)
{
    return _hypercall2(int, acm_op, cmd, arg);
}

static inline int
HYPERVISOR_xen_version(
    int cmd, void *arg)
{
    return _hypercall2(int, xen_version, cmd, arg);
}

static inline int
HYPERVISOR_console_io(
    int cmd, int count, char *str)
{
    return _hypercall3(int, console_io, cmd, count, str);
}

static inline int
HYPERVISOR_physdev_op(
    int cmd, void *arg)
{
    int rc = _hypercall2(int, physdev_op, cmd, arg);
    if (unlikely(rc == -ENOSYS)) {
        struct physdev_op op;
        op.cmd = cmd;
        memcpy(&op.u, arg, sizeof(op.u));
        rc = _hypercall1(int, physdev_op_compat, &op);
    }
    return rc;
}

//XXX __HYPERVISOR_grant_table_op is used for this hypercall constant.
static inline int
____HYPERVISOR_grant_table_op(
    unsigned int cmd, void *uop, unsigned int count,
    unsigned long pa1, unsigned long pa2)
{
    return _hypercall5(int, grant_table_op, cmd, uop, count, pa1, pa2);
}

int HYPERVISOR_grant_table_op(unsigned int cmd, void *uop, unsigned int count);

static inline int
HYPERVISOR_vcpu_op(
	int cmd, int vcpuid, void *extra_args)
{
    return _hypercall3(int, vcpu_op, cmd, vcpuid, extra_args);
}

extern int HYPERVISOR_suspend(unsigned long srec);

static inline unsigned long
HYPERVISOR_hvm_op(
	int cmd, void *arg)
{
	return _hypercall2(unsigned long, hvm_op, cmd, arg);
}

static inline int
HYPERVISOR_callback_op(
	int cmd, void *arg)
{
	return _hypercall2(int, callback_op, cmd, arg);
}

extern fastcall unsigned int __do_IRQ(unsigned int irq, struct pt_regs *regs);
static inline void exit_idle(void) {}
#define do_IRQ(irq, regs) ({			\
	irq_enter();				\
	__do_IRQ((irq), (regs));		\
	irq_exit();				\
})

#include <linux/err.h>
#ifdef CONFIG_XEN
#include <asm/xen/privop.h>
#endif /* CONFIG_XEN */

static inline unsigned long
__HYPERVISOR_ioremap(unsigned long ioaddr, unsigned long size)
{
	return _hypercall3(unsigned long, ia64_dom0vp_op,
	                   IA64_DOM0VP_ioremap, ioaddr, size);
}

static inline unsigned long
HYPERVISOR_ioremap(unsigned long ioaddr, unsigned long size)
{
	unsigned long ret = ioaddr;
	if (is_running_on_xen()) {
		ret = __HYPERVISOR_ioremap(ioaddr, size);
		if (unlikely(ret == -ENOSYS))
			panic("hypercall %s failed with %ld. "
			      "Please check Xen and Linux config mismatch\n",
			      __func__, -ret);
		else if (unlikely(IS_ERR_VALUE(ret)))
			ret = ioaddr;
	}
	return ret;
}

static inline unsigned long
__HYPERVISOR_phystomach(unsigned long gpfn)
{
	return _hypercall2(unsigned long, ia64_dom0vp_op,
	                   IA64_DOM0VP_phystomach, gpfn);
}

static inline unsigned long
HYPERVISOR_phystomach(unsigned long gpfn)
{
	unsigned long ret = gpfn;
	if (is_running_on_xen()) {
		ret = __HYPERVISOR_phystomach(gpfn);
	}
	return ret;
}

static inline unsigned long
__HYPERVISOR_machtophys(unsigned long mfn)
{
	return _hypercall2(unsigned long, ia64_dom0vp_op,
	                   IA64_DOM0VP_machtophys, mfn);
}

static inline unsigned long
HYPERVISOR_machtophys(unsigned long mfn)
{
	unsigned long ret = mfn;
	if (is_running_on_xen()) {
		ret = __HYPERVISOR_machtophys(mfn);
	}
	return ret;
}

static inline unsigned long
__HYPERVISOR_zap_physmap(unsigned long gpfn, unsigned int extent_order)
{
	return _hypercall3(unsigned long, ia64_dom0vp_op,
	                   IA64_DOM0VP_zap_physmap, gpfn, extent_order);
}

static inline unsigned long
HYPERVISOR_zap_physmap(unsigned long gpfn, unsigned int extent_order)
{
	unsigned long ret = 0;
	if (is_running_on_xen()) {
		ret = __HYPERVISOR_zap_physmap(gpfn, extent_order);
	}
	return ret;
}

static inline unsigned long
__HYPERVISOR_add_physmap(unsigned long gpfn, unsigned long mfn,
			 unsigned long flags, domid_t domid)
{
	return _hypercall5(unsigned long, ia64_dom0vp_op,
	                   IA64_DOM0VP_add_physmap, gpfn, mfn, flags, domid);
}

static inline unsigned long
HYPERVISOR_add_physmap(unsigned long gpfn, unsigned long mfn,
		       unsigned long flags, domid_t domid)
{
	unsigned long ret = 0;
	BUG_ON(!is_running_on_xen());//XXX
	if (is_running_on_xen()) {
		ret = __HYPERVISOR_add_physmap(gpfn, mfn, flags, domid);
	}
	return ret;
}

// for balloon driver
#define HYPERVISOR_update_va_mapping(va, new_val, flags) (0)

#endif /* __HYPERCALL_H__ */