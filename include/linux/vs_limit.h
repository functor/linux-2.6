#ifndef _VX_VS_LIMIT_H
#define _VX_VS_LIMIT_H


// #define VX_DEBUG

#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>

#include "vserver/context.h"
#include "vserver/limit.h"


/* file limits */

#define VX_DEBUG_ACC_FILE	0
#define VX_DEBUG_ACC_OPENFD	0

#if	(VX_DEBUG_ACC_FILE) || (VX_DEBUG_ACC_OPENFD)
#define vxdprintk(x...) printk("vxd: " x)
#else
#define vxdprintk(x...)
#endif


#define vx_acc_cres(v,d,r) \
	__vx_acc_cres((v), (r), (d), __FILE__, __LINE__)

static inline void __vx_acc_cres(struct vx_info *vxi,
	int res, int dir, char *file, int line)
{
        if (vxi) {
	if ((res == RLIMIT_NOFILE && VX_DEBUG_ACC_FILE) ||
			(res == RLIMIT_OPENFD && VX_DEBUG_ACC_OPENFD))
	printk("vx_acc_cres[%5d,%2d]: %5d%s in %s:%d\n",
                        (vxi?vxi->vx_id:-1), res,
                        (vxi?atomic_read(&vxi->limit.rcur[res]):0),
			(dir>0)?"++":"--", file, line);
                if (dir > 0)
                        atomic_inc(&vxi->limit.rcur[res]);
                else
                        atomic_dec(&vxi->limit.rcur[res]);
        }
}

#define vx_nproc_inc(p)	vx_acc_cres(current->vx_info, 1, RLIMIT_NPROC)
#define vx_nproc_dec(p)	vx_acc_cres(current->vx_info,-1, RLIMIT_NPROC)

#define vx_files_inc(f)	vx_acc_cres(current->vx_info, 1, RLIMIT_NOFILE)
#define vx_files_dec(f)	vx_acc_cres(current->vx_info,-1, RLIMIT_NOFILE)

#define vx_openfd_inc(f) vx_acc_cres(current->vx_info, 1, RLIMIT_OPENFD)
#define vx_openfd_dec(f) vx_acc_cres(current->vx_info,-1, RLIMIT_OPENFD)

/*
#define vx_openfd_inc(f) do {					\
	vx_acc_cres(current->vx_info, 1, RLIMIT_OPENFD);	\
	printk("vx_openfd_inc: %d[#%d] in %s:%d\n",		\
		f, current->xid, __FILE__, __LINE__);		\
	} while (0)

#define vx_openfd_dec(f) do {					\
	vx_acc_cres(current->vx_info,-1, RLIMIT_OPENFD);	\
	printk("vx_openfd_dec: %d[#%d] in %s:%d\n",		\
		f, current->xid, __FILE__, __LINE__);		\
	} while (0)
*/

#define vx_cres_avail(v,n,r) \
        __vx_cres_avail((v), (r), (n), __FILE__, __LINE__)

static inline int __vx_cres_avail(struct vx_info *vxi,
                int res, int num, char *file, int line)
{
	unsigned long value;

	if ((res == RLIMIT_NOFILE && VX_DEBUG_ACC_FILE) ||
		(res == RLIMIT_OPENFD && VX_DEBUG_ACC_OPENFD))
                printk("vx_cres_avail[%5d,%2d]: %5ld > %5d + %5d in %s:%d\n",
                        (vxi?vxi->vx_id:-1), res,
			(vxi?vxi->limit.rlim[res]:1),
                        (vxi?atomic_read(&vxi->limit.rcur[res]):0),
			num, file, line);
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

#define vx_openfd_avail(n) \
	vx_cres_avail(current->vx_info, (n), RLIMIT_OPENFD)


/* socket limits */

#define vx_sock_inc(f)	vx_acc_cres(current->vx_info, 1, VLIMIT_SOCK)
#define vx_sock_dec(f)	vx_acc_cres(current->vx_info,-1, VLIMIT_SOCK)

#define vx_sock_avail(n) \
	vx_cres_avail(current->vx_info, (n), VLIMIT_SOCK)


#else
#warning duplicate inclusion
#endif
