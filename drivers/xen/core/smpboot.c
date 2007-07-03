/*
 *	Xen SMP booting functions
 *
 *	See arch/i386/kernel/smpboot.c for copyright and credits for derived
 *	portions of this file.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/smp_lock.h>
#include <linux/irq.h>
#include <linux/bootmem.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <asm/desc.h>
#include <asm/arch_hooks.h>
#include <asm/pgalloc.h>
#if defined(__i386__)
#include <asm/pda.h>
#endif
#include <xen/evtchn.h>
#include <xen/interface/vcpu.h>
#include <xen/cpu_hotplug.h>
#include <xen/xenbus.h>

extern irqreturn_t smp_reschedule_interrupt(int, void *);
extern irqreturn_t smp_call_function_interrupt(int, void *);

extern void local_setup_timer(unsigned int cpu);
extern void local_teardown_timer(unsigned int cpu);

extern void hypervisor_callback(void);
extern void failsafe_callback(void);
extern void system_call(void);
extern void smp_trap_init(trap_info_t *);

/* Number of siblings per CPU package */
int smp_num_siblings = 1;
EXPORT_SYMBOL(smp_num_siblings);
#if defined(__i386__)
int cpu_llc_id[NR_CPUS] __cpuinitdata = {[0 ... NR_CPUS-1] = BAD_APICID};
#elif defined(__x86_64__)
u8 cpu_llc_id[NR_CPUS] __cpuinitdata  = {[0 ... NR_CPUS-1] = BAD_APICID};
#endif
EXPORT_SYMBOL(cpu_llc_id);

cpumask_t cpu_online_map;
EXPORT_SYMBOL(cpu_online_map);
cpumask_t cpu_possible_map;
EXPORT_SYMBOL(cpu_possible_map);

struct cpuinfo_x86 cpu_data[NR_CPUS] __cacheline_aligned;
EXPORT_SYMBOL(cpu_data);

#ifdef CONFIG_HOTPLUG_CPU
DEFINE_PER_CPU(int, cpu_state) = { 0 };
#endif

static DEFINE_PER_CPU(int, resched_irq);
static DEFINE_PER_CPU(int, callfunc_irq);
static char resched_name[NR_CPUS][15];
static char callfunc_name[NR_CPUS][15];

u8 cpu_2_logical_apicid[NR_CPUS] = { [0 ... NR_CPUS-1] = BAD_APICID };

void *xquad_portio;

cpumask_t cpu_sibling_map[NR_CPUS] __cacheline_aligned;
EXPORT_SYMBOL(cpu_sibling_map);
cpumask_t cpu_core_map[NR_CPUS] __cacheline_aligned;
EXPORT_SYMBOL(cpu_core_map);

#if defined(__i386__)
u8 x86_cpu_to_apicid[NR_CPUS] = { [0 ... NR_CPUS-1] = 0xff };
EXPORT_SYMBOL(x86_cpu_to_apicid);
#endif

void __init prefill_possible_map(void)
{
	int i, rc;

	for (i = 0; i < NR_CPUS; i++) {
		rc = HYPERVISOR_vcpu_op(VCPUOP_is_up, i, NULL);
		if (rc >= 0)
			cpu_set(i, cpu_possible_map);
	}
}

void __init smp_alloc_memory(void)
{
}

static inline void
set_cpu_sibling_map(int cpu)
{
	cpu_data[cpu].phys_proc_id = cpu;
	cpu_data[cpu].cpu_core_id = 0;

	cpu_sibling_map[cpu] = cpumask_of_cpu(cpu);
	cpu_core_map[cpu]    = cpumask_of_cpu(cpu);

	cpu_data[cpu].booted_cores = 1;
}

