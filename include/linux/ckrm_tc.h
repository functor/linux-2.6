/* ckrm_tc.h - Header file to be used by task class users
 *
 * Copyright (C) Hubertus Franke, IBM Corp. 2003, 2004
 * 
 * Provides data structures, macros and kernel API for the 
 * classtype, taskclass.
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
 */

/* Changes
 *
 * 12 Apr 2004
 *        Created.
 */

#ifndef _LINUX_CKRM_TC_H_
#define _LINUX_CKRM_TC_H_

#ifdef CONFIG_CKRM_TYPE_TASKCLASS
#include <linux/ckrm_rc.h>

#define TASK_CLASS_TYPE_NAME "taskclass"

typedef struct ckrm_task_class {
	struct ckrm_core_class core;
} ckrm_task_class_t;

// Index into genmfdesc array, defined in rcfs/dir_modules.c,
// which has the mfdesc entry that taskclass wants to use
#define TC_MF_IDX  0

extern int ckrm_forced_reclassify_pid(int, struct ckrm_task_class *);

#else // CONFIG_CKRM_TYPE_TASKCLASS

#define ckrm_forced_reclassify_pid(a, b) (0)

#endif

#endif // _LINUX_CKRM_TC_H_
