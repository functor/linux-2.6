#ifndef _VX_INLINE_H
#define _VX_INLINE_H


// #define VX_DEBUG

#include <linux/kernel.h>
#include <linux/sched.h>

#include "vserver/context.h"
#include "vserver/limit.h"
#include "vserver/cvirt.h"

#if defined(VX_DEBUG)
#define vxdprintk(x...) printk("vxd: " x)
#else
#define vxdprintk(x...)
#endif



void free_vx_info(struct vx_info *);

extern int proc_pid_vx_info(struct task_struct *, char *);


#define get_vx_info(i)	__get_vx_info(i,__FILE__,__LINE__)

static __inline__ struct vx_info *__get_vx_info(struct vx_info *vxi,
	const char *_file, int _line)
{
	if (!vxi)
		return NULL;
	vxdprintk("get_vx_info(%p[#%d.%d])\t%s:%d\n",
		vxi, vxi?vxi->vx_id:0, vxi?atomic_read(&vxi->vx_refcount):0,
		_file, _line);
	atomic_inc(&vxi->vx_refcount);
	return vxi;
}

#define put_vx_info(i)	__put_vx_info(i,__FILE__,__LINE__)

static __inline__ void __put_vx_info(struct vx_info *vxi, const char *_file, int _line)
{
	if (!vxi)
		return;
	vxdprintk("put_vx_info(%p[#%d.%d])\t%s:%d\n",
		vxi, vxi?vxi->vx_id:0, vxi?atomic_read(&vxi->vx_refcount):0,
		_file, _line);
	if (atomic_dec_and_lock(&vxi->vx_refcount, &vxlist_lock)) {
		list_del(&vxi->vx_list);
		spin_unlock(&vxlist_lock);
		free_vx_info(vxi);
	}
}

#define set_vx_info(p,i) __set_vx_info(p,i,__FILE__,__LINE__)

static inline void __set_vx_info(struct vx_info **vxp, struct vx_info *vxi,
	const char *_file, int _line)
{
	BUG_ON(*vxp);
	if (!vxi)
		return;
	vxdprintk("set_vx_info(%p[#%d.%d])\t%s:%d\n",
		vxi, vxi?vxi->vx_id:0, vxi?atomic_read(&vxi->vx_refcount):0,
		_file, _line);
	*vxp = __get_vx_info(vxi, _file, _line);
}

#define	clr_vx_info(p)	__clr_vx_info(p,__FILE__,__LINE__)

static inline void __clr_vx_info(struct vx_info **vxp,
	const char *_file, int _line)
{
	struct vx_info *vxo = *vxp;

	vxdprintk("clr_vx_info(%p[#%d.%d])\t%s:%d\n",
		vxo, vxo?vxo->vx_id:0, vxo?atomic_read(&vxo->vx_refcount):0,
		_file, _line);
	*vxp = NULL;
	wmb();
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


#define VX_DEBUG_ACC_RSS   0
#define VX_DEBUG_ACC_VM    0
#define VX_DEBUG_ACC_VML   0

#undef	vxdprintk
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
                        atomic_inc(&vxi->limit.res[res]);
                else
                        atomic_dec(&vxi->limit.res[res]);
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
                atomic_add(pages, &vxi->limit.res[res]);
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
        if ((res == RLIMIT_RSS && VX_DEBUG_ACC_RSS) ||
                (res == RLIMIT_AS && VX_DEBUG_ACC_VM) ||
                (res == RLIMIT_MEMLOCK && VX_DEBUG_ACC_VML))
                printk("vx_pages_avail[%5d,%2d]: %5ld > %5d + %5d in %s:%d\n",
                        (vxi?vxi->vx_id:-1), res,
			(vxi?vxi->limit.rlim[res]:1),
                        (vxi?atomic_read(&vxi->limit.res[res]):0),
			pages, file, line);
        if (!vxi)
                return 1;
        if (vxi->limit.rlim[res] == RLIM_INFINITY)
                return 1;
        if (atomic_read(&vxi->limit.res[res]) + pages < vxi->limit.rlim[res])
                return 1;
        return 0;
}

#define vx_vmpages_avail(m,p)  vx_pages_avail(m, p, RLIMIT_AS)
#define vx_vmlocked_avail(m,p) vx_pages_avail(m, p, RLIMIT_MEMLOCK)
#define vx_rsspages_avail(m,p) vx_pages_avail(m, p, RLIMIT_RSS)

/* file limits */

