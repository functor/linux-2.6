/*
 * arch/ppc/platforms/ev64260.c
 *
 * Board setup routines for the Marvell/Galileo EV-64260-BP Evaluation Board.
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *
 * 2001-2003 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

/*
 * The EV-64260-BP port is the result of hard work from many people from
 * many companies.  In particular, employees of Marvell/Galileo, Mission
 * Critical Linux, Xyterra, and MontaVista Software were heavily involved.
 *
 * Note: I have not been able to get *all* PCI slots to work reliably
 *	at 66 MHz.  I recommend setting jumpers J15 & J16 to short pins 1&2
 *	so that 33 MHz is used. --MAG
 * Note: The 750CXe and 7450 are not stable with a 125MHz or 133MHz TCLK/SYSCLK.
 * 	At 100MHz, they are solid.
 */
#include <linux/config.h>

#include <linux/delay.h>
#include <linux/pci.h>
#include <linux/ide.h>
#include <linux/irq.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/console.h>
#include <linux/initrd.h>
#include <linux/root_dev.h>
#if !defined(CONFIG_SERIAL_MPSC_CONSOLE)
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/serial_core.h>
#endif
#include <asm/bootinfo.h>
#include <asm/machdep.h>
#include <asm/mv64x60.h>
#include <asm/ppcboot.h>
#include <asm/todc.h>
#include <asm/time.h>
#include <asm/ocp.h>

#include <platforms/ev64260.h>

#define BOARD_VENDOR	"Marvell/Galileo"
#define BOARD_MACHINE	"EV-64260-BP"

/* Set IDE controllers into Native mode? */
/* XXXX
#define SET_PCI_IDE_NATIVE
*/

ulong	ev64260_mem_size = 0;
bd_t	ppcboot_bd;
int	ppcboot_bd_valid=0;

static mv64x60_handle_t	bh;

#if !defined(CONFIG_SERIAL_MPSC_CONSOLE)
extern void gen550_progress(char *, unsigned short);
extern void gen550_init(int, struct serial_struct *);
#endif

static const unsigned int cpu_7xx[16] = { /* 7xx & 74xx (but not 745x) */
	18, 15, 14, 2, 4, 13, 5, 9, 6, 11, 8, 10, 16, 12, 7, 0
};
static const unsigned int cpu_745x[2][16] = { /* PLL_EXT 0 & 1 */
	{ 1, 15, 14,  2,  4, 13,  5,  9,  6, 11,  8, 10, 16, 12,  7,  0 },
	{ 0, 30,  0,  2,  0, 26,  0, 18,  0, 22, 20, 24, 28, 32,  0,  0 }
};


TODC_ALLOC();

static int
ev64260_get_bus_speed(void)
{
	int	speed;

	if (ppcboot_bd_valid) {
		speed = ppcboot_bd.bi_busfreq;
	}
	else {
		speed = 100000000; /* Only 100MHz is stable */
	}

	return speed;
}

static int
ev64260_get_cpu_speed(void)
{
	unsigned long	pvr, hid1, pll_ext;

	pvr = PVR_VER(mfspr(PVR));

	if (pvr != PVR_VER(PVR_7450)) {
		hid1 = mfspr(HID1) >> 28;
		return ev64260_get_bus_speed() * cpu_7xx[hid1]/2;
	}
	else {
		hid1 = (mfspr(HID1) & 0x0001e000) >> 13;
		pll_ext = 0; /* No way to read; must get from schematic */
		return ev64260_get_bus_speed() * cpu_745x[pll_ext][hid1]/2;
	}
}

unsigned long __init
ev64260_find_end_of_memory(void)
{
	if(!ppcboot_bd_valid) {
		return mv64x60_get_mem_size(CONFIG_MV64X60_NEW_BASE,
			MV64x60_TYPE_GT64260A);
	}
	return ppcboot_bd.bi_memsize;
}

