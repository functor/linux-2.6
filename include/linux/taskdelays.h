#ifndef _LINUX_TASKDELAYS_H
#define _LINUX_TASKDELAYS_H

#include <linux/config.h>
#include <linux/types.h>

struct task_delay_info {
#ifdef CONFIG_DELAY_ACCT
	/* delay statistics in usecs */
	uint64_t waitcpu_total;
	uint64_t runcpu_total;
	uint64_t iowait_total;
	uint64_t mem_iowait_total;
	uint32_t runs;
	uint32_t num_iowaits;
	uint32_t num_memwaits;
#endif
};

#endif				// _LINUX_TASKDELAYS_H
