/*
 * drivers/serial/mpsc/mpsc.c
 *
 * Generic driver for the MPSC (UART mode) on Marvell parts (e.g., GT64240,
 * GT64260, MV64340, MV64360, GT96100, ... ).
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *
 * Based on an old MPSC driver that was in the linuxppc tree.  It appears to
 * have been created by Chris Zankel (formerly of MontaVista) but there
 * is no proper Copyright so I'm not sure.  Parts were, apparently, also
 * taken from PPCBoot (now U-Boot).  Also based on drivers/serial/8250.c
 * by Russell King.
 *
 * 2004 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */
/*
 * The MPSC interface is much like a typical network controller's interface.
 * That is, you set up separate rings of descriptors for transmitting and
 * receiving data.  There is also a pool of buffers with (one buffer per
 * descriptor) that incoming data are dma'd into or outgoing data are dma'd
 * out of.
 *
 * The MPSC requires two other controllers to be able to work.  The Baud Rate
 * Generator (BRG) provides a clock at programmable frequencies which determines
 * the baud rate.  The Serial DMA Controller (SDMA) takes incoming data from the
 * MPSC and DMA's it into memory or DMA's outgoing data and passes it to the
 * MPSC.  It is actually the SDMA interrupt that the driver uses to keep the
 * transmit and receive "engines" going (i.e., indicate data has been
 * transmitted or received).
 *
 * NOTES:
 *
 * 1) Some chips have an erratum where several regs cannot be
 * read.  To work around that, we keep a local copy of those regs in
 * 'mpsc_port_info_t' and use the *_M macros when accessing those regs.
 *
 * 2) Some chips have an erratum where the chip will hang when the SDMA ctlr
 * accesses system mem in a cache coherent region.  This *should* be a
 * show-stopper when coherency is turned on but it seems to work okay as
 * long as there are no snoop hits.  Therefore, there are explicit cache
 * management macros in addition to the dma_* calls--the dma_* calls don't
 * do cache mgmt on coherent systems--to manage the cache ensuring there
 * are no snoop hits.
 *
 * 3) AFAICT, hardware flow control isn't supported by the controller --MAG.
 */

#include "mpsc.h"

/*
 * Define how this driver is known to the outside (we've been assigned a
 * range on the "Low-density serial ports" major).
 */
#define MPSC_MAJOR		204
#define MPSC_MINOR_START	5	/* XXXX */
#define	MPSC_DRIVER_NAME	"MPSC"
#define	MPSC_DEVFS_NAME		"ttym/"
#define	MPSC_DEV_NAME		"ttyM"
#define	MPSC_VERSION		"1.00"

static mpsc_port_info_t	mpsc_ports[MPSC_NUM_CTLRS];


#undef DEBUG

#ifdef DEBUG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif

/*
 ******************************************************************************
 *
 * Baud Rate Generator Routines (BRG)
 *
 ******************************************************************************
 */
static void
mpsc_brg_init(mpsc_port_info_t *pi, u32 clk_src)
{
	if (pi->brg_can_tune) {
		MPSC_MOD_FIELD_M(pi, brg, BRG_BCR, 1, 25, 0);
	}

	MPSC_MOD_FIELD_M(pi, brg, BRG_BCR, 4, 18, clk_src);
	MPSC_MOD_FIELD(pi, brg, BRG_BTR, 16, 0, 0);
	return;
}

static void
mpsc_brg_enable(mpsc_port_info_t *pi)
{
	MPSC_MOD_FIELD_M(pi, brg, BRG_BCR, 1, 16, 1);
	return;
}

static void
mpsc_brg_disable(mpsc_port_info_t *pi)
{
	MPSC_MOD_FIELD_M(pi, brg, BRG_BCR, 1, 16, 0);
	return;
}

static inline void
mpsc_set_baudrate(mpsc_port_info_t *pi, u32 baud)
{
	/*
	 * To set the baud, we adjust the CDV field in the BRG_BCR reg.
	 * From manual: Baud = clk / ((CDV+1)*2) ==> CDV = (clk / (baud*2)) - 1.
	 * However, the input clock is divided by 16 in the MPSC b/c of how
	 * 'MPSC_MMCRH' was set up so we have to divide 'clk' used in our
	 * calculation by 16 to account for that.  So the real calculation
	 * that accounts for the way the mpsc is set up is:
	 * CDV = (clk / (baud*32)) - 1 ==> CDV = (clk / (baud << 5)) -1.
	 */
	u32	cdv = (pi->port.uartclk/(baud << 5)) - 1;

	mpsc_brg_disable(pi);
	MPSC_MOD_FIELD_M(pi, brg, BRG_BCR, 16, 0, cdv);
	mpsc_brg_enable(pi);

	return;
}

/*
 ******************************************************************************
 *
 * Serial DMA Routines (SDMA)
 *
 ******************************************************************************
 */

static void
mpsc_sdma_burstsize(mpsc_port_info_t *pi, u32 burst_size)
{
	u32	v;

	DBG("mpsc_sdma_burstsize[%d]: burst_size: %d\n",
		pi->port.line, burst_size);

	burst_size >>= 3; /* Divide by 8 b/c reg values are 8-byte chunks */

	if (burst_size < 2) v = 0x0;		/* 1 64-bit word */
	else if (burst_size < 4) v = 0x1;	/* 2 64-bit words */
	else if (burst_size < 8) v = 0x2;	/* 4 64-bit words */
	else v = 0x3;				/* 8 64-bit words */

	MPSC_MOD_FIELD(pi, sdma, SDMA_SDC, 2, 12, v);
	return;
}

static void
mpsc_sdma_init(mpsc_port_info_t *pi, u32 burst_size)
{
	DBG("mpsc_sdma_init[%d]: burst_size: %d\n", pi->port.line, burst_size);

	MPSC_MOD_FIELD(pi, sdma, SDMA_SDC, 10, 0, 0x03f);
	mpsc_sdma_burstsize(pi, burst_size);
	return;
}

static inline u32
mpsc_sdma_intr_mask(mpsc_port_info_t *pi, u32 mask)
{
	u32	old, v;

	DBG("mpsc_sdma_intr_mask[%d]: mask: 0x%x\n", pi->port.line, mask);

	old = v = MPSC_READ_M(pi, sdma_intr, SDMA_INTR_MASK);
	mask &= 0xf;
	if (pi->port.line) mask <<= 8;
	v &= ~mask;
	MPSC_WRITE_M(pi, sdma_intr, SDMA_INTR_MASK, v);

	if (pi->port.line) old >>= 8;
	return old & 0xf;
}

