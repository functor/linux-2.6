/* include/linux/ckrm_tc.h - general definitions for the CKRM TaskClass
 *
 * Copyright (C) Hubertus Franke,  IBM Corp. 2004
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef _CKRM_TC_H
#define _CKRM_TC_H

#include <linux/ckrm_rc.h>

#define TASK_CLASS_TYPE_NAME "taskclass"

typedef struct ckrm_task_class {
	struct ckrm_core_class core;
} ckrm_task_class_t;

// Index into genmfdesc array, defined in rcfs/dir_modules.c,
// which has the mfdesc entry that taskclass wants to use
#define TC_MF_IDX  0

extern int ckrm_forced_reclassify_pid(int pid, struct ckrm_task_class *cls);

#endif // _CKRM_TC_H
