#ifndef _VX_VS_CVIRT_H
#define _VX_VS_CVIRT_H


// #define VX_DEBUG

#include "vserver/cvirt.h"
#include "vs_base.h"

#if defined(VX_DEBUG)
#define vxdprintk(x...) printk("vxd: " x)
#else
#define vxdprintk(x...)
#endif


/* utsname virtualization */

static inline struct new_utsname *vx_new_utsname(void)
{
	if (current->vx_info)
		return &current->vx_info->cvirt.utsname;
	return &system_utsname;
}

#define vx_new_uts(x)		((vx_new_utsname())->x)


/* pid faking stuff */


#define vx_map_tgid(v,p) \
	__vx_map_tgid((v), (p), __FILE__, __LINE__)

static inline int __vx_map_tgid(struct vx_info *vxi, int pid,
	char *file, int line)
{
	if (vxi && __vx_flags(vxi->vx_flags, VXF_INFO_INIT, 0)) {
		vxdprintk("vx_map_tgid: %p/%llx: %d -> %d in %s:%d\n",
			vxi, vxi->vx_flags, pid,
			(pid == vxi->vx_initpid)?1:pid,
			file, line);
		if (pid == vxi->vx_initpid)
			return 1;
	}
	return pid;
}

#define vx_rmap_tgid(v,p) \
	__vx_rmap_tgid((v), (p), __FILE__, __LINE__)

static inline int __vx_rmap_tgid(struct vx_info *vxi, int pid,
	char *file, int line)
{
	if (vxi && __vx_flags(vxi->vx_flags, VXF_INFO_INIT, 0)) {
		vxdprintk("vx_rmap_tgid: %p/%llx: %d -> %d in %s:%d\n",
			vxi, vxi->vx_flags, pid,
			(pid == 1)?vxi->vx_initpid:pid,
			file, line);
		if ((pid == 1) && vxi->vx_initpid)
			return vxi->vx_initpid;
	}
	return pid;
}

#undef	vxdprintk
#define vxdprintk(x...)

#else
#warning duplicate inclusion
#endif