#if 0	/* XXXX */
#ifdef SET_PCI_IDE_NATIVE
static void __init
set_pci_native_mode(void)
{
	struct pci_dev *dev;

	/* Better way of doing this ??? */
	pci_for_each_dev(dev) {	
		int class = dev->class >> 8;

		/* enable pci native mode */
		if (class == PCI_CLASS_STORAGE_IDE) {
			u8 reg;

			pci_read_config_byte(dev, 0x9, &reg);
			if (reg == 0x8a) {
				printk("PCI: Enabling PCI IDE native mode on %s\n", dev->slot_name); 
				pci_write_config_byte(dev, 0x9,  0x8f);

				/* let the pci code set this device up after we change it */
				pci_setup_device(dev); 
			} else if (reg != 0x8f) {
				printk("PCI: IDE chip in unknown mode 0x%02x on %s", reg, dev->slot_name);
			}
		}
	}
}
#endif
#endif

static void __init
ev64260_pci_fixups(void)
{
#ifdef SET_PCI_IDE_NATIVE
	set_pci_native_mode();
#endif
}


/*
 * Marvell/Galileo EV-64260-BP Evaluation Board PCI interrupt routing.
 * Note: By playing with J8 and JP1-4, you can get 2 IRQ's from the first
 *	PCI bus (in which cast, INTPIN B would be EV64260_PCI_1_IRQ).
 *	This is the most IRQs you can get from one bus with this board, though.
 */
static int __init
ev64260_map_irq(struct pci_dev *dev, unsigned char idsel, unsigned char pin)
{
	struct pci_controller	*hose = pci_bus_to_hose(dev->bus->number);

	if (hose->index == 0) {
		static char pci_irq_table[][4] =
		/*
		 *	PCI IDSEL/INTPIN->INTLINE 
		 * 	   A   B   C   D
		 */
		{
			{EV64260_PCI_0_IRQ,0,0,0}, /* IDSEL 7 - PCI bus 0 */
			{EV64260_PCI_0_IRQ,0,0,0}, /* IDSEL 8 - PCI bus 0 */
		};

		const long min_idsel = 7, max_idsel = 8, irqs_per_slot = 4;
		return PCI_IRQ_TABLE_LOOKUP;
	}
	else {
		static char pci_irq_table[][4] =
		/*
		 *	PCI IDSEL/INTPIN->INTLINE 
		 * 	   A   B   C   D
		 */
		{
			{ EV64260_PCI_1_IRQ,0,0,0}, /* IDSEL 7 - PCI bus 1 */
			{ EV64260_PCI_1_IRQ,0,0,0}, /* IDSEL 8 - PCI bus 1 */
		};

		const long min_idsel = 7, max_idsel = 8, irqs_per_slot = 4;
		return PCI_IRQ_TABLE_LOOKUP;
	}
}

static void __init
ev64260_setup_peripherals(void)
{
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2BOOT_WIN,
		EV64260_EMB_FLASH_BASE, EV64260_EMB_FLASH_SIZE, 0);
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_0_WIN,
		EV64260_EXT_SRAM_BASE, EV64260_EXT_SRAM_SIZE, 0);
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_1_WIN,
		EV64260_TODC_BASE, EV64260_TODC_SIZE, 0);
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_2_WIN,
		EV64260_UART_BASE, EV64260_UART_SIZE, 0);
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_3_WIN,
		EV64260_EXT_FLASH_BASE, EV64260_EXT_FLASH_SIZE, 0);

	TODC_INIT(TODC_TYPE_DS1501, 0, 0,
			ioremap(EV64260_TODC_BASE, EV64260_TODC_SIZE), 8);

	mv64x60_clr_bits(&bh, MV64x60_CPU_CONFIG, ((1<<28) | (1<<29)));
	mv64x60_set_bits(&bh, MV64x60_CPU_CONFIG, (1<<27));

	if (ev64260_get_bus_speed() > 100000000) {
		mv64x60_set_bits(&bh, MV64x60_CPU_CONFIG, (1<<23));
	}

	mv64x60_set_bits(&bh, MV64x60_PCI0_PCI_DECODE_CNTL,((1<<0) | (1<<3)));
	mv64x60_set_bits(&bh, MV64x60_PCI1_PCI_DECODE_CNTL,((1<<0) | (1<<3)));

        /*
         * Enabling of PCI internal-vs-external arbitration
         * is a platform- and errata-dependent decision.
         */
        if (bh.type == MV64x60_TYPE_GT64260A )  {
                mv64x60_set_bits(&bh, MV64x60_PCI0_ARBITER_CNTL, (1<<31));
                mv64x60_set_bits(&bh, MV64x60_PCI1_ARBITER_CNTL, (1<<31));
        }

        mv64x60_set_bits(&bh, MV64x60_CPU_MASTER_CNTL, (1<<9)); /* Only 1 cpu */

	/*
	 * The EV-64260-BP uses several Multi-Purpose Pins (MPP) on the 64260
	 * bridge as interrupt inputs (via the General Purpose Ports (GPP)
	 * register).  Need to route the MPP inputs to the GPP and set the
	 * polarity correctly.
	 *
	 * In MPP Control 2 Register
	 *   MPP 21 -> GPP 21 (DUART channel A intr) bits 20-23 -> 0
	 *   MPP 22 -> GPP 22 (DUART channel B intr) bits 24-27 -> 0
	 */
	mv64x60_clr_bits(&bh, MV64x60_MPP_CNTL_2, (0xf<<20) | (0xf<<24) );

	/*
	 * In MPP Control 3 Register
	 *   MPP 26 -> GPP 26 (RTC INT)		bits  8-11 -> 0
	 *   MPP 27 -> GPP 27 (PCI 0 INTA)	bits 12-15 -> 0
	 *   MPP 29 -> GPP 29 (PCI 1 INTA)	bits 20-23 -> 0
	 */
	mv64x60_clr_bits(&bh, MV64x60_MPP_CNTL_3,
		(0xf<<8) | (0xf<<12) | (0xf<<20));

