/*
 * arch/ppc/platforms/dmv182p.c
 * Setup code for the Dy-4 SVME/DMV-182
 *
 * Copyright (C) 2004 TimeSys Corporation
 * Copyright (C) 2004 Red Hat, Inc.
 *
 * Original 2.4 port by Scott Wood <scott.wood@timesys.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/config.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/initrd.h>
#include <linux/root_dev.h>
#include <linux/delay.h>

#include <asm/serial.h>
#include <asm/bootinfo.h>
#include <asm/machdep.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/mv64x60.h>
#include <asm/processor.h>
#include <asm/time.h>
#include <asm/atomic.h>
#include <asm/bitops.h>
#include <asm/todc.h>
#include <linux/tty.h>  /* for linux/serial_core.h */
#include <linux/serial.h>
#include <linux/serial_core.h>

#include "dmv182.h"

extern int mv64360_get_irq(struct pt_regs *regs);
extern void mv64360_init_irq(void);

extern void gen550_progress(char *s, unsigned short hex);
extern void gen550_init(int, struct uart_port *);

static void __init dmv182_setup_peripherals(void);
static void __init dmv182_setup_bridge(void);

static struct mv64x60_handle bh;

TODC_ALLOC();

static void __init dmv182_map_io(void)
{
	io_block_mapping((unsigned long)dmv182_board_io_virt,
	                 dmv182_board_io_phys, 0x10000000, _PAGE_IO);
}

// This sets up BAT3 to cover the serial port and Discovery chip.

static void __init dmv182_setup_bats(void)
{
	int tmp1, tmp2;

	asm volatile("lis %0, 0xe000;"
	             "ori %0, %0, 0x002a;"
	             "lis %1, 0xf000;"
	             "ori %1, %1, 0x1ffe;"
	             "mtspr %2, %0;"
	             "mtspr %3, %1"
	           : "=r" (tmp1), "=r" (tmp2)
	           : "i" (DBAT3L), "i" (DBAT3U)
	           : "memory");
}

static u8 *const irqstat = dmv182_fpga_io + 0x80;
static u8 *const irqmask = dmv182_fpga_io + 0x81;

// These two functions transform an IRQ number into
// byte and bit indices into the above arrays.

static inline int irqreg(unsigned int irq)
{
	return ((irq - 96) >> 3) * 3;
}

static inline int irqbit(unsigned int irq)
{
	return (irq - 96) & 7;
}

// FIXME: CPU1 and affinity support
// The Marvell code doesn't appear to support anything
// other than doorbells on CPU1 at the moment.

static void dmv182_mask_irq(unsigned int irq)
{
	irqmask[irqreg(irq)] &= ~(1 << irqbit(irq));
}

static void dmv182_unmask_irq(unsigned int irq)
{
	irqmask[irqreg(irq)] |= 1 << irqbit(irq);
}

static unsigned int dmv182_startup_irq(unsigned int irq)
{
	dmv182_unmask_irq(irq);
	return 0;
}

struct hw_interrupt_type dmv182_pic = {
	.typename     = " DMV182_PIC ",
	.startup      = dmv182_startup_irq,
	.shutdown     = dmv182_mask_irq,
	.enable       = dmv182_unmask_irq,
	.disable      = dmv182_mask_irq,
	.ack          = dmv182_mask_irq,
	.end          = dmv182_unmask_irq,
	.set_affinity = NULL
};

atomic_t spurious_interrupts;

static irqreturn_t dmv182_cascade(int irq, void *dev_id, struct pt_regs *regs)
{
	int i, j;
	int cpu = smp_processor_id();
	int irqs;

	for (i = 0, j = 96; i < 24; i += 3, j += 8) {
		irqs = irqstat[i] & irqmask[i + cpu];
		
		if (irqs)
			break;
	}
	
	if (i < 24) {
		ppc_irq_dispatch_handler(regs, j + ffs(irqs) - 1);
		return IRQ_HANDLED;
	}

	atomic_inc(&spurious_interrupts);
	return IRQ_NONE;
}

#ifdef CONFIG_SMP
static irqreturn_t dmv182_doorbell(int irq, void *dev_id, struct pt_regs *regs);
#endif

