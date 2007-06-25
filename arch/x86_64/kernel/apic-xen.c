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
#include <linux/module.h>
#include <linux/ioport.h>

#include <asm/atomic.h>
#include <asm/smp.h>
#include <asm/mtrr.h>
#include <asm/mpspec.h>
#include <asm/pgalloc.h>
#include <asm/mach_apic.h>
#include <asm/nmi.h>
#include <asm/idle.h>
#include <asm/proto.h>
#include <asm/timex.h>
#include <asm/apic.h>

int apic_mapped;
int apic_verbosity;

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
  	 * But don't ack when the APIC is disabled. -AK
	 */
	if (!disable_apic)
		ack_APIC_irq();
}

int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}

void smp_local_timer_interrupt(void)
{
	profile_tick(CPU_PROFILING);
#ifndef CONFIG_XEN
#ifdef CONFIG_SMP
	update_process_times(user_mode(get_irq_regs()));
#endif
	if (apic_runs_main_timer > 1 && smp_processor_id() == boot_cpu_id)
		main_timer_handler();
#endif
	/*
	 * We take the 'long' return path, and there every subsystem
	 * grabs the appropriate locks (kernel lock/ irq lock).
	 *
	 * We might want to decouple profiling from the 'long path',
	 * and do the profiling totally in assembly.
	 *
	 * Currently this isn't too much of an issue (performance wise),
	 * we can take more than 100K local irqs per second on a 100 MHz P5.
	 */
}

/*
 * Local APIC timer interrupt. This is the most natural way for doing
 * local interrupts, but local timer interrupts can be emulated by
 * broadcast interrupts too. [in case the hw doesn't support APIC timers]
 *
 * [ if a single-CPU system runs an SMP kernel then we call the local
 *   interrupt as well. Thus we cannot inline the local irq ... ]
 */
void smp_apic_timer_interrupt(struct pt_regs *regs)
{
	struct pt_regs *old_regs = set_irq_regs(regs);

	/*
	 * the NMI deadlock-detector uses this.
	 */
	add_pda(apic_timer_irqs, 1);

	/*
	 * NOTE! We'd better ACK the irq immediately,
	 * because timer handling can be slow.
	 */
	ack_APIC_irq();
	/*
	 * update_process_times() expects us to have done irq_enter().
	 * Besides, if we don't timer interrupts ignore the global
	 * interrupt lock, which is the WrongThing (tm) to do.
	 */
	exit_idle();
	irq_enter();
	smp_local_timer_interrupt();
	irq_exit();
	set_irq_regs(old_regs);
}

/*
 * This interrupt should _never_ happen with our APIC/SMP architecture
 */
asmlinkage void smp_spurious_interrupt(void)
{
	unsigned int v;
	exit_idle();
	irq_enter();
	/*
	 * Check if this really is a spurious interrupt and ACK it
	 * if it is a vectored one.  Just in case...
	 * Spurious interrupts should not be ACKed.
	 */
	v = apic_read(APIC_ISR + ((SPURIOUS_APIC_VECTOR & ~0x1f) >> 1));
	if (v & (1 << (SPURIOUS_APIC_VECTOR & 0x1f)))
		ack_APIC_irq();

#if 0
	static unsigned long last_warning; 
	static unsigned long skipped; 

	/* see sw-dev-man vol 3, chapter 7.4.13.5 */
	if (time_before(last_warning+30*HZ,jiffies)) { 
		printk(KERN_INFO "spurious APIC interrupt on CPU#%d, %ld skipped.\n",
		       smp_processor_id(), skipped);
		last_warning = jiffies; 
		skipped = 0;
	} else { 
		skipped++; 
	} 
#endif 
	irq_exit();
}

/*
 * This interrupt should never happen with our APIC/SMP architecture
 */

asmlinkage void smp_error_interrupt(void)
{
	unsigned int v, v1;

	exit_idle();
	irq_enter();
	/* First tickle the hardware, only then report what went on. -- REW */
	v = apic_read(APIC_ESR);
	apic_write(APIC_ESR, 0);
	v1 = apic_read(APIC_ESR);
	ack_APIC_irq();
	atomic_inc(&irq_err_count);

	/* Here is what the APIC error bits mean:
	   0: Send CS error
	   1: Receive CS error
	   2: Send accept error
	   3: Receive accept error
	   4: Reserved
	   5: Send illegal vector
	   6: Received illegal vector
	   7: Illegal register address
	*/
	if (num_online_cpus() > 1)
		printk (KERN_DEBUG "APIC error on CPU%d: %02x(%02x)\n",
	        smp_processor_id(), v , v1);
	irq_exit();
}

int disable_apic; 

/*
 * This initializes the IO-APIC and APIC hardware if this is
 * a UP kernel.
 */
int __init APIC_init_uniprocessor (void)
{
	if (smp_found_config && !skip_ioapic_setup && nr_ioapics)
		setup_IO_APIC();
	return 0;
}

#ifndef CONFIG_XEN
static __init int setup_disableapic(char *str) 
{ 
	disable_apic = 1;
	clear_bit(X86_FEATURE_APIC, boot_cpu_data.x86_capability);
	return 0;
}
early_param("disableapic", setup_disableapic);

/* same as disableapic, for compatibility */
static __init int setup_nolapic(char *str) 
{ 
	return setup_disableapic(str);
} 
early_param("nolapic", setup_nolapic);

static __init int setup_noapictimer(char *str) 
{ 
	if (str[0] != ' ' && str[0] != 0)
		return 0;
	disable_apic_timer = 1;
	return 1;
} 

static __init int setup_apicmaintimer(char *str)
{
	apic_runs_main_timer = 1;
	nohpet = 1;
	return 1;
}
__setup("apicmaintimer", setup_apicmaintimer);

static __init int setup_noapicmaintimer(char *str)
{
	apic_runs_main_timer = -1;
	return 1;
}
__setup("noapicmaintimer", setup_noapicmaintimer);

static __init int setup_apicpmtimer(char *s)
{
	apic_calibrate_pmtmr = 1;
	notsc_setup(NULL);
	return setup_apicmaintimer(NULL);
}
__setup("apicpmtimer", setup_apicpmtimer);

__setup("noapictimer", setup_noapictimer); 

#endif
