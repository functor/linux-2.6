/*
 * arch/ppc/syslib/mv64x60_ocp.c
 * 
 * Common OCP definitions for the Marvell GT64260/MV64360/MV64460/...
 * line of host bridges.
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *
 * 2004 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/mv64x60.h>
#include <asm/ocp.h>

static mv64x60_ocp_mpsc_data_t	mv64x60_ocp_mpsc0_def = {
	.mirror_regs		= 0,
	.cache_mgmt		= 0,
	.max_idle		= 0,
	.default_baud		= 9600,
	.default_bits		= 8,
	.default_parity		= 'n',
	.default_flow		= 'n',
	.chr_1_val		= 0x00000000,
	.chr_2_val		= 0x00000000,
	.chr_10_val		= 0x00000003,
	.mpcr_val		= 0,
	.mrr_val		= 0x3ffffe38,
	.rcrr_val		= 0,
	.tcrr_val		= 0,
	.intr_mask_val		= 0,
	.bcr_val		= 0,
	.sdma_irq		= MV64x60_IRQ_SDMA_0,
	.brg_can_tune		= 0,
	.brg_clk_src		= 8,		/* Default to TCLK */
	.brg_clk_freq		= 100000000,	/* Default to 100 MHz */
};
static mv64x60_ocp_mpsc_data_t	mv64x60_ocp_mpsc1_def = {
	.mirror_regs		= 0,
	.cache_mgmt		= 0,
	.max_idle		= 0,
	.default_baud		= 9600,
	.default_bits		= 8,
	.default_parity		= 'n',
	.default_flow		= 'n',
	.chr_1_val		= 0x00000000,
	.chr_1_val		= 0x00000000,
	.chr_2_val		= 0x00000000,
	.chr_10_val		= 0x00000003,
	.mpcr_val		= 0,
	.mrr_val		= 0x3ffffe38,
	.rcrr_val		= 0,
	.tcrr_val		= 0,
	.intr_mask_val		= 0,
	.bcr_val		= 0,
	.sdma_irq		= MV64x60_IRQ_SDMA_1,
	.brg_can_tune		= 0,
	.brg_clk_src		= 8,		/* Default to TCLK */
	.brg_clk_freq		= 100000000,	/* Default to 100 MHz */
};
MV64x60_OCP_SYSFS_MPSC_DATA()

struct ocp_def core_ocp[] = {
	/* Base address for the block of bridge's regs */
	{ .vendor       = OCP_VENDOR_MARVELL,			/* 0x00 */
	  .function     = OCP_FUNC_HB,
	  .index        = 0,
	  .paddr        = 0,
	  .pm           = OCP_CPM_NA,
	},
	/* 10/100 Ethernet controller */
	{ .vendor       = OCP_VENDOR_MARVELL,			/* 0x01 */
	  .function     = OCP_FUNC_EMAC,
	  .index        = 0,
	  .paddr        = GT64260_ENET_0_OFFSET,
	  .irq          = MV64x60_IRQ_ETH_0,
	  .pm           = OCP_CPM_NA,
	},
	{ .vendor       = OCP_VENDOR_MARVELL,			/* 0x02 */
	  .function     = OCP_FUNC_EMAC,
	  .index        = 1,
	  .paddr        = GT64260_ENET_1_OFFSET,
	  .irq          = MV64x60_IRQ_ETH_1,
	  .pm           = OCP_CPM_NA,
	},
	{ .vendor       = OCP_VENDOR_MARVELL,			/* 0x03 */
	  .function     = OCP_FUNC_EMAC,
	  .index        = 2,
	  .paddr        = GT64260_ENET_2_OFFSET,
	  .irq          = MV64x60_IRQ_ETH_2,
	  .pm           = OCP_CPM_NA,
	},
	/* Multi-Protocol Serial Controller (MPSC) */
	{ .vendor       = OCP_VENDOR_MARVELL,			/* 0x04 */
	  .function     = OCP_FUNC_MPSC,
	  .index        = 0,
	  .paddr	= MV64x60_MPSC_0_OFFSET,
	  .irq          = MV64x60_IRQ_MPSC_0,
	  .pm           = OCP_CPM_NA,
	  .additions	= &mv64x60_ocp_mpsc0_def,
	  .show		= &mv64x60_ocp_show_mpsc
	},
	{ .vendor       = OCP_VENDOR_MARVELL,			/* 0x05 */
	  .function     = OCP_FUNC_MPSC,
	  .index        = 1,
	  .paddr	= MV64x60_MPSC_1_OFFSET,
	  .irq          = MV64x60_IRQ_MPSC_1,
	  .pm           = OCP_CPM_NA,
	  .additions	= &mv64x60_ocp_mpsc1_def,
	  .show		= &mv64x60_ocp_show_mpsc
	},
	/* Inter-Integrated Circuit Controller */
	{ .vendor       = OCP_VENDOR_MARVELL,			/* 0x06 */
	  .function     = OCP_FUNC_I2C,
	  .index        = 0,
	  .paddr        = GT64260_I2C_OFFSET,
	  .irq          = MV64x60_IRQ_I2C,
	  .pm           = OCP_CPM_NA,
	},
	/* Programmable Interrupt Controller */
	{ .vendor       = OCP_VENDOR_MARVELL,			/* 0x07 */
	  .function     = OCP_FUNC_PIC,
	  .index        = 0,
	  .paddr        = GT64260_IC_OFFSET,
	  .pm           = OCP_CPM_NA,
	},
	{ .vendor	= OCP_VENDOR_INVALID
	}
};