static void __init dmv182_init_irq(void)
{
	int i;

	if (ppc_md.progress)
		ppc_md.progress("dmv182_init_irq", 0x1821);
	
	for (i = 96; i < 160; i++) {
		dmv182_mask_irq(i);
		irqmask[irqreg(i) + 1] &= ~(1 << irqbit(i));
		irq_desc[i].handler = &dmv182_pic;
		irq_desc[i].status = IRQ_LEVEL | IRQ_DISABLED;
	}
	
	mv64360_init_irq();
	
	if (request_irq(94, dmv182_cascade, SA_INTERRUPT,
	                "DMV182 CPU0 cascade", NULL) < 0)
	{
		panic("Could not request CPU0 cascade IRQ\n");
	}

#ifdef CONFIG_SMP
#if 0
	if (request_irq(95, dmv182_cascade, SA_INTERRUPT,
	                "DMV182 CPU1 cascade", NULL) < 0)
	{
		panic("Could not request CPU1 cascade IRQ\n");
	}
#endif

	if (request_irq(60, dmv182_doorbell, SA_INTERRUPT,
	                "CPU0 doorbell", NULL) < 0)
	{
		panic("Could not request CPU1 doorbell IRQ\n");
	}

	if (request_irq(28, dmv182_doorbell, SA_INTERRUPT,
	                "CPU1 doorbell", NULL) < 0)
	{
		panic("Could not request CPU1 doorbell IRQ\n");
	}
	
	// Clear and unmask all doorbell interrupts.
	
	mv64x60_write(&bh, MV64360_CPU0_DOORBELL_CLR, 0xff);
	mv64x60_write(&bh, MV64360_CPU1_DOORBELL_CLR, 0xff);
	mv64x60_write(&bh, MV64360_CPU0_DOORBELL_MASK, 0xff);
	mv64x60_write(&bh, MV64360_CPU1_DOORBELL_MASK, 0xff);
#endif
}

// It's really device numbers, not idsels, but we have
// to call it that so the PCI_IRQ_TABLE_LOOKUP will work.

static int __init dmv182_map_irq(struct pci_dev *dev,
                                  unsigned char idsel,
                                  unsigned char pin)
{
	struct pci_controller *hose = pci_bus_to_hose(dev->bus->number);

#if 0
	printk("map irq: hose %d, bus %d, slot %d, first %d\n", hose->index,
	       dev->bus->number, idsel, hose->first_busno);
#endif

	if (hose->index != 0 && hose->index != 1) {
		printk(KERN_ERR "map_irq: unknown hose %d\n", hose->index);
		return 0;
	}
	
	// Some of this is guesswork...
	// In particular, I don't know if the ABCD mappings are right,
	// and I don't know which IPM goes with which slot (the manual
	// merely says "IPM" for both).

	if (hose->index == 0) {
		static u8 pci_irq_table[][4] = 
		/*
		 * PCI IDSEL/INTPIN->INTLINE
		 *  A   B   C   D
		 */
		{
			{ DMV182_IRQ_PMC1A, DMV182_IRQ_PMC1B,
			  DMV182_IRQ_PMC1C, DMV182_IRQ_PMC1D }, // PMC Slot 1 A
			{ DMV182_IRQ_PMC1A, DMV182_IRQ_PMC1B,
			  DMV182_IRQ_PMC1C, DMV182_IRQ_PMC1D }, // PMC Slot 1 B
		};
		
		const int min_idsel = 4, max_idsel = 5, irqs_per_slot = 4;
 		return PCI_IRQ_TABLE_LOOKUP;
	} else if (dev->bus->parent && dev->bus->primary == hose->first_busno &&
	           dev->bus->self->devfn == 0x10) {
		static u8 pci_irq_table[][4] = 
		/*
		 * PCI IDSEL/INTPIN->INTLINE
		 *  A   B   C   D
		 */
		{
			{ DMV182_IRQ_IPM0, DMV182_IRQ_IPM0,
			  DMV182_IRQ_IPM0, DMV182_IRQ_IPM0 }, // IPM... 0?
			{ DMV182_IRQ_IPM1, DMV182_IRQ_IPM1,
			  DMV182_IRQ_IPM1, DMV182_IRQ_IPM1 }, // IPM... 1?
			{ DMV182_IRQ_USB_A, DMV182_IRQ_USB_B,
			  DMV182_IRQ_USB_C, DMV182_IRQ_USB_SMI }, // USB
			{ DMV182_IRQ_VME_CPU0, DMV182_IRQ_VME_CPU1, 0, 0 }, // VME
		};
		
		const int min_idsel = 1, max_idsel = 4, irqs_per_slot = 4;
		return PCI_IRQ_TABLE_LOOKUP;
	} else {
		static u8 pci_irq_table[][4] = 
		/*
		 * PCI IDSEL/INTPIN->INTLINE
		 *  A   B   C   D
		 */
		{
			{ DMV182_IRQ_PMC2A, DMV182_IRQ_PMC2B,
			  DMV182_IRQ_PMC2C, DMV182_IRQ_PMC2D }, // PMC Slot 2 A
			{ DMV182_IRQ_PMC2A, DMV182_IRQ_PMC2B,
			  DMV182_IRQ_PMC2C, DMV182_IRQ_PMC2D }, // PMC Slot 2 B
		};
		
		const int min_idsel = 4, max_idsel = 5, irqs_per_slot = 4;
		return PCI_IRQ_TABLE_LOOKUP;
	}
}