#define GPP_EXTERNAL_INTERRUPTS \
		((1<<21) | (1<<22) | (1<<26) | (1<<27) | (1<<29))
	/* DUART & PCI interrupts are inputs */
	mv64x60_clr_bits(&bh, MV64x60_GPP_IO_CNTL, GPP_EXTERNAL_INTERRUPTS);
	/* DUART & PCI interrupts are active low */
	mv64x60_set_bits(&bh, MV64x60_GPP_LEVEL_CNTL, GPP_EXTERNAL_INTERRUPTS);

	/* Clear any pending interrupts for these inputs and enable them. */
	mv64x60_write(&bh, MV64x60_GPP_INTR_CAUSE, ~GPP_EXTERNAL_INTERRUPTS);
	mv64x60_set_bits(&bh, MV64x60_GPP_INTR_MASK, GPP_EXTERNAL_INTERRUPTS);

	/*
	 * Set MPSC Multiplex RMII
	 * NOTE: ethernet driver modifies bit 0 and 1
	 */
	mv64x60_write(&bh, GT64260_MPP_SERIAL_PORTS_MULTIPLEX, 0x00001102);
	return;
}


static void __init
ev64260_setup_bridge(void)
{
	mv64x60_setup_info_t	si;
	int			i;

	memset(&si, 0, sizeof(si));

	si.phys_reg_base = CONFIG_MV64X60_NEW_BASE;
	si.map_irq = ev64260_map_irq;

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

	for (i=0; i<MV64x60_CPU2MEM_WINDOWS; i++) {
		si.cpu_prot_options[i] = 0;
		si.cpu_snoop_options[i] = GT64260_CPU_SNOOP_WB;
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
		si.pci_1.snoop_options[i] = GT64260_PCI_SNOOP_WB;
	}

        /* Lookup PCI host bridges */
        if (mv64x60_init(&bh, &si)) {
                printk("Bridge initialization failed.\n");
        }

	return;
}

