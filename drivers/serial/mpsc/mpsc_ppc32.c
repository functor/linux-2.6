/*
 * drivers/serial/mpsc/mpsc_ppc32.c
 *
 * Middle layer that sucks data from the ppc32 OCP--that is, chip &
 * platform-specific data--and puts it into the mpsc_port_info_t structure
 * for the mpsc driver to use.
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *
 * 2004 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
#include "mpsc.h"
#include <asm/ocp.h>
#include <asm/mv64x60.h>

static void mpsc_ocp_remove(struct ocp_device *ocpdev);

static int
mpsc_ocp_probe(struct ocp_device *ocpdev)
{
	mpsc_port_info_t	*pi;
	mv64x60_ocp_mpsc_data_t	*dp;
	u32			base;
	int			rc = -ENODEV;

	if ((pi = mpsc_device_probe(ocpdev->def->index)) != NULL) {
		dp = (mv64x60_ocp_mpsc_data_t *)ocpdev->def->additions;

		pi->mpsc_base_p = ocpdev->def->paddr;

		if (ocpdev->def->index == 0) {
			base = pi->mpsc_base_p - MV64x60_MPSC_0_OFFSET;
			pi->sdma_base_p = base + MV64x60_SDMA_0_OFFSET;
			pi->brg_base_p = base + MV64x60_BRG_0_OFFSET;
		}
		else { /* Must be 1 */
			base = pi->mpsc_base_p - MV64x60_MPSC_1_OFFSET;
			pi->sdma_base_p = base + MV64x60_SDMA_1_OFFSET;
			pi->brg_base_p = base + MV64x60_BRG_1_OFFSET;
		}

		pi->mpsc_routing_base_p = base + MV64x60_MPSC_ROUTING_OFFSET;
		pi->sdma_intr_base_p = base + MV64x60_SDMA_INTR_OFFSET;

		pi->port.irq = dp->sdma_irq;
		pi->port.uartclk = dp->brg_clk_freq;

		pi->mirror_regs = dp->mirror_regs;
		pi->cache_mgmt = dp->cache_mgmt;
		pi->brg_can_tune = dp->brg_can_tune;
		pi->brg_clk_src = dp->brg_clk_src;
		pi->mpsc_max_idle = dp->max_idle;
		pi->default_baud = dp->default_baud;
		pi->default_bits = dp->default_bits;
		pi->default_parity = dp->default_parity;
		pi->default_flow = dp->default_flow;

		/* Initial values of mirrored regs */
		pi->MPSC_CHR_1_m = dp->chr_1_val;
		pi->MPSC_CHR_2_m = dp->chr_2_val;
		pi->MPSC_CHR_10_m = dp->chr_10_val;
		pi->MPSC_MPCR_m = dp->mpcr_val;
		pi->MPSC_MRR_m = dp->mrr_val;
		pi->MPSC_RCRR_m = dp->rcrr_val;
		pi->MPSC_TCRR_m = dp->tcrr_val;
		pi->SDMA_INTR_MASK_m = dp->intr_mask_val;
		pi->BRG_BCR_m = dp->bcr_val;

		pi->port.iotype = UPIO_MEM;

		rc = 0;
	}

	return rc;
}

static void
mpsc_ocp_remove(struct ocp_device *ocpdev)
{
	(void)mpsc_device_remove(ocpdev->def->index);
	return;
}

static struct ocp_device_id mpsc_ocp_ids[] = {
	{.vendor = OCP_VENDOR_MARVELL, .function = OCP_FUNC_MPSC},
	{.vendor = OCP_VENDOR_INVALID}
};

static struct ocp_driver mpsc_ocp_driver = {
	.name = "mpsc",
	.id_table = mpsc_ocp_ids,
	.probe = mpsc_ocp_probe,
	.remove = mpsc_ocp_remove,
};

int
mpsc_platform_register_driver(void)
{
	return ocp_register_driver(&mpsc_ocp_driver);
}

void
mpsc_platform_unregister_driver(void)
{
	ocp_unregister_driver(&mpsc_ocp_driver);
	return;
}