static unsigned char dmv182_pci_swizzle(struct pci_dev *dev,
                                         unsigned char *pinp)
{
	struct pci_controller *hose = dev->sysdata;

	// The devices under this particular bridge have their IRQs
	// directly routed to the PIC, rather than through the parent
	// bus.  Thus, don't swizzle them.  The bus is determined by
	// the devfn of the parent, rather than its own bus number,
	// in case a PMC card is added that has its own bridge(s),
	// causing the numbering to change.
	
	if (hose->index == 1 && dev->bus->parent &&
	    dev->bus->primary == hose->first_busno &&
	    dev->bus->self->devfn == 0x10)
		return PCI_SLOT(dev->devfn);
	
	return common_swizzle(dev, pinp);
}

static unsigned long __init
dmv182_pci_bridge_reserve_space(struct pci_controller *hose,
                                 unsigned char bus, unsigned char devfn)
{
	// Reserve 768 MiB for the bus containing VME.  This
	// will allow one to map the entire RAM of a 512 MiB
	// card over VME, while still allowing space for other
	// stuff on the bridge.
	if (hose->first_busno == bus && devfn == 0x10)
		return 0x30000000;
	
	return 0;
}

static void __init dmv182_setup_caches(void)
{
#if 0 // This actually causes the TimeSys 2.4 port to blow up too, for me 

	// Why can't L2CR be set by generic 745x code?
	// And what's with the underscore?
	_set_L2CR(0xc0000000);

	_set_L3CR(0x9e8a0180);
#endif
}

#ifdef CONFIG_SERIAL_8250
static void __init dmv182_early_serial_map(void)
{
        struct uart_port uart_req;
	void *membase = ioremap(0xe0010000, PAGE_SIZE);

        /* Setup serial port access */
        memset(&uart_req, 0, sizeof (uart_req));
        uart_req.irq = DMV182_IRQ_SERIAL_CH1;
        uart_req.flags = 0;
	uart_req.type = PORT_16550;
        uart_req.uartclk = BASE_BAUD * 16;
        uart_req.iotype = SERIAL_IO_MEM;
        uart_req.mapbase = (unsigned long)dmv182_fpga_io + 0x18;
        uart_req.membase = membase + 0x18;

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
        gen550_init(0, &uart_req);
#endif

        if (early_serial_setup(&uart_req) != 0)
                printk("Early serial init of port 0 failed\n");

        /* Assume early_serial_setup() doesn't modify uart_req */
        uart_req.line = 1;
        uart_req.mapbase = (unsigned long)dmv182_fpga_io + 0x20;
        uart_req.membase = membase + 0x20;
        uart_req.irq = DMV182_IRQ_SERIAL_CH2;

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
        gen550_init(1, &uart_req);
#endif

        if (early_serial_setup(&uart_req) != 0)
                printk("Early serial init of port 1 failed\n");
}
#endif

static void __init dmv182_setup_arch(void)
{
	if (ppc_md.progress)
		ppc_md.progress("dmv182_setup_arch", 0x1820);

	ppc_md.pci_swizzle = dmv182_pci_swizzle;

	dmv182_setup_caches();

	// Enable snooping.
//	MV_SET_REG_BITS(MV64360_CPU_MASTER_CONTROL, (1 << 12) | (1 << 13));

	// Set up the RTC.
	dmv182_setup_bridge();
	dmv182_setup_peripherals();

#ifdef CONFIG_SERIAL_8250
	dmv182_early_serial_map();
#endif
	if (ppc_md.progress)
		ppc_md.progress("dmv182_setup_arch end", 0x182f);
}

