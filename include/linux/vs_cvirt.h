#ifndef _VX_VS_CVIRT_H
#define _VX_VS_CVIRT_H

#include "vserver/cvirt.h"
#include "vserver/debug.h"
#include "vs_base.h"


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
		vxlprintk(VXD_CBIT(cvirt, 2),
			"vx_map_tgid: %p/%llx: %d -> %d",
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
		vxlprintk(VXD_CBIT(cvirt, 2),
			"vx_rmap_tgid: %p/%llx: %d -> %d",
			vxi, vxi->vx_flags, pid,
			(pid == 1)?vxi->vx_initpid:pid,
			file, line);
		if ((pid == 1) && vxi->vx_initpid)
			return vxi->vx_initpid;
	}
	return pid;
}


#else
#warning duplicate inclusion
#endif
