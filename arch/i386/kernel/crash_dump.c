/*
 * Architecture specific (i386) functions for kexec based crash dumps.
 *
 * Created by: Hariprasad Nellitheertha (hari@in.ibm.com)
 *
 * Copyright (C) IBM Corporation, 2004. All rights reserved.
 *
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/smp.h>
#include <linux/irq.h>

#include <asm/crash_dump.h>
#include <asm/processor.h>
#include <asm/hardirq.h>
#include <asm/nmi.h>
#include <asm/hw_irq.h>

struct pt_regs crash_smp_regs[NR_CPUS];
long crash_smp_current_task[NR_CPUS];

#ifdef CONFIG_SMP
static atomic_t waiting_for_dump_ipi;
static int crash_dump_expect_ipi[NR_CPUS];
extern void crash_dump_send_ipi(void);
extern void stop_this_cpu(void *);

static int crash_dump_nmi_callback(struct pt_regs *regs, int cpu)
{
	if (!crash_dump_expect_ipi[cpu])
		return 0;

	crash_dump_expect_ipi[cpu] = 0;
	crash_dump_save_this_cpu(regs, cpu);
	atomic_dec(&waiting_for_dump_ipi);

	stop_this_cpu(NULL);

	return 1;
}

void __crash_dump_stop_cpus(void)
{
	int i, cpu, other_cpus;

	preempt_disable();
	cpu = smp_processor_id();
	other_cpus = num_online_cpus()-1;

	if (other_cpus > 0) {
		atomic_set(&waiting_for_dump_ipi, other_cpus);

		for (i = 0; i < NR_CPUS; i++)
			crash_dump_expect_ipi[i] = (i != cpu && cpu_online(i));

		set_nmi_callback(crash_dump_nmi_callback);
		/* Ensure the new callback function is set before sending
		 * out the IPI
		 */
		wmb();

		crash_dump_send_ipi();
		while (atomic_read(&waiting_for_dump_ipi) > 0)
			cpu_relax();

		unset_nmi_callback();
	} else {
		local_irq_disable();
		disable_local_APIC();
		local_irq_enable();
	}
	preempt_enable();
}
#else
void __crash_dump_stop_cpus(void) {}
#endif

void crash_get_current_regs(struct pt_regs *regs)
{
	__asm__ __volatile__("movl %%ebx,%0" : "=m"(regs->ebx));
	__asm__ __volatile__("movl %%ecx,%0" : "=m"(regs->ecx));
	__asm__ __volatile__("movl %%edx,%0" : "=m"(regs->edx));
	__asm__ __volatile__("movl %%esi,%0" : "=m"(regs->esi));
	__asm__ __volatile__("movl %%edi,%0" : "=m"(regs->edi));
	__asm__ __volatile__("movl %%ebp,%0" : "=m"(regs->ebp));
	__asm__ __volatile__("movl %%eax,%0" : "=m"(regs->eax));
	__asm__ __volatile__("movl %%esp,%0" : "=m"(regs->esp));
	__asm__ __volatile__("movw %%ss, %%ax;" :"=a"(regs->xss));
	__asm__ __volatile__("movw %%cs, %%ax;" :"=a"(regs->xcs));
	__asm__ __volatile__("movw %%ds, %%ax;" :"=a"(regs->xds));
	__asm__ __volatile__("movw %%es, %%ax;" :"=a"(regs->xes));
	__asm__ __volatile__("pushfl; popl %0" :"=m"(regs->eflags));

	regs->eip = (unsigned long)current_text_addr();
}

void crash_dump_save_this_cpu(struct pt_regs *regs, int cpu)
{
	crash_smp_current_task[cpu] = (long)current;
	crash_smp_regs[cpu] = *regs;
}