static inline void
mpsc_sdma_intr_unmask(mpsc_port_info_t *pi, u32 mask)
{
	u32	v;

	DBG("mpsc_sdma_intr_unmask[%d]: clk_src: 0x%x\n", pi->port.line, mask);

	v = MPSC_READ_M(pi, sdma_intr, SDMA_INTR_MASK);
	mask &= 0xf;
	if (pi->port.line) mask <<= 8;
	v |= mask;
	MPSC_WRITE_M(pi, sdma_intr, SDMA_INTR_MASK, v);
	return;
}

static inline void
mpsc_sdma_intr_ack(mpsc_port_info_t *pi)
{
	DBG("mpsc_sdma_intr_ack[%d]: Acknowledging IRQ\n", pi->port.line);
	MPSC_WRITE(pi, sdma_intr, SDMA_INTR_CAUSE, 0);
	return;
}

static inline void
mpsc_sdma_set_rx_ring(mpsc_port_info_t *pi, mpsc_rx_desc_t *rxre_p)
{
	DBG("mpsc_sdma_set_rx_ring[%d]: rxre_p: 0x%x\n",
		pi->port.line, (uint)rxre_p);

	MPSC_WRITE(pi, sdma, SDMA_SCRDP, (u32)rxre_p);
	return;
}

static inline void
mpsc_sdma_set_tx_ring(mpsc_port_info_t *pi, volatile mpsc_tx_desc_t *txre_p)
{
	MPSC_WRITE(pi, sdma, SDMA_SFTDP, (int)txre_p);
	MPSC_WRITE(pi, sdma, SDMA_SCTDP, (int)txre_p);
	return;
}

static inline void
mpsc_sdma_cmd(mpsc_port_info_t *pi, u32 val)
{
	u32	v;

	v = MPSC_READ(pi, sdma, SDMA_SDCM);
	if (val)
		v |= val;
	else
		v = 0;
	MPSC_WRITE(pi, sdma, SDMA_SDCM, v);
	return;
}

static inline void
mpsc_sdma_start_tx(mpsc_port_info_t *pi, volatile mpsc_tx_desc_t *txre_p)
{
	mpsc_sdma_set_tx_ring(pi, txre_p);
	mpsc_sdma_cmd(pi, SDMA_SDCM_TXD);
	return;
}

static inline void
mpsc_sdma_stop(mpsc_port_info_t *pi)
{
	DBG("mpsc_sdma_stop[%d]: Stopping SDMA\n", pi->port.line);

	/* Abort any SDMA transfers */
	mpsc_sdma_cmd(pi, 0);
	mpsc_sdma_cmd(pi, SDMA_SDCM_AR | SDMA_SDCM_AT);

	/* Clear the SDMA current and first TX and RX pointers */
	mpsc_sdma_set_tx_ring(pi, 0);
	mpsc_sdma_set_rx_ring(pi, 0);
	/* udelay(100); XXXX was in original gt64260 driver */

	/* Disable interrupts */
	mpsc_sdma_intr_mask(pi, 0xf);
	mpsc_sdma_intr_ack(pi);
        udelay(1000);

	return;
}

/*
 ******************************************************************************
 *
 * Multi-Protocol Serial Controller Routines (MPSC)
 *
 ******************************************************************************
 */

static void
mpsc_hw_init(mpsc_port_info_t *pi)
{
	DBG("mpsc_hw_init[%d]: Initializing hardware\n", pi->port.line);

	/* Set up clock routing */
	MPSC_MOD_FIELD_M(pi, mpsc_routing, MPSC_MRR, 3, 0, 0);
	MPSC_MOD_FIELD_M(pi, mpsc_routing, MPSC_MRR, 3, 6, 0);
	MPSC_MOD_FIELD_M(pi, mpsc_routing, MPSC_RCRR, 4, 0, 0);
	MPSC_MOD_FIELD_M(pi, mpsc_routing, MPSC_RCRR, 4, 8, 1);
	MPSC_MOD_FIELD_M(pi, mpsc_routing, MPSC_TCRR, 4, 0, 0);
	MPSC_MOD_FIELD_M(pi, mpsc_routing, MPSC_TCRR, 4, 8, 1);

	/* Put MPSC in UART mode & enabel Tx/Rx egines */
	MPSC_WRITE(pi, mpsc, MPSC_MMCRL, 0x000004c4);

	/* No preamble, 16x divider, low-latency,  */
	MPSC_WRITE(pi, mpsc, MPSC_MMCRH, 0x04400400);

	MPSC_WRITE_M(pi, mpsc, MPSC_CHR_1, 0);
	MPSC_WRITE_M(pi, mpsc, MPSC_CHR_2, 0);
	MPSC_WRITE(pi, mpsc, MPSC_CHR_3, pi->mpsc_max_idle);
	MPSC_WRITE(pi, mpsc, MPSC_CHR_4, 0);
	MPSC_WRITE(pi, mpsc, MPSC_CHR_5, 0);
	MPSC_WRITE(pi, mpsc, MPSC_CHR_6, 0);
	MPSC_WRITE(pi, mpsc, MPSC_CHR_7, 0);
	MPSC_WRITE(pi, mpsc, MPSC_CHR_8, 0);
	MPSC_WRITE(pi, mpsc, MPSC_CHR_9, 0);
	MPSC_WRITE(pi, mpsc, MPSC_CHR_10, 0);

	return;
}

static inline void
mpsc_enter_hunt(mpsc_port_info_t *pi)
{
	u32	v;

	DBG("mpsc_enter_hunt[%d]: Hunting...\n", pi->port.line);

	MPSC_MOD_FIELD_M(pi, mpsc, MPSC_CHR_2, 1, 31, 1);

	if (pi->mirror_regs) {
		udelay(100);
	}
	else
		do {
			v = MPSC_READ_M(pi, mpsc, MPSC_CHR_2);
		} while (v & MPSC_CHR_2_EH);

	return;
}

static void
mpsc_freeze(mpsc_port_info_t *pi)
{
	DBG("mpsc_freeze[%d]: Freezing\n", pi->port.line);

	MPSC_MOD_FIELD_M(pi, mpsc, MPSC_MPCR, 1, 9, 1);
	return;
}

static inline void
mpsc_unfreeze(mpsc_port_info_t *pi)
{
	MPSC_MOD_FIELD_M(pi, mpsc, MPSC_MPCR, 1, 9, 0);

	DBG("mpsc_unfreeze[%d]: Unfrozen\n", pi->port.line);
	return;
}