static void xen_smp_intr_init(unsigned int cpu)
{
	sprintf(resched_name[cpu], "resched%d", cpu);
	per_cpu(resched_irq, cpu) =
		bind_ipi_to_irqhandler(
			RESCHEDULE_VECTOR,
			cpu,
			smp_reschedule_interrupt,
			SA_INTERRUPT,
			resched_name[cpu],
			NULL);
	BUG_ON(per_cpu(resched_irq, cpu) < 0);

	sprintf(callfunc_name[cpu], "callfunc%d", cpu);
	per_cpu(callfunc_irq, cpu) =
		bind_ipi_to_irqhandler(
			CALL_FUNCTION_VECTOR,
			cpu,
			smp_call_function_interrupt,
			SA_INTERRUPT,
			callfunc_name[cpu],
			NULL);
	BUG_ON(per_cpu(callfunc_irq, cpu) < 0);

	if (cpu != 0)
		local_setup_timer(cpu);
}

#ifdef CONFIG_HOTPLUG_CPU
static void xen_smp_intr_exit(unsigned int cpu)
{
	if (cpu != 0)
		local_teardown_timer(cpu);

	unbind_from_irqhandler(per_cpu(resched_irq, cpu), NULL);
	unbind_from_irqhandler(per_cpu(callfunc_irq, cpu), NULL);
}
#endif

#ifdef __i386__
static inline void set_kernel_gs(void)
{
	/* Set %gs for this CPU's PDA.  Memory clobber is to create a
	   barrier with respect to any PDA operations, so the compiler
	   doesn't move any before here. */
	asm volatile ("mov %0, %%gs" : : "r" (__KERNEL_PDA) : "memory");
}
#endif

void cpu_bringup(void)
{
#ifdef __i386__
	set_kernel_gs();
	secondary_cpu_init();
#else
	cpu_init();
#endif
	touch_softlockup_watchdog();
	preempt_disable();
	local_irq_enable();
}

static void cpu_bringup_and_idle(void)
{
	cpu_bringup();
	cpu_idle();
}

