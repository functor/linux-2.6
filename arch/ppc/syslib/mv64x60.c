/*
 * arch/ppc/syslib/mv64x60.c
 * 
 * Common routines for the Marvell/Galileo Discovery line of host bridges
 * (e.g, gt64260 and mv64360).
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *	   Rabeeh Khoury <rabeeh@galileo.co.il>
 *
 * 2001-2002 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/machdep.h>
#include <asm/pci-bridge.h>
#include <asm/mv64x60.h>
#include <asm/delay.h>
#include <asm/ocp.h>


#undef	DEBUG

#ifdef	DEBUG
#define	DBG(x...) printk(x)
#else
#define	DBG(x...)
#endif	/* DEBUG */


static u32 mv64x60_mask(u32 val, u32 num_bits);
static u32 mv64x60_shift_left(u32 val, u32 num_bits);
static u32 mv64x60_shift_right(u32 val, u32 num_bits);
static void mv64x60_early_init(mv64x60_handle_t *bh, mv64x60_setup_info_t *si);
static int mv64x60_get_type(mv64x60_handle_t *bh);
static int mv64x60_setup_for_chip(mv64x60_handle_t *bh);
static void mv64x60_get_mem_windows(mv64x60_handle_t *bh,
	u32 mem_windows[MV64x60_CPU2MEM_WINDOWS][2]);
static u32 mv64x60_calc_mem_size(mv64x60_handle_t *bh,
	u32 mem_windows[MV64x60_CPU2MEM_WINDOWS][2]);
static void mv64x60_config_cpu2mem_windows(mv64x60_handle_t *bh,
	mv64x60_setup_info_t *si, u32 mem_windows[MV64x60_CPU2MEM_WINDOWS][2]);
static void mv64x60_config_cpu2pci_windows(mv64x60_handle_t *bh,
	mv64x60_setup_info_t *si);
static void mv64x60_set_cpu2pci_window(mv64x60_handle_t *bh,
	mv64x60_pci_info_t *pi, u32 *win_tab, u32 *remap_tab);
static void mv64x60_config_pci2mem_windows(mv64x60_handle_t *bh,
	mv64x60_setup_info_t *si, u32 mem_windows[MV64x60_CPU2MEM_WINDOWS][2]);
static void mv64x60_alloc_hoses(mv64x60_handle_t *bh, mv64x60_setup_info_t *si);
static void mv64x60_init_hoses(mv64x60_handle_t *bh, mv64x60_setup_info_t *si);
static void mv64x60_init_resources(struct pci_controller *hose,
	mv64x60_pci_info_t *pi, u32 io_base);
static void mv64x60_set_pci_params(struct pci_controller *hose,
	mv64x60_pci_info_t *pi);
static void mv64x60_enumerate_buses(mv64x60_handle_t *bh,
	mv64x60_setup_info_t *si);
static int mv64x60_pci_exclude_device(u8 bus, u8 devfn);
static void mv64x60_fixup_ocp(struct ocp_device *, void *arg);

static u32 gt64260_translate_size(u32 base, u32 size, u32 num_bits);
static u32 gt64260_untranslate_size(u32 base, u32 size, u32 num_bits);
static void gt64260_set_pci2mem_window(struct pci_controller *hose,
	u32 window, u32 base);
static u32 gt64260_is_enabled_32bit(mv64x60_handle_t *bh, u32 window);
static void gt64260_enable_window_32bit(mv64x60_handle_t *bh, u32 window);
static void gt64260_disable_window_32bit(mv64x60_handle_t *bh, u32 window);
static void gt64260_enable_window_64bit(mv64x60_handle_t *bh, u32 window);
static void gt64260_disable_window_64bit(mv64x60_handle_t *bh, u32 window);
static void gt64260_disable_all_windows(mv64x60_handle_t *bh,
					mv64x60_setup_info_t *si);
static void gt64260a_chip_specific_init(mv64x60_handle_t *bh,
	mv64x60_setup_info_t *si);
static void gt64260b_chip_specific_init(mv64x60_handle_t *bh,
	mv64x60_setup_info_t *si);

static u32 mv64360_translate_size(u32 base_addr, u32 size, u32 num_bits);
static u32 mv64360_untranslate_size(u32 base_addr, u32 size, u32 num_bits);
static void mv64360_set_pci2mem_window(struct pci_controller *hose,
	u32 window, u32 base);
static u32 mv64360_is_enabled_32bit(mv64x60_handle_t *bh, u32 window);
static void mv64360_enable_window_32bit(mv64x60_handle_t *bh, u32 window);
static void mv64360_disable_window_32bit(mv64x60_handle_t *bh, u32 window);
static void mv64360_enable_window_64bit(mv64x60_handle_t *bh, u32 window);
static void mv64360_disable_window_64bit(mv64x60_handle_t *bh, u32 window);
static void mv64360_disable_all_windows(mv64x60_handle_t *bh,
					mv64x60_setup_info_t *si);
static void mv64360_chip_specific_init(mv64x60_handle_t *bh,
	mv64x60_setup_info_t *si);
static void mv64460_chip_specific_init(mv64x60_handle_t *bh,
	mv64x60_setup_info_t *si);


u8	mv64x60_pci_exclude_bridge = TRUE;

spinlock_t mv64x60_lock = SPIN_LOCK_UNLOCKED;
spinlock_t mv64x60_rmw_lock = SPIN_LOCK_UNLOCKED;