static inline void
mpsc_set_char_length(mpsc_port_info_t *pi, u32 len)
{
	DBG("mpsc_set_char_length[%d]: char len: %d\n", pi->port.line, len);

	MPSC_MOD_FIELD_M(pi, mpsc, MPSC_MPCR, 2, 12, len);
	return;
}

static inline void
mpsc_set_stop_bit_length(mpsc_port_info_t *pi, u32 len)
{
	DBG("mpsc_set_stop_bit_length[%d]: stop bits: %d\n",pi->port.line,len);

	MPSC_MOD_FIELD_M(pi, mpsc, MPSC_MPCR, 1, 14, len);
	return;
}

static inline void
mpsc_set_parity(mpsc_port_info_t *pi, u32 p)
{
	DBG("mpsc_set_parity[%d]: parity bits: 0x%x\n", pi->port.line, p);

	MPSC_MOD_FIELD_M(pi, mpsc, MPSC_CHR_2, 2, 2, p);	/* TPM */
	MPSC_MOD_FIELD_M(pi, mpsc, MPSC_CHR_2, 2, 18, p);	/* RPM */
	return;
}

/*
 ******************************************************************************
 *
 * Driver Init Routines
 *
 ******************************************************************************
 */

static void
mpsc_init_hw(mpsc_port_info_t *pi)
{
	DBG("mpsc_init_hw[%d]: Initializing\n", pi->port.line);

	mpsc_brg_init(pi, pi->brg_clk_src);
	mpsc_brg_enable(pi);
	mpsc_sdma_init(pi, dma_get_cache_alignment());/* burst a cacheline */
	mpsc_sdma_stop(pi); 
	mpsc_hw_init(pi);

	return;
}

static int
mpsc_alloc_ring_mem(mpsc_port_info_t *pi)
{
	int	rc = 0;
	static void mpsc_free_ring_mem(mpsc_port_info_t *pi);

	DBG("mpsc_alloc_ring_mem[%d]: Allocating ring mem\n", pi->port.line);

	pi->desc_region_size = MPSC_TXR_SIZE + MPSC_RXR_SIZE +
		(2 * MPSC_DESC_ALIGN);
	pi->buf_region_size = MPSC_TXB_SIZE + MPSC_RXB_SIZE +
		(2 * MPSC_BUF_ALIGN);

	if (!pi->desc_region) {
		if (!dma_supported(pi->port.dev, 0xffffffff)) {
			printk(KERN_ERR "MPSC: inadequate DMA support\n");
			rc = -ENXIO;
		}
		else if ((pi->desc_region = dma_alloc_coherent(pi->port.dev,
			pi->desc_region_size, &pi->desc_region_p,
			GFP_KERNEL)) == NULL) {

			printk(KERN_ERR "MPSC: can't alloc Desc region\n");
			rc = -ENOMEM;
		}
		else if ((pi->buf_region = kmalloc(pi->buf_region_size,
			GFP_KERNEL)) == NULL) {

			printk(KERN_ERR "MPSC: can't alloc bufs\n");
			mpsc_free_ring_mem(pi);
			rc = -ENOMEM;
		}
	}

	return rc;
}

static void
mpsc_free_ring_mem(mpsc_port_info_t *pi)
{
	DBG("mpsc_free_ring_mem[%d]: Freeing ring mem\n", pi->port.line);

	if (pi->desc_region) {
		MPSC_CACHE_INVALIDATE(pi, pi->desc_region,
			pi->desc_region + pi->desc_region_size);
		dma_free_coherent(pi->port.dev, pi->desc_region_size,
			pi->desc_region, pi->desc_region_p);
		pi->desc_region = NULL;
		pi->desc_region_p = (dma_addr_t)NULL;
	}

	if (pi->buf_region) {
		MPSC_CACHE_INVALIDATE(pi, pi->buf_region,
			pi->buf_region + pi->buf_region_size);
		kfree(pi->buf_region);
		pi->buf_region = NULL;
	}

	return;
}

static void
mpsc_init_rings(mpsc_port_info_t *pi)
{
	mpsc_rx_desc_t	*rxre, *rxre_p;
	mpsc_tx_desc_t	*txre, *txre_p;
	u32		bp_p, save_first, i;
	u8		*bp;

	DBG("mpsc_init_rings[%d]: Initializing rings\n", pi->port.line);

	BUG_ON((pi->desc_region == NULL) || (pi->buf_region == NULL));

	memset(pi->desc_region, 0, pi->desc_region_size);
	memset(pi->buf_region, 0, pi->buf_region_size);

	pi->rxr = (mpsc_rx_desc_t *)ALIGN((u32)pi->desc_region,
		(u32)MPSC_DESC_ALIGN);
	pi->rxr_p = (mpsc_rx_desc_t *)ALIGN((u32)pi->desc_region_p,
		(u32)MPSC_DESC_ALIGN);
	pi->rxb = (u8 *)ALIGN((u32)pi->buf_region, (u32)MPSC_BUF_ALIGN);
	pi->rxb_p = __pa(pi->rxb);

	rxre = pi->rxr;
	rxre_p = pi->rxr_p;
	save_first = (u32)rxre_p;
	bp = pi->rxb;
	bp_p = pi->rxb_p;
	for (i=0; i<MPSC_RXR_ENTRIES; i++,rxre++,rxre_p++) {
		rxre->bufsize = cpu_to_be16(MPSC_RXBE_SIZE);
		rxre->bytecnt = cpu_to_be16(0);
		rxre->cmdstat = cpu_to_be32(SDMA_DESC_CMDSTAT_O |
			SDMA_DESC_CMDSTAT_EI | SDMA_DESC_CMDSTAT_F |
			SDMA_DESC_CMDSTAT_L);
		rxre->link = cpu_to_be32(rxre_p + 1);
		rxre->buf_ptr = cpu_to_be32(bp_p);
		MPSC_CACHE_FLUSH(pi, rxre, rxre + 1);
		dma_map_single(pi->port.dev, bp, MPSC_RXBE_SIZE,
			DMA_FROM_DEVICE);
		MPSC_CACHE_INVALIDATE(pi, bp, bp + MPSC_RXBE_SIZE);
		bp += MPSC_RXBE_SIZE;
		bp_p += MPSC_RXBE_SIZE;
	}
	(--rxre)->link = cpu_to_be32(save_first); /* Wrap last back to first */
	MPSC_CACHE_FLUSH(pi, rxre, rxre + 1);

	pi->txr = (mpsc_tx_desc_t *)ALIGN((u32)&pi->rxr[MPSC_RXR_ENTRIES],
		(u32)MPSC_DESC_ALIGN);
	pi->txr_p = (mpsc_tx_desc_t *)ALIGN((u32)&pi->rxr_p[MPSC_RXR_ENTRIES],
		(u32)MPSC_DESC_ALIGN);
	pi->txb = (u8 *)ALIGN((u32)(pi->rxb + MPSC_RXB_SIZE),
		(u32)MPSC_BUF_ALIGN);
	pi->txb_p = __pa(pi->txb);

	txre = pi->txr;
	txre_p = pi->txr_p;
	save_first = (u32)txre_p;
	bp = pi->txb;
	bp_p = pi->txb_p;
	for (i=0; i<MPSC_TXR_ENTRIES; i++,txre++,txre_p++) {
		txre->link = cpu_to_be32(txre_p + 1);
		txre->buf_ptr = cpu_to_be32(bp_p);
		MPSC_CACHE_FLUSH(pi, txre, txre + 1);
		dma_map_single(pi->port.dev, bp, MPSC_TXBE_SIZE, DMA_TO_DEVICE);
		bp += MPSC_TXBE_SIZE;
		bp_p += MPSC_TXBE_SIZE;
	}
	(--txre)->link = cpu_to_be32(save_first); /* Wrap last back to first */
	MPSC_CACHE_FLUSH(pi, txre, txre + 1);

	return;
}

