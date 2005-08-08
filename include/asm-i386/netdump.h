#ifndef _ASM_I386_NETDUMP_H
#define _ASM_I386_NETDUMP_H

/*
 * linux/include/asm-i386/netdump.h
 *
 * Copyright (c) 2003, 2004 Red Hat, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef __KERNEL__

#include <asm/irq.h>

extern int page_is_ram (unsigned long);
const static int platform_supports_netdump = 1;
extern union irq_ctx *netdump_irq_ctx;

#define platform_timestamp(x) rdtscll(x)

#define platform_fix_regs()						\
{									\
       unsigned long esp;						\
       unsigned short ss;						\
       esp = (unsigned long) ((char *)regs + sizeof (struct pt_regs));	\
       ss = __KERNEL_DS;						\
       if (regs->xcs & 3) {						\
               esp = regs->esp;						\
               ss = regs->xss & 0xffff;					\
       }								\
       myregs = *regs;							\
       myregs.esp = esp;						\
       myregs.xss = (myregs.xss & 0xffff0000) | ss;			\
};

static inline void platform_init_stack(void **stackptr)
{
	*stackptr = (void *)kmalloc(sizeof(union irq_ctx), GFP_KERNEL);
	if (*stackptr)
		memset(*stackptr, 0, sizeof(union irq_ctx));
	else
		printk(KERN_WARNING
		       "netdump: unable to allocate separate stack\n");
}

static inline void platform_start_netdump(void *stackptr, struct pt_regs *regs)
{
	u32 *dsp;
	union irq_ctx * curctx;
	union irq_ctx * dumpctx;

	if (!stackptr)
		netpoll_netdump(regs);
	else {
		curctx = (union irq_ctx *) current_thread_info();
		dumpctx = (union irq_ctx *) stackptr;

		/* build the stack frame on the IRQ stack */
		dsp = (u32*) ((char*)dumpctx + sizeof(*dumpctx));
		dumpctx->tinfo.task = curctx->tinfo.task;
		dumpctx->tinfo.real_stack = curctx->tinfo.real_stack;
		dumpctx->tinfo.virtual_stack = curctx->tinfo.virtual_stack;
		dumpctx->tinfo.previous_esp = current_stack_pointer();

		*--dsp = (u32) regs;

		asm volatile(
			"       xchgl   %%ebx,%%esp     \n"
			"	call    netpoll_netdump \n"
			"	xchgl   %%ebx,%%esp     \n"
			: : "b"(dsp) :	"memory", "cc", "edx", "ecx"
		);
	}
}

#define platform_cleanup_stack(stackptr)	\
do {						\
	if (stackptr)				\
		kfree(stackptr);		\
} while (0)

#define platform_max_pfn() (num_physpages)

#endif /* __KERNEL__ */

#endif /* _ASM_I386_NETDUMP_H */