#if defined(CONFIG_SERIAL_8250) && !defined(CONFIG_SERIAL_MPSC_CONSOLE)
static void __init
ev64260_early_serial_map(void)
{
	struct uart_port	port;
	static char		first_time = 1;

	if (first_time) {
		memset(&port, 0, sizeof(port));

		port.membase = ioremap(EV64260_SERIAL_0, EV64260_UART_SIZE);
		port.irq = EV64260_UART_0_IRQ;
		port.uartclk = BASE_BAUD * 16;
		port.regshift = 2;
		port.iotype = SERIAL_IO_MEM;
		port.flags = STD_COM_FLAGS;

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
		gen550_init(0, &port);
#endif

		if (early_serial_setup(&port) != 0) {
			printk("Early serial init of port 0 failed\n");
		}

#if 0	/* XXXX */
		/* Assume early_serial_setup() doesn't modify port */
		port.membase = ioremap(EV64260_SERIAL_1, EV64260_UART_SIZE);
		port.irq = EV64260_UART_1_IRQ;

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
		gen550_init(1, &port);
#endif

		if (early_serial_setup(&port) != 0) {
			printk("Early serial init of port 1 failed\n");
		}
#endif

		first_time = 0;
	}

	return;
}
#elif defined(CONFIG_SERIAL_MPSC_CONSOLE)
static void __init
ev64260_early_serial_map(void)
{
#ifdef	CONFIG_KGDB
	static char	first_time = 1;


#if defined(CONFIG_KGDB_TTYS0)
#define KGDB_PORT 0
#elif defined(CONFIG_KGDB_TTYS1)
#define KGDB_PORT 1
#else
#error "Invalid kgdb_tty port"
#endif

	if (first_time) {
		gt_early_mpsc_init(KGDB_PORT, B9600|CS8|CREAD|HUPCL|CLOCAL);
		first_time = 0;
	}

	return;
#endif
}
#endif

static void __init
ev64260_fixup_ocp(void)
{
#if defined(CONFIG_SERIAL_MPSC)
	struct ocp_device	*dev;
	mv64x60_ocp_mpsc_data_t	*dp;

	if ((dev = ocp_find_device(OCP_VENDOR_MARVELL, OCP_FUNC_MPSC, 0))
								!= NULL) {
		dp = (mv64x60_ocp_mpsc_data_t *)dev->def->additions;

		dp->max_idle = 40;	/* XXXX what should this be? */
		dp->default_baud = EV64260_DEFAULT_BAUD;
		dp->brg_clk_src = EV64260_MPSC_CLK_SRC;
		dp->brg_clk_freq = EV64260_MPSC_CLK_FREQ;
	}

	if ((dev = ocp_find_device(OCP_VENDOR_MARVELL, OCP_FUNC_MPSC, 1))
								!= NULL) {
		dp = (mv64x60_ocp_mpsc_data_t *)dev->def->additions;

		dp->max_idle = 40;	/* XXXX what should this be? */
		dp->default_baud = 9600;	/* XXXX */
		dp->brg_clk_src = EV64260_MPSC_CLK_SRC;
		dp->brg_clk_freq = EV64260_MPSC_CLK_FREQ;
	}
#endif

	return;
}

static void __init
ev64260_setup_arch(void)
{
	if ( ppc_md.progress )
		ppc_md.progress("ev64260_setup_arch: enter", 0);

#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start)
		ROOT_DEV = Root_RAM0;
	else
#endif
#ifdef	CONFIG_ROOT_NFS
		ROOT_DEV = Root_NFS;
#else
		ROOT_DEV = Root_SDA2;
#endif

	if ( ppc_md.progress )
		ppc_md.progress("ev64260_setup_arch: Enabling L2 cache", 0);

	/* Enable L2 and L3 caches (if 745x) */
	_set_L2CR(_get_L2CR() | L2CR_L2E);
	_set_L3CR(_get_L3CR() | L3CR_L3E);

	if ( ppc_md.progress )
		ppc_md.progress("ev64260_setup_arch: Initializing bridge", 0);

	ev64260_setup_bridge();		/* set up PCI bridge(s) */
	ev64260_setup_peripherals();	/* set up chip selects/GPP/MPP etc */

	if ( ppc_md.progress )
		ppc_md.progress("ev64260_setup_arch: bridge init complete", 0);

	/* Set OCP values to reflect this board's setup */
	ev64260_fixup_ocp();

#ifdef	CONFIG_DUMMY_CONSOLE
	conswitchp = &dummy_con;
#endif
#if defined(CONFIG_SERIAL_8250) || defined(CONFIG_SERIAL_MPSC_CONSOLE)
	ev64260_early_serial_map();
#endif

	printk(BOARD_VENDOR " " BOARD_MACHINE "\n");
	printk("EV-64260-BP port (C) 2001 MontaVista Software, Inc. (source@mvista.com)\n");

	if ( ppc_md.progress )
		ppc_md.progress("ev64260_setup_arch: exit", 0);

	return;
}

