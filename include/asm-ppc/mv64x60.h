/*
 * include/asm-ppc/mv64x60.h
 * 
 * Prototypes, etc. for the Marvell/Galileo MV64x60 host bridge routines.
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *
 * 2001-2002 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#ifndef __ASMPPC_MV64x60_H
#define __ASMPPC_MV64x60_H

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/config.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/machdep.h>
#include <asm/pci-bridge.h>
#include <asm/mv64x60_defs.h>

extern u8      mv64x60_pci_exclude_bridge;

extern spinlock_t mv64x60_lock;
extern spinlock_t mv64x60_rmw_lock;

#ifndef	TRUE
#define	TRUE	1
#endif

#ifndef	FALSE
#define	FALSE	0
#endif

/* 32-bit Window table entry defines */
#define	MV64x60_CPU2MEM_0_WIN			0
#define	MV64x60_CPU2MEM_1_WIN			1
#define	MV64x60_CPU2MEM_2_WIN			2
#define	MV64x60_CPU2MEM_3_WIN			3
#define	MV64x60_CPU2DEV_0_WIN			4
#define	MV64x60_CPU2DEV_1_WIN			5
#define	MV64x60_CPU2DEV_2_WIN			6
#define	MV64x60_CPU2DEV_3_WIN			7
#define	MV64x60_CPU2BOOT_WIN			8
#define	MV64x60_CPU2PCI0_IO_WIN			9
#define	MV64x60_CPU2PCI0_MEM_0_WIN		10
#define	MV64x60_CPU2PCI0_MEM_1_WIN		11
#define	MV64x60_CPU2PCI0_MEM_2_WIN		12
#define	MV64x60_CPU2PCI0_MEM_3_WIN		13
#define	MV64x60_CPU2PCI1_IO_WIN			14
#define	MV64x60_CPU2PCI1_MEM_0_WIN		15
#define	MV64x60_CPU2PCI1_MEM_1_WIN		16
#define	MV64x60_CPU2PCI1_MEM_2_WIN		17
#define	MV64x60_CPU2PCI1_MEM_3_WIN		18
#define	MV64x60_CPU2SRAM_WIN			19
#define	MV64x60_CPU2PCI0_IO_REMAP_WIN		20
#define	MV64x60_CPU2PCI1_IO_REMAP_WIN		21
#define	MV64x60_CPU_PROT_0_WIN			22
#define	MV64x60_CPU_PROT_1_WIN			23
#define	MV64x60_CPU_PROT_2_WIN			24
#define	MV64x60_CPU_PROT_3_WIN			25
#define	MV64x60_CPU_SNOOP_0_WIN			26
#define	MV64x60_CPU_SNOOP_1_WIN			27
#define	MV64x60_CPU_SNOOP_2_WIN			28
#define	MV64x60_CPU_SNOOP_3_WIN			29
#define	MV64x60_PCI02MEM_REMAP_0_WIN		30
#define	MV64x60_PCI02MEM_REMAP_1_WIN		31
#define	MV64x60_PCI02MEM_REMAP_2_WIN		32
#define	MV64x60_PCI02MEM_REMAP_3_WIN		33
#define	MV64x60_PCI12MEM_REMAP_0_WIN		34
#define	MV64x60_PCI12MEM_REMAP_1_WIN		35
#define	MV64x60_PCI12MEM_REMAP_2_WIN		36
#define	MV64x60_PCI12MEM_REMAP_3_WIN		37

#define	MV64x60_32BIT_WIN_COUNT			38

/* 64-bit Window table entry defines */
#define	MV64x60_CPU2PCI0_MEM_0_REMAP_WIN	0
#define	MV64x60_CPU2PCI0_MEM_1_REMAP_WIN	1
#define	MV64x60_CPU2PCI0_MEM_2_REMAP_WIN	2
#define	MV64x60_CPU2PCI0_MEM_3_REMAP_WIN	3
#define	MV64x60_CPU2PCI1_MEM_0_REMAP_WIN	4
#define	MV64x60_CPU2PCI1_MEM_1_REMAP_WIN	5
#define	MV64x60_CPU2PCI1_MEM_2_REMAP_WIN	6
#define	MV64x60_CPU2PCI1_MEM_3_REMAP_WIN	7
#define	MV64x60_PCI02MEM_ACC_CNTL_0_WIN		8
#define	MV64x60_PCI02MEM_ACC_CNTL_1_WIN		9
#define	MV64x60_PCI02MEM_ACC_CNTL_2_WIN		10
#define	MV64x60_PCI02MEM_ACC_CNTL_3_WIN		11
#define	MV64x60_PCI12MEM_ACC_CNTL_0_WIN		12
#define	MV64x60_PCI12MEM_ACC_CNTL_1_WIN		13
#define	MV64x60_PCI12MEM_ACC_CNTL_2_WIN		14
#define	MV64x60_PCI12MEM_ACC_CNTL_3_WIN		15
#define	MV64x60_PCI02MEM_SNOOP_0_WIN		16
#define	MV64x60_PCI02MEM_SNOOP_1_WIN		17
#define	MV64x60_PCI02MEM_SNOOP_2_WIN		18
#define	MV64x60_PCI02MEM_SNOOP_3_WIN		19
#define	MV64x60_PCI12MEM_SNOOP_0_WIN		20
#define	MV64x60_PCI12MEM_SNOOP_1_WIN		21
#define	MV64x60_PCI12MEM_SNOOP_2_WIN		22
#define	MV64x60_PCI12MEM_SNOOP_3_WIN		23