static void
mpsc_uninit_rings(mpsc_port_info_t *pi)
{
	u32		bp_p, i;

	DBG("mpsc_uninit_rings[%d]: Uninitializing rings\n", pi->port.line);

	BUG_ON((pi->desc_region == NULL) || (pi->buf_region == NULL));

	bp_p = pi->rxb_p;
	for (i=0; i<MPSC_RXR_ENTRIES; i++) {
		dma_unmap_single(pi->port.dev, bp_p, MPSC_RXBE_SIZE,
			DMA_FROM_DEVICE);
		bp_p += MPSC_RXBE_SIZE;
	}
	pi->rxr = NULL;
	pi->rxr_p = NULL;
	pi->rxr_posn = 0;
	pi->rxb = NULL;
	pi->rxb_p = 0;

	bp_p = pi->txb_p;
	for (i=0; i<MPSC_TXR_ENTRIES; i++) {
		dma_unmap_single(pi->port.dev, bp_p, MPSC_TXBE_SIZE,
			DMA_TO_DEVICE);
		bp_p += MPSC_TXBE_SIZE;
	}
	pi->txr = NULL;
	pi->txr_p = NULL;
	pi->txr_posn = 0;
	pi->txb = NULL;
	pi->txb_p = 0;

	return;
}

static int
mpsc_make_ready(mpsc_port_info_t *pi)
{
	int	rc;

	DBG("mpsc_make_ready[%d]: Making cltr ready\n", pi->port.line);

	if (!pi->ready) {
		mpsc_init_hw(pi);
		if ((rc = mpsc_alloc_ring_mem(pi)))
			return rc;
		mpsc_init_rings(pi);
		pi->ready = 1;
	}

	return 0;
}

/*
 ******************************************************************************
 *
 * Interrupt Handling Routines
 *
 ******************************************************************************
 */

static inline void
mpsc_rx_intr(mpsc_port_info_t *pi, struct pt_regs *regs)
{
	volatile mpsc_rx_desc_t	*rxre = &pi->rxr[pi->rxr_posn];
	struct tty_struct	*tty = pi->port.info->tty;
	u32			cmdstat, bytes_in;
	u8			*bp;
	dma_addr_t		bp_p;
	static void mpsc_start_rx(mpsc_port_info_t *pi);

	DBG("mpsc_rx_intr[%d]: Handling Rx intr\n", pi->port.line);

	/*
	 * Loop through Rx descriptors handling ones that have been completed.
	 */
	MPSC_CACHE_INVALIDATE(pi, rxre, rxre + 1);

	while (!((cmdstat = be32_to_cpu(rxre->cmdstat)) & SDMA_DESC_CMDSTAT_O)){
		bytes_in = be16_to_cpu(rxre->bytecnt);

		if (unlikely((tty->flip.count + bytes_in) >= TTY_FLIPBUF_SIZE)){
			tty->flip.work.func((void *)tty);

			if ((tty->flip.count + bytes_in) >= TTY_FLIPBUF_SIZE) {
				/* Take what we can, throw away the rest */
				bytes_in = TTY_FLIPBUF_SIZE - tty->flip.count;
				cmdstat &= ~SDMA_DESC_CMDSTAT_PE;
			}
		}

		bp = pi->rxb + (pi->rxr_posn * MPSC_RXBE_SIZE);
		bp_p = pi->txb_p + (pi->rxr_posn * MPSC_RXBE_SIZE);

		dma_sync_single_for_cpu(pi->port.dev, bp_p, MPSC_RXBE_SIZE,
			DMA_FROM_DEVICE);
		MPSC_CACHE_INVALIDATE(pi, bp, bp + MPSC_RXBE_SIZE);

		/*
		 * Other than for parity error, the manual provides little
		 * info on what data will be in a frame flagged by any of
		 * these errors.  For parity error, it is the last byte in
		 * the buffer that had the error.  As for the rest, I guess
		 * we'll assume there is no data in the buffer.
		 * If there is...it gets lost.
		 */
		if (cmdstat & (SDMA_DESC_CMDSTAT_BR | SDMA_DESC_CMDSTAT_FR |
			SDMA_DESC_CMDSTAT_OR)) {

			pi->port.icount.rx++;

			if (cmdstat & SDMA_DESC_CMDSTAT_BR) { /* Break */
				pi->port.icount.brk++;

				if (uart_handle_break(&pi->port))
					goto next_frame;
			}
			else if (cmdstat & SDMA_DESC_CMDSTAT_FR) /* Framing */
				pi->port.icount.frame++;
			else if (cmdstat & SDMA_DESC_CMDSTAT_OR) /* Overrun */
				pi->port.icount.overrun++;

			cmdstat &= pi->port.read_status_mask;

			if (!(cmdstat & pi->port.ignore_status_mask)) {
				if (cmdstat & SDMA_DESC_CMDSTAT_BR)
					*tty->flip.flag_buf_ptr = TTY_BREAK;
				else if (cmdstat & SDMA_DESC_CMDSTAT_FR)
					*tty->flip.flag_buf_ptr = TTY_FRAME;
				else if (cmdstat & SDMA_DESC_CMDSTAT_OR)
					*tty->flip.flag_buf_ptr = TTY_OVERRUN;

				tty->flip.flag_buf_ptr++;
				*tty->flip.char_buf_ptr = '\0';
				tty->flip.char_buf_ptr++;
				tty->flip.count++;
			}
		}
		else {
			if (uart_handle_sysrq_char(&pi->port, *bp, regs)) {
				bp++;
				bytes_in--;
			}

			memcpy(tty->flip.char_buf_ptr, bp, bytes_in);
			memset(tty->flip.flag_buf_ptr, TTY_NORMAL, bytes_in);

			tty->flip.char_buf_ptr += bytes_in;
			tty->flip.flag_buf_ptr += bytes_in;
			tty->flip.count += bytes_in;
			pi->port.icount.rx += bytes_in;

			cmdstat &= SDMA_DESC_CMDSTAT_PE;

			if (cmdstat) {	/* Parity */
				pi->port.icount.parity++;

				if (!(cmdstat & pi->port.read_status_mask))
					*(tty->flip.flag_buf_ptr-1) = TTY_FRAME;
			}
		}

next_frame:
		dma_sync_single_for_device(pi->port.dev, bp_p,
			MPSC_RXBE_SIZE, DMA_FROM_DEVICE);
		rxre->bytecnt = cpu_to_be16(0);
		wmb(); /* ensure other writes done before cmdstat update */
		rxre->cmdstat = cpu_to_be32(SDMA_DESC_CMDSTAT_O |
			SDMA_DESC_CMDSTAT_EI | SDMA_DESC_CMDSTAT_F |
			SDMA_DESC_CMDSTAT_L);
		MPSC_CACHE_FLUSH(pi, rxre, rxre + 1);

		/* Advance to next descriptor */
		pi->rxr_posn = (pi->rxr_posn + 1) & (MPSC_RXR_ENTRIES - 1);
		rxre = &pi->rxr[pi->rxr_posn];
		MPSC_CACHE_INVALIDATE(pi, rxre, rxre + 1);
	}

	/* Restart rx engine, if its stopped */
	if ((MPSC_READ(pi, sdma, SDMA_SDCM) & SDMA_SDCM_ERD) == 0) {
		mpsc_start_rx(pi);
	}

	tty_flip_buffer_push(tty);
	return;
}

