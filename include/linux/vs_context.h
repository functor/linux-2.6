#ifndef _VX_VS_CONTEXT_H
#define _VX_VS_CONTEXT_H


#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>

#include "vserver/context.h"
#include "vserver/debug.h"


extern int proc_pid_vx_info(struct task_struct *, char *);


#define get_vx_info(i)	__get_vx_info(i,__FILE__,__LINE__)

static inline struct vx_info *__get_vx_info(struct vx_info *vxi,
	const char *_file, int _line)
{
	if (!vxi)
		return NULL;
	vxlprintk(VXD_CBIT(xid, 2), "get_vx_info(%p[#%d.%d])",
		vxi, vxi?vxi->vx_id:0, vxi?atomic_read(&vxi->vx_usecnt):0,
		_file, _line);
	atomic_inc(&vxi->vx_usecnt);
	return vxi;
}


extern void free_vx_info(struct vx_info *);

#define put_vx_info(i)	__put_vx_info(i,__FILE__,__LINE__)

static inline void __put_vx_info(struct vx_info *vxi, const char *_file, int _line)
{
	if (!vxi)
		return;
	vxlprintk(VXD_CBIT(xid, 2), "put_vx_info(%p[#%d.%d])",
		vxi, vxi?vxi->vx_id:0, vxi?atomic_read(&vxi->vx_usecnt):0,
		_file, _line);
	if (atomic_dec_and_test(&vxi->vx_usecnt))
		free_vx_info(vxi);
}

#define set_vx_info(p,i) __set_vx_info(p,i,__FILE__,__LINE__)

static inline void __set_vx_info(struct vx_info **vxp, struct vx_info *vxi,
	const char *_file, int _line)
{
	struct vx_info *vxo;

	if (!vxi)
		return;

	vxlprintk(VXD_CBIT(xid, 3), "set_vx_info(%p[#%d.%d.%d])",
		vxi, vxi?vxi->vx_id:0,
		vxi?atomic_read(&vxi->vx_usecnt):0,
		vxi?atomic_read(&vxi->vx_refcnt):0,
		_file, _line);

	atomic_inc(&vxi->vx_refcnt);
	vxo = xchg(vxp, __get_vx_info(vxi, _file, _line));
	BUG_ON(vxo);
}

#define clr_vx_info(p)	__clr_vx_info(p,__FILE__,__LINE__)

static inline void __clr_vx_info(struct vx_info **vxp,
	const char *_file, int _line)
{
	struct vx_info *vxo;

	vxo = xchg(vxp, NULL);
	if (!vxo)
		return;

	vxlprintk(VXD_CBIT(xid, 3), "clr_vx_info(%p[#%d.%d.%d])",
		vxo, vxo?vxo->vx_id:0,
		vxo?atomic_read(&vxo->vx_usecnt):0,
		vxo?atomic_read(&vxo->vx_refcnt):0,
		_file, _line);

	if (atomic_dec_and_test(&vxo->vx_refcnt))
		unhash_vx_info(vxo);
	__put_vx_info(vxo, _file, _line);
}


#define task_get_vx_info(i)	__task_get_vx_info(i,__FILE__,__LINE__)

static __inline__ struct vx_info *__task_get_vx_info(struct task_struct *p,
	const char *_file, int _line)
{
	struct vx_info *vxi;

	task_lock(p);
	vxlprintk(VXD_CBIT(xid, 5), "task_get_vx_info(%p)",
		p, _file, _line);
	vxi = __get_vx_info(p->vx_info, _file, _line);
	task_unlock(p);
	return vxi;
}


#define vx_verify_info(p,i)	\
	__vx_verify_info((p)->vx_info,i,__FILE__,__LINE__)

static __inline__ void __vx_verify_info(
	struct vx_info *vxa, struct vx_info *vxb,
	const char *_file, int _line)
{
	if (vxa == vxb)
		return;
	printk(KERN_ERR "vx bad assumption (%p==%p) at %s:%d\n",
		vxa, vxb, _file, _line);
}


#else
#warning duplicate inclusion
#endif