#define	MV64x60_64BIT_WIN_COUNT			24


/*
 * Define a structure that's used to pass in config information to the
 * core routines.
 */
typedef struct {
	u32	cpu_base;
	u32	pci_base_hi;
	u32	pci_base_lo;
	u32	size;
	u32	swap;
} mv64x60_pci_window_t;

typedef	struct {
	u8	enable_bus;	/* allow access to this PCI bus? */
	u8	enumerate_bus;	/* enumerate devices on this bus? */

	mv64x60_pci_window_t	pci_io;
	mv64x60_pci_window_t	pci_mem[3];

	u32	acc_cntl_options[MV64x60_CPU2MEM_WINDOWS];
	u32	snoop_options[MV64x60_CPU2MEM_WINDOWS];
	u16	pci_cmd_bits;
	u16	latency_timer;
} mv64x60_pci_info_t;

typedef struct {
	u32	phys_reg_base;

	u32	window_preserve_mask_32;
	u32	window_preserve_mask_64;

	u32	base_irq;	/* Starting irq # for this intr ctlr */
	int	((*map_irq)(struct pci_dev *, unsigned char, unsigned char));

	u32	cpu_prot_options[MV64x60_CPU2MEM_WINDOWS];
	u32	cpu_snoop_options[MV64x60_CPU2MEM_WINDOWS];

	mv64x60_pci_info_t	pci_0;
	mv64x60_pci_info_t	pci_1;
} mv64x60_setup_info_t;

/*
 * Define the 'handle' struct that will be passed between the 64x60 core
 * code and the platform-specific code that will use it.  The handle
 * will contain pointers to chip-specific routines & information.
 */
typedef struct {
	u32	base_reg;
	u32	size_reg;
	u8	base_bits;
	u8	size_bits;
	u32	(*get_from_field)(u32 val, u32 num_bits);
	u32	(*map_to_field)(u32 val, u32 num_bits);
	u32	extra;
} mv64x60_32bit_window_t;

typedef struct {
	u32	base_hi_reg;
	u32	base_lo_reg;
	u32	size_reg;
	u8	base_lo_bits;
	u8	size_bits;
	u32	(*get_from_field)(u32 val, u32 num_bits);
	u32	(*map_to_field)(u32 val, u32 num_bits);
	u32	extra;
} mv64x60_64bit_window_t;

typedef	struct mv64x60_handle	mv64x60_handle_t;

typedef struct {
	u32	(*translate_size)(u32 base, u32 size, u32 num_bits);
	u32	(*untranslate_size)(u32 base, u32 size, u32 num_bits);
	void	(*set_pci2mem_window)(struct pci_controller *hose, u32 window,
			u32 base);
	u32	(*is_enabled_32bit)(mv64x60_handle_t *bh, u32 window);
	void	(*enable_window_32bit)(mv64x60_handle_t *bh, u32 window);
	void	(*disable_window_32bit)(mv64x60_handle_t *bh, u32 window);
	void	(*enable_window_64bit)(mv64x60_handle_t *bh, u32 window);
	void	(*disable_window_64bit)(mv64x60_handle_t *bh, u32 window);
	void	(*disable_all_windows)(mv64x60_handle_t *bh,
				       mv64x60_setup_info_t *si);
	void	(*chip_specific_init)(mv64x60_handle_t *bh,
				      mv64x60_setup_info_t *si);

	mv64x60_32bit_window_t	*window_tab_32bit;
	mv64x60_64bit_window_t	*window_tab_64bit;
} mv64x60_chip_info_t;

struct mv64x60_handle {
	u32	type;		/* type of bridge */
	u32	v_base;		/* virtual base addr of bridge regs */
	u32	p_base;		/* physical base addr of bridge regs */
	u32	base_irq;	/* Base irq # for intrs on this intr cltr */

	u32	io_base_a;	/* vaddr of pci 0's I/O space */
	u32	io_base_b;	/* vaddr of pci 1's I/O space */

	struct pci_controller	*hose_a;
	struct pci_controller	*hose_b;

	mv64x60_chip_info_t	*ci;	/* chip/bridge-specific info */
};


/* Define I/O routines for accessing registers on the 64x60 bridge. */
extern inline void
mv64x60_write(mv64x60_handle_t *bh, u32 offset, u32 val) {
	out_le32((volatile u32 *)(bh->v_base + offset), val);
}

extern inline u32
mv64x60_read(mv64x60_handle_t *bh, u32 offset) {
	return in_le32((volatile u32 *)(bh->v_base + offset));
}

