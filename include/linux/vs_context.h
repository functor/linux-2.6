#ifndef _VX_VS_CONTEXT_H
#define _VX_VS_CONTEXT_H


// #define VX_DEBUG

#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>

#include "vserver/context.h"

#undef	vxdprintk
#if defined(VX_DEBUG)
#define vxdprintk(x...) printk("vxd: " x)
#else
#define vxdprintk(x...)
#endif



extern int proc_pid_vx_info(struct task_struct *, char *);


#define get_vx_info(i)	__get_vx_info(i,__FILE__,__LINE__)

static inline struct vx_info *__get_vx_info(struct vx_info *vxi,
	const char *_file, int _line)
{
	if (!vxi)
		return NULL;
	vxdprintk("get_vx_info(%p[#%d.%d])\t%s:%d\n",
		vxi, vxi?vxi->vx_id:0, vxi?atomic_read(&vxi->vx_usecnt):0,
		_file, _line);
	atomic_inc(&vxi->vx_usecnt);
	return vxi;
}


#define	free_vx_info(i)	\
	call_rcu(&i->vx_rcu, rcu_free_vx_info);

#define put_vx_info(i)	__put_vx_info(i,__FILE__,__LINE__)

static inline void __put_vx_info(struct vx_info *vxi, const char *_file, int _line)
{
	if (!vxi)
		return;
	vxdprintk("put_vx_info(%p[#%d.%d])\t%s:%d\n",
		vxi, vxi?vxi->vx_id:0, vxi?atomic_read(&vxi->vx_usecnt):0,
		_file, _line);
	if (atomic_dec_and_test(&vxi->vx_usecnt))
		free_vx_info(vxi);
}

#define set_vx_info(p,i) __set_vx_info(p,i,__FILE__,__LINE__)

static inline void __set_vx_info(struct vx_info **vxp, struct vx_info *vxi,
	const char *_file, int _line)
{
	BUG_ON(*vxp);
	if (!vxi)
		return;
	vxdprintk("set_vx_info(%p[#%d.%d.%d])\t%s:%d\n",
		vxi, vxi?vxi->vx_id:0,
		vxi?atomic_read(&vxi->vx_usecnt):0,
		vxi?atomic_read(&vxi->vx_refcnt):0,
		_file, _line);
	atomic_inc(&vxi->vx_refcnt);
	*vxp = __get_vx_info(vxi, _file, _line);
}

#define	clr_vx_info(p)	__clr_vx_info(p,__FILE__,__LINE__)

static inline void __clr_vx_info(struct vx_info **vxp,
	const char *_file, int _line)
{
	struct vx_info *vxo = *vxp;

	if (!vxo)
		return;
	vxdprintk("clr_vx_info(%p[#%d.%d.%d])\t%s:%d\n",
		vxo, vxo?vxo->vx_id:0,
		vxo?atomic_read(&vxo->vx_usecnt):0,
		vxo?atomic_read(&vxo->vx_refcnt):0,
		_file, _line);
	*vxp = NULL;
	wmb();
	if (vxo && atomic_dec_and_test(&vxo->vx_refcnt))
		unhash_vx_info(vxo);
	__put_vx_info(vxo, _file, _line);
}


#define task_get_vx_info(i)	__task_get_vx_info(i,__FILE__,__LINE__)

static __inline__ struct vx_info *__task_get_vx_info(struct task_struct *p,
	const char *_file, int _line)
{
	struct vx_info *vxi;
	
	task_lock(p);
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


#undef	vxdprintk
#define vxdprintk(x...)

#else
#warning duplicate inclusion
#endif