void cpu_initialize_context(unsigned int cpu)
{
	vcpu_guest_context_t ctxt;
	struct task_struct *idle = idle_task(cpu);
#ifdef __x86_64__
	struct desc_ptr *gdt_descr = &cpu_gdt_descr[cpu];
#else
	struct Xgt_desc_struct *gdt_descr = &per_cpu(cpu_gdt_descr, cpu);
#endif

	if (cpu == 0)
		return;

	memset(&ctxt, 0, sizeof(ctxt));

	ctxt.flags = VGCF_IN_KERNEL;
	ctxt.user_regs.ds = __USER_DS;
	ctxt.user_regs.es = __USER_DS;
	ctxt.user_regs.fs = 0;
	ctxt.user_regs.gs = 0;
	ctxt.user_regs.ss = __KERNEL_DS;
	ctxt.user_regs.eip = (unsigned long)cpu_bringup_and_idle;
	ctxt.user_regs.eflags = X86_EFLAGS_IF | 0x1000; /* IOPL_RING1 */

	memset(&ctxt.fpu_ctxt, 0, sizeof(ctxt.fpu_ctxt));

	smp_trap_init(ctxt.trap_ctxt);

	ctxt.ldt_ents = 0;

	ctxt.gdt_frames[0] = virt_to_mfn(gdt_descr->address);
	ctxt.gdt_ents      = gdt_descr->size / 8;

#ifdef __i386__
	ctxt.user_regs.cs = __KERNEL_CS;
	ctxt.user_regs.esp = idle->thread.esp0 - sizeof(struct pt_regs);

	ctxt.kernel_ss = __KERNEL_DS;
	ctxt.kernel_sp = idle->thread.esp0;

	ctxt.event_callback_cs     = __KERNEL_CS;
	ctxt.event_callback_eip    = (unsigned long)hypervisor_callback;
	ctxt.failsafe_callback_cs  = __KERNEL_CS;
	ctxt.failsafe_callback_eip = (unsigned long)failsafe_callback;

	ctxt.ctrlreg[3] = xen_pfn_to_cr3(virt_to_mfn(swapper_pg_dir));
#else /* __x86_64__ */
	ctxt.user_regs.cs = __KERNEL_CS;
	ctxt.user_regs.esp = idle->thread.rsp0 - sizeof(struct pt_regs);

	ctxt.kernel_ss = __KERNEL_DS;
	ctxt.kernel_sp = idle->thread.rsp0;

	ctxt.event_callback_eip    = (unsigned long)hypervisor_callback;
	ctxt.failsafe_callback_eip = (unsigned long)failsafe_callback;
	ctxt.syscall_callback_eip  = (unsigned long)system_call;

	ctxt.ctrlreg[3] = xen_pfn_to_cr3(virt_to_mfn(init_level4_pgt));

	ctxt.gs_base_kernel = (unsigned long)(cpu_pda(cpu));
#endif

	BUG_ON(HYPERVISOR_vcpu_op(VCPUOP_initialise, cpu, &ctxt));
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int cpu;
	struct task_struct *idle;
#ifdef __x86_64__
	struct desc_ptr *gdt_descr;
#else
	struct Xgt_desc_struct *gdt_descr;
#endif

	boot_cpu_data.apicid = 0;
	cpu_data[0] = boot_cpu_data;

	cpu_2_logical_apicid[0] = 0;
	x86_cpu_to_apicid[0] = 0;

	current_thread_info()->cpu = 0;

	for (cpu = 0; cpu < NR_CPUS; cpu++) {
		cpus_clear(cpu_sibling_map[cpu]);
		cpus_clear(cpu_core_map[cpu]);
	}

	set_cpu_sibling_map(0);

	xen_smp_intr_init(0);

	/* Restrict the possible_map according to max_cpus. */
	while ((num_possible_cpus() > 1) && (num_possible_cpus() > max_cpus)) {
		for (cpu = NR_CPUS-1; !cpu_isset(cpu, cpu_possible_map); cpu--)
			continue;
		cpu_clear(cpu, cpu_possible_map);
	}

	for_each_possible_cpu (cpu) {
#ifdef __i386__
		struct i386_pda *pda;
		struct desc_struct *gdt;
#endif

		if (cpu == 0)
			continue;

#ifdef __x86_64__
		gdt_descr = &cpu_gdt_descr[cpu];
#else
		gdt_descr = &per_cpu(cpu_gdt_descr, cpu);
#endif
		gdt_descr->address = get_zeroed_page(GFP_KERNEL);
		if (unlikely(!gdt_descr->address)) {
			printk(KERN_CRIT "CPU%d failed to allocate GDT\n",
			       cpu);
			continue;
		}
		gdt_descr->size = GDT_SIZE;
		memcpy((void *)gdt_descr->address, cpu_gdt_table, GDT_SIZE);
#ifdef __i386__
		gdt = (struct desc_struct *)gdt_descr->address;
		pda = kmalloc_node(sizeof(*pda), GFP_KERNEL, cpu_to_node(cpu));

		if (unlikely(!pda)) {
			printk(KERN_CRIT "CPU%d failed to allocate PDA\n",
			       cpu);
			continue;
		}
		cpu_pda(cpu) = pda;
		cpu_pda(cpu)->cpu_number = cpu;
		pack_descriptor((u32 *)&gdt[GDT_ENTRY_PDA].a,
				(u32 *)&gdt[GDT_ENTRY_PDA].b,
				(unsigned long)pda, sizeof(*pda) - 1,
				0x80 | DESCTYPE_S | 0x2, 0); /* present read-write data segment */
#endif
		make_page_readonly(
			(void *)gdt_descr->address,
			XENFEAT_writable_descriptor_tables);

		cpu_data[cpu] = boot_cpu_data;
		cpu_data[cpu].apicid = cpu;

		cpu_2_logical_apicid[cpu] = cpu;
		x86_cpu_to_apicid[cpu] = cpu;

		idle = fork_idle(cpu);
		if (IS_ERR(idle))
			panic("failed fork for CPU %d", cpu);

		cpu_pda(cpu)->pcurrent = idle;
#ifdef __x86_64__
		cpu_pda(cpu)->cpunumber = cpu;
		clear_ti_thread_flag(idle->thread_info, TIF_FORK);
#endif

		irq_ctx_init(cpu);

#ifdef CONFIG_HOTPLUG_CPU
		if (is_initial_xendomain())
			cpu_set(cpu, cpu_present_map);
#else
		cpu_set(cpu, cpu_present_map);
#endif

		cpu_initialize_context(cpu);
	}

	init_xenbus_allowed_cpumask();

	/*
	 * Here we can be sure that there is an IO-APIC in the system. Let's
	 * go and set it up:
	 */
#ifdef CONFIG_X86_IO_APIC
	if (!skip_ioapic_setup && nr_ioapics)
		setup_IO_APIC();
#endif
}

