/* ckrm_ce.h - Header file to be used by Classification Engine of CKRM
 *
 * Copyright (C) Hubertus Franke, IBM Corp. 2003
 *           (C) Shailabh Nagar,  IBM Corp. 2003
 *           (C) Chandra Seetharaman, IBM Corp. 2003
 * 
 * Provides data structures, macros and kernel API of CKRM for 
 * classification engine.
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
 * 12 Nov 2003
 *        Created.
 * 22 Apr 2004
 *        Adopted to classtypes
 */

#ifndef _LINUX_CKRM_CE_H
#define _LINUX_CKRM_CE_H

#ifdef CONFIG_CKRM

#include <linux/ckrm.h>		// getting the event names

/* Action parameters identifying the cause of a task<->class notify callback 
 * these can perculate up to user daemon consuming records send by the 
 * classification engine
 */

#ifdef __KERNEL__

typedef void *(*ce_classify_fct_t) (enum ckrm_event event, void *obj, ...);
typedef void (*ce_notify_fct_t) (enum ckrm_event event, void *classobj,
				 void *obj);

typedef struct ckrm_eng_callback {
	/* general state information */
	int always_callback;	/* set if CE should always be called back 
				   regardless of numclasses */




	/* callbacks which are called without holding locks */

	unsigned long c_interest;	/* set of classification events of 
					   interest to CE 
					*/

	/* generic classify */
	ce_classify_fct_t classify;	
	/* class added */
	void (*class_add) (const char *name, void *core, int classtype);
	/* class deleted */
	void (*class_delete) (const char *name, void *core, int classtype);


	/* callbacks which are called while holding task_lock(tsk) */

	unsigned long n_interest;	/* set of notification events of 
					   interest to CE 
					*/
	/* notify on class switch */
	ce_notify_fct_t notify;	

} ckrm_eng_callback_t;

struct inode;
struct dentry;

typedef struct rbce_eng_callback {
	int (*mkdir) (struct inode *, struct dentry *, int);	// mkdir
	int (*rmdir) (struct inode *, struct dentry *);	// rmdir
	int (*mnt) (void);
	int (*umnt) (void);
} rbce_eng_callback_t;

extern int ckrm_register_engine(const char *name, ckrm_eng_callback_t *);
extern int ckrm_unregister_engine(const char *name);

extern void *ckrm_classobj(char *, int *classtype);
extern int get_exe_path_name(struct task_struct *t, char *filename,
			     int max_size);

extern int rcfs_register_engine(rbce_eng_callback_t *);
extern int rcfs_unregister_engine(rbce_eng_callback_t *);

extern int ckrm_reclassify(int pid);

#ifndef _LINUX_CKRM_RC_H
// ckrm kernel has inlined functions for this which are exported
extern void ckrm_core_grab(void *);
extern void ckrm_core_drop(void *);
#endif

#endif				// CONFIG_CKRM

#endif				// __KERNEL__

#endif				// _LINUX_CKRM_CE_H