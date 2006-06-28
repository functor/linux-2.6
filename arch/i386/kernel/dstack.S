/*
 *  linux/arch/i386/dstack.S
 *
 *  Copyright (C) 2005 Marc E. Fiuczynski
 */

/*
 * dstack.s contains the system specific support for stack overflows
 * and discontiguous stack support.
 */

#include <linux/config.h>
#include <linux/linkage.h>	
#include <asm/thread_info.h>	
#include <asm/errno.h>
#include <asm/segment.h>
#include <asm/smp.h>
#include <asm/page.h>

.data
.globl stack_overflowed
stack_overflowed:
	.long 0
.text

ENTRY(mcount)
#warning stack check enabled
	push %eax
	movl $(THREAD_SIZE - 1),%eax
	andl %esp,%eax
	cmpl $STACK_WARN,%eax
	jle 1f
2:
	popl %eax
	ret
1:
	/* prevent infinite recursion from call to mcount from the
	 * stack_overflow function.  Need to revisit this code for
	 * SMP based systems.
	 */
	lock; btsl $0,stack_overflowed
	jc 2b

	/* prepare to jmp to stack_overflow directly, as if it were 
	 * called directly by the caller of mcount.  
	 */
	pushl %ebp
	pushl %ebx
	pushl %esi
	pushl %edi
	
	call stack_overflow
	/* Note that stack_overflow() will clear the stack_overflowed
	 * variable.
	 */

	popl %edi
	popl %esi
	popl %ebx
	popl %ebp
	
	popl %eax	
	ret