static inline void
mpsc_send_tx_data(mpsc_port_info_t *pi, volatile mpsc_tx_desc_t *txre,
	volatile mpsc_tx_desc_t *txre_p, void *bp, u32 count, u32 intr)
{
	dma_sync_single_for_device(pi->port.dev, be32_to_cpu(txre->buf_ptr),
			MPSC_TXBE_SIZE, DMA_TO_DEVICE);
	MPSC_CACHE_FLUSH(pi, bp, bp + MPSC_TXBE_SIZE);

	txre->bytecnt = cpu_to_be16(count);
	txre->shadow = txre->bytecnt;
	wmb(); /* ensure cmdstat is last field updated */
	txre->cmdstat = cpu_to_be32(SDMA_DESC_CMDSTAT_O | SDMA_DESC_CMDSTAT_F |
		SDMA_DESC_CMDSTAT_L | ((intr) ? SDMA_DESC_CMDSTAT_EI : 0));
	MPSC_CACHE_FLUSH(pi, txre, txre + 1);

	/* Start Tx engine, if its stopped */
	if ((MPSC_READ(pi, sdma, SDMA_SDCM) & SDMA_SDCM_TXD) == 0) {
		mpsc_sdma_start_tx(pi, txre_p);
	}

	return;
}

static inline void
mpsc_tx_intr(mpsc_port_info_t *pi)
{
	volatile mpsc_tx_desc_t	*txre = &pi->txr[pi->txr_posn];
	volatile mpsc_tx_desc_t	*txre_p = &pi->txr_p[pi->txr_posn];
	struct circ_buf		*xmit = &pi->port.info->xmit;
	u8			*bp;
	u32			i;

	MPSC_CACHE_INVALIDATE(pi, txre, txre + 1);

	while (!(be32_to_cpu(txre->cmdstat) & SDMA_DESC_CMDSTAT_O)) {
		bp = &pi->txb[pi->txr_posn * MPSC_TXBE_SIZE];

		dma_sync_single_for_cpu(pi->port.dev,be32_to_cpu(txre->buf_ptr),
			MPSC_TXBE_SIZE, DMA_TO_DEVICE);

		if (pi->port.x_char) {
			/*
			 * Ideally, we should use the TCS field in CHR_1 to
			 * put the x_char out immediately but errata prevents
			 * us from being able to read CHR_2 to know that its
			 * safe to write to CHR_1.  Instead, just put it
			 * in-band with all the other Tx data.
			 */
			*bp = pi->port.x_char;
			pi->port.x_char = 0;
			i = 1;
		}
		else if (!uart_circ_empty(xmit) && !uart_tx_stopped(&pi->port)){
			i = MIN(MPSC_TXBE_SIZE, uart_circ_chars_pending(xmit));
			i = MIN(i, CIRC_CNT_TO_END(xmit->head, xmit->tail,
							UART_XMIT_SIZE));
			memcpy(bp, &xmit->buf[xmit->tail], i);
			xmit->tail = (xmit->tail + i) & (UART_XMIT_SIZE - 1);
		}
		else { /* No more data to transmit or tx engine is stopped */
			MPSC_CACHE_INVALIDATE(pi, txre, txre + 1);
			return;
		}

		mpsc_send_tx_data(pi, txre, txre_p, bp, i, 1);
		pi->port.icount.tx += i;

		if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
			uart_write_wakeup(&pi->port);

		/* Advance to next descriptor */
		pi->txr_posn = (pi->txr_posn + 1) & (MPSC_TXR_ENTRIES - 1);
		txre = &pi->txr[pi->txr_posn];
		txre_p = &pi->txr_p[pi->txr_posn];
		MPSC_CACHE_INVALIDATE(pi, txre, txre + 1);
	}

	return;
}

/*
 * This is the driver's interrupt handler.  To avoid a race, we first clear
 * the interrupt, then handle any completed Rx/Tx descriptors.  When done
 * handling those descriptors, we restart the Rx/Tx engines if they're stopped.
 */
static irqreturn_t
mpsc_sdma_intr(int irq, void *dev_id, struct pt_regs *regs)
{
	mpsc_port_info_t	*pi = dev_id;
	ulong			iflags;

	DBG("mpsc_sdma_intr[%d]: SDMA Interrupt Received\n", pi->port.line);

	spin_lock_irqsave(&pi->port.lock, iflags);
	mpsc_sdma_intr_ack(pi);
	mpsc_rx_intr(pi, regs);
	mpsc_tx_intr(pi);
	spin_unlock_irqrestore(&pi->port.lock, iflags);

	DBG("mpsc_sdma_intr[%d]: SDMA Interrupt Handled\n", pi->port.line);
	return IRQ_HANDLED;
}

