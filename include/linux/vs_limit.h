#ifndef _VX_VS_LIMIT_H
#define _VX_VS_LIMIT_H


#include "vserver/limit.h"
#include "vserver/debug.h"


/* file limits */


static inline void __vx_acc_cres(struct vx_info *vxi,
	int res, int dir, void *_data, char *_file, int _line)
{
	if (VXD_RLIMIT(res, RLIMIT_NOFILE) ||
		VXD_RLIMIT(res, RLIMIT_NPROC) ||
		VXD_RLIMIT(res, VLIMIT_NSOCK))
		vxlprintk(1, "vx_acc_cres[%5d,%s,%2d]: %5d%s (%p)",
			(vxi?vxi->vx_id:-1), vlimit_name[res], res,
			(vxi?atomic_read(&vxi->limit.rcur[res]):0),
			(dir>0)?"++":"--", _data, _file, _line);
	if (vxi) {
		if (dir > 0)
			atomic_inc(&vxi->limit.rcur[res]);
		else
			atomic_dec(&vxi->limit.rcur[res]);
	}
}

#define vx_acc_cres(v,d,p,r) \
	__vx_acc_cres((v), (r), (d), (p), __FILE__, __LINE__)

#define vx_acc_cres_cond(x,d,p,r) \
	__vx_acc_cres(((x) == vx_current_xid()) ? current->vx_info : 0,\
	(r), (d), (p), __FILE__, __LINE__)

#define vx_nproc_inc(p) \
	vx_acc_cres((p)->vx_info, 1, (p), RLIMIT_NPROC)

#define vx_nproc_dec(p) \
	vx_acc_cres((p)->vx_info,-1, (p), RLIMIT_NPROC)

#define vx_files_inc(f) \
	vx_acc_cres_cond((f)->f_xid, 1, (f), RLIMIT_NOFILE)

#define vx_files_dec(f) \
	vx_acc_cres_cond((f)->f_xid,-1, (f), RLIMIT_NOFILE)


#define vx_cres_avail(v,n,r) \
	__vx_cres_avail((v), (r), (n), __FILE__, __LINE__)

static inline int __vx_cres_avail(struct vx_info *vxi,
		int res, int num, char *_file, int _line)
{
	unsigned long value;

	if (VXD_RLIMIT(res, RLIMIT_NOFILE) ||
		VXD_RLIMIT(res, RLIMIT_NPROC) ||
		VXD_RLIMIT(res, VLIMIT_NSOCK))
		vxlprintk(1, "vx_cres_avail[%5d,%s,%2d]: %5ld > %5d + %5d",
			(vxi?vxi->vx_id:-1), vlimit_name[res], res,
			(vxi?vxi->limit.rlim[res]:1),
			(vxi?atomic_read(&vxi->limit.rcur[res]):0),
			num, _file, _line);
	if (!vxi)
		return 1;
	value = atomic_read(&vxi->limit.rcur[res]);
	if (value > vxi->limit.rmax[res])
		vxi->limit.rmax[res] = value;
	if (vxi->limit.rlim[res] == RLIM_INFINITY)
		return 1;
	if (value + num <= vxi->limit.rlim[res])
		return 1;
	atomic_inc(&vxi->limit.lhit[res]);
	return 0;
}

#define vx_nproc_avail(n) \
	vx_cres_avail(current->vx_info, (n), RLIMIT_NPROC)

#define vx_files_avail(n) \
	vx_cres_avail(current->vx_info, (n), RLIMIT_NOFILE)


/* socket limits */

#define vx_sock_inc(s) \
	vx_acc_cres((s)->sk_vx_info, 1, (s), VLIMIT_NSOCK)
#define vx_sock_dec(s) \
	vx_acc_cres((s)->sk_vx_info,-1, (s), VLIMIT_NSOCK)

#define vx_sock_avail(n) \
	vx_cres_avail(current->vx_info, (n), VLIMIT_NSOCK)

#else
#warning duplicate inclusion
#endif