static mv64x60_32bit_window_t gt64260_32bit_windows[] __initdata = {
	/* CPU->MEM Windows */
	[MV64x60_CPU2MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_0_BASE,
		.size_reg		= MV64x60_CPU2MEM_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_1_BASE,
		.size_reg		= MV64x60_CPU2MEM_1_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_2_BASE,
		.size_reg		= MV64x60_CPU2MEM_2_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_3_BASE,
		.size_reg		= MV64x60_CPU2MEM_3_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->Device Windows */
	[MV64x60_CPU2DEV_0_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_0_BASE,
		.size_reg		= MV64x60_CPU2DEV_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2DEV_1_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_1_BASE,
		.size_reg		= MV64x60_CPU2DEV_1_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2DEV_2_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_2_BASE,
		.size_reg		= MV64x60_CPU2DEV_2_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2DEV_3_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_3_BASE,
		.size_reg		= MV64x60_CPU2DEV_3_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->Boot Window */
	[MV64x60_CPU2BOOT_WIN] = {
		.base_reg		= MV64x60_CPU2BOOT_0_BASE,
		.size_reg		= MV64x60_CPU2BOOT_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 0 Windows */
	[MV64x60_CPU2PCI0_IO_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_IO_BASE,
		.size_reg		= MV64x60_CPU2PCI0_IO_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_0_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_1_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_1_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_2_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_2_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_3_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_3_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 Windows */
	[MV64x60_CPU2PCI1_IO_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_IO_BASE,
		.size_reg		= MV64x60_CPU2PCI1_IO_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_0_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_0_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_1_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_1_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_2_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_2_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_3_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_3_SIZE,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->SRAM Window (64260 has no integrated SRAM) */
	/* CPU->PCI 0 Remap I/O Window */
	[MV64x60_CPU2PCI0_IO_REMAP_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_IO_REMAP,
		.size_reg		= 0,
		.base_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 Remap I/O Window */
	[MV64x60_CPU2PCI1_IO_REMAP_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_IO_REMAP,
		.size_reg		= 0,
		.base_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU Memory Protection Windows */
	[MV64x60_CPU_PROT_0_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_0,
		.size_reg		= MV64x60_CPU_PROT_SIZE_0,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_PROT_1_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_1,
		.size_reg		= MV64x60_CPU_PROT_SIZE_1,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_PROT_2_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_2,
		.size_reg		= MV64x60_CPU_PROT_SIZE_2,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_PROT_3_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_3,
		.size_reg		= MV64x60_CPU_PROT_SIZE_3,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU Snoop Windows */
	[MV64x60_CPU_SNOOP_0_WIN] = {
		.base_reg		= GT64260_CPU_SNOOP_BASE_0,
		.size_reg		= GT64260_CPU_SNOOP_SIZE_0,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_SNOOP_1_WIN] = {
		.base_reg		= GT64260_CPU_SNOOP_BASE_1,
		.size_reg		= GT64260_CPU_SNOOP_SIZE_1,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_SNOOP_2_WIN] = {
		.base_reg		= GT64260_CPU_SNOOP_BASE_2,
		.size_reg		= GT64260_CPU_SNOOP_SIZE_2,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU_SNOOP_3_WIN] = {
		.base_reg		= GT64260_CPU_SNOOP_BASE_3,
		.size_reg		= GT64260_CPU_SNOOP_SIZE_3,
		.base_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 0->System Memory Remap Windows */
	[MV64x60_PCI02MEM_REMAP_0_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_0_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_1_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_2_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_3_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	/* PCI 1->System Memory Remap Windows */
	[MV64x60_PCI12MEM_REMAP_0_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_0_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_1_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_2_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_3_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 20,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
};

static mv64x60_64bit_window_t gt64260_64bit_windows[] __initdata = {
	/* CPU->PCI 0 MEM Remap Windows */
	[MV64x60_CPU2PCI0_MEM_0_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_0_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_0_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_1_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_1_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_1_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_2_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_2_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_2_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_3_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_3_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_3_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 MEM Remap Windows */
	[MV64x60_CPU2PCI1_MEM_0_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_0_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_0_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_1_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_1_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_1_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_2_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_2_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_2_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_3_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_3_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_3_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 12,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 0->MEM Access Control Windows */
	[MV64x60_PCI02MEM_ACC_CNTL_0_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_0_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_0_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_0_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_1_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_1_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_1_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_1_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_2_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_2_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_2_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_2_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_3_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_3_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_3_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_3_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 1->MEM Access Control Windows */
	[MV64x60_PCI12MEM_ACC_CNTL_0_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_0_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_0_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_0_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_1_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_1_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_1_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_1_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_2_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_2_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_2_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_2_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_3_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_3_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_3_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_3_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 0->MEM Snoop Windows */
	[MV64x60_PCI02MEM_SNOOP_0_WIN] = {
		.base_hi_reg		= GT64260_PCI0_SNOOP_0_BASE_HI,
		.base_lo_reg		= GT64260_PCI0_SNOOP_0_BASE_LO,
		.size_reg		= GT64260_PCI0_SNOOP_0_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_SNOOP_1_WIN] = {
		.base_hi_reg		= GT64260_PCI0_SNOOP_1_BASE_HI,
		.base_lo_reg		= GT64260_PCI0_SNOOP_1_BASE_LO,
		.size_reg		= GT64260_PCI0_SNOOP_1_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_SNOOP_2_WIN] = {
		.base_hi_reg		= GT64260_PCI0_SNOOP_2_BASE_HI,
		.base_lo_reg		= GT64260_PCI0_SNOOP_2_BASE_LO,
		.size_reg		= GT64260_PCI0_SNOOP_2_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI02MEM_SNOOP_3_WIN] = {
		.base_hi_reg		= GT64260_PCI0_SNOOP_3_BASE_HI,
		.base_lo_reg		= GT64260_PCI0_SNOOP_3_BASE_LO,
		.size_reg		= GT64260_PCI0_SNOOP_3_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 1->MEM Snoop Windows */
	[MV64x60_PCI12MEM_SNOOP_0_WIN] = {
		.base_hi_reg		= GT64260_PCI1_SNOOP_0_BASE_HI,
		.base_lo_reg		= GT64260_PCI1_SNOOP_0_BASE_LO,
		.size_reg		= GT64260_PCI1_SNOOP_0_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_SNOOP_1_WIN] = {
		.base_hi_reg		= GT64260_PCI1_SNOOP_1_BASE_HI,
		.base_lo_reg		= GT64260_PCI1_SNOOP_1_BASE_LO,
		.size_reg		= GT64260_PCI1_SNOOP_1_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_SNOOP_2_WIN] = {
		.base_hi_reg		= GT64260_PCI1_SNOOP_2_BASE_HI,
		.base_lo_reg		= GT64260_PCI1_SNOOP_2_BASE_LO,
		.size_reg		= GT64260_PCI1_SNOOP_2_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_PCI12MEM_SNOOP_3_WIN] = {
		.base_hi_reg		= GT64260_PCI1_SNOOP_3_BASE_HI,
		.base_lo_reg		= GT64260_PCI1_SNOOP_3_BASE_LO,
		.size_reg		= GT64260_PCI1_SNOOP_3_SIZE,
		.base_lo_bits		= 12,
		.size_bits		= 12,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
};

static mv64x60_chip_info_t gt64260a_ci __initdata = {
	.translate_size		= gt64260_translate_size,
	.untranslate_size	= gt64260_untranslate_size,
	.set_pci2mem_window	= gt64260_set_pci2mem_window,
	.is_enabled_32bit	= gt64260_is_enabled_32bit,
	.enable_window_32bit	= gt64260_enable_window_32bit,
	.disable_window_32bit	= gt64260_disable_window_32bit,
	.enable_window_64bit	= gt64260_enable_window_64bit,
	.disable_window_64bit	= gt64260_disable_window_64bit,
	.disable_all_windows	= gt64260_disable_all_windows,
	.chip_specific_init	= gt64260a_chip_specific_init,
	.window_tab_32bit	= gt64260_32bit_windows,
	.window_tab_64bit	= gt64260_64bit_windows,
};

static mv64x60_chip_info_t gt64260b_ci __initdata = {
	.translate_size		= gt64260_translate_size,
	.untranslate_size	= gt64260_untranslate_size,
	.set_pci2mem_window	= gt64260_set_pci2mem_window,
	.is_enabled_32bit	= gt64260_is_enabled_32bit,
	.enable_window_32bit	= gt64260_enable_window_32bit,
	.disable_window_32bit	= gt64260_disable_window_32bit,
	.enable_window_64bit	= gt64260_enable_window_64bit,
	.disable_window_64bit	= gt64260_disable_window_64bit,
	.disable_all_windows	= gt64260_disable_all_windows,
	.chip_specific_init	= gt64260b_chip_specific_init,
	.window_tab_32bit	= gt64260_32bit_windows,
	.window_tab_64bit	= gt64260_64bit_windows,
};


static mv64x60_32bit_window_t mv64360_32bit_windows[] __initdata = {
	/* CPU->MEM Windows */
	[MV64x60_CPU2MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_0_BASE,
		.size_reg		= MV64x60_CPU2MEM_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_1_BASE,
		.size_reg		= MV64x60_CPU2MEM_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 1 },
	[MV64x60_CPU2MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_2_BASE,
		.size_reg		= MV64x60_CPU2MEM_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 2 },
	[MV64x60_CPU2MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2MEM_3_BASE,
		.size_reg		= MV64x60_CPU2MEM_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 3 },
	/* CPU->Device Windows */
	[MV64x60_CPU2DEV_0_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_0_BASE,
		.size_reg		= MV64x60_CPU2DEV_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 4 },
	[MV64x60_CPU2DEV_1_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_1_BASE,
		.size_reg		= MV64x60_CPU2DEV_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 5 },
	[MV64x60_CPU2DEV_2_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_2_BASE,
		.size_reg		= MV64x60_CPU2DEV_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 6 },
	[MV64x60_CPU2DEV_3_WIN] = {
		.base_reg		= MV64x60_CPU2DEV_3_BASE,
		.size_reg		= MV64x60_CPU2DEV_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 7 },
	/* CPU->Boot Window */
	[MV64x60_CPU2BOOT_WIN] = {
		.base_reg		= MV64x60_CPU2BOOT_0_BASE,
		.size_reg		= MV64x60_CPU2BOOT_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 8 },
	/* CPU->PCI 0 Windows */
	[MV64x60_CPU2PCI0_IO_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_IO_BASE,
		.size_reg		= MV64x60_CPU2PCI0_IO_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 9 },
	[MV64x60_CPU2PCI0_MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_0_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 10 },
	[MV64x60_CPU2PCI0_MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_1_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 11 },
	[MV64x60_CPU2PCI0_MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_2_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 12 },
	[MV64x60_CPU2PCI0_MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_MEM_3_BASE,
		.size_reg		= MV64x60_CPU2PCI0_MEM_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 13 },
	/* CPU->PCI 1 Windows */
	[MV64x60_CPU2PCI1_IO_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_IO_BASE,
		.size_reg		= MV64x60_CPU2PCI1_IO_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 14 },
	[MV64x60_CPU2PCI1_MEM_0_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_0_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_0_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 15 },
	[MV64x60_CPU2PCI1_MEM_1_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_1_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_1_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 16 },
	[MV64x60_CPU2PCI1_MEM_2_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_2_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_2_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 17 },
	[MV64x60_CPU2PCI1_MEM_3_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_MEM_3_BASE,
		.size_reg		= MV64x60_CPU2PCI1_MEM_3_SIZE,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 18 },
	/* CPU->SRAM Window */
	[MV64x60_CPU2SRAM_WIN] = {
		.base_reg		= MV64360_CPU2SRAM_BASE,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 19 },
	/* CPU->PCI 0 Remap I/O Window */
	[MV64x60_CPU2PCI0_IO_REMAP_WIN] = {
		.base_reg		= MV64x60_CPU2PCI0_IO_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 Remap I/O Window */
	[MV64x60_CPU2PCI1_IO_REMAP_WIN] = {
		.base_reg		= MV64x60_CPU2PCI1_IO_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU Memory Protection Windows */
	[MV64x60_CPU_PROT_0_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_0,
		.size_reg		= MV64x60_CPU_PROT_SIZE_0,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0x80000000 | 31 },
	[MV64x60_CPU_PROT_1_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_1,
		.size_reg		= MV64x60_CPU_PROT_SIZE_1,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0x80000000 | 31 },
	[MV64x60_CPU_PROT_2_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_2,
		.size_reg		= MV64x60_CPU_PROT_SIZE_2,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0x80000000 | 31 },
	[MV64x60_CPU_PROT_3_WIN] = {
		.base_reg		= MV64x60_CPU_PROT_BASE_3,
		.size_reg		= MV64x60_CPU_PROT_SIZE_3,
		.base_bits		= 16,
		.size_bits		= 16,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0x80000000 | 31 },
	/* CPU Snoop Windows -- don't exist on 64360 */
	/* PCI 0->System Memory Remap Windows */
	[MV64x60_PCI02MEM_REMAP_0_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_0_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_1_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_2_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI02MEM_REMAP_3_WIN] = {
		.base_reg		= MV64x60_PCI0_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	/* PCI 1->System Memory Remap Windows */
	[MV64x60_PCI12MEM_REMAP_0_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_0_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_1_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_2_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
	[MV64x60_PCI12MEM_REMAP_3_WIN] = {
		.base_reg		= MV64x60_PCI1_SLAVE_MEM_1_REMAP,
		.size_reg		= 0,
		.base_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0 },
};

static mv64x60_64bit_window_t mv64360_64bit_windows[MV64x60_64BIT_WIN_COUNT]
								__initdata = {
	/* CPU->PCI 0 MEM Remap Windows */
	[MV64x60_CPU2PCI0_MEM_0_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_0_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_0_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_1_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_1_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_1_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_2_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_2_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_2_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI0_MEM_3_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI0_MEM_3_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI0_MEM_3_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* CPU->PCI 1 MEM Remap Windows */
	[MV64x60_CPU2PCI1_MEM_0_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_0_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_0_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_1_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_1_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_1_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_2_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_2_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_2_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	[MV64x60_CPU2PCI1_MEM_3_REMAP_WIN] = {
		.base_hi_reg		= MV64x60_CPU2PCI1_MEM_3_REMAP_HI,
		.base_lo_reg		= MV64x60_CPU2PCI1_MEM_3_REMAP_LO,
		.size_reg		= 0,
		.base_lo_bits		= 16,
		.size_bits		= 0,
		.get_from_field		= mv64x60_shift_left,
		.map_to_field		= mv64x60_shift_right,
		.extra			= 0 },
	/* PCI 0->MEM Access Control Windows */
	[MV64x60_PCI02MEM_ACC_CNTL_0_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_0_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_0_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_0_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0x80000000 | 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_1_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_1_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_1_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_1_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0x80000000 | 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_2_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_2_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_2_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_2_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0x80000000 | 0 },
	[MV64x60_PCI02MEM_ACC_CNTL_3_WIN] = {
		.base_hi_reg		= MV64x60_PCI0_ACC_CNTL_3_BASE_HI,
		.base_lo_reg		= MV64x60_PCI0_ACC_CNTL_3_BASE_LO,
		.size_reg		= MV64x60_PCI0_ACC_CNTL_3_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0x80000000 | 0 },
	/* PCI 1->MEM Access Control Windows */
	[MV64x60_PCI12MEM_ACC_CNTL_0_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_0_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_0_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_0_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0x80000000 | 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_1_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_1_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_1_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_1_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0x80000000 | 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_2_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_2_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_2_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_2_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0x80000000 | 0 },
	[MV64x60_PCI12MEM_ACC_CNTL_3_WIN] = {
		.base_hi_reg		= MV64x60_PCI1_ACC_CNTL_3_BASE_HI,
		.base_lo_reg		= MV64x60_PCI1_ACC_CNTL_3_BASE_LO,
		.size_reg		= MV64x60_PCI1_ACC_CNTL_3_SIZE,
		.base_lo_bits		= 20,
		.size_bits		= 20,
		.get_from_field		= mv64x60_mask,
		.map_to_field		= mv64x60_mask,
		.extra			= 0x80000000 | 0 },
	/* PCI 0->MEM Snoop Windows -- don't exist on 64360 */
	/* PCI 1->MEM Snoop Windows -- don't exist on 64360 */
};

static mv64x60_chip_info_t mv64360_ci __initdata = {
	.translate_size		= mv64360_translate_size,
	.untranslate_size	= mv64360_untranslate_size,
	.set_pci2mem_window	= mv64360_set_pci2mem_window,
	.is_enabled_32bit	= mv64360_is_enabled_32bit,
	.enable_window_32bit	= mv64360_enable_window_32bit,
	.disable_window_32bit	= mv64360_disable_window_32bit,
	.enable_window_64bit	= mv64360_enable_window_64bit,
	.disable_window_64bit	= mv64360_disable_window_64bit,
	.disable_all_windows	= mv64360_disable_all_windows,
	.chip_specific_init	= mv64360_chip_specific_init,
	.window_tab_32bit	= mv64360_32bit_windows,
	.window_tab_64bit	= mv64360_64bit_windows,
};

static mv64x60_chip_info_t mv64460_ci __initdata = {
	.translate_size		= mv64360_translate_size,
	.untranslate_size	= mv64360_untranslate_size,
	.set_pci2mem_window	= mv64360_set_pci2mem_window,
	.is_enabled_32bit	= mv64360_is_enabled_32bit,
	.enable_window_32bit	= mv64360_enable_window_32bit,
	.disable_window_32bit	= mv64360_disable_window_32bit,
	.enable_window_64bit	= mv64360_enable_window_64bit,
	.disable_window_64bit	= mv64360_disable_window_64bit,
	.disable_all_windows	= mv64360_disable_all_windows,
	.chip_specific_init	= mv64460_chip_specific_init,
	.window_tab_32bit	= mv64360_32bit_windows,
	.window_tab_64bit	= mv64360_64bit_windows,
};


/*
 *****************************************************************************
 *
 *	Bridge Initialization Routines
 *
 *****************************************************************************
 */
/*
 * mv64x60_init()
 *
 * Initialze the bridge based on setting passed in via 'si'.  The bridge
 * handle, 'bh', will be set so that it can be used to make subsequent
 * calls to routines in this file.
 */
int __init
mv64x60_init(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	u32	mem_windows[MV64x60_CPU2MEM_WINDOWS][2];
	int	rc = 0;

	if (ppc_md.progress)
		ppc_md.progress("mv64x60_init: Enter", 0x0);

	mv64x60_early_init(bh, si);
	mv64x60_alloc_hoses(bh, si); /* Allocate pci hose structures */
	if (mv64x60_get_type(bh))
		return -1;

	if (mv64x60_setup_for_chip(bh) != 0) {
		iounmap((void *)bh->v_base);

		if (ppc_md.progress)
			ppc_md.progress("mv64x60_init: Exit--error", 0x0);
		return -1;
	}

	bh->ci->disable_all_windows(bh, si); /* Disable windows except mem ctlr */
	mv64x60_config_cpu2pci_windows(bh, si); /* Init CPU->PCI windows */
	mv64x60_get_mem_windows(bh, mem_windows); /* Read mem ctlr regs */
	mv64x60_config_cpu2mem_windows(bh, si, mem_windows); /* CPU->MEM setup*/
	mv64x60_config_pci2mem_windows(bh, si, mem_windows); /* PCI->Sys MEM */
	mv64x60_init_hoses(bh, si); /* Init hose structs & PCI params */
	bh->ci->chip_specific_init(bh, si);
	mv64x60_enumerate_buses(bh, si); /* Enumerate PCI buses */
	ocp_for_each_device(mv64x60_fixup_ocp, (void *)bh);

	if (ppc_md.progress)
		ppc_md.progress("mv64x60_init: Exit", 0x0);

	return rc;
} /* mv64x60_init() */

/*
 *****************************************************************************
 *
 *	Pre-Bridge-Init Routines (Externally Visible)
 *
 *****************************************************************************
 */
/*
 * mv64x60_get_mem_size()
 *
 * Calculate the amount of memory that the memory controller is set up for.
 * This should only be used by board-specific code if there is no other
 * way to determine the amount of memory in the system.
 */
u32 __init
mv64x60_get_mem_size(u32 bridge_base, u32 chip_type)
{
	mv64x60_handle_t	bh;
	u32			mem_windows[MV64x60_CPU2MEM_WINDOWS][2];

	memset(&bh, 0, sizeof(bh));

	bh.type = chip_type;
	bh.p_base = bridge_base;
	bh.v_base = bridge_base;

	(void)mv64x60_setup_for_chip(&bh);
	mv64x60_get_mem_windows(&bh, mem_windows);
	return mv64x60_calc_mem_size(&bh, mem_windows);
}

/*
 *****************************************************************************
 *
 *	Window Config Routines (Externally Visible)
 *
 *****************************************************************************
 */
/*
 * mv64x60_get_32bit_window()
 *
 * Determine the base address and size of a 32-bit window on the bridge.
 */
void __init
mv64x60_get_32bit_window(mv64x60_handle_t *bh, u32 window, u32 *base, u32 *size)
{
	u32	val, base_reg, size_reg, base_bits, size_bits;
	u32	(*get_from_field)(u32 val, u32 num_bits);

	base_reg  = bh->ci->window_tab_32bit[window].base_reg;

	if (base_reg != 0) {
		size_reg  = bh->ci->window_tab_32bit[window].size_reg;
		base_bits = bh->ci->window_tab_32bit[window].base_bits;
		size_bits = bh->ci->window_tab_32bit[window].size_bits;
		get_from_field= bh->ci->window_tab_32bit[window].get_from_field;

		val = mv64x60_read(bh, base_reg);
		*base = get_from_field(val, base_bits);

		if (size_reg != 0) {
			val = mv64x60_read(bh, size_reg);
			val = get_from_field(val, size_bits);
			*size = bh->ci->untranslate_size(*base, val, size_bits);
		}
		else {
			*size = 0;
		}
	}
	else {
		*base = 0;
		*size = 0;
	}

	DBG("get 32bit window: %d, base: 0x%x, size: 0x%x\n",
		window, *base, *size);

	return;
}

/*
 * mv64x60_set_32bit_window()
 *
 * Set the base address and size of a 32-bit window on the bridge.
 */
void __init
mv64x60_set_32bit_window(mv64x60_handle_t *bh, u32 window, u32 base, u32 size,
								u32 other_bits)
{
	u32	val, base_reg, size_reg, base_bits, size_bits;
	u32	(*map_to_field)(u32 val, u32 num_bits);

	DBG("set 32bit window: %d, base: 0x%x, size: 0x%x, other: 0x%x\n",
		window, base, size, other_bits);

	base_reg  = bh->ci->window_tab_32bit[window].base_reg;

	if (base_reg != 0) {
		size_reg  = bh->ci->window_tab_32bit[window].size_reg;
		base_bits = bh->ci->window_tab_32bit[window].base_bits;
		size_bits = bh->ci->window_tab_32bit[window].size_bits;
		map_to_field = bh->ci->window_tab_32bit[window].map_to_field;

		val = map_to_field(base, base_bits) | other_bits;
		mv64x60_write(bh, base_reg, val);

		if (size_reg != 0) {
			val = bh->ci->translate_size(base, size, size_bits);
			val = map_to_field(val, size_bits);
			mv64x60_write(bh, size_reg, val);
		}
		(void)mv64x60_read(bh, base_reg); /* Flush FIFO */
	}

	return;
}

/*
 * mv64x60_get_64bit_window()
 *
 * Determine the base address and size of a 64-bit window on the bridge.
 */
void __init
mv64x60_get_64bit_window(mv64x60_handle_t *bh, u32 window, u32 *base_hi,
							u32 *base_lo, u32 *size)
{
	u32	val, base_lo_reg, size_reg, base_lo_bits, size_bits;
	u32	(*get_from_field)(u32 val, u32 num_bits);

	base_lo_reg = bh->ci->window_tab_64bit[window].base_lo_reg;

	if (base_lo_reg != 0) {
		size_reg = bh->ci->window_tab_64bit[window].size_reg;
		base_lo_bits = bh->ci->window_tab_64bit[window].base_lo_bits;
		size_bits = bh->ci->window_tab_64bit[window].size_bits;
		get_from_field= bh->ci->window_tab_64bit[window].get_from_field;

		*base_hi = mv64x60_read(bh, 
			bh->ci->window_tab_64bit[window].base_hi_reg);

		val = mv64x60_read(bh, base_lo_reg);
		*base_lo = get_from_field(val, base_lo_bits);

		if (size_reg != 0) {
			val = mv64x60_read(bh, size_reg);
			val = get_from_field(val, size_bits);
			*size = bh->ci->untranslate_size(*base_lo, val,
								size_bits);
		}
		else {
			*size = 0;
		}
	}
	else {
		*base_hi = 0;
		*base_lo = 0;
		*size = 0;
	}

	DBG("get 64bit window: %d, base hi: 0x%x, base lo: 0x%x, size: 0x%x\n",
		window, *base_hi, *base_lo, *size);

	return;
}

/*
 * mv64x60_set_64bit_window()
 *
 * Set the base address and size of a 64-bit window on the bridge.
 */
void __init
mv64x60_set_64bit_window(mv64x60_handle_t *bh, u32 window,
			u32 base_hi, u32 base_lo, u32 size, u32 other_bits)
{
	u32	val, base_lo_reg, size_reg, base_lo_bits, size_bits;
	u32	(*map_to_field)(u32 val, u32 num_bits);

	DBG("set 64bit window: %d, base hi: 0x%x, base lo: 0x%x, " \
		"size: 0x%x, other: 0x%x\n",
		window, base_hi, base_lo, size, other_bits);

	base_lo_reg = bh->ci->window_tab_64bit[window].base_lo_reg;

	if (base_lo_reg != 0) {
		size_reg = bh->ci->window_tab_64bit[window].size_reg;
		base_lo_bits = bh->ci->window_tab_64bit[window].base_lo_bits;
		size_bits = bh->ci->window_tab_64bit[window].size_bits;
		map_to_field = bh->ci->window_tab_64bit[window].map_to_field;

		mv64x60_write(bh, bh->ci->window_tab_64bit[window].base_hi_reg,
			base_hi);

		val = map_to_field(base_lo, base_lo_bits) | other_bits;
		mv64x60_write(bh, base_lo_reg, val);

		if (size_reg != 0) {
			val = bh->ci->translate_size(base_lo, size, size_bits);
			val = map_to_field(val, size_bits);
			mv64x60_write(bh, size_reg, val);
		}

		(void)mv64x60_read(bh, base_lo_reg); /* Flush FIFO */
	}

	return;
}

/*
 * mv64x60_mask()
 *
 * Take the high-order 'num_bits' of 'val' & mask off low bits.
 */
static u32 __init
mv64x60_mask(u32 val, u32 num_bits)
{
	DBG("mask val: 0x%x, num_bits: %d == 0x%x\n", val,
		num_bits, val & (0xffffffff << (32 - num_bits)));

	return val & (0xffffffff << (32 - num_bits));
}

/*
 * mv64x60_mask_shift_left()
 *
 * Take the low-order 'num_bits' of 'val', shift left to align at bit 31 (MSB).
 */
static u32 __init
mv64x60_shift_left(u32 val, u32 num_bits)
{
	DBG("shift left val: 0x%x, num_bits: %d == 0x%x\n", val,
		num_bits, val << (32 - num_bits));

	return val << (32 - num_bits);
}

/*
 * mv64x60_shift_right()
 *
 * Take the high-order 'num_bits' of 'val', shift right to align at bit 0 (LSB).
 */
static u32 __init
mv64x60_shift_right(u32 val, u32 num_bits)
{
	DBG("shift right val: 0x%x, num_bits: %d == 0x%x\n", val, num_bits,
		val >> (32 - num_bits));

	return val >> (32 - num_bits);
}

/*
 *****************************************************************************
 *
 *	Early Init Routines
 *
 *****************************************************************************
 */
/*
 * mv64x60_early_init()
 *
 * Do some bridge work that must take place before we start messing with
 * the bridge for real.
 */
static void __init
mv64x60_early_init(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	memset(bh, 0, sizeof(*bh));

	bh->p_base = si->phys_reg_base;
	bh->v_base = (u32)ioremap(bh->p_base, MV64x60_INTERNAL_SPACE_SIZE);
	bh->base_irq = si->base_irq;

	/* Bit 12 MUST be 0; set bit 27--don't auto-update cpu remap regs */
	mv64x60_clr_bits(bh, MV64x60_CPU_CONFIG, (1<<12));
	mv64x60_set_bits(bh, MV64x60_CPU_CONFIG, (1<<27));

	/*
	 * Turn off timer/counters.  Not turning off watchdog timer because
	 * can't read its reg on the 64260A so don't know if we'll be enabling
	 * or disabling.
	 */
	mv64x60_clr_bits(bh, MV64x60_TIMR_CNTR_0_3_CNTL,
			((1<<0) | (1<<8) | (1<<16) | (1<<24)));

#ifdef	CONFIG_GT64260
	mv64x60_clr_bits(bh, GT64260_TIMR_CNTR_4_7_CNTL,
			((1<<0) | (1<<8) | (1<<16) | (1<<24)));
#endif

#if 0
XXXX Put in PCI_x_RETRY adjustment XXXX
#endif

	return;
}

/*
 *****************************************************************************
 *
 *	Chip Identification Routines
 *
 *****************************************************************************
 */
/*
 * mv64x60_get_type()
 *
 * Determine the type of bridge chip we have.
 */
static int __init mv64x60_get_type(struct mv64x60_handle *bh)
{
	struct pci_controller *hose = bh->hose_a;
	int pcidev;
	int devfn;
	u16 val;
	u8 save_exclude;

	pcidev = (mv64x60_read(bh, MV64x60_PCI0_P2P_CONFIG) >> 24) & 0xf;
	devfn = PCI_DEVFN(pcidev, 0); 

	save_exclude = mv64x60_pci_exclude_bridge;
	mv64x60_pci_exclude_bridge = FALSE;

	/* Sanity check of bridge's Vendor ID */
	early_read_config_word(hose, 0, devfn, PCI_VENDOR_ID, &val);

	if (val != PCI_VENDOR_ID_MARVELL)
		return -1;

	/* Figure out the type of Marvell bridge it is */
	early_read_config_word(hose, 0, devfn, PCI_DEVICE_ID, &val);

	switch (val) {
	case PCI_DEVICE_ID_MARVELL_GT64260:
		early_read_config_word(hose, 0, devfn,
				       PCI_CLASS_REVISION, &val);

		switch (val & 0xff) {
		case GT64260_REV_A:
			bh->type = MV64x60_TYPE_GT64260A;
			break;
		case GT64260_REV_B:
			bh->type = MV64x60_TYPE_GT64260B;
			break;
		}
		break;

	case PCI_DEVICE_ID_MARVELL_MV64360:
		/* Marvell won't tell me how to distinguish a 64361 & 64362 */
		bh->type = MV64x60_TYPE_MV64360;
		break;

	case PCI_DEVICE_ID_MARVELL_MV64460:
		bh->type = MV64x60_TYPE_MV64460;
		break;

	default:
		printk(KERN_CRIT "Unknown Marvell bridge type %04x\n", val);
		return -1;
	}

	mv64x60_pci_exclude_bridge = save_exclude;
	return 0;
}

/*
 * mv64x60_setup_for_chip()
 *
 * Set 'bh' to use the proper set of routine for the bridge chip that we have.
 */
static int __init
mv64x60_setup_for_chip(mv64x60_handle_t *bh)
{
	int	rc = 0;

	/* Set up chip-specific info based on the chip/bridge type */
	switch(bh->type) {
		case MV64x60_TYPE_GT64260A:
			bh->ci = &gt64260a_ci;
			break;

		case MV64x60_TYPE_GT64260B:
			bh->ci = &gt64260b_ci;
			break;

		case MV64x60_TYPE_MV64360:
			bh->ci = &mv64360_ci;
			break;

#if 0 /* Marvell won't tell me how to distinguish--MAG */
		case MV64x60_TYPE_MV64361:
		case MV64x60_TYPE_MV64362:
#endif
		case MV64x60_TYPE_MV64460:
			bh->ci = &mv64460_ci;
			break;

		case MV64x60_TYPE_INVALID:
		default:
			if (ppc_md.progress)
				ppc_md.progress("mv64x60: Unsupported bridge",
									0x0);
			printk("mv64x60: Unsupported bridge\n");
			rc = -1;
	}

	return rc;
}

/*
 *****************************************************************************
 *
 *	System Memory Window Related Routines
 *
 *****************************************************************************
 */
/*
 * mv64x60_get_mem_windows()
 *
 * Get the values in the memory controller & return in the 'mem_windows' array.
 */
static void __init
mv64x60_get_mem_windows(mv64x60_handle_t *bh,
	u32 mem_windows[MV64x60_CPU2MEM_WINDOWS][2])
{
	u32	i;
	u32	windows[] = { MV64x60_CPU2MEM_0_WIN, MV64x60_CPU2MEM_1_WIN,
				MV64x60_CPU2MEM_2_WIN, MV64x60_CPU2MEM_3_WIN };

	for (i=0; i<MV64x60_CPU2MEM_WINDOWS; i++) {
		if (bh->ci->is_enabled_32bit(bh, i)) {
			mv64x60_get_32bit_window(bh, windows[i],
				&mem_windows[i][0], &mem_windows[i][1]);
		}
		else {
			mem_windows[i][0] = 0;
			mem_windows[i][1] = 0;
		}
	}

	return;
}

/*
 * mv64x60_calc_mem_size()
 *
 * Using the memory controller register values in 'mem_windows', determine
 * how much memory it is set up for.
 */
static u32 __init
mv64x60_calc_mem_size(mv64x60_handle_t *bh,
	u32 mem_windows[MV64x60_CPU2MEM_WINDOWS][2])
{
	u32	i, total = 0;

	for (i=0; i<MV64x60_CPU2MEM_WINDOWS; i++) {
		total += mem_windows[i][1];
	}

	return total;
}

/*
 *****************************************************************************
 *
 *	CPU->System MEM Config Routines
 *
 *****************************************************************************
 */
/*
 * mv64x60_config_cpu2mem_windows()
 *
 * Configure CPU->Memory windows on the bridge.
 */
static void __init
mv64x60_config_cpu2mem_windows(mv64x60_handle_t *bh, mv64x60_setup_info_t *si,
	u32 mem_windows[MV64x60_CPU2MEM_WINDOWS][2])
{
	u32	i;
	u32	prot_windows[] = {
			MV64x60_CPU_PROT_0_WIN, MV64x60_CPU_PROT_1_WIN,
			MV64x60_CPU_PROT_2_WIN, MV64x60_CPU_PROT_3_WIN };
	u32	cpu_snoop_windows[] = {
			MV64x60_CPU_SNOOP_0_WIN, MV64x60_CPU_SNOOP_1_WIN,
			MV64x60_CPU_SNOOP_2_WIN, MV64x60_CPU_SNOOP_3_WIN };

	/* Set CPU protection & snoop windows */
	for (i=0; i<MV64x60_CPU2MEM_WINDOWS; i++) {
		if (bh->ci->is_enabled_32bit(bh, i)) {
			mv64x60_set_32bit_window(bh, prot_windows[i],
				mem_windows[i][0], mem_windows[i][1],
				si->cpu_prot_options[i]);
			bh->ci->enable_window_32bit(bh, prot_windows[i]);

			if (bh->ci->window_tab_32bit[cpu_snoop_windows[i]].
								base_reg != 0) {
				mv64x60_set_32bit_window(bh,
					cpu_snoop_windows[i], mem_windows[i][0],
					mem_windows[i][1],
					si->cpu_snoop_options[i]);
				bh->ci->enable_window_32bit(bh,
					cpu_snoop_windows[i]);
			}

		}
	}

	return;
}

/*
 *****************************************************************************
 *
 *	CPU->PCI Config Routines
 *
 *****************************************************************************
 */

/*
 * mv64x60_config_cpu2pci_windows()
 *
 * Configure the CPU->PCI windows on the bridge.
 */
static void __init
mv64x60_config_cpu2pci_windows(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	if (ppc_md.progress)
		ppc_md.progress("mv64x60_config_bridge: Enter", 0x0);

	/*
	 * Set up various parts of the bridge including CPU->PCI windows.
	 * Depending on the board, there may be only one hose that needs to
	 * be set up.
	 */
	if (si->pci_0.enable_bus) {
		u32	win_tab[] = { MV64x60_CPU2PCI0_IO_WIN,
				MV64x60_CPU2PCI0_MEM_0_WIN,
				MV64x60_CPU2PCI0_MEM_1_WIN,
				MV64x60_CPU2PCI0_MEM_2_WIN };
		u32	remap_tab[] = { MV64x60_CPU2PCI0_IO_REMAP_WIN,
				MV64x60_CPU2PCI0_MEM_0_REMAP_WIN,
				MV64x60_CPU2PCI0_MEM_1_REMAP_WIN,
				MV64x60_CPU2PCI0_MEM_2_REMAP_WIN };

		mv64x60_set_cpu2pci_window(bh, &si->pci_0, win_tab, remap_tab);
	}

	if (si->pci_1.enable_bus) {
		u32	win_tab[] = { MV64x60_CPU2PCI1_IO_WIN,
				MV64x60_CPU2PCI1_MEM_0_WIN,
				MV64x60_CPU2PCI1_MEM_1_WIN,
				MV64x60_CPU2PCI1_MEM_2_WIN };
		u32	remap_tab[] = { MV64x60_CPU2PCI1_IO_REMAP_WIN,
				MV64x60_CPU2PCI1_MEM_0_REMAP_WIN,
				MV64x60_CPU2PCI1_MEM_1_REMAP_WIN,
				MV64x60_CPU2PCI1_MEM_2_REMAP_WIN };

		mv64x60_set_cpu2pci_window(bh, &si->pci_1, win_tab, remap_tab);
	}

	return;
} /* mv64x60_config_bridge() */

/*
 * mv64x60_set_cpu2pci_window()
 *
 * Configure the CPU->PCI windows for one of the PCI buses.
 */
static void __init
mv64x60_set_cpu2pci_window(mv64x60_handle_t *bh, mv64x60_pci_info_t *pi,
						u32 *win_tab, u32 *remap_tab)
{
	int	i;

	if (pi->pci_io.size > 0) {
		mv64x60_set_32bit_window(bh, win_tab[0], pi->pci_io.cpu_base,
					pi->pci_io.size, pi->pci_io.swap);
		mv64x60_set_32bit_window(bh, remap_tab[0],
					pi->pci_io.pci_base_lo, 0, 0);
		bh->ci->enable_window_32bit(bh, win_tab[0]);
	}
	else { /* Actually, the window should already be disabled */
		bh->ci->disable_window_32bit(bh, win_tab[0]);
	}

	for (i=0; i<3; i++) {
		if (pi->pci_mem[i].size > 0) {
			mv64x60_set_32bit_window(bh, win_tab[i+1],
				pi->pci_mem[i].cpu_base, pi->pci_mem[i].size,
				pi->pci_mem[i].swap);
			mv64x60_set_64bit_window(bh, remap_tab[i+1],
				pi->pci_mem[i].pci_base_hi,
				pi->pci_mem[i].pci_base_lo, 0, 0);
			bh->ci->enable_window_32bit(bh, win_tab[i+1]);
		}
		else { /* Actually, the window should already be disabled */
			bh->ci->disable_window_32bit(bh, win_tab[i+1]);
		}
	}

	return;
}

/*
 *****************************************************************************
 *
 *	PCI->System MEM Config Routines
 *
 *****************************************************************************
 */
/*
 * mv64x60_config_pci2mem_windows()
 *
 * Configure the PCI->Memory windows on the bridge.
 */
static void __init
mv64x60_config_pci2mem_windows(mv64x60_handle_t *bh, mv64x60_setup_info_t *si,
	u32 mem_windows[MV64x60_CPU2MEM_WINDOWS][2])
{
	u32	i;
	u32	pci_0_acc_windows[] = {
			MV64x60_PCI02MEM_ACC_CNTL_0_WIN,
			MV64x60_PCI02MEM_ACC_CNTL_1_WIN,
			MV64x60_PCI02MEM_ACC_CNTL_2_WIN,
			MV64x60_PCI02MEM_ACC_CNTL_3_WIN };
	u32	pci_1_acc_windows[] = {
			MV64x60_PCI12MEM_ACC_CNTL_0_WIN,
			MV64x60_PCI12MEM_ACC_CNTL_1_WIN,
			MV64x60_PCI12MEM_ACC_CNTL_2_WIN,
			MV64x60_PCI12MEM_ACC_CNTL_3_WIN };
	u32	pci_0_snoop_windows[] = {
			MV64x60_PCI02MEM_SNOOP_0_WIN,
			MV64x60_PCI02MEM_SNOOP_1_WIN,
			MV64x60_PCI02MEM_SNOOP_2_WIN,
			MV64x60_PCI02MEM_SNOOP_3_WIN };
	u32	pci_1_snoop_windows[] = {
			MV64x60_PCI12MEM_SNOOP_0_WIN,
			MV64x60_PCI12MEM_SNOOP_1_WIN,
			MV64x60_PCI12MEM_SNOOP_2_WIN,
			MV64x60_PCI12MEM_SNOOP_3_WIN };
	u32	pci_0_size[] = {
			MV64x60_PCI0_MEM_0_SIZE, MV64x60_PCI0_MEM_1_SIZE,
			MV64x60_PCI0_MEM_2_SIZE, MV64x60_PCI0_MEM_3_SIZE };
	u32	pci_1_size[] = {
			MV64x60_PCI1_MEM_0_SIZE, MV64x60_PCI1_MEM_1_SIZE,
			MV64x60_PCI1_MEM_2_SIZE, MV64x60_PCI1_MEM_3_SIZE };

	/* Clear bit 0 of PCI addr decode control so PCI->CPU remap 1:1 */
	mv64x60_clr_bits(bh, MV64x60_PCI0_PCI_DECODE_CNTL, 0x00000001);
	mv64x60_clr_bits(bh, MV64x60_PCI1_PCI_DECODE_CNTL, 0x00000001);

	/*
	 * Set the access control, snoop, BAR size, and window base addresses.
	 * PCI->MEM windows base addresses will match exactly what the
	 * CPU->MEM windows are.
	 */
	for (i=0; i<MV64x60_CPU2MEM_WINDOWS; i++) {
		if (bh->ci->is_enabled_32bit(bh, i)) {
			if (si->pci_0.enable_bus) {
				mv64x60_set_64bit_window(bh,
					pci_0_acc_windows[i], 0,
					mem_windows[i][0], mem_windows[i][1],
					si->pci_0.acc_cntl_options[i]);
				bh->ci->enable_window_64bit(bh,
					pci_0_acc_windows[i]);

				if (bh->ci->window_tab_64bit[
					pci_0_snoop_windows[i]].base_lo_reg
									!= 0) {
					mv64x60_set_64bit_window(bh,
						pci_0_snoop_windows[i], 0,
						mem_windows[i][0],
						mem_windows[i][1],
						si->pci_0.snoop_options[i]);
					bh->ci->enable_window_64bit(bh,
						pci_0_snoop_windows[i]);
				}

				bh->ci->set_pci2mem_window(bh->hose_a, i,
					mem_windows[i][0]);
				mv64x60_write(bh, pci_0_size[i],
					mv64x60_mask(mem_windows[i][1] -1, 20));

				/* Enable the window */
				mv64x60_clr_bits(bh, MV64x60_PCI0_BAR_ENABLE,
									1 << i);
			}
			if (si->pci_1.enable_bus) {
				mv64x60_set_64bit_window(bh,
					pci_1_acc_windows[i], 0,
					mem_windows[i][0], mem_windows[i][1],
					si->pci_1.acc_cntl_options[i]);
				bh->ci->enable_window_64bit(bh,
					pci_1_acc_windows[i]);

				if (bh->ci->window_tab_64bit[
					pci_1_snoop_windows[i]].base_lo_reg
									!= 0) {
					mv64x60_set_64bit_window(bh,
						pci_1_snoop_windows[i], 0,
						mem_windows[i][0],
						mem_windows[i][1],
						si->pci_1.snoop_options[i]);
					bh->ci->enable_window_64bit(bh,
						pci_1_snoop_windows[i]);
				}

				bh->ci->set_pci2mem_window(bh->hose_b, i,
					mem_windows[i][0]);
				mv64x60_write(bh, pci_1_size[i],
					mv64x60_mask(mem_windows[i][1] -1, 20));

				/* Enable the window */
				mv64x60_clr_bits(bh, MV64x60_PCI1_BAR_ENABLE,
									1 << i);
			}
		}
	}

	return;
}

/*
 *****************************************************************************
 *
 *	Hose & Resource Alloc/Init Routines
 *
 *****************************************************************************
 */
/*
 * mv64x60_alloc_hoses()
 *
 * Allocate the PCI hose structures for the bridge's PCI buses.
 */
static void __init
mv64x60_alloc_hoses(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	/*
	 * Alloc first hose struct even when its not to be configured b/c the
	 * chip identification routines need to use it.
	 */
	bh->hose_a = pcibios_alloc_controller();
	setup_indirect_pci(bh->hose_a,
		bh->p_base + MV64x60_PCI0_CONFIG_ADDR,
		bh->p_base + MV64x60_PCI0_CONFIG_DATA);

	if (si->pci_1.enable_bus) {
		bh->hose_b = pcibios_alloc_controller();
		setup_indirect_pci(bh->hose_b,
			bh->p_base + MV64x60_PCI1_CONFIG_ADDR,
			bh->p_base + MV64x60_PCI1_CONFIG_DATA);
	}

	return;
}

/*
 * mv64x60_init_hoses()
 *
 * Initialize the PCI hose structures for the bridge's PCI hoses.
 */
static void __init
mv64x60_init_hoses(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	if (si->pci_1.enable_bus) {
		bh->io_base_b = (u32)ioremap(si->pci_1.pci_io.cpu_base,
							si->pci_1.pci_io.size);
		isa_io_base = bh->io_base_b;
	}

	if (si->pci_0.enable_bus) {
		bh->io_base_a = (u32)ioremap(si->pci_0.pci_io.cpu_base,
							si->pci_0.pci_io.size);
		isa_io_base = bh->io_base_a;

		mv64x60_init_resources(bh->hose_a, &si->pci_0, bh->io_base_a);
		mv64x60_set_pci_params(bh->hose_a, &si->pci_0);
	}

	/* Must do here so proper isa_io_base is used in calculations */
	if (si->pci_1.enable_bus) {
		mv64x60_init_resources(bh->hose_b, &si->pci_1, bh->io_base_b);
		mv64x60_set_pci_params(bh->hose_b, &si->pci_1);
	}

	return;
}

/*
 * mv64x60_init_resources()
 *
 * Calculate the offsets, etc. for the hose structures to reflect all of
 * the address remapping that happens as you go from CPU->PCI and PCI->MEM.
 */
static void __init
mv64x60_init_resources(struct pci_controller *hose, mv64x60_pci_info_t *pi,
				u32 io_base)
{
	int		i;
	/* 2 hoses; 4 resources/hose; sting <= 64 bytes; not work if > 1 chip */
	static char	s[2][4][64];

	if (pi->pci_io.size != 0) {
		sprintf(s[hose->index][0], "PCI hose %d I/O Space",
			hose->index);
		pci_init_resource(&hose->io_resource, io_base - isa_io_base,
			io_base - isa_io_base + pi->pci_io.size - 1,
			IORESOURCE_IO, s[hose->index][0]);
		hose->io_space.start = pi->pci_io.pci_base_lo;
		hose->io_space.end = pi->pci_io.pci_base_lo + pi->pci_io.size-1;
		hose->io_base_virt = (void *)isa_io_base;
	}

	for (i=0; i<3; i++) {
		if (pi->pci_mem[i].size != 0) {
			sprintf(s[hose->index][i+1], "PCI hose %d MEM Space %d",
				hose->index, i);
			pci_init_resource(&hose->mem_resources[i],
				pi->pci_mem[i].cpu_base,
				pi->pci_mem[i].cpu_base + pi->pci_mem[i].size-1,
				IORESOURCE_MEM, s[hose->index][i+1]);
		}
	}
	
	hose->mem_space.end = pi->pci_mem[0].pci_base_lo +
						pi->pci_mem[0].size - 1;
	hose->pci_mem_offset = pi->pci_mem[0].cpu_base -
						pi->pci_mem[0].pci_base_lo;

	return;
} /* mv64x60_init_resources() */

/*
 * mv64x60_set_pci_params()
 *
 * Configure a hose's PCI config space parameters.
 */
static void __init
mv64x60_set_pci_params(struct pci_controller *hose, mv64x60_pci_info_t *pi)
{
	u32	devfn;
	u16	u16_val;
	u8	save_exclude;

	devfn = PCI_DEVFN(0,0);

	save_exclude = mv64x60_pci_exclude_bridge;
	mv64x60_pci_exclude_bridge = FALSE;

	/* Set class code to indicate host bridge */
	u16_val = PCI_CLASS_BRIDGE_HOST; /* 0x0600 (host bridge) */
	early_write_config_word(hose, 0, devfn, PCI_CLASS_DEVICE, u16_val);

	/* Enable 64260 to be PCI master & respond to PCI MEM cycles */
	early_read_config_word(hose, 0, devfn, PCI_COMMAND, &u16_val);
	u16_val &= ~(PCI_COMMAND_IO | PCI_COMMAND_INVALIDATE |
		PCI_COMMAND_PARITY | PCI_COMMAND_SERR | PCI_COMMAND_FAST_BACK);
	u16_val |= pi->pci_cmd_bits | PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;
	early_write_config_word(hose, 0, devfn, PCI_COMMAND, u16_val);

	/* Set latency timer, cache line size, clear BIST */
	u16_val = (pi->latency_timer << 8) | (L1_CACHE_LINE_SIZE >> 2);
	early_write_config_word(hose, 0, devfn, PCI_CACHE_LINE_SIZE, u16_val);

	mv64x60_pci_exclude_bridge = save_exclude;
	return;
}

/*
 *****************************************************************************
 *
 *	PCI Related Routine
 *
 *****************************************************************************
 */
/*
 * mv64x60_enumerate_buses()
 *
 * If requested, enumerate the PCI buses and set the appropriate
 * info in the hose structures.
 */
static void __init
mv64x60_enumerate_buses(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	u32	val;

	pci_dram_offset = 0; /* System mem at same addr on PCI & cpu bus */

	ppc_md.pci_exclude_device = mv64x60_pci_exclude_device;
	ppc_md.pci_swizzle = common_swizzle;
	ppc_md.pci_map_irq = si->map_irq;

	/* Now that the bridge is set up, its safe to scan the PCI buses */
	if (si->pci_0.enable_bus) {
		if (si->pci_0.enumerate_bus) {
			/* Set bus number for PCI 0 to 0 */
			val = mv64x60_read(bh, MV64x60_PCI0_P2P_CONFIG);
			val &= 0xe0000000;
			val |= 0x000000ff;
			mv64x60_write(bh, MV64x60_PCI0_P2P_CONFIG, val);
			/* Flush FIFO*/
			(void)mv64x60_read(bh, MV64x60_PCI0_P2P_CONFIG);

#if 0
XXXX Different if in PCI-X mode (look at mv64360_find_bridges()) XXXX
#endif

			bh->hose_a->first_busno = 0;
			bh->hose_a->last_busno  = 0xff;

			bh->hose_a->last_busno = pciauto_bus_scan(bh->hose_a,
						bh->hose_a->first_busno);
		}
		else {
			/* Assume bridge set up correctly by someone else */
			val = mv64x60_read(bh, MV64x60_PCI0_P2P_CONFIG);
			bh->hose_a->first_busno = (val & 0x00ff0000) >> 16;
		}
	}

	if (si->pci_1.enable_bus) {
		if (si->pci_1.enumerate_bus) {
			if (si->pci_0.enable_bus) {
				bh->hose_b->first_busno =
					bh->hose_a->last_busno + 1;

				/* Set bus number for PCI 1 hose */
				val = mv64x60_read(bh, MV64x60_PCI1_P2P_CONFIG);
				val &= 0xe0000000;
				val |= (bh->hose_b->first_busno << 16) | 0xff;
				mv64x60_write(bh, MV64x60_PCI1_P2P_CONFIG, val);
				/* Flush FIFO */
				(void)mv64x60_read(bh, MV64x60_PCI1_P2P_CONFIG);
			}
			else {
				bh->hose_b->first_busno = 0;
			}

			bh->hose_b->last_busno  = 0xff;
			bh->hose_b->last_busno = pciauto_bus_scan(bh->hose_b,
						bh->hose_b->first_busno);
		}
		else {
			/* Assume bridge set up correctly by someone else */
			val = mv64x60_read(bh, MV64x60_PCI1_P2P_CONFIG);
			bh->hose_b->first_busno = (val & 0x00ff0000) >> 16;
			bh->hose_b->last_busno = 0xff; /* No way to know */
		}
	}

	if (si->pci_0.enable_bus && !si->pci_0.enumerate_bus) {
		if (si->pci_1.enable_bus) {
			bh->hose_a->last_busno = bh->hose_b->first_busno - 1;
		}
		else {
			bh->hose_a->last_busno = 0xff; /* No way to know */
		}
	}

	return;
}

/*
 * mv64x60_exclude_pci_device()
 *
 * This routine is used to make the bridge not appear when the
 * PCI subsystem is accessing PCI devices (in PCI config space).
 */
static int
mv64x60_pci_exclude_device(u8 bus, u8 devfn)
{
	struct pci_controller	*hose;

	hose = pci_bus_to_hose(bus);

	/* Skip slot 0 on both hoses */
	if ((mv64x60_pci_exclude_bridge == TRUE) &&
	    (PCI_SLOT(devfn) == 0) &&
	    (hose->first_busno == bus)) {
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	else {
		return PCIBIOS_SUCCESSFUL;
	}
} /* mv64x60_pci_exclude_device() */

/*
 *****************************************************************************
 *
 *	OCP Fixup Routines
 *
 *****************************************************************************
 */
/*
 * mv64x60_fixup_ocp()
 *
 * Adjust the 'paddr' field in the bridge's OCP entries to reflect where they
 * really are in the physical address space.
 */
static void __init
mv64x60_fixup_ocp(struct ocp_device *dev, void *arg)
{
	mv64x60_handle_t	*bh = (mv64x60_handle_t *)arg;

	if (dev->def->vendor == OCP_VENDOR_MARVELL) {
		dev->def->paddr += bh->p_base;
	}

	return;
}

/*
 *****************************************************************************
 *
 *	GT64260-Specific Routines
 *
 *****************************************************************************
 */
/*
 * gt64260_translate_size()
 *
 * On the GT64260, the size register is really the "top" address of the window.
 */
static u32 __init
gt64260_translate_size(u32 base, u32 size, u32 num_bits)
{
	return base + mv64x60_mask(size - 1, num_bits);
}

/*
 * gt64260_untranslate_size()
 *
 * Translate the top address of a window into a window size.
 */
static u32 __init
gt64260_untranslate_size(u32 base, u32 size, u32 num_bits)
{
	if (size >= base) {
		size = size - base + (1 << (32 - num_bits));
	}
	else {
		size = 0;
	}

	return size;
}

/*
 * gt64260_set_pci2mem_window()
 *
 * The PCI->MEM window registers are actually in PCI config space so need
 * to set them by setting the correct config space BARs.
 */
static void __init
gt64260_set_pci2mem_window(struct pci_controller *hose, u32 window, u32 base)
{
	u32	reg_addrs[] = { 0x10, 0x14, 0x18, 0x1c };

	DBG("set pci->mem window: %d, hose: %d, base: 0x%x\n", window,
		hose->index, base);

	early_write_config_dword(hose, hose->first_busno,
			PCI_DEVFN(0, 0), reg_addrs[window],
			mv64x60_mask(base, 20) | 0x8);
	return;
}

/*
 * gt64260_is_enabled_32bit()
 *
 * On a GT64260, a window is enabled iff its top address is >= to its base
 * address.
 */
static u32 __init
gt64260_is_enabled_32bit(mv64x60_handle_t *bh, u32 window)
{
	u32	rc = 0;

	if ((gt64260_32bit_windows[window].base_reg != 0) &&
		(gt64260_32bit_windows[window].size_reg != 0) &&
		((mv64x60_read(bh, gt64260_32bit_windows[window].size_reg) &
			((1 << gt64260_32bit_windows[window].size_bits) - 1)) >=
		 (mv64x60_read(bh, gt64260_32bit_windows[window].base_reg) &
			((1 << gt64260_32bit_windows[window].base_bits) - 1)))){

		rc = 1;
	}

	if (rc) {
		DBG("32bit window %d is enabled\n", window);
	}
	else {
		DBG("32bit window %d is disabled\n", window);
	}

	return rc;
}

/*
 * gt64260_enable_window_32bit()
 *
 * On the GT64260, a window is enabled iff the top address is >= to the base
 * address of the window.  Since the window has already been configured by
 * the time this routine is called, we have nothing to do here.
 */
static void __init
gt64260_enable_window_32bit(mv64x60_handle_t *bh, u32 window)
{
	DBG("enable 32bit window: %d\n", window);
	return;
}

/*
 * gt64260_disable_window_32bit()
 *
 * On a GT64260, you disable a window by setting its top address to be less
 * than its base address.
 */
static void __init
gt64260_disable_window_32bit(mv64x60_handle_t *bh, u32 window)
{
	DBG("disable 32bit window: %d, base_reg: 0x%x, size_reg: 0x%x\n",
		window, gt64260_32bit_windows[window].base_reg,
		gt64260_32bit_windows[window].size_reg);

	if ((gt64260_32bit_windows[window].base_reg != 0) &&
		(gt64260_32bit_windows[window].size_reg != 0)) {

		/* To disable, make bottom reg higher than top reg */
		mv64x60_write(bh, gt64260_32bit_windows[window].base_reg,0xfff);
		mv64x60_write(bh, gt64260_32bit_windows[window].size_reg, 0);
	}

	return;
}

/*
 * gt64260_enable_window_64bit()
 *
 * On the GT64260, a window is enabled iff the top address is >= to the base
 * address of the window.  Since the window has already been configured by
 * the time this routine is called, we have nothing to do here.
 */
static void __init
gt64260_enable_window_64bit(mv64x60_handle_t *bh, u32 window)
{
	DBG("enable 64bit window: %d\n", window);
	return;	/* Enabled when window configured (i.e., when top >= base) */
}

/*
 * gt64260_disable_window_64bit()
 *
 * On a GT64260, you disable a window by setting its top address to be less
 * than its base address.
 */
static void __init
gt64260_disable_window_64bit(mv64x60_handle_t *bh, u32 window)
{
	DBG("disable 64bit window: %d, base_reg: 0x%x, size_reg: 0x%x\n",
		window, gt64260_64bit_windows[window].base_lo_reg,
		gt64260_64bit_windows[window].size_reg);

	if ((gt64260_64bit_windows[window].base_lo_reg != 0) &&
		(gt64260_64bit_windows[window].size_reg != 0)) {

		/* To disable, make bottom reg higher than top reg */
		mv64x60_write(bh, gt64260_64bit_windows[window].base_lo_reg,
									0xfff);
		mv64x60_write(bh, gt64260_64bit_windows[window].base_hi_reg, 0);
		mv64x60_write(bh, gt64260_64bit_windows[window].size_reg, 0);
	}

	return;
}

/*
 * gt64260_disable_all_windows()
 *
 * The GT64260 has several windows that aren't represented in the table of
 * windows at the top of this file.  This routine turns all of them off
 * except for the memory controller windows, of course.
 */
static void __init
gt64260_disable_all_windows(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	u32	i;

	/* Disable 32bit windows (don't disable cpu->mem windows) */
	for (i=MV64x60_CPU2DEV_0_WIN; i<MV64x60_32BIT_WIN_COUNT; i++) {
		if (!(si->window_preserve_mask_32 & (1<<i)))
			gt64260_disable_window_32bit(bh, i);
	}

	/* Disable 64bit windows */
	for (i=0; i<MV64x60_64BIT_WIN_COUNT; i++) {
		if (!(si->window_preserve_mask_64 & (1<<i)))
			gt64260_disable_window_64bit(bh, i);
	}

	/* Turn off cpu protection windows not in gt64260_32bit_windows[] */
	mv64x60_write(bh, GT64260_CPU_PROT_BASE_4, 0xfff);
	mv64x60_write(bh, GT64260_CPU_PROT_SIZE_4, 0);
	mv64x60_write(bh, GT64260_CPU_PROT_BASE_5, 0xfff);
	mv64x60_write(bh, GT64260_CPU_PROT_SIZE_5, 0);
	mv64x60_write(bh, GT64260_CPU_PROT_BASE_6, 0xfff);
	mv64x60_write(bh, GT64260_CPU_PROT_SIZE_6, 0);
	mv64x60_write(bh, GT64260_CPU_PROT_BASE_7, 0xfff);
	mv64x60_write(bh, GT64260_CPU_PROT_SIZE_7, 0);

	/* Turn off PCI->MEM access cntl wins not in gt64260_64bit_windows[] */
	mv64x60_write(bh, MV64x60_PCI0_ACC_CNTL_4_BASE_LO, 0xfff);
	mv64x60_write(bh, MV64x60_PCI0_ACC_CNTL_4_BASE_HI, 0);
	mv64x60_write(bh, MV64x60_PCI0_ACC_CNTL_4_SIZE, 0);
	mv64x60_write(bh, MV64x60_PCI0_ACC_CNTL_5_BASE_LO, 0xfff);
	mv64x60_write(bh, MV64x60_PCI0_ACC_CNTL_5_BASE_HI, 0);
	mv64x60_write(bh, MV64x60_PCI0_ACC_CNTL_5_SIZE, 0);
	mv64x60_write(bh, GT64260_PCI0_ACC_CNTL_6_BASE_LO, 0xfff);
	mv64x60_write(bh, GT64260_PCI0_ACC_CNTL_6_BASE_HI, 0);
	mv64x60_write(bh, GT64260_PCI0_ACC_CNTL_6_SIZE, 0);
	mv64x60_write(bh, GT64260_PCI0_ACC_CNTL_7_BASE_LO, 0xfff);
	mv64x60_write(bh, GT64260_PCI0_ACC_CNTL_7_BASE_HI, 0);
	mv64x60_write(bh, GT64260_PCI0_ACC_CNTL_7_SIZE, 0);

	mv64x60_write(bh, MV64x60_PCI1_ACC_CNTL_4_BASE_LO, 0xfff);
	mv64x60_write(bh, MV64x60_PCI1_ACC_CNTL_4_BASE_HI, 0);
	mv64x60_write(bh, MV64x60_PCI1_ACC_CNTL_4_SIZE, 0);
	mv64x60_write(bh, MV64x60_PCI1_ACC_CNTL_5_BASE_LO, 0xfff);
	mv64x60_write(bh, MV64x60_PCI1_ACC_CNTL_5_BASE_HI, 0);
	mv64x60_write(bh, MV64x60_PCI1_ACC_CNTL_5_SIZE, 0);
	mv64x60_write(bh, GT64260_PCI1_ACC_CNTL_6_BASE_LO, 0xfff);
	mv64x60_write(bh, GT64260_PCI1_ACC_CNTL_6_BASE_HI, 0);
	mv64x60_write(bh, GT64260_PCI1_ACC_CNTL_6_SIZE, 0);
	mv64x60_write(bh, GT64260_PCI1_ACC_CNTL_7_BASE_LO, 0xfff);
	mv64x60_write(bh, GT64260_PCI1_ACC_CNTL_7_BASE_HI, 0);
	mv64x60_write(bh, GT64260_PCI1_ACC_CNTL_7_SIZE, 0);

	/* Disable all PCI-><whatever> windows */
	mv64x60_set_bits(bh, MV64x60_PCI0_BAR_ENABLE, 0x07ffffff);
	mv64x60_set_bits(bh, MV64x60_PCI1_BAR_ENABLE, 0x07ffffff);

	return;
}

/*
 * gt64260a_chip_specific_init()
 *
 * Implement errata work arounds for the GT64260A.
 */
static void
gt64260a_chip_specific_init(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	struct ocp_device	*dev;
	mv64x60_ocp_mpsc_data_t	*mpsc_dp;
	u8			save_exclude;
	u32			val;

	/* R#18 */
	/* cpu read buffer to buffer 1 (reg 0x0448) */
	mv64x60_set_bits(bh, GT64260_SDRAM_CONFIG, (1<<26));

	/* No longer errata so turn on */
	/* Enable pci read/write combine, master write trigger,
	* disable slave sync barrier
	* readmultiple (reg 0x0c00 and 0x0c80)
	*/
	if (si->pci_0.enable_bus) {
		mv64x60_set_bits(bh, MV64x60_PCI0_CMD,
			((1<<4) | (1<<5) | (1<<9) | (1<<13)));
	}

	if (si->pci_1.enable_bus) {
		mv64x60_set_bits(bh, MV64x60_PCI1_CMD,
			((1<<4) | (1<<5) | (1<<9) | (1<<13)));
	}

#if 1	/* XXXX */
	/*
	 * Dave Wilhardt found that bit 4 in the PCI Command registers must
	 * be set if you are using cache coherency.
	 *
	 * Note: he also said that bit 4 must be on in all PCI devices but
	 *       that has not been implemented yet.
	 */
	save_exclude = mv64x60_pci_exclude_bridge;
	mv64x60_pci_exclude_bridge = FALSE;

	early_read_config_dword(bh->hose_a,
			bh->hose_a->first_busno,
			PCI_DEVFN(0,0),
			PCI_COMMAND,
			&val);
	val |= PCI_COMMAND_INVALIDATE;
	early_write_config_dword(bh->hose_a,
			bh->hose_a->first_busno,
			PCI_DEVFN(0,0),
			PCI_COMMAND,
			val);

	early_read_config_dword(bh->hose_b,
			bh->hose_b->first_busno,
			PCI_DEVFN(0,0),
			PCI_COMMAND,
			&val);
	val |= PCI_COMMAND_INVALIDATE;
	early_write_config_dword(bh->hose_b,
			bh->hose_b->first_busno,
			PCI_DEVFN(0,0),
			PCI_COMMAND,
			val);

	mv64x60_pci_exclude_bridge = save_exclude;
#endif

	if ((dev = ocp_find_device(OCP_VENDOR_MARVELL, OCP_FUNC_MPSC, 0))
								!= NULL) {
		mpsc_dp = (mv64x60_ocp_mpsc_data_t *)dev->def->additions;
		mpsc_dp->mirror_regs = 1;
		mpsc_dp->cache_mgmt = 1;
	}

	if ((dev = ocp_find_device(OCP_VENDOR_MARVELL, OCP_FUNC_MPSC, 1))
								!= NULL) {
		mpsc_dp = (mv64x60_ocp_mpsc_data_t *)dev->def->additions;
		mpsc_dp->mirror_regs = 1;
		mpsc_dp->cache_mgmt = 1;
	}

	return;
}

/*
 * gt64260b_chip_specific_init()
 *
 * Implement errata work arounds for the GT64260B.
 */
static void
gt64260b_chip_specific_init(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	struct ocp_device	*dev;
	mv64x60_ocp_mpsc_data_t	*mpsc_dp;

	/* R#18 */
	/* cpu read buffer to buffer 1 (reg 0x0448) */
	mv64x60_set_bits(bh, GT64260_SDRAM_CONFIG, (1<<26));

	/* No longer errata so turn on */
	/* Enable pci read/write combine, master write trigger,
	* disable slave sync barrier
	* readmultiple (reg 0x0c00 and 0x0c80)
	*/
	if (si->pci_0.enable_bus) {
		mv64x60_set_bits(bh, MV64x60_PCI0_CMD,
			((1<<4) | (1<<5) | (1<<9) | (1<<13)));
	}

	if (si->pci_1.enable_bus) {
		mv64x60_set_bits(bh, MV64x60_PCI1_CMD,
			((1<<4) | (1<<5) | (1<<9) | (1<<13)));
	}

	mv64x60_set_bits(bh, GT64260_CPU_WB_PRIORITY_BUFFER_DEPTH, 0xf);

	/*
	 * The 64260B is not supposed to have the bug where the MPSC & ENET
	 * can't access cache coherent regions.  However, testing has shown
	 * that the MPSC, at least, still has this bug.
	 */
	if ((dev = ocp_find_device(OCP_VENDOR_MARVELL, OCP_FUNC_MPSC, 0))
								!= NULL) {
		mpsc_dp = (mv64x60_ocp_mpsc_data_t *)dev->def->additions;
		mpsc_dp->cache_mgmt = 1;
	}

	if ((dev = ocp_find_device(OCP_VENDOR_MARVELL, OCP_FUNC_MPSC, 1))
								!= NULL) {
		mpsc_dp = (mv64x60_ocp_mpsc_data_t *)dev->def->additions;
		mpsc_dp->cache_mgmt = 1;
	}

	return;
}

/*
 *****************************************************************************
 *
 *	MV64360-Specific Routines
 *
 *****************************************************************************
 */
/*
 * mv64360_translate_size()
 *
 * On the MV64360, the size register is set similar to the size you get
 * from a pci config space BAR register.  That is, programmed from LSB to MSB
 * as a sequence of 1's followed by a sequence of 0's. IOW, "size -1" with the
 * assumption that the size is a power of 2.
 */
static u32 __init
mv64360_translate_size(u32 base_addr, u32 size, u32 num_bits)
{
	return mv64x60_mask(size - 1, num_bits);
}

/*
 * mv64360_untranslate_size()
 *
 * Translate the size register value of a window into a window size.
 */
static u32 __init
mv64360_untranslate_size(u32 base_addr, u32 size, u32 num_bits)
{
	if (size > 0) {
		size >>= (32 - num_bits);
		size++;
		size <<= (32 - num_bits);
	}

	return size;
}

/*
 * mv64360_set_pci2mem_window()
 *
 * The PCI->MEM window registers are actually in PCI config space so need
 * to set them by setting the correct config space BARs.
 */
static void __init
mv64360_set_pci2mem_window(struct pci_controller *hose, u32 window, u32 base)
{
	struct {
		u32	fcn;
		u32	base_hi_bar;
		u32	base_lo_bar;
	} reg_addrs[] = {{ 0, 0x14, 0x10 }, { 0, 0x1c, 0x18 },
		{ 1, 0x14, 0x10 }, { 1, 0x1c, 0x18 }};

	DBG("set pci->mem window: %d, hose: %d, base: 0x%x\n", window,
		hose->index, base);

	early_write_config_dword(hose, hose->first_busno,
			PCI_DEVFN(0, reg_addrs[window].fcn),
			reg_addrs[window].base_hi_bar, 0);
	early_write_config_dword(hose, hose->first_busno,
			PCI_DEVFN(0, reg_addrs[window].fcn),
			reg_addrs[window].base_lo_bar,
			mv64x60_mask(base, 20) | 0xc);
	return;
}

/*
 * mv64360_is_enabled_32bit()
 *
 * On a MV64360, a window is enabled by either clearing a bit in the
 * CPU BAR Enable reg or setting a bit in the window's base reg.
 * Note that this doesn't work for windows on the PCI slave side but we don't
 * check those so its okay.
 */
static u32 __init
mv64360_is_enabled_32bit(mv64x60_handle_t *bh, u32 window)
{
	u32	rc = 0;

	if ((mv64360_32bit_windows[window].base_reg != 0) &&
		(mv64360_32bit_windows[window].size_reg != 0)) {

		if (mv64360_32bit_windows[window].extra & 0x80000000) {
			rc = (mv64x60_read(bh,
				mv64360_32bit_windows[window].base_reg) & 
				(1 << (mv64360_32bit_windows[window].extra &
								0xff))) != 0;
		}
		else {
			rc = (mv64x60_read(bh, MV64360_CPU_BAR_ENABLE) & 
				(1 << mv64360_32bit_windows[window].extra)) ==0;
		}
	}

	if (rc) {
		DBG("32bit window %d is enabled\n", window);
	}
	else {
		DBG("32bit window %d is disabled\n", window);
	}

	return rc;
}

/*
 * mv64360_enable_window_32bit()
 *
 * On a MV64360, a window is enabled by either clearing a bit in the
 * CPU BAR Enable reg or setting a bit in the window's base reg.
 */
static void __init
mv64360_enable_window_32bit(mv64x60_handle_t *bh, u32 window)
{
	DBG("enable 32bit window: %d\n", window);

	if ((mv64360_32bit_windows[window].base_reg != 0) &&
		(mv64360_32bit_windows[window].size_reg != 0)) {

		if (mv64360_32bit_windows[window].extra & 0x80000000) {
			mv64x60_set_bits(bh,
				mv64360_32bit_windows[window].base_reg,
				(1 << (mv64360_32bit_windows[window].extra &
									0xff)));
		}
		else {
			mv64x60_clr_bits(bh, MV64360_CPU_BAR_ENABLE,
				(1 << mv64360_32bit_windows[window].extra));
		}
	}

	return;
}

/*
 * mv64360_disable_window_32bit()
 *
 * On a MV64360, a window is disabled by either setting a bit in the
 * CPU BAR Enable reg or clearing a bit in the window's base reg.
 */
static void __init
mv64360_disable_window_32bit(mv64x60_handle_t *bh, u32 window)
{
	DBG("disable 32bit window: %d, base_reg: 0x%x, size_reg: 0x%x\n",
		window, mv64360_32bit_windows[window].base_reg,
		mv64360_32bit_windows[window].size_reg);

	if ((mv64360_32bit_windows[window].base_reg != 0) &&
		(mv64360_32bit_windows[window].size_reg != 0)) {

		if (mv64360_32bit_windows[window].extra & 0x80000000) {
			mv64x60_clr_bits(bh,
				mv64360_32bit_windows[window].base_reg,
				(1 << (mv64360_32bit_windows[window].extra &
									0xff)));
		}
		else {
			mv64x60_set_bits(bh, MV64360_CPU_BAR_ENABLE,
				(1 << mv64360_32bit_windows[window].extra));
		}
	}

	return;
}

/*
 * mv64360_enable_window_64bit()
 *
 * On the MV64360, a 64-bit window is enabled by setting a bit in the window's 
 * base reg.
 */
static void __init
mv64360_enable_window_64bit(mv64x60_handle_t *bh, u32 window)
{
	DBG("enable 64bit window: %d\n", window);

	/* For 64360, 'extra' field holds bit that enables the window */
	if ((mv64360_64bit_windows[window].base_lo_reg!= 0) &&
		(mv64360_64bit_windows[window].size_reg != 0)) {

		if (mv64360_64bit_windows[window].extra & 0x80000000) {
			mv64x60_set_bits(bh,
				mv64360_64bit_windows[window].base_lo_reg,
				(1 << (mv64360_64bit_windows[window].extra &
									0xff)));
		} /* Should be no 'else' ones */
	}

	return;
}

/*
 * mv64360_disable_window_64bit()
 *
 * On a MV64360, a 64-bit window is disabled by clearing a bit in the window's
 * base reg.
 */
static void __init
mv64360_disable_window_64bit(mv64x60_handle_t *bh, u32 window)
{
	DBG("disable 64bit window: %d, base_reg: 0x%x, size_reg: 0x%x\n",
		window, mv64360_64bit_windows[window].base_lo_reg,
		mv64360_64bit_windows[window].size_reg);

	if ((mv64360_64bit_windows[window].base_lo_reg != 0) &&
		(mv64360_64bit_windows[window].size_reg != 0)) {

		if (mv64360_64bit_windows[window].extra & 0x80000000) {
			mv64x60_clr_bits(bh,
				mv64360_64bit_windows[window].base_lo_reg,
				(1 << (mv64360_64bit_windows[window].extra &
									0xff)));
		} /* Should be no 'else' ones */
	}

	return;
}

/*
 * mv64360_disable_all_windows()
 *
 * The MV64360 has a few windows that aren't represented in the table of
 * windows at the top of this file.  This routine turns all of them off
 * except for the memory controller windows, of course.
 */
static void __init
mv64360_disable_all_windows(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	u32	i;

	/* Disable 32bit windows (don't disable cpu->mem windows) */
	for (i=MV64x60_CPU2DEV_0_WIN; i<MV64x60_32BIT_WIN_COUNT; i++) {
		if (!(si->window_preserve_mask_32 & (1<<i)))
			mv64360_disable_window_32bit(bh, i);
	}

	/* Disable 64bit windows */
	for (i=0; i<MV64x60_64BIT_WIN_COUNT; i++) {
		if (!(si->window_preserve_mask_64 & (1<<i)))
			mv64360_disable_window_64bit(bh, i);
	}

	/* Turn off PCI->MEM access cntl wins not in mv64360_64bit_windows[] */
	mv64x60_clr_bits(bh, MV64x60_PCI0_ACC_CNTL_4_BASE_LO, 0);
	mv64x60_clr_bits(bh, MV64x60_PCI0_ACC_CNTL_5_BASE_LO, 0);
	mv64x60_clr_bits(bh, MV64x60_PCI1_ACC_CNTL_4_BASE_LO, 0);
	mv64x60_clr_bits(bh, MV64x60_PCI1_ACC_CNTL_5_BASE_LO, 0);

	/* Disable all PCI-><whatever> windows */
	mv64x60_set_bits(bh, MV64x60_PCI0_BAR_ENABLE, 0x0000f9ff);
	mv64x60_set_bits(bh, MV64x60_PCI1_BAR_ENABLE, 0x0000f9ff);

	return;
}

/*
 * mv64360_chip_specific_init()
 *
 * No errata work arounds for the MV64360 implemented at this point.
 */
static void
mv64360_chip_specific_init(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	struct ocp_device	*dev;
	mv64x60_ocp_mpsc_data_t	*mpsc_dp;

	if ((dev = ocp_find_device(OCP_VENDOR_MARVELL, OCP_FUNC_MPSC, 0))
								!= NULL) {
		mpsc_dp = (mv64x60_ocp_mpsc_data_t *)dev->def->additions;
		mpsc_dp->brg_can_tune = 1;
	}

	if ((dev = ocp_find_device(OCP_VENDOR_MARVELL, OCP_FUNC_MPSC, 1))
								!= NULL) {
		mpsc_dp = (mv64x60_ocp_mpsc_data_t *)dev->def->additions;
		mpsc_dp->brg_can_tune = 1;
	}

	return;
}

/*
 * mv64460_chip_specific_init()
 *
 * No errata work arounds for the MV64460 implemented at this point.
 */
static void
mv64460_chip_specific_init(mv64x60_handle_t *bh, mv64x60_setup_info_t *si)
{
	mv64360_chip_specific_init(bh, si); /* XXXX check errata */
	return;
}