static void
ev64260_reset_board(void *addr)
{
	local_irq_disable();

	/* disable and invalidate the L2 cache */
	_set_L2CR(0);
	_set_L2CR(0x200000);

	/* flush and disable L1 I/D cache */
	__asm__ __volatile__
	("mfspr   3,1008\n\t"
	 "ori	5,5,0xcc00\n\t"
	 "ori	4,3,0xc00\n\t"
	 "andc	5,3,5\n\t"
	 "sync\n\t"
	 "mtspr	1008,4\n\t"
	 "isync\n\t"
	 "sync\n\t"
	 "mtspr	1008,5\n\t"
	 "isync\n\t"
	 "sync\n\t");

	/* unmap any other random cs's that might overlap with bootcs */
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_0_WIN, 0, 0, 0);
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_1_WIN, 0, 0, 0);
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_2_WIN, 0, 0, 0);
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2DEV_3_WIN, 0, 0, 0);

	/* map bootrom back in to gt @ reset defaults */
	mv64x60_set_32bit_window(&bh, MV64x60_CPU2BOOT_WIN,
						0xff800000, 8*1024*1024, 0);

	/* move gt reg base back to default, setup default pci0 swapping
	 * config... */
	mv64x60_write(&bh, MV64x60_INTERNAL_SPACE_DECODE,
		(1<<24) | MV64x60_INTERNAL_SPACE_DEFAULT_ADDR>>20);

	/* NOTE: FROM NOW ON no more GT_REGS accesses.. 0x1 is not mapped
	 * via BAT or MMU, and MSR IR/DR is ON */
#if 0
	/* BROKEN... IR/DR is still on !!  won't work!! */
	/* Set exception prefix high - to the firmware */
	_nmask_and_or_msr(0, MSR_IP);

	out_8((u_char *)EV64260_BOARD_MODRST_REG, 0x01);
#else
	/* SRR0 has system reset vector, SRR1 has default MSR value */
	/* rfi restores MSR from SRR1 and sets the PC to the SRR0 value */
	/* NOTE: assumes reset vector is at 0xfff00100 */
	__asm__ __volatile__
	("mtspr   26, %0\n\t"
	 "li      4,(1<<6)\n\t"
	 "mtspr   27,4\n\t"
	 "rfi\n\t"
	 :: "r" (addr):"r4");
#endif
	return;
}

static void
ev64260_restart(char *cmd)
{
	volatile ulong	i = 10000000;

	ev64260_reset_board((void *)0xfff00100);

	while (i-- > 0);
	panic("restart failed\n");
}

static void
ev64260_halt(void)
{
	local_irq_disable();
	while (1);
	/* NOTREACHED */
}

static void
ev64260_power_off(void)
{
	ev64260_halt();
	/* NOTREACHED */
}

static int
ev64260_show_cpuinfo(struct seq_file *m)
{
	uint pvid;

	pvid = mfspr(PVR);
	seq_printf(m, "vendor\t\t: " BOARD_VENDOR "\n");
	seq_printf(m, "machine\t\t: " BOARD_MACHINE "\n");
	seq_printf(m, "cpu MHz\t\t: %d\n", ev64260_get_cpu_speed()/1000/1000);
	seq_printf(m, "bus MHz\t\t: %d\n", ev64260_get_bus_speed()/1000/1000);

	return 0;
}

/* DS1501 RTC has too much variation to use RTC for calibration */
static void __init
ev64260_calibrate_decr(void)
{
	ulong freq;

	freq = ev64260_get_bus_speed()/4;

	printk("time_init: decrementer frequency = %lu.%.6lu MHz\n",
	       freq/1000000, freq%1000000);

	tb_ticks_per_jiffy = freq / HZ;
	tb_to_us = mulhwu_scale_factor(freq, 1000000);

	return;
}

