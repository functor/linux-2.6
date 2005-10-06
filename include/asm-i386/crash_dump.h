/* asm-i386/crash_dump.h */
#include <linux/bootmem.h>
#include <linux/irq.h>
#include <asm/apic.h>

#ifdef CONFIG_CRASH_DUMP
extern unsigned int dump_enabled;
extern unsigned int crashed;

extern void __crash_relocate_mem(unsigned long, unsigned long);
extern unsigned long __init find_max_low_pfn(void);
extern void __init find_max_pfn(void);

extern struct pt_regs crash_smp_regs[NR_CPUS];
extern long crash_smp_current_task[NR_CPUS];
extern void crash_dump_save_this_cpu(struct pt_regs *, int);
extern void __crash_dump_stop_cpus(void);
extern void crash_get_current_regs(struct pt_regs *regs);

#define CRASH_BACKUP_BASE ((unsigned long)CONFIG_BACKUP_BASE * 0x100000)
#define CRASH_BACKUP_SIZE ((unsigned long)CONFIG_BACKUP_SIZE * 0x100000)
#define CRASH_RELOCATE_SIZE 0xa0000

static inline void crash_relocate_mem(void)
{
	if (crashed)
		__crash_relocate_mem(CRASH_BACKUP_BASE + CRASH_BACKUP_SIZE,
					CRASH_RELOCATE_SIZE);
}

static inline void set_saved_max_pfn(void)
{
	find_max_pfn();
	saved_max_pfn = find_max_low_pfn();
}

static inline void crash_reserve_bootmem(void)
{
	if (!dump_enabled) {
		reserve_bootmem(CRASH_BACKUP_BASE,
			CRASH_BACKUP_SIZE + CRASH_RELOCATE_SIZE + PAGE_SIZE);
	}
}

static inline void crash_dump_stop_cpus(void)
{
	int cpu;

	if (!crashed)
		return;

	cpu = smp_processor_id();

	crash_smp_current_task[cpu] = (long)current;
	crash_get_current_regs(&crash_smp_regs[cpu]);

	/* This also captures the register states of the other cpus */
	__crash_dump_stop_cpus();
#if defined(CONFIG_X86_IO_APIC)
	disable_IO_APIC();
#endif
#if defined(CONFIG_X86_LOCAL_APIC)
	disconnect_bsp_APIC();
#endif
}

static inline void crash_dump_save_registers(void)
{
	void *addr;

	addr = __va(CRASH_BACKUP_BASE + CRASH_BACKUP_SIZE + CRASH_RELOCATE_SIZE);
	memcpy(addr, crash_smp_regs, (sizeof(struct pt_regs)*NR_CPUS));
	addr += sizeof(struct pt_regs)*NR_CPUS;
	memcpy(addr, crash_smp_current_task, (sizeof(long)*NR_CPUS));
}
#else
#define crash_relocate_mem() do { } while(0)
#define set_saved_max_pfn() do { } while(0)
#define crash_reserve_bootmem() do { } while(0)
#define crash_dump_stop_cpus() do { } while(0)
#define crash_dump_save_registers() do { } while(0)
#endif
