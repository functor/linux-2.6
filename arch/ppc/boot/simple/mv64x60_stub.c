/*
 * arch/ppc/boot/simple/mv64x60_stub.c
 *
 * Stub for board_init() routine called from mv64x60_init().
 *
 * Author: Mark A. Greer <mgreer@mvista.com>
 *
 * 2002 (c) MontaVista, Software, Inc.  This file is licensed under the terms
 * of the GNU General Public License version 2.  This program is licensed
 * "as is" without any warranty of any kind, whether express or implied.
 */

long	mv64x60_console_baud = 9600;		/* Default baud: 9600 */
long	mv64x60_mpsc_clk_src = 8;		/* Default clk src: TCLK */
long	mv64x60_mpsc_clk_freq = 100000000;	/* Default clk freq: 100 MHz */

void
mv64x60_board_init(void)
{
}
