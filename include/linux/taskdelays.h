/* taskdelays.h - for delay accounting
 *
 * Copyright (C) Hubertus Franke, IBM Corp. 2003, 2004
 * 
 * Has the data structure for delay counting.
 *
 * Latest version, more details at http://ckrm.sf.net
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2.1 of the GNU Lesser General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 *
 */

/* Changes
 *
 * 24 Aug 2003
 *    Created.
 */

#ifndef _LINUX_TASKDELAYS_H
#define _LINUX_TASKDELAYS_H

#include <linux/config.h>
#include <linux/types.h>

struct task_delay_info {
#if defined CONFIG_DELAY_ACCT 
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