/*
 ******************************************************************************
 *
 * serial_core.c Interface routines
 *
 ******************************************************************************
 */

static uint
_mpsc_tx_empty(mpsc_port_info_t *pi)
{
	return (((MPSC_READ(pi, sdma, SDMA_SDCM) & SDMA_SDCM_TXD) == 0) ?
							TIOCSER_TEMT : 0);
}

static uint
mpsc_tx_empty(struct uart_port *port)
{
	mpsc_port_info_t	*pi = (mpsc_port_info_t *)port;
	ulong			iflags;
	uint			rc;

	spin_lock_irqsave(&pi->port.lock, iflags);
	rc = _mpsc_tx_empty(pi);
	spin_unlock_irqrestore(&pi->port.lock, iflags);

	return rc;
}

static void
mpsc_set_mctrl(struct uart_port *port, uint mctrl)
{
	/* Have no way to set modem control lines AFAICT */
	return;
}

static uint
mpsc_get_mctrl(struct uart_port *port)
{
	mpsc_port_info_t	*pi = (mpsc_port_info_t *)port;
	u32			mflags, status;
	ulong			iflags;

	spin_lock_irqsave(&pi->port.lock, iflags);
	status = MPSC_READ_M(pi, mpsc, MPSC_CHR_10);
	spin_unlock_irqrestore(&pi->port.lock, iflags);

	mflags = 0;
	if (status & 0x1)
		mflags |= TIOCM_CTS;
	if (status & 0x2)
		mflags |= TIOCM_CAR;

	return mflags | TIOCM_DSR;	/* No way to tell if DSR asserted */
}

static void
mpsc_stop_tx(struct uart_port *port, uint tty_start)
{
	mpsc_port_info_t *pi = (mpsc_port_info_t *)port;

	DBG("mpsc_stop_tx[%d]: tty_start: %d\n", port->line, tty_start);

	mpsc_freeze(pi);
	return;
}

static void
mpsc_start_tx(struct uart_port *port, uint tty_start)
{
	mpsc_port_info_t *pi = (mpsc_port_info_t *)port;

	mpsc_unfreeze(pi);
	mpsc_tx_intr(pi); /* Load Tx data into Tx ring bufs & go */

	DBG("mpsc_start_tx[%d]: tty_start: %d\n", port->line, tty_start);
	return;
}

static void
mpsc_start_rx(mpsc_port_info_t *pi)
{
	DBG("mpsc_start_rx[%d]: Starting...\n", pi->port.line);

	if (pi->rcv_data) {
		mb();
		mpsc_enter_hunt(pi);
		mpsc_sdma_cmd(pi, SDMA_SDCM_ERD);
	}
	return;
}

static void
mpsc_stop_rx(struct uart_port *port)
{
	mpsc_port_info_t *pi = (mpsc_port_info_t *)port;

	DBG("mpsc_stop_rx[%d]: Stopping...\n", port->line);

	mpsc_sdma_cmd(pi, SDMA_SDCM_AR);
	return;
}

static void
mpsc_enable_ms(struct uart_port *port)
{
	return;		/* Not supported */
}

static void
mpsc_break_ctl(struct uart_port *port, int ctl)
{
	mpsc_port_info_t	*pi = (mpsc_port_info_t *)port;
	ulong			flags;

	spin_lock_irqsave(&pi->port.lock, flags);
	if (ctl) {
		/* Send as many BRK chars as we can */
		MPSC_WRITE_M(pi, mpsc, MPSC_CHR_1, 0x00ff0000);
	}
	else {
		/* Stop sending BRK chars */
		MPSC_WRITE_M(pi, mpsc, MPSC_CHR_1, 0);
	}
	spin_unlock_irqrestore(&pi->port.lock, flags);

	return;
}

static int
mpsc_startup(struct uart_port *port)
{
	mpsc_port_info_t	*pi = (mpsc_port_info_t *)port;
	int			rc;

	DBG("mpsc_startup[%d]: Starting up MPSC, irq: %d\n",
		port->line, pi->port.irq);

	if ((rc = mpsc_make_ready(pi)) == 0) {
		/* Setup IRQ handler */
		mpsc_sdma_intr_ack(pi);
		mpsc_sdma_intr_unmask(pi, 0xf);

		if (request_irq(pi->port.irq, mpsc_sdma_intr, 0, "MPSC/SDMA",
									pi)) {
			printk(KERN_ERR "MPSC: Can't get SDMA IRQ");
			printk("MPSC: Can't get SDMA IRQ %d\n", pi->port.irq);
		}

		mpsc_sdma_set_rx_ring(pi, &pi->rxr_p[pi->rxr_posn]);
		mpsc_start_rx(pi);
	}

	return rc;
}

static void
mpsc_shutdown(struct uart_port *port)
{
	mpsc_port_info_t *pi = (mpsc_port_info_t *)port;
	static void mpsc_release_port(struct uart_port *port);

	DBG("mpsc_shutdown[%d]: Shutting down MPSC\n", port->line);

	mpsc_sdma_stop(pi);
	free_irq(pi->port.irq, pi);
	return;
}

static void
mpsc_set_termios(struct uart_port *port, struct termios *termios,
		       struct termios *old)
{
	mpsc_port_info_t	*pi = (mpsc_port_info_t *)port;
	u32			baud, quot;
	ulong			flags;
	u32			chr_bits, stop_bits, par;

	pi->c_iflag = termios->c_iflag;
	pi->c_cflag = termios->c_cflag;

	switch (termios->c_cflag & CSIZE) {
		case CS5:
			chr_bits = MPSC_MPCR_CL_5;
			break;
		case CS6:
			chr_bits = MPSC_MPCR_CL_6;
			break;
		case CS7:
			chr_bits = MPSC_MPCR_CL_7;
			break;
		default:
		case CS8:
			chr_bits = MPSC_MPCR_CL_8;
			break;
	}

	if (termios->c_cflag & CSTOPB)
		stop_bits = MPSC_MPCR_SBL_2;
	else
		stop_bits = MPSC_MPCR_SBL_1;

	if (termios->c_cflag & PARENB) {
		if (termios->c_cflag & PARODD)
			par = MPSC_CHR_2_PAR_ODD;
		else
			par = MPSC_CHR_2_PAR_EVEN;
#ifdef	CMSPAR
		if (termios->c_cflag & CMSPAR) {
			if (termios->c_cflag & PARODD)
				par = MPSC_CHR_2_PAR_MARK;
			else
				par = MPSC_CHR_2_PAR_SPACE;
		}
#endif
	}

