#ifndef __DMV182_H
#define __DMV182_H

#include <linux/config.h>
#include <linux/types.h>

#define dmv182_board_io_phys 0xe0000000
#define dmv182_board_io_size 0x00040000

#ifdef __BOOTER__
#define dmv182_board_io_virt ((u8 *)dmv182_board_io_phys)
#else
#define dmv182_board_io_virt ((u8 *)0xf0000000)
#endif

#define dmv182_fpga_io (dmv182_board_io_virt + 0x10000)
#define dmv182_rtc     (dmv182_board_io_virt + 0x20000)
#define dmv182_nvram   (dmv182_board_io_virt + 0x30000)

// This has to go above the mv64360 interrupts, as even though
// the mv64360 code can handle relocating its interrupt range,
// the device drivers themselves are oblivious to this.

#define DMV182_IRQ_TEMPA        96
#define DMV182_IRQ_TEMPB        97
#define DMV182_IRQ_TEMPC        98
#define DMV182_IRQ_TEMPD        99
#define DMV182_IRQ_PMC1A       100
#define DMV182_IRQ_PMC1B       101
#define DMV182_IRQ_PMC1C       102
#define DMV182_IRQ_PMC1D       103
#define DMV182_IRQ_PMC2A       104
#define DMV182_IRQ_PMC2B       105
#define DMV182_IRQ_PMC2C       106
#define DMV182_IRQ_PMC2D       107
#define DMV182_IRQ_ENET_PHY2   108
#define DMV182_IRQ_ENET_PHY1   109
#define DMV182_IRQ_IPM0        110
#define DMV182_IRQ_IPM1        111
#define DMV182_IRQ_USB_A       112
#define DMV182_IRQ_USB_B       113
#define DMV182_IRQ_USB_C       114
#define DMV182_IRQ_USB_SMI     115
#define DMV182_IRQ_RTC         116
#define DMV182_IRQ_WDOG_CPU0   117
#define DMV182_IRQ_WDOG_CPU1   118
#define DMV182_IRQ_TIMER0_CPU0 120
#define DMV182_IRQ_TIMER1_CPU0 121
#define DMV182_IRQ_TIMER2_CPU0 122
#define DMV182_IRQ_TIMER0_CPU1 123
#define DMV182_IRQ_TIMER1_CPU1 124
#define DMV182_IRQ_TIMER2_CPU1 125
#define DMV182_IRQ_SERIAL_CH1  126
#define DMV182_IRQ_SERIAL_CH2  127
#define DMV182_IRQ_VME_CPU0    128
#define DMV182_IRQ_VME_CPU1    129

// 28 FPGA interrupts starting from here
#define DMV182_IRQ_FPGA        132

#endif
