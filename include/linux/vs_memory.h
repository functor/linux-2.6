#ifndef _VX_VS_MEMORY_H
#define _VX_VS_MEMORY_H


// #define VX_DEBUG

#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>

#include "vserver/context.h"
#include "vserver/limit.h"


#define VX_DEBUG_ACC_RSS   0
#define VX_DEBUG_ACC_VM    0
#define VX_DEBUG_ACC_VML   0

#if	(VX_DEBUG_ACC_RSS) || (VX_DEBUG_ACC_VM) || (VX_DEBUG_ACC_VML)
#define vxdprintk(x...) printk("vxd: " x)
#else
#define vxdprintk(x...)
#endif

#define vx_acc_page(m, d, v, r) \
	__vx_acc_page(&(m->v), m->mm_vx_info, r, d, __FILE__, __LINE__)

static inline void __vx_acc_page(unsigned long *v, struct vx_info *vxi,
                int res, int dir, char *file, int line)
{
        if (v) {
                if (dir > 0)
                        ++(*v);
                else
                        --(*v);
        }
        if (vxi) {
                if (dir > 0)
                        atomic_inc(&vxi->limit.rcur[res]);
                else
                        atomic_dec(&vxi->limit.rcur[res]);
        }
}


#define vx_acc_pages(m, p, v, r) \
	__vx_acc_pages(&(m->v), m->mm_vx_info, r, p, __FILE__, __LINE__)

static inline void __vx_acc_pages(unsigned long *v, struct vx_info *vxi,
                int res, int pages, char *file, int line)
{
        if ((res == RLIMIT_RSS && VX_DEBUG_ACC_RSS) ||
		(res == RLIMIT_AS && VX_DEBUG_ACC_VM) ||
		(res == RLIMIT_MEMLOCK && VX_DEBUG_ACC_VML))
		vxdprintk("vx_acc_pages  [%5d,%2d]: %5d += %5d in %s:%d\n",
			(vxi?vxi->vx_id:-1), res,
			(vxi?atomic_read(&vxi->limit.res[res]):0),
			pages, file, line);
        if (pages == 0)
                return;
        if (v)
                *v += pages;
        if (vxi)
                atomic_add(pages, &vxi->limit.rcur[res]);
}



#define vx_acc_vmpage(m,d)     vx_acc_page(m, d, total_vm,  RLIMIT_AS)
#define vx_acc_vmlpage(m,d)    vx_acc_page(m, d, locked_vm, RLIMIT_MEMLOCK)
#define vx_acc_rsspage(m,d)    vx_acc_page(m, d, rss,	    RLIMIT_RSS)

#define vx_acc_vmpages(m,p)    vx_acc_pages(m, p, total_vm,  RLIMIT_AS)
#define vx_acc_vmlpages(m,p)   vx_acc_pages(m, p, locked_vm, RLIMIT_MEMLOCK)
#define vx_acc_rsspages(m,p)   vx_acc_pages(m, p, rss,       RLIMIT_RSS)

#define vx_pages_add(s,r,p)    __vx_acc_pages(0, s, r, p, __FILE__, __LINE__)
#define vx_pages_sub(s,r,p)    __vx_pages_add(s, r, -(p))

#define vx_vmpages_inc(m)      vx_acc_vmpage(m, 1)
#define vx_vmpages_dec(m)      vx_acc_vmpage(m,-1)
#define vx_vmpages_add(m,p)    vx_acc_vmpages(m, p)
#define vx_vmpages_sub(m,p)    vx_acc_vmpages(m,-(p))

#define vx_vmlocked_inc(m)     vx_acc_vmlpage(m, 1)
#define vx_vmlocked_dec(m)     vx_acc_vmlpage(m,-1)
#define vx_vmlocked_add(m,p)   vx_acc_vmlpages(m, p)
#define vx_vmlocked_sub(m,p)   vx_acc_vmlpages(m,-(p))

#define vx_rsspages_inc(m)     vx_acc_rsspage(m, 1)
#define vx_rsspages_dec(m)     vx_acc_rsspage(m,-1)
#define vx_rsspages_add(m,p)   vx_acc_rsspages(m, p)
#define vx_rsspages_sub(m,p)   vx_acc_rsspages(m,-(p))



#define vx_pages_avail(m, p, r) \
        __vx_pages_avail((m)->mm_vx_info, (r), (p), __FILE__, __LINE__)

static inline int __vx_pages_avail(struct vx_info *vxi,
                int res, int pages, char *file, int line)
{
	unsigned long value;

        if ((res == RLIMIT_RSS && VX_DEBUG_ACC_RSS) ||
                (res == RLIMIT_AS && VX_DEBUG_ACC_VM) ||
                (res == RLIMIT_MEMLOCK && VX_DEBUG_ACC_VML))
                printk("vx_pages_avail[%5d,%2d]: %5ld > %5d + %5d in %s:%d\n",
                        (vxi?vxi->vx_id:-1), res,
			(vxi?vxi->limit.rlim[res]:1),
                        (vxi?atomic_read(&vxi->limit.rcur[res]):0),
			pages, file, line);
        if (!vxi)
                return 1;
	value = atomic_read(&vxi->limit.rcur[res]);	
	if (value > vxi->limit.rmax[res])
		vxi->limit.rmax[res] = value;
        if (vxi->limit.rlim[res] == RLIM_INFINITY)
                return 1;
        if (value + pages <= vxi->limit.rlim[res])
                return 1;
	atomic_inc(&vxi->limit.lhit[res]);
        return 0;
}

#define vx_vmpages_avail(m,p)  vx_pages_avail(m, p, RLIMIT_AS)
#define vx_vmlocked_avail(m,p) vx_pages_avail(m, p, RLIMIT_MEMLOCK)
#define vx_rsspages_avail(m,p) vx_pages_avail(m, p, RLIMIT_RSS)

#else
#warning duplicate inclusion
#endif
