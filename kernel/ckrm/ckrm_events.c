/* ckrm_events.c - Class-based Kernel Resource Management (CKRM)
 *               - event handling routines
 *
 * Copyright (C) Hubertus Franke, IBM Corp. 2003, 2004
 *           (C) Chandra Seetharaman,  IBM Corp. 2003
 * 
 * 
 * Provides API for event registration and handling for different
 * classtypes.
 *
 * Latest version, more details at http://ckrm.sf.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

/* Changes
 *
 * 29 Sep 2004
 *        Separated from ckrm.c
 *  
 */

#include <linux/config.h>
#include <linux/stddef.h>
#include <linux/ckrm_events.h>

/*******************************************************************
 *   Event callback invocation
 *******************************************************************/

struct ckrm_hook_cb *ckrm_event_callbacks[CKRM_NONLATCHABLE_EVENTS];

/* Registration / Deregistration / Invocation functions */

int ckrm_register_event_cb(enum ckrm_event ev, struct ckrm_hook_cb *cb)
{
	struct ckrm_hook_cb **cbptr;

	if ((ev < CKRM_LATCHABLE_EVENTS) || (ev >= CKRM_NONLATCHABLE_EVENTS))
		return 1;
	cbptr = &ckrm_event_callbacks[ev];
	while (*cbptr != NULL)
		cbptr = &((*cbptr)->next);
	*cbptr = cb;
	return 0;
}

int ckrm_unregister_event_cb(enum ckrm_event ev, struct ckrm_hook_cb *cb)
{
	struct ckrm_hook_cb **cbptr;

	if ((ev < CKRM_LATCHABLE_EVENTS) || (ev >= CKRM_NONLATCHABLE_EVENTS))
		return -1;
	cbptr = &ckrm_event_callbacks[ev];
	while ((*cbptr != NULL) && (*cbptr != cb))
		cbptr = &((*cbptr)->next);
	if (*cbptr)
		(*cbptr)->next = cb->next;
	return (*cbptr == NULL);
}

int ckrm_register_event_set(struct ckrm_event_spec especs[])
{
	struct ckrm_event_spec *espec = especs;

	for (espec = especs; espec->ev != -1; espec++)
		ckrm_register_event_cb(espec->ev, &espec->cb);
	return 0;
}

int ckrm_unregister_event_set(struct ckrm_event_spec especs[])
{
	struct ckrm_event_spec *espec = especs;

	for (espec = especs; espec->ev != -1; espec++)
		ckrm_unregister_event_cb(espec->ev, &espec->cb);
	return 0;
}

#define ECC_PRINTK(fmt, args...) \
// printk("%s: " fmt, __FUNCTION__ , ## args)

void ckrm_invoke_event_cb_chain(enum ckrm_event ev, void *arg)
{
	struct ckrm_hook_cb *cb, *anchor;

	ECC_PRINTK("%d %x\n", current, ev, arg);
	if ((anchor = ckrm_event_callbacks[ev]) != NULL) {
		for (cb = anchor; cb; cb = cb->next)
			(*cb->fct) (arg);
	}
}

