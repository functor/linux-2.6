/*
 *  linux/arch/i386/kernel/stackoverflow.c
 *
 *  Copyright (C) 2005  Marc E. Fiuczynski
 *
 */

/*
 * This file handles the architecture-dependent parts of stack overflow handling...
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/thread_info.h>
#include <asm/processor.h>

void dstackdump(void)
{
        unsigned long esp = current_stack_pointer();
	int panicing = ((esp&(THREAD_SIZE-1)) <= STACK_PANIC);

	oops_in_progress = 1;
	printk( "esp: 0x%lx masked: 0x%lx STACK_PANIC:0x%lx %d %d\n",
		esp, (esp&(THREAD_SIZE-1)), STACK_PANIC, 
		(((esp&(THREAD_SIZE-1)) <= STACK_PANIC)), panicing);

	show_trace(current,(void*)esp);

	if (panicing)
	  panic("stack overflow\n");

	oops_in_progress = 0;

	/* Just let it happen once per task, as otherwise it goes nuts
	 * in printing stack traces.  This means that I need to dump
	 * the stack_overflowed boolean into the task or thread_info
	 * structure.  For now just turn it off all together.
	 */

	/* stack_overflowed = 0; */
}