#if 0	/* XXXX */
#ifdef CONFIG_USE_PPCBOOT
static void parse_ppcbootinfo(unsigned long r3,
	unsigned long r4, unsigned long r5,
	unsigned long r6, unsigned long r7)
{
    bd_t *bd=NULL;
    char *cmdline_start=NULL;
    int cmdline_len=0;

    if(r3) {
	if((r3 & 0xf0000000) == 0) r3 += KERNELBASE;
	if((r3 & 0xf0000000) == KERNELBASE) {
	    bd=(void *)r3;

	    /* hack for ppcboot loaders that report freqs in Mhz */
	    if(bd->bi_intfreq<1000000) bd->bi_intfreq*=1000000;
	    if(bd->bi_busfreq<1000000) bd->bi_busfreq*=1000000;

	    memcpy(&ppcboot_bd,bd,sizeof(ppcboot_bd));
	    ppcboot_bd_valid=1;
	}
    }

#ifdef CONFIG_BLK_DEV_INITRD
    if(r4 && r5 && r5>r4) {
	if((r4 & 0xf0000000) == 0) r4 += KERNELBASE;
	if((r5 & 0xf0000000) == 0) r5 += KERNELBASE;
	if((r4 & 0xf0000000) == KERNELBASE) {
	    initrd_start=r4;
	    initrd_end=r5;
	    initrd_below_start_ok = 1;
	}
    }
#endif /* CONFIG_BLK_DEV_INITRD */

    if(r6 && r7 && r7>r6) {
	if((r6 & 0xf0000000) == 0) r6 += KERNELBASE;
	if((r7 & 0xf0000000) == 0) r7 += KERNELBASE;
	if((r6 & 0xf0000000) == KERNELBASE) {
	    cmdline_start=(void *)r6;
	    cmdline_len=(r7-r6);
	    strncpy(cmd_line,cmdline_start,cmdline_len);
	}
    }

    if(ppcboot_bd_valid) {			
	printk("found bd_t @%p\n", bd);
	printk("memstart=%08lx\n", bd->bi_memstart);
	printk("memsize=%08lx\n", bd->bi_memsize);
	printk("enetaddr=%02x%02x%02x%02x%02x%02x\n",
		bd->bi_enetaddr[0],
		bd->bi_enetaddr[1],
		bd->bi_enetaddr[2],
		bd->bi_enetaddr[3],
		bd->bi_enetaddr[4],
		bd->bi_enetaddr[5]
		);
	printk("intfreq=%ld\n", bd->bi_intfreq);
	printk("busfreq=%ld\n", bd->bi_busfreq);
	printk("baudrate=%ld\n", bd->bi_baudrate);
    }

#ifdef CONFIG_BLK_DEV_INITRD
    if(initrd_start) {
	printk("found initrd @%lx-%lx\n", initrd_start, initrd_end);
    }
#endif /* CONFIG_BLK_DEV_INITRD */

    if(cmdline_start && cmdline_len) {
	printk("found cmdline: '%s'\n", cmd_line);
    }
}
#endif	/* USE PPC_BOOT */
#endif

#if 0	/* XXXX */
#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
static int
ev64260_ide_check_region(ide_ioreg_t from, unsigned int extent)
{
        return check_region(from, extent);
}

static void
ev64260_ide_request_region(ide_ioreg_t from,
                        unsigned int extent,
                        const char *name)
{
        request_region(from, extent, name);
	return;
}

static void
ev64260_ide_release_region(ide_ioreg_t from,
                        unsigned int extent)
{
        release_region(from, extent);
	return;
}

static void __init
ev64260_ide_pci_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port,
                ide_ioreg_t ctrl_port, int *irq)
{
	struct pci_dev	*dev;
#if 1 /* NTL */
        int i;

	//printk("regs %d to %d @ 0x%x\n", IDE_DATA_OFFSET, IDE_STATUS_OFFSET, data_port);
        for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
                hw->io_ports[i] = data_port;
                data_port++;
        }

	//printk("ctrl %d @ 0x%x\n", IDE_CONTROL_OFFSET, ctrl_port);
        hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
