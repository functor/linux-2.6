#ifndef __DMV182_SERIAL_H
#define __DMV182_SERIAL_H

#include <linux/serial.h>
#include <platforms/dmv182.h>

#define BASE_BAUD (36864000 / 16)
#define RS_TABLE_SIZE 2

#define STD_UART_OP(num) \
	{ .baud_base = BASE_BAUD, \
	  .irq = DMV182_IRQ_SERIAL_CH##num, \
	  .flags = ASYNC_SKIP_TEST | ASYNC_BUGGY_UART, \
	  .iomem_base = dmv182_fpga_io + 0x18 + 8 * (num - 1), \
	  .io_type = SERIAL_IO_MEM },

#define SERIAL_PORT_DFNS STD_UART_OP(1) STD_UART_OP(2)

#endif
