#ifndef _VX_VS_BASE_H
#define _VX_VS_BASE_H

#include "vserver/context.h"


#define vx_task_xid(t)	((t)->xid)

#define vx_current_xid() vx_task_xid(current)

#define vx_check(c,m)	__vx_check(vx_current_xid(),c,m)

#define vx_weak_check(c,m)	((m) ? vx_check(c,m) : 1)


/*
 * check current context for ADMIN/WATCH and
 * optionally agains supplied argument
 */
static __inline__ int __vx_check(xid_t cid, xid_t id, unsigned int mode)
{
	if (mode & VX_ARG_MASK) {
		if ((mode & VX_IDENT) &&
			(id == cid))
			return 1;
	}
	if (mode & VX_ATR_MASK) {
		if ((mode & VX_DYNAMIC) &&
			(id >= MIN_D_CONTEXT) &&
			(id <= MAX_S_CONTEXT))
			return 1;
		if ((mode & VX_STATIC) &&
			(id > 1) && (id < MIN_D_CONTEXT))
			return 1;
	}
	return (((mode & VX_ADMIN) && (cid == 0)) ||
		((mode & VX_WATCH) && (cid == 1)));
}


#define __vx_flags(v,m,f)	(((v) & (m)) ^ (f))

#define	__vx_task_flags(t,m,f) \
	(((t) && ((t)->vx_info)) ? \
		__vx_flags((t)->vx_info->vx_flags,(m),(f)) : 0)

#define vx_current_flags() \
	((current->vx_info) ? current->vx_info->vx_flags : 0)

#define vx_flags(m,f)	__vx_flags(vx_current_flags(),(m),(f))


#define vx_current_ccaps() \
	((current->vx_info) ? current->vx_info->vx_ccaps : 0)

#define vx_ccaps(c)	(vx_current_ccaps() & (c))

#define vx_current_bcaps() \
	(((current->vx_info) && !vx_flags(VXF_STATE_SETUP, 0)) ? \
	current->vx_info->vx_bcaps : cap_bset)


/* generic flag merging */

#define	vx_mask_flags(v,f,m)	(((v) & ~(m)) | ((f) & (m)))

#define	vx_mask_mask(v,f,m)	(((v) & ~(m)) | ((v) & (f) & (m)))

#endif
