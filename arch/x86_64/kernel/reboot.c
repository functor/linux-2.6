/* Various gunk just to reboot the machine. */ 
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/string.h>
#include <asm/io.h>
#include <asm/kdebug.h>
#include <asm/delay.h>
#include <asm/hw_irq.h>
#include <asm/system.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <asm/apic.h>

/*
 * Power off function, if any
 */
void (*pm_power_off)(void);

static long no_idt[3];
static int reboot_mode = 0;
enum reboot_types reboot_type = BOOT_KBD;
EXPORT_SYMBOL(reboot_type);
int reboot_override;
EXPORT_SYMBOL(reboot_override);

void (*machine_reset)(void);
EXPORT_SYMBOL(machine_reset);

/* reboot=b[ios] | t[riple] | k[bd] [, [w]arm | [c]old] | [a]cpi
   bios	  Use the CPU reboot vector for warm reset
   warm   Don't set the cold reboot flag
   cold   Set the cold reboot flag
   triple Force a triple fault (init)
   kbd    Use the keyboard controller. cold reset (default)
   acpi   Use the ACPI reset mechanism defined in the FADT
 */ 
static int __init reboot_setup(char *str)
{
	for (;;) {
		switch (*str) {
		case 'w': 
			reboot_mode = 0x1234;
			break;

		case 'c':
			reboot_mode = 0;
			break;

		case 't':
		case 'b':
		case 'k':
		case 'a':
			reboot_type = *str;
			reboot_override = 1;
			break;
		}
		if((str = strchr(str,',')) != NULL)
			str++;
		else
			break;
	}
	return 1;
}

__setup("reboot=", reboot_setup);

/* overwrites random kernel memory. Should not be kernel .text */
#define WARMBOOT_TRAMP 0x1000UL

static void reboot_warm(void)
{
	extern unsigned char warm_reboot[], warm_reboot_end[];
	printk("warm reboot\n");

	local_irq_disable(); 
		
	/* restore identity mapping */
	init_level4_pgt[0] = __pml4(__pa(level3_ident_pgt) | 7); 
	__flush_tlb_all(); 

	/* Move the trampoline to low memory */
	memcpy(__va(WARMBOOT_TRAMP), warm_reboot, warm_reboot_end - warm_reboot); 

	/* Start it in compatibility mode. */
	asm volatile( "   pushq $0\n" 		/* ss */
		     "   pushq $0x2000\n" 	/* rsp */
	             "   pushfq\n"		/* eflags */
		     "   pushq %[cs]\n"
		     "   pushq %[target]\n"
		     "   iretq" :: 
		      [cs] "i" (__KERNEL_COMPAT32_CS), 
		      [target] "b" (WARMBOOT_TRAMP));
}

static inline void kb_wait(void)
{
	int i;

	for (i=0; i<0x10000; i++)
		if ((inb_p(0x64) & 0x02) == 0)
			break;
}

void machine_shutdown(void)
{
	/* Stop the cpus and apics */
#ifdef CONFIG_SMP
	int reboot_cpu_id;

	/* The boot cpu is always logical cpu 0 */
	reboot_cpu_id = 0;

	/* Make certain the cpu I'm about to reboot on is online */
	if (!cpu_isset(reboot_cpu_id, cpu_online_map)) {
		reboot_cpu_id = smp_processor_id();
	}

	/* Make certain I only run on the appropriate processor */
	set_cpus_allowed(current, cpumask_of_cpu(reboot_cpu_id));

	/* O.K Now that I'm on the appropriate processor,
	 * stop all of the others.
	 */
	smp_send_stop();
#endif

	local_irq_disable();

#ifndef CONFIG_SMP
	disable_local_APIC();
#endif

	disable_IO_APIC();

	local_irq_enable();
}

void machine_restart(char * __unused)
{
	int i;

	machine_shutdown();
	
	/* Tell the BIOS if we want cold or warm reboot */
	*((unsigned short *)__va(0x472)) = reboot_mode;
       
	for (;;) {
		/* Could also try the reset bit in the Hammer NB */
		switch (reboot_type) { 
		case BOOT_ACPI:
			if (machine_reset)
				(*machine_reset)();
			else
				reboot_type = BOOT_KBD;
			break;
		
		case BOOT_BIOS:
			reboot_warm();

		case BOOT_KBD:
		for (i=0; i<100; i++) {
			kb_wait();
			udelay(50);
			outb(0xfe,0x64);         /* pulse reset low */
			udelay(50);
		}

		case BOOT_TRIPLE: 
		__asm__ __volatile__("lidt (%0)": :"r" (&no_idt));
		__asm__ __volatile__("int3");

			reboot_type = BOOT_KBD;
			break;
		}      
	}      
}

EXPORT_SYMBOL(machine_restart);

void machine_halt(void)
{
}

EXPORT_SYMBOL(machine_halt);

void machine_power_off(void)
{
	if (pm_power_off)
		pm_power_off();
}

EXPORT_SYMBOL(machine_power_off);