	baud = uart_get_baud_rate(port, termios, old, 0, port->uartclk); 
	quot = uart_get_divisor(port, baud);

	spin_lock_irqsave(&pi->port.lock, flags);

	uart_update_timeout(port, termios->c_cflag, baud);

	mpsc_set_char_length(pi, chr_bits);
	mpsc_set_stop_bit_length(pi, stop_bits);
	mpsc_set_parity(pi, par);
	mpsc_set_baudrate(pi, baud);

	/* Characters/events to read */
	pi->rcv_data = 1;
	pi->port.read_status_mask = SDMA_DESC_CMDSTAT_OR;

	if (termios->c_iflag & INPCK)
		pi->port.read_status_mask |= SDMA_DESC_CMDSTAT_PE |
			SDMA_DESC_CMDSTAT_FR;

	if (termios->c_iflag & (BRKINT | PARMRK))
		pi->port.read_status_mask |= SDMA_DESC_CMDSTAT_BR;

	/* Characters/events to ignore */
	pi->port.ignore_status_mask = 0;

	if (termios->c_iflag & IGNPAR)
		pi->port.ignore_status_mask |= SDMA_DESC_CMDSTAT_PE |
			SDMA_DESC_CMDSTAT_FR;

	if (termios->c_iflag & IGNBRK) {
		pi->port.ignore_status_mask |= SDMA_DESC_CMDSTAT_BR;

		if (termios->c_iflag & IGNPAR)
			pi->port.ignore_status_mask |= SDMA_DESC_CMDSTAT_OR;
	}

	/* Ignore all chars if CREAD not set */
	if (!(termios->c_cflag & CREAD))
		pi->rcv_data = 0;

	spin_unlock_irqrestore(&pi->port.lock, flags);
	return;
}

static const char *
mpsc_type(struct uart_port *port)
{
	DBG("mpsc_type[%d]: port type: %s\n", port->line, MPSC_DRIVER_NAME);
	return MPSC_DRIVER_NAME;
}

static int
mpsc_request_port(struct uart_port *port)
{
	/* Should make chip/platform specific call */
	return 0;
}

static void
mpsc_release_port(struct uart_port *port)
{
	mpsc_port_info_t *pi = (mpsc_port_info_t *)port;

	mpsc_uninit_rings(pi);
	mpsc_free_ring_mem(pi);
	pi->ready = 0;

	return;
}

static void
mpsc_config_port(struct uart_port *port, int flags)
{
	return;
}

static int
mpsc_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	mpsc_port_info_t *pi = (mpsc_port_info_t *)port;
	int	rc = 0;

	DBG("mpsc_verify_port[%d]: Verifying port data\n", pi->port.line);

	if (ser->type != PORT_UNKNOWN && ser->type != PORT_MPSC)
		rc = -EINVAL;
	if (pi->port.irq != ser->irq)
		rc = -EINVAL;
	if (ser->io_type != SERIAL_IO_MEM)
		rc = -EINVAL;
	if (pi->port.uartclk / 16 != ser->baud_base) /* XXXX Not sure */
		rc = -EINVAL;
	if ((void *)pi->port.mapbase != ser->iomem_base)
		rc = -EINVAL;
	if (pi->port.iobase != ser->port)
		rc = -EINVAL;
	if (ser->hub6 != 0)
		rc = -EINVAL;

	return rc;
}

static struct uart_ops mpsc_pops = {
	.tx_empty	= mpsc_tx_empty,
	.set_mctrl	= mpsc_set_mctrl,
	.get_mctrl	= mpsc_get_mctrl,
	.stop_tx	= mpsc_stop_tx,
	.start_tx	= mpsc_start_tx,
	.stop_rx	= mpsc_stop_rx,
	.enable_ms	= mpsc_enable_ms,
	.break_ctl	= mpsc_break_ctl,
	.startup	= mpsc_startup,
	.shutdown	= mpsc_shutdown,
	.set_termios	= mpsc_set_termios,
	.type		= mpsc_type,
	.release_port	= mpsc_release_port,
	.request_port	= mpsc_request_port,
	.config_port	= mpsc_config_port,
	.verify_port	= mpsc_verify_port,
};

/*
 ******************************************************************************
 *
 * Console Interface Routines
 *
 ******************************************************************************
 */

#ifdef CONFIG_SERIAL_MPSC_CONSOLE
static void
mpsc_console_write(struct console *co, const char *s, uint count)
{
	mpsc_port_info_t	*pi = &mpsc_ports[co->index];
	volatile mpsc_tx_desc_t	*txre = &pi->txr[pi->txr_posn];
	volatile mpsc_tx_desc_t	*txre_p = &pi->txr_p[pi->txr_posn];
	u8			*bp, *dp, add_cr = 0;
	int			i;

	/*
	 * Step thru tx ring one entry at a time, filling up its buf, sending
	 * the data out and moving to the next ring entry until its all out.
	 */
	MPSC_CACHE_INVALIDATE(pi, txre, txre + 1);

	while (count > 0) {
		while (_mpsc_tx_empty(pi) != TIOCSER_TEMT);

		BUG_ON(be32_to_cpu(txre->cmdstat) & SDMA_DESC_CMDSTAT_O);

		bp = dp = &pi->txb[pi->txr_posn * MPSC_TXBE_SIZE];

		dma_sync_single_for_cpu(pi->port.dev,be32_to_cpu(txre->buf_ptr),
			MPSC_TXBE_SIZE, DMA_TO_DEVICE);

		for (i=0; i<MPSC_TXBE_SIZE; i++) {
			if (count == 0)
				break;

			if (add_cr) {
				*(dp++) = '\r';
				add_cr = 0;
			}
			else {
				*(dp++) = *s;

				if (*(s++) == '\n') { /* add '\r' after '\n' */
					add_cr = 1;
					count++;
				}
			}

			count--;
		}

		mpsc_send_tx_data(pi, txre, txre_p, bp, i, 0);

		/* Advance to next descriptor */
		pi->txr_posn = (pi->txr_posn + 1) & (MPSC_TXR_ENTRIES - 1);
		txre = &pi->txr[pi->txr_posn];
		txre_p = &pi->txr_p[pi->txr_posn];
		MPSC_CACHE_INVALIDATE(pi, txre, txre + 1);
	}

	while (_mpsc_tx_empty(pi) != TIOCSER_TEMT);
	return;
}