#endif

	pci_for_each_dev(dev) {
		if (((dev->class >> 8) == PCI_CLASS_STORAGE_IDE) ||
		    ((dev->class >> 8) == PCI_CLASS_STORAGE_RAID)) {
			hw->irq = dev->irq;

			if (irq != NULL) {
				*irq = dev->irq;
			}
		}
	}

	return;
}
#endif
#endif

#if !defined(CONFIG_USE_PPCBOOT)
/*
 * Set BAT 3 to map 0xfb000000 to 0xfc000000 of physical memory space.
 */
static __inline__ void
ev64260_set_bat(void)
{
	mb();
	mtspr(DBAT1U, 0xfb0001fe);
	mtspr(DBAT1L, 0xfb00002a);
	mb();

	return;
}
#endif

#if defined(CONFIG_SERIAL_TEXT_DEBUG) || defined(CONFIG_KGDB)
static void __init
ev64260_map_io(void)
{
	io_block_mapping(0xfb000000, 0xfb000000, 0x01000000, _PAGE_IO);
}
#endif

void __init
platform_init(unsigned long r3, unsigned long r4, unsigned long r5,
	      unsigned long r6, unsigned long r7)
{
#ifdef CONFIG_BLK_DEV_INITRD
	extern int	initrd_below_start_ok;

	initrd_start=initrd_end=0;
	initrd_below_start_ok=0;
#endif /* CONFIG_BLK_DEV_INITRD */

	ppcboot_bd_valid=0;
	memset(&ppcboot_bd,0,sizeof(ppcboot_bd));

#ifdef CONFIG_USE_PPCBOOT
	parse_ppcbootinfo(r3, r4, r5, r6, r7);
#else
	parse_bootinfo(find_bootinfo());
#endif

	isa_mem_base = 0;
	isa_io_base = 0xa0000000;	/* XXXX */
	pci_dram_offset = 0x80000000;	/* XXXX */

	loops_per_jiffy = ev64260_get_cpu_speed() / HZ;

	ppc_md.setup_arch = ev64260_setup_arch;
	ppc_md.show_cpuinfo = ev64260_show_cpuinfo;
	ppc_md.init_IRQ = gt64260_init_irq;
	ppc_md.get_irq = gt64260_get_irq;

	ppc_md.pcibios_fixup = ev64260_pci_fixups;

	ppc_md.restart = ev64260_restart;
	ppc_md.power_off = ev64260_power_off;
	ppc_md.halt = ev64260_halt;

	ppc_md.find_end_of_memory = ev64260_find_end_of_memory;

	ppc_md.init = NULL;

	ppc_md.time_init = todc_time_init;
	ppc_md.set_rtc_time = todc_set_rtc_time;
	ppc_md.get_rtc_time = todc_get_rtc_time;

	ppc_md.nvram_read_val = todc_direct_read_val;
	ppc_md.nvram_write_val = todc_direct_write_val;

	ppc_md.calibrate_decr = ev64260_calibrate_decr;

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
        ppc_ide_md.ide_init_hwif = ev64260_ide_pci_init_hwif_ports;
#endif

	bh.p_base = CONFIG_MV64X60_NEW_BASE;

#if !defined(CONFIG_USE_PPCBOOT)
	ev64260_set_bat();
#endif

#ifdef	CONFIG_SERIAL_8250
#if defined(CONFIG_SERIAL_TEXT_DEBUG)
	ppc_md.setup_io_mappings = ev64260_map_io;
	ppc_md.progress = gen550_progress;
#endif
#if defined(CONFIG_KGDB)
	ppc_md.setup_io_mappings = ev64260_map_io;
	ppc_md.early_serial_map = ev64260_early_serial_map;
#endif
#elif defined(CONFIG_SERIAL_MPSC_CONSOLE)
#ifdef	CONFIG_SERIAL_TEXT_DEBUG
	ppc_md.setup_io_mappings = ev64260_map_io;
	ppc_md.progress = gt64260_mpsc_progress;
#endif	/* CONFIG_SERIAL_TEXT_DEBUG */
#ifdef	CONFIG_KGDB
	ppc_md.setup_io_mappings = ev64260_map_io;
	ppc_md.early_serial_map = ev64260_early_serial_map;
#endif	/* CONFIG_KGDB */

#endif

	return;
}