static void __init dmv182_calibrate_decr(void)
{
	if (ppc_md.progress)
		ppc_md.progress("dmv182_calibrate_decr", 0x1822);
	
	tb_ticks_per_jiffy = 25000000 / HZ;
	tb_to_us = mulhwu_scale_factor(25000000, 1000000);
}

static void dmv182_halt(void)
{
	local_irq_disable();
	for(;;);
}

static void dmv182_restart(char *cmd)
{
	unsigned long reg;
	volatile unsigned long *ptr = NULL;
	struct pci_dev *dev;

	local_irq_disable();

	/* 
	 * The best way to reset the board is through the Universe VME. 
	 * Since the VME driver may or may not be loaded, we can't rely
	 * on that, so the best way I can think of in resetting the board
	 * is to search all the PCI devices looking for the Universe chip 
	 * and write to its command register to reset the board.
	 */
	dev = pci_find_device(PCI_VENDOR_ID_TUNDRA, 0, NULL);
	if (dev) {
		printk("Found VME device %s\n",dev->slot_name);

		for (reg = 0; reg < 6; reg++) {
			struct resource *res = dev->resource + reg;
			if ((res->flags & PCI_BASE_ADDRESS_SPACE) ==
			    PCI_BASE_ADDRESS_SPACE_MEMORY) {
				ptr = ioremap(res->start + 0x404, sizeof(ptr)); /* CTRL_REG */
				break;
			}
		}
	}

	if (!ptr) {
		printk("No VME device found to reset board\n");
		return;
	}

	printk("**** resetting board through VME ****\n");
	mdelay(10);

	reg = *ptr;
	reg |= 0x8000; /* reset only the board and not the entire chassis. */
	*ptr = reg;

	for(;;);
}