static int __init
mpsc_console_setup(struct console *co, char *options)
{
	mpsc_port_info_t	*pi;
	int			baud, bits, parity, flow;

	DBG("mpsc_console_setup[%d]: options: %s\n", co->index, options);

	if (co->index >= MPSC_NUM_CTLRS)
		co->index = 0;

	pi = &mpsc_ports[co->index];

	baud = pi->default_baud;
	bits = pi->default_bits;
	parity = pi->default_parity;
	flow = pi->default_flow;

	if (!pi->port.ops)
		return -ENODEV;

	spin_lock_init(&pi->port.lock); /* Temporary fix--copied from 8250.c */

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(&pi->port, co, baud, parity, bits, flow);
}

extern struct uart_driver mpsc_reg;
static struct console mpsc_console = {
	.name		= MPSC_DEV_NAME,
	.write		= mpsc_console_write,
	.device		= uart_console_device,
	.setup		= mpsc_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &mpsc_reg,
};

static int __init
mpsc_console_init(void)
{
	DBG("mpsc_console_init: Enter\n");
	register_console(&mpsc_console);
	return 0;
}
console_initcall(mpsc_console_init);

static int __init
mpsc_late_console_init(void)
{
	DBG("mpsc_late_console_init: Enter\n");

	if (!(mpsc_console.flags & CON_ENABLED))
		register_console(&mpsc_console);
	return 0;
}
late_initcall(mpsc_late_console_init);

#define MPSC_CONSOLE	&mpsc_console
#else
#define MPSC_CONSOLE	NULL
#endif

/*
 ******************************************************************************
 *
 * Driver Interface Routines
 *
 ******************************************************************************
 */

static void
mpsc_map_regs(mpsc_port_info_t *pi)
{
	pi->mpsc_base = (u32)ioremap(pi->mpsc_base_p, MPSC_REG_BLOCK_SIZE);
	pi->mpsc_routing_base = (u32)ioremap(pi->mpsc_routing_base_p,
						MPSC_ROUTING_REG_BLOCK_SIZE);
	pi->sdma_base = (u32)ioremap(pi->sdma_base_p, SDMA_REG_BLOCK_SIZE);
	pi->sdma_intr_base = (u32)ioremap(pi->sdma_intr_base_p,
						SDMA_INTR_REG_BLOCK_SIZE);
	pi->brg_base = (u32)ioremap(pi->brg_base_p, BRG_REG_BLOCK_SIZE);

	return;
}

static void
mpsc_unmap_regs(mpsc_port_info_t *pi)
{
	iounmap((void *)pi->mpsc_base);
	iounmap((void *)pi->mpsc_routing_base);
	iounmap((void *)pi->sdma_base);
	iounmap((void *)pi->sdma_intr_base);
	iounmap((void *)pi->brg_base);

	pi->mpsc_base = 0;
	pi->mpsc_routing_base = 0;
	pi->sdma_base = 0;
	pi->sdma_intr_base = 0;
	pi->brg_base = 0;

	return;
}

/* Called from platform specific device probe routine */
mpsc_port_info_t *
mpsc_device_probe(int index)
{
	mpsc_port_info_t	*pi = NULL;

	if ((index >= 0) && (index < MPSC_NUM_CTLRS))
		pi = &mpsc_ports[index];

	return pi;
}

/* Called from platform specific device remove routine */
mpsc_port_info_t *
mpsc_device_remove(int index)
{
	mpsc_port_info_t	*pi = NULL;

	if ((index >= 0) && (index < MPSC_NUM_CTLRS))
		pi = &mpsc_ports[index];

	return pi;
}

static struct uart_driver mpsc_reg = {
	.owner			= THIS_MODULE,
	.driver_name		= MPSC_DRIVER_NAME,
	.devfs_name		= MPSC_DEVFS_NAME,
	.dev_name		= MPSC_DEV_NAME,
	.major			= MPSC_MAJOR,
	.minor			= MPSC_MINOR_START,
	.nr			= MPSC_NUM_CTLRS,
	.cons			= MPSC_CONSOLE,
};

static int __init
mpsc_init(void)
{
	mpsc_port_info_t	*pi;
	int			i, j, rc;

	printk(KERN_INFO "Serial: MPSC driver $Revision: 1.00 $\n");

	if ((rc = mpsc_platform_register_driver()) >= 0) {
		if ((rc = uart_register_driver(&mpsc_reg)) < 0) {
			mpsc_platform_unregister_driver();
		}
		else {
			for (i=0; i<MPSC_NUM_CTLRS; i++) {
				pi = &mpsc_ports[i];

				pi->port.line = i;
				pi->port.type = PORT_MPSC;
				pi->port.fifosize = MPSC_TXBE_SIZE;
				pi->port.membase = (char *)pi->mpsc_base;
				pi->port.mapbase = (ulong)pi->mpsc_base;
				pi->port.ops = &mpsc_pops;

				mpsc_map_regs(pi);

				if ((rc = mpsc_make_ready(pi)) >= 0) {
					uart_add_one_port(&mpsc_reg, &pi->port);
				}
				else { /* on failure, undo everything */
					for (j=0; j<i; j++) {
						mpsc_unmap_regs(&mpsc_ports[j]);
						uart_remove_one_port(&mpsc_reg,
							&mpsc_ports[j].port);
					}

					uart_unregister_driver(&mpsc_reg);
					mpsc_platform_unregister_driver();
					break;
				}
			}
		}
	}

	return rc;
}

static void __exit
mpsc_exit(void)
{
	int	i;

	DBG("mpsc_exit: Exiting\n");

	for (i=0; i<MPSC_NUM_CTLRS; i++) {
		mpsc_unmap_regs(&mpsc_ports[i]);
		uart_remove_one_port(&mpsc_reg, &mpsc_ports[i].port);
	}

	uart_unregister_driver(&mpsc_reg);
	mpsc_platform_unregister_driver();

	return;
}

int
register_serial(struct serial_struct *req)
{
	return uart_register_port(&mpsc_reg, &mpsc_ports[req->line].port);
}

void
unregister_serial(int line)
{
	uart_unregister_port(&mpsc_reg, line);
	return;
}

module_init(mpsc_init);
module_exit(mpsc_exit);

EXPORT_SYMBOL(register_serial);
EXPORT_SYMBOL(unregister_serial);

MODULE_AUTHOR("Mark A. Greer <mgreer@mvista.com>");
MODULE_DESCRIPTION("Generic Marvell MPSC serial/UART driver $Revision: 1.00 $");
MODULE_VERSION(MPSC_VERSION);
MODULE_LICENSE("GPL");
MODULE_ALIAS_CHARDEV_MAJOR(MPSC_MAJOR);
