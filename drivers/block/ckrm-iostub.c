/* ckrm-iostub.c - Stub file for ckrm_io module
 *
 * Copyright (C) Shailabh Nagar,  IBM Corp. 2004
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
 * 07 Aug 2004: Created
 * 
 */

#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/ckrm-io.h>

static spinlock_t stub_lock = SPIN_LOCK_UNLOCKED;

static icls_tsk_t tskiclstub;
static icls_ioprio_t tskiopriostub;
static icls_tsk_t tskcfqprivstub;

void cki_cfq_set(icls_tsk_t tskicls, icls_ioprio_t tskioprio, icls_tsk_t tskcfqpriv)
{
	spin_lock(&stub_lock);
	tskiclstub = tskicls;
	tskiopriostub = tskioprio;
	tskcfqprivstub = tskcfqpriv;
	spin_unlock(&stub_lock);
}

void *cki_hash_key(struct task_struct *tsk)
{
	void *ret;
	spin_lock(&stub_lock);
	if (tskiclstub)
		ret = (*tskiclstub)(tsk);
	else 
		ret = (void *) tsk->tgid;
	spin_unlock(&stub_lock);
	return ret;
}

int cki_ioprio(struct task_struct *tsk)
{
	int ret;
	spin_lock(&stub_lock);
	if (tskiopriostub) 
		ret = (*tskiopriostub)(tsk);
	else 
		ret = tsk->ioprio;
	spin_unlock(&stub_lock);
	return ret;
}

void *cki_cfqpriv(struct task_struct *tsk)
{
	void *ret;
	spin_lock(&stub_lock);
	if (tskiclstub)
		ret = (*tskcfqprivstub)(tsk);
	else 
		ret = NULL;
	spin_unlock(&stub_lock);
	return ret;
}    

EXPORT_SYMBOL(cki_cfq_set);
EXPORT_SYMBOL(cki_hash_key);
EXPORT_SYMBOL(cki_ioprio);
EXPORT_SYMBOL(cki_cfqpriv);