void board_get_mac(int port, u8 *addr)
{
	if (port < 1 || port > 2) {
		printk(KERN_ERR "Unknown port %d in board_get_mac()...\n", port);
		return;
	}
	
	memcpy(addr, (u8 *)dmv182_nvram + 8 + (2 - port) * 6, 6);
	printk(KERN_NOTICE "Ethernet port %d MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
	       port, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

#ifdef CONFIG_SMP

static int dmv182_smp_probe(void)
{
	return 2;
}

void __secondary_start(void);

static void dmv182_kick_cpu(int cpu)
{
	BUG_ON(cpu != 1);

	*(u32 *)(PAGE_OFFSET + 4) = (u32)__secondary_start - PAGE_OFFSET;
	wmb();
	*(u32 *)(PAGE_OFFSET + 0) = 0x38a3fd19;
	wmb();

	/* Set MaskBR1 to allow CPU1 to get access to the bus. */
	mv64x60_modify(&bh, MV64x60_CPU_MASTER_CNTL, 0, 1<<9);
}

static void dmv182_setup_cpu(int cpu)
{
	int whoami = mv64x60_read(&bh, MV64360_WHO_AM_I);
	
	if (cpu != whoami) {
		printk("CPU %d whoami %d\n", cpu, whoami);
		BUG();
	}

	// Enable broadcasting of synchronization and cache/tlb
	// flushing/invalidation instructions
	
	mtspr(SPRN_HID1, mfspr(SPRN_HID1) | HID1_ABE | HID1_SYNCBE);
	asm volatile("sync; isync" : : : "memory");

	if (cpu == 1)
		dmv182_setup_caches();
}

static void dmv182_message_pass(int target, int msg, ulong data, int wait)
{
	int i;
	int reg;
	
	if (unlikely(msg < 0 || msg > 7)) {
		printk(KERN_ERR "dmv182_message_pass: bad message %x\n", msg);
		return;
	}
	
	for_each_online_cpu(i) {
		reg = MV64360_CPUx_DOORBELL(i);

		if (target == MSG_ALL ||
		    (target == MSG_ALL_BUT_SELF && i != smp_processor_id()) ||
		    target == i)
			mv64x60_modify(&bh, reg, 1 << msg, 1 << msg);
	}
}

static irqreturn_t dmv182_doorbell(int irq, void *dev_id, struct pt_regs *regs)
{
	u32 bits = mv64x60_read(&bh, MV64360_CPUx_DOORBELL(smp_processor_id()));

	bits &= 0xff;

	mv64x60_write(&bh, MV64360_CPU0_DOORBELL_CLR +
		      smp_processor_id() * 0x10, bits);

	while (bits) {
		int msg = __ilog2(bits);;
		smp_message_recv(msg, regs);
		bits &= ~(1 << msg);
	}
	return IRQ_HANDLED;
}

static struct smp_ops_t dmv182_smp_ops = {
	.probe        = dmv182_smp_probe,
	.kick_cpu     = dmv182_kick_cpu,
	.setup_cpu    = dmv182_setup_cpu,
	.message_pass = dmv182_message_pass,
	.give_timebase = smp_generic_give_timebase,
	.take_timebase = smp_generic_take_timebase,
};

#endif

static void __init dmv182_setup_bridge(void)
{
	mv64x60_setup_info_t si;

	memset(&si, 0, sizeof(si));

	si.phys_reg_base = CONFIG_MV64X60_NEW_BASE;
	si.map_irq = dmv182_map_irq;

	si.pci_0.enable_bus = 1;
	si.pci_0.enumerate_bus = 1;
	si.pci_0.pci_io.cpu_base = 0xa0000000;
	si.pci_0.pci_io.pci_base_hi = 0;
	si.pci_0.pci_io.pci_base_lo = 0;
	si.pci_0.pci_io.size = 0x01000000;
	si.pci_0.pci_io.swap = 0x01000000; /* XXXX No swapping */
	si.pci_0.pci_mem[0].cpu_base = 0x80000000;
	si.pci_0.pci_mem[0].pci_base_hi = 0;
	si.pci_0.pci_mem[0].pci_base_lo = 0x80000000;
	si.pci_0.pci_mem[0].size = 0x10000000;
	si.pci_0.pci_mem[0].swap = 0x01000000; /* XXXX No swapping */
	si.pci_0.pci_mem[1].cpu_base = 0;
	si.pci_0.pci_mem[1].pci_base_hi = 0;
	si.pci_0.pci_mem[1].pci_base_lo = 0;
	si.pci_0.pci_mem[1].size = 0;	/* Don't use this window */
	si.pci_0.pci_mem[1].swap = 0;
	si.pci_0.pci_mem[2].cpu_base = 0;
	si.pci_0.pci_mem[2].pci_base_hi = 0;
	si.pci_0.pci_mem[2].pci_base_lo = 0;
	si.pci_0.pci_mem[2].size = 0;	/* Don't use this window */
	si.pci_0.pci_mem[1].swap = 0;
	si.pci_0.pci_cmd_bits = 0;
	si.pci_0.latency_timer = 0x8;

	si.pci_1.enable_bus = 1;
	si.pci_1.enumerate_bus = 1;
	si.pci_1.pci_io.cpu_base = 0xa1000000;
	si.pci_1.pci_io.pci_base_hi = 0;
	si.pci_1.pci_io.pci_base_lo = 0x01000000;
	si.pci_1.pci_io.size = 0x01000000;
	si.pci_1.pci_io.swap = 0x01000000; /* XXXX No swapping */
	si.pci_1.pci_mem[0].cpu_base = 0x90000000;
	si.pci_1.pci_mem[0].pci_base_hi = 0;
	si.pci_1.pci_mem[0].pci_base_lo = 0x90000000;
	si.pci_1.pci_mem[0].size = 0x10000000;
	si.pci_1.pci_mem[0].swap = 0x01000000; /* XXXX No swapping */
	si.pci_1.pci_mem[1].cpu_base = 0;
	si.pci_1.pci_mem[1].pci_base_hi = 0;
	si.pci_1.pci_mem[1].pci_base_lo = 0;
	si.pci_1.pci_mem[1].size = 0;	/* Don't use this window */
	si.pci_1.pci_mem[1].swap = 0;
	si.pci_1.pci_mem[2].cpu_base = 0;
	si.pci_1.pci_mem[2].pci_base_hi = 0;
	si.pci_1.pci_mem[2].pci_base_lo = 0;
	si.pci_1.pci_mem[2].size = 0;	/* Don't use this window */
	si.pci_1.pci_mem[1].swap = 0;
	si.pci_1.pci_cmd_bits = 0;
	si.pci_1.latency_timer = 0x8;
	si.pci_1.pci_cmd_bits = 0;
	si.pci_1.latency_timer = 0x8;

	si.window_preserve_mask_32 = 0x1f0;
#if 0
	for (i=0; i<MV64x60_CPU2MEM_WINDOWS; i++) {
		si.cpu_prot_options[i] = 0;
//		si.cpu_snoop_options[i] = GT64260_CPU_SNOOP_WB;
		si.pci_0.acc_cntl_options[i] =
			/* Breaks PCI (especially slot 4)
			GT64260_PCI_ACC_CNTL_PREFETCHEN |
			*/
			GT64260_PCI_ACC_CNTL_DREADEN |
			GT64260_PCI_ACC_CNTL_RDPREFETCH |
			GT64260_PCI_ACC_CNTL_RDLINEPREFETCH |
			GT64260_PCI_ACC_CNTL_RDMULPREFETCH |
			GT64260_PCI_ACC_CNTL_SWAP_NONE |
			GT64260_PCI_ACC_CNTL_MBURST_32_BTYES;
		si.pci_0.snoop_options[i] = GT64260_PCI_SNOOP_WB;
		si.pci_1.acc_cntl_options[i] =
			/* Breaks PCI (especially slot 4)
			GT64260_PCI_ACC_CNTL_PREFETCHEN |
			*/
			GT64260_PCI_ACC_CNTL_DREADEN |
			GT64260_PCI_ACC_CNTL_RDPREFETCH |
			GT64260_PCI_ACC_CNTL_RDLINEPREFETCH |
			GT64260_PCI_ACC_CNTL_RDMULPREFETCH |
			GT64260_PCI_ACC_CNTL_SWAP_NONE |
			GT64260_PCI_ACC_CNTL_MBURST_32_BTYES;
//		si.pci_1.snoop_options[i] = GT64260_PCI_SNOOP_WB;
	}
#endif

	mv64x60_pci_exclude_bridge = 0;

        /* Lookup PCI host bridges */
        if (mv64x60_init(&bh, &si)) {
                printk("Bridge initialization failed.\n");
        }

	return;
}

static void __init dmv182_setup_peripherals(void)
{
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2BOOT_WIN,
				 0xf0000000, 0x08000000, 0); // FLASH
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_0_WIN,
				 0xe0010000, 0x10000, 0); // I/O FPGA
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_1_WIN,
				 0xe0000000, 0x10000, 0); // EPLD
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_2_WIN,
				 0xe0020000, 0x10000, 0); // RTC
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_3_WIN,
				 0xe0030000, 0x10000, 0); // NVRAM

	TODC_INIT(TODC_TYPE_DS1501, 0, 0, dmv182_rtc, 8);
}