void __init smp_prepare_boot_cpu(void)
{
}

#ifdef CONFIG_HOTPLUG_CPU

/*
 * Initialize cpu_present_map late to skip SMP boot code in init/main.c.
 * But do it early enough to catch critical for_each_present_cpu() loops
 * in i386-specific code.
 */
static int __init initialize_cpu_present_map(void)
{
	cpu_present_map = cpu_possible_map;
	return 0;
}
core_initcall(initialize_cpu_present_map);

static void
remove_siblinginfo(int cpu)
{
	cpu_data[cpu].phys_proc_id = BAD_APICID;
	cpu_data[cpu].cpu_core_id = BAD_APICID;

	cpus_clear(cpu_sibling_map[cpu]);
	cpus_clear(cpu_core_map[cpu]);

	cpu_data[cpu].booted_cores = 0;
}

int __cpu_disable(void)
{
	cpumask_t map = cpu_online_map;
	int cpu = smp_processor_id();

	if (cpu == 0)
		return -EBUSY;

	remove_siblinginfo(cpu);

	cpu_clear(cpu, map);
	fixup_irqs(map);
	cpu_clear(cpu, cpu_online_map);

	return 0;
}

void __cpu_die(unsigned int cpu)
{
	while (HYPERVISOR_vcpu_op(VCPUOP_is_up, cpu, NULL)) {
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout(HZ/10);
	}

	xen_smp_intr_exit(cpu);

	if (num_online_cpus() == 1)
		alternatives_smp_switch(0);
}

#else /* !CONFIG_HOTPLUG_CPU */

int __cpu_disable(void)
{
	return -ENOSYS;
}

void __cpu_die(unsigned int cpu)
{
	BUG();
}

#endif /* CONFIG_HOTPLUG_CPU */

int __cpuinit __cpu_up(unsigned int cpu)
{
	int rc;

	rc = cpu_up_check(cpu);
	if (rc)
		return rc;

	if (num_online_cpus() == 1)
		alternatives_smp_switch(1);

	/* This must be done before setting cpu_online_map */
	set_cpu_sibling_map(cpu);
	wmb();

	xen_smp_intr_init(cpu);
	cpu_set(cpu, cpu_online_map);

	rc = HYPERVISOR_vcpu_op(VCPUOP_up, cpu, NULL);
	BUG_ON(rc);

	return 0;
}

void __init smp_cpus_done(unsigned int max_cpus)
{
}

#ifdef CONFIG_X86_MPPARSE
/*
 * If the BIOS enumerates physical processors before logical,
 * maxcpus=N at enumeration-time can be used to disable HT.
 */
static int __init parse_maxcpus(char *arg)
{
	extern unsigned int maxcpus;

	maxcpus = simple_strtoul(arg, NULL, 0);
	return 0;
}
early_param("maxcpus", parse_maxcpus);
#endif

#if defined(CONFIG_XEN_UNPRIVILEGED_GUEST) && defined(CONFIG_X86_32)
int setup_profiling_timer(unsigned int multiplier)
{
	return -EINVAL;
}
#endif