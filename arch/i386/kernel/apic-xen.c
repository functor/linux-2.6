/*
 *	Local APIC handling, local APIC timers
 *
 *	(c) 1999, 2000 Ingo Molnar <mingo@redhat.com>
 *
 *	Fixes
 *	Maciej W. Rozycki	:	Bits for genuine 82489DX APICs;
 *					thanks to Eric Gilmore
 *					and Rolf G. Tews
 *					for testing these extensively.
 *	Maciej W. Rozycki	:	Various updates and fixes.
 *	Mikael Pettersson	:	Power Management for UP-APIC.
 *	Pavel Machek and
 *	Mikael Pettersson	:	PM converted to driver model.
 */

#include <linux/init.h>

#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/mc146818rtc.h>
#include <linux/kernel_stat.h>
#include <linux/sysdev.h>
#include <linux/cpu.h>
#include <linux/module.h>

#include <asm/atomic.h>
#include <asm/smp.h>
#include <asm/mtrr.h>
#include <asm/mpspec.h>
#include <asm/desc.h>
#include <asm/arch_hooks.h>
#include <asm/hpet.h>
#include <asm/i8253.h>
#include <asm/nmi.h>

#include <mach_apic.h>
#include <mach_apicdef.h>
#include <mach_ipi.h>

#include "io_ports.h"

#ifndef CONFIG_XEN
/*
 * cpu_mask that denotes the CPUs that needs timer interrupt coming in as
 * IPIs in place of local APIC timers
 */
static cpumask_t timer_bcast_ipi;
#endif

/*
 * Knob to control our willingness to enable the local APIC.
 */
static int enable_local_apic __initdata = 0; /* -1=force-disable, +1=force-enable */

static inline void lapic_disable(void)
{
	enable_local_apic = -1;
	clear_bit(X86_FEATURE_APIC, boot_cpu_data.x86_capability);
}

static inline void lapic_enable(void)
{
	enable_local_apic = 1;
}

/*
 * Debug level
 */
int apic_verbosity;

static int modern_apic(void)
{
#ifndef CONFIG_XEN
	unsigned int lvr, version;
	/* AMD systems use old APIC versions, so check the CPU */
	if (boot_cpu_data.x86_vendor == X86_VENDOR_AMD &&
		boot_cpu_data.x86 >= 0xf)
		return 1;
	lvr = apic_read(APIC_LVR);
	version = GET_APIC_VERSION(lvr);
	return version >= 0x14;
#else
	return 1;
#endif
}

/*
 * 'what should we do if we get a hw irq event on an illegal vector'.
 * each architecture has to answer this themselves.
 */
void ack_bad_irq(unsigned int irq)
{
	printk("unexpected IRQ trap at vector %02x\n", irq);
	/*
	 * Currently unexpected vectors happen only on SMP and APIC.
	 * We _must_ ack these because every local APIC has only N
	 * irq slots per priority level, and a 'hanging, unacked' IRQ
	 * holds up an irq slot - in excessive cases (when multiple
	 * unexpected vectors occur) that might lock up the APIC
	 * completely.
	 * But only ack when the APIC is enabled -AK
	 */
	if (cpu_has_apic)
		ack_APIC_irq();
}

#ifndef CONFIG_XEN
void __init apic_intr_init(void)
{
#ifdef CONFIG_SMP
	smp_intr_init();
#endif
	/* self generated IPI for local APIC timer */
	set_intr_gate(LOCAL_TIMER_VECTOR, apic_timer_interrupt);

	/* IPI vectors for APIC spurious and error interrupts */
	set_intr_gate(SPURIOUS_APIC_VECTOR, spurious_interrupt);
	set_intr_gate(ERROR_APIC_VECTOR, error_interrupt);

	/* thermal monitor LVT interrupt */
#ifdef CONFIG_X86_MCE_P4THERMAL
	set_intr_gate(THERMAL_APIC_VECTOR, thermal_interrupt);
#endif
}

/* Using APIC to generate smp_local_timer_interrupt? */
int using_apic_timer __read_mostly = 0;

static int enabled_via_apicbase;

void enable_NMI_through_LVT0 (void * dummy)
{
	unsigned int v, ver;

	ver = apic_read(APIC_LVR);
	ver = GET_APIC_VERSION(ver);
	v = APIC_DM_NMI;			/* unmask and set to NMI */
	if (!APIC_INTEGRATED(ver))		/* 82489DX */
		v |= APIC_LVT_LEVEL_TRIGGER;
	apic_write_around(APIC_LVT0, v);
}
#endif /* !CONFIG_XEN */

int get_physical_broadcast(void)
{
	if (modern_apic())
		return 0xff;
	else
		return 0xf;
}

#ifndef CONFIG_XEN
#ifndef CONFIG_SMP
static void up_apic_timer_interrupt_call(void)
{
	int cpu = smp_processor_id();

	/*
	 * the NMI deadlock-detector uses this.
	 */
	per_cpu(irq_stat, cpu).apic_timer_irqs++;

	smp_local_timer_interrupt();
}
#endif

void smp_send_timer_broadcast_ipi(void)
{
	cpumask_t mask;

	cpus_and(mask, cpu_online_map, timer_bcast_ipi);
	if (!cpus_empty(mask)) {
#ifdef CONFIG_SMP
		send_IPI_mask(mask, LOCAL_TIMER_VECTOR);
#else
		/*
		 * We can directly call the apic timer interrupt handler
		 * in UP case. Minus all irq related functions
		 */
		up_apic_timer_interrupt_call();
#endif
	}
}
#endif

int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}

/*
 * This initializes the IO-APIC and APIC hardware if this is
 * a UP kernel.
 */
int __init APIC_init_uniprocessor (void)
{
#ifdef CONFIG_X86_IO_APIC
	if (smp_found_config)
		if (!skip_ioapic_setup && nr_ioapics)
			setup_IO_APIC();
#endif

	return 0;
}

static int __init parse_lapic(char *arg)
{
	lapic_enable();
	return 0;
}
early_param("lapic", parse_lapic);

static int __init parse_nolapic(char *arg)
{
	lapic_disable();
	return 0;
}
early_param("nolapic", parse_nolapic);