unsigned long __init dmv182_find_end_of_memory(void)
{
#if 0
	return mv64x60_get_mem_size(0xfff00000 /*CONFIG_MV64X60_NEW_BASE*/,
						  MV64x60_TYPE_MV64360);
#endif
	/* But it dies if we enable more than 512MiB. Debug later... */
	return 0x20000000;
}

void __init platform_init(unsigned long r3, unsigned long r4,
                          unsigned long r5, unsigned long r6,
                          unsigned long r7)
{
	parse_bootinfo(find_bootinfo());
	
	dmv182_setup_bats();

#if defined(CONFIG_SERIAL_TEXT_DEBUG)
	ppc_md.progress = gen550_progress;
#endif
	ppc_md.setup_io_mappings = dmv182_map_io;
	ppc_md.find_end_of_memory = dmv182_find_end_of_memory;
	ppc_md.setup_arch = dmv182_setup_arch;
	ppc_md.init_IRQ = dmv182_init_irq;
	ppc_md.get_irq = mv64360_get_irq;
	ppc_md.calibrate_decr = dmv182_calibrate_decr;
//	ppc_md.pci_bridge_reserve_space = dmv182_pci_bridge_reserve_space;
	
	ppc_md.halt = dmv182_halt;
	ppc_md.power_off = dmv182_halt;
	ppc_md.restart = dmv182_restart;
#ifdef CONFIG_SMP
 	ppc_md.smp_ops = &dmv182_smp_ops;
#endif
#ifdef CONFIG_GEN_RTC
	ppc_md.time_init = todc_time_init;
	ppc_md.set_rtc_time = todc_set_rtc_time;
	ppc_md.get_rtc_time = todc_get_rtc_time;

	ppc_md.nvram_read_val = todc_direct_read_val;
	ppc_md.nvram_write_val = todc_direct_write_val;
#endif
}