extern inline void
mv64x60_modify(mv64x60_handle_t *bh, u32 offs, u32 data, u32 mask)
{
	uint32_t reg;
	unsigned long flags;

	spin_lock_irqsave(&mv64x60_rmw_lock, flags);
	reg = mv64x60_read(bh, offs) & (~mask); /* zero any bits we care about*/
	reg |= data & mask; /* set bits from the data */
	mv64x60_write(bh, offs, reg);
	spin_unlock_irqrestore(&mv64x60_rmw_lock, flags);
}

#define	mv64x60_set_bits(bh, offs, bits) mv64x60_modify(bh, offs, ~0, bits) 
#define	mv64x60_clr_bits(bh, offs, bits) mv64x60_modify(bh, offs, 0, bits)


/* Externally visible function prototypes */
int mv64x60_init(mv64x60_handle_t *bh, mv64x60_setup_info_t *si);
u32 mv64x60_get_mem_size(u32 bridge_base, u32 chip_type);
void mv64x60_get_32bit_window(mv64x60_handle_t *bh, u32 window,
					u32 *base, u32 *size);
void mv64x60_set_32bit_window(mv64x60_handle_t *bh, u32 window, u32 base,
					u32 size, u32 other_bits);
void mv64x60_get_64bit_window(mv64x60_handle_t *bh, u32 window, u32 *base_hi,
					u32 *base_lo, u32 *size);
void mv64x60_set_64bit_window(mv64x60_handle_t *bh, u32 window, u32 base_hi,
					u32 base_lo, u32 size, u32 other_bits);


void gt64260_init_irq(void);
int gt64260_get_irq(struct pt_regs *regs);

/*
 * OCP Related Definitions
 */
typedef struct {
	u8	mirror_regs;
	u8	cache_mgmt;
	u8	max_idle;
	int	default_baud;
	int	default_bits;
	int	default_parity;
	int	default_flow;
	u32	chr_1_val;
	u32	chr_2_val;
	u32	chr_10_val;
	u32	mpcr_val;
	u32	mrr_val;
	u32	rcrr_val;
	u32	tcrr_val;
	u32	intr_mask_val;
	u32	bcr_val;
	u32	sdma_irq;
	u8	brg_can_tune;
	u8	brg_clk_src;
	u32	brg_clk_freq;
} mv64x60_ocp_mpsc_data_t;

#define MV64x60_OCP_SYSFS_MPSC_DATA()					\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%d\n", mpsc, mirror_regs)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%d\n", mpsc, cache_mgmt)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%d\n", mpsc, max_idle)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%d\n", mpsc, default_baud)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%d\n", mpsc, default_bits)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%c\n", mpsc, default_parity)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%c\n", mpsc, default_flow)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "0x%x\n", mpsc, chr_1_val)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "0x%x\n", mpsc, chr_2_val)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "0x%x\n", mpsc, chr_10_val)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "0x%x\n", mpsc, mpcr_val)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "0x%x\n", mpsc, mrr_val)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "0x%x\n", mpsc, rcrr_val)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "0x%x\n", mpsc, tcrr_val)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "0x%x\n", mpsc, intr_mask_val)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "0x%x\n", mpsc, bcr_val)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%d\n", mpsc, sdma_irq)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%d\n", mpsc, brg_can_tune)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%d\n", mpsc, brg_clk_src)	\
OCP_SYSFS_ADDTL(mv64x60_ocp_mpsc_data_t, "%d\n", mpsc, brg_clk_freq)	\
									\
void									\
mv64x60_ocp_show_mpsc(struct device *dev)				\
{									\
	device_create_file(dev, &dev_attr_mpsc_mirror_regs);		\
	device_create_file(dev, &dev_attr_mpsc_cache_mgmt);		\
	device_create_file(dev, &dev_attr_mpsc_max_idle);		\
	device_create_file(dev, &dev_attr_mpsc_default_baud);		\
	device_create_file(dev, &dev_attr_mpsc_default_bits);		\
	device_create_file(dev, &dev_attr_mpsc_default_parity);		\
	device_create_file(dev, &dev_attr_mpsc_default_flow);		\
	device_create_file(dev, &dev_attr_mpsc_chr_1_val);		\
	device_create_file(dev, &dev_attr_mpsc_chr_2_val);		\
	device_create_file(dev, &dev_attr_mpsc_chr_10_val);		\
	device_create_file(dev, &dev_attr_mpsc_mpcr_val);		\
	device_create_file(dev, &dev_attr_mpsc_mrr_val);		\
	device_create_file(dev, &dev_attr_mpsc_rcrr_val);		\
	device_create_file(dev, &dev_attr_mpsc_tcrr_val);		\
	device_create_file(dev, &dev_attr_mpsc_intr_mask_val);		\
	device_create_file(dev, &dev_attr_mpsc_bcr_val);		\
	device_create_file(dev, &dev_attr_mpsc_sdma_irq);		\
	device_create_file(dev, &dev_attr_mpsc_brg_can_tune);		\
	device_create_file(dev, &dev_attr_mpsc_brg_clk_src);		\
	device_create_file(dev, &dev_attr_mpsc_brg_clk_freq);		\
}

#endif /* __ASMPPC_MV64x60_H */