#define VX_DEBUG_ACC_FILE	0
#define VX_DEBUG_ACC_OPENFD	0

#undef	vxdprintk
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
                        (vxi?atomic_read(&vxi->limit.res[res]):0),
			(dir>0)?"++":"--", file, line);
                if (dir > 0)
                        atomic_inc(&vxi->limit.res[res]);
                else
                        atomic_dec(&vxi->limit.res[res]);
        }
}

#define vx_files_inc(f)	vx_acc_cres(current->vx_info, 1, RLIMIT_NOFILE)
#define vx_files_dec(f)	vx_acc_cres(current->vx_info,-1, RLIMIT_NOFILE)

#define vx_openfd_inc(f) vx_acc_cres(current->vx_info, 1, RLIMIT_OPENFD)
#define vx_openfd_dec(f) vx_acc_cres(current->vx_info,-1, RLIMIT_OPENFD)

#define vx_cres_avail(v,n,r) \
        __vx_cres_avail((v), (r), (n), __FILE__, __LINE__)

static inline int __vx_cres_avail(struct vx_info *vxi,
                int res, int num, char *file, int line)
{
	if ((res == RLIMIT_NOFILE && VX_DEBUG_ACC_FILE) ||
		(res == RLIMIT_OPENFD && VX_DEBUG_ACC_OPENFD))
                printk("vx_cres_avail[%5d,%2d]: %5ld > %5d + %5d in %s:%d\n",
                        (vxi?vxi->vx_id:-1), res,
			(vxi?vxi->limit.rlim[res]:1),
                        (vxi?atomic_read(&vxi->limit.res[res]):0),
			num, file, line);
        if (!vxi)
                return 1;
        if (vxi->limit.rlim[res] == RLIM_INFINITY)
                return 1;
        if (vxi->limit.rlim[res] < atomic_read(&vxi->limit.res[res]) + num)
                return 0;
        return 1;
}

#define vx_files_avail(n) \
	vx_cres_avail(current->vx_info, (n), RLIMIT_NOFILE)

#define vx_openfd_avail(n) \
	vx_cres_avail(current->vx_info, (n), RLIMIT_OPENFD)

/* socket limits */

#define vx_sock_inc(f)	vx_acc_cres(current->vx_info, 1, VLIMIT_SOCK)
#define vx_sock_dec(f)	vx_acc_cres(current->vx_info,-1, VLIMIT_SOCK)

#define vx_sock_avail(n) \
	vx_cres_avail(current->vx_info, (n), VLIMIT_SOCK)

/* procfs ioctls */

#define	FIOC_GETXFLG	_IOR('x', 5, long)
#define	FIOC_SETXFLG	_IOW('x', 6, long)

/* utsname virtualization */

static inline struct new_utsname *vx_new_utsname(void)
{
	if (current->vx_info)
		return &current->vx_info->cvirt.utsname;
	return &system_utsname;
}

#define vx_new_uts(x)		((vx_new_utsname())->x)

/* generic flag merging */

#define	vx_mask_flags(v,f,m)	(((v) & ~(m)) | ((f) & (m)))

#define	vx_mask_mask(v,f,m)	(((v) & ~(m)) | ((v) & (f) & (m)))


/* socket accounting */

#include <linux/socket.h>

static inline int vx_sock_type(int family)
{
	int type = 4;

	if (family > 0 && family < 3)
		type = family;
	else if (family == PF_INET6)
		type = 3;
	return type;
}

#define vx_acc_sock(v,f,p,s) \
	__vx_acc_sock((v), (f), (p), (s), __FILE__, __LINE__)

static inline void __vx_acc_sock(struct vx_info *vxi,
	int family, int pos, int size, char *file, int line)
{
        if (vxi) {
		int type = vx_sock_type(family);

		atomic_inc(&vxi->cacct.sock[type][pos].count);
		atomic_add(size, &vxi->cacct.sock[type][pos].total);
        }
}

#define vx_sock_recv(sk,s) \
	vx_acc_sock((sk)->sk_vx_info, (sk)->sk_family, 0, (s))
#define vx_sock_send(sk,s) \
	vx_acc_sock((sk)->sk_vx_info, (sk)->sk_family, 1, (s))
#define vx_sock_fail(sk,s) \
	vx_acc_sock((sk)->sk_vx_info, (sk)->sk_family, 2, (s))


#define	sock_vx_init(s)	 do {		\
	(s)->sk_xid = 0;		\
	(s)->sk_vx_info = NULL;		\
	} while (0)


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

#endif
