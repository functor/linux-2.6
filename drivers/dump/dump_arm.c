/*
 * Architecture specific (ARM/XScale) functions for Linux crash dumps.
 *
 * Created by: Fleming Feng (fleming.feng@intel.com)
 *
 * Copyright(C) 2003 Intel Corp. All rights reserved.
 *
 * This code is released under version 2 of the GNU GPL.
 */

/*
 * The hooks for dumping the kernel virtual memory to disk are in this
 * file.  Any time a modification is made to the virtual memory mechanism,
 * these routines must be changed to use the new mechanisms.
 */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/dump.h>
#include <linux/mm.h>
#include <asm/processor.h>
#include <asm/hardirq.h>
#include <asm/kdebug.h>

static __s32	saved_irq_count;	/* saved preempt_count() flags */

static int alloc_dha_stack(void)
{
	int i;
	void *ptr;
	
	if (dump_header_asm.dha_stack[0])
		return 0;

       	ptr = vmalloc(THREAD_SIZE * num_online_cpus());
	if (!ptr) {
		printk("vmalloc for dha_stacks failed\n");
		return -ENOMEM;
	}

	for( i = 0; i < num_online_cpus(); i++){
		dump_header_asm.dha_stack[i] = (u32)((unsigned long)ptr +
					             (i * THREAD_SIZE));
	}

	return 0;
}

static int free_dha_stack(void) 
{
	if (dump_header_asm.dha_stack[0]){
		vfree((void*)dump_header_asm.dha_stack[0]);
		dump_header_asm.dha_stack[0] = 0;
	}
	return 0;
}

void __dump_save_regs(struct pt_regs* dest_regs, const struct pt_regs* regs)
{

	/* Here, because the arm version uses _dump_regs_t,
	 * instead of pt_regs in dump_header_asm, while the
	 * the function is defined inside architecture independent
 	 * header file include/linux/dump.h, the size of block of
	 * memory copied is not equal to pt_regs.
	 */

	memcpy(dest_regs, regs, sizeof(_dump_regs_t));

}

#ifdef CONFIG_SMP
/* FIXME: This is reserved for possible future usage for SMP system 
 * based on ARM/XScale. Currently, there is no information for an 
 * SMP system based on ARM/XScale, they are not used!
 */
/* save registers on other processor */
void
__dump_save_other_cpus(void)
{

	/* Dummy now! */

	return;
	
}
#else	/* !CONFIG_SMP */
#define save_other_cpu_state() do { } while (0)
#endif	/* !CONFIG_SMP */

/* 
 * Kludge - dump from interrupt context is unreliable (Fixme)
 *
 * We do this so that softirqs initiated for dump i/o 
 * get processed and we don't hang while waiting for i/o
 * to complete or in any irq synchronization attempt.
 *
 * This is not quite legal of course, as it has the side 
 * effect of making all interrupts & softirqs triggered 
 * while dump is in progress complete before currently 
 * pending softirqs and the currently executing interrupt 
 * code. 
 */
static inline void
irq_bh_save(void)
{
	saved_irq_count = irq_count();
	preempt_count() &= ~(HARDIRQ_MASK|SOFTIRQ_MASK);
}

static inline void
irq_bh_restore(void)
{
	preempt_count() |= saved_irq_count;
}

/*
 * Name: __dump_irq_enable
 * Func: Reset system so interrupts are enabled.
 *	 This is used for dump methods that requires interrupts
 * 	 Eventually, all methods will have interrupts disabled
 *	 and this code can be removed.
 *	 
 *	 Re-enable interrupts
 */
int
__dump_irq_enable(void)
{
	irq_bh_save();
	local_irq_enable();
	return 0;
}

/* Name: __dump_irq_restore
 * Func: Resume the system state in an architecture-specific way.
 */
void 
__dump_irq_restore(void)
{
	local_irq_disable();
	irq_bh_restore();
}
	

/*
 * Name: __dump_configure_header()
 * Func: Meant to fill in arch specific header fields except per-cpu state
 * 	 already captured in dump_lcrash_configure_header.
 */
int
__dump_configure_header(const struct pt_regs *regs)
{
	return (0);
}

/*
 * Name: dump_die_event
 * Func: Called from notify_die
 */
static int dump_die_event(struct notifier_block* this,
			  unsigned long event,
			  void* arg)
{
	const struct die_args* args = (const struct die_args*)arg;

	switch(event){
		case DIE_PANIC:	
		case DIE_OOPS:
		case DIE_WATCHDOG:
			dump_execute(args->str, args->regs);
			break;		
	}
	return NOTIFY_DONE;

}

static struct notifier_block dump_die_block = {
	.notifier_call = dump_die_event,
};

/* Name: __dump_init()
 * Func: Initialize the dumping routine process.
 */
void
__dump_init(uint64_t local_memory_start)
{
	/* hook into PANIC and OOPS */
	register_die_notifier(&dump_die_block);
}

/*
 * Name: __dump_open()
 * Func: Open the dump device (architecture specific).  This is in
 *       case it's necessary in the future.
 */
void
__dump_open(void)
{

	alloc_dha_stack();

	return;
}

/*
 * Name: __dump_cleanup()
 * Func: Free any architecture specific data structures. This is called
 *       when the dump module is being removed.
 */
void
__dump_cleanup(void)
{
	free_dha_stack();
	unregister_die_notifier(&dump_die_block);

	/* return */
	return;
}

/* 
 * Name: __dump_page_valid()
 * Func: Check if page is valid to dump.
 */
int
__dump_page_valid(unsigned long index)
{
	if(!pfn_valid(index))
		return 0;
	else
		return 1;
}

/* 
 * Name: manual_handle_crashdump 
 * Func: Interface for the lkcd dump command. Calls dump_execute()
 */
int
manual_handle_crashdump(void) {
	
	_dump_regs_t regs;	

	get_current_general_regs(&regs);
	get_current_cp14_regs(&regs);
	get_current_cp15_regs(&regs);
	dump_execute("manual", &regs);
	return 0;
}
