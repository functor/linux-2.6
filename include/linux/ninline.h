#ifndef _NX_INLINE_H
#define _NX_INLINE_H


// #define NX_DEBUG

#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>

#include "vserver/network.h"

#if defined(NX_DEBUG)
#define nxdprintk(x...) printk("nxd: " x)
#else
#define nxdprintk(x...)
#endif


extern int proc_pid_nx_info(struct task_struct *, char *);


#define get_nx_info(i)	__get_nx_info(i,__FILE__,__LINE__)

static inline struct nx_info *__get_nx_info(struct nx_info *nxi,
	const char *_file, int _line)
{
	if (!nxi)
		return NULL;
	nxdprintk("get_nx_info(%p[#%d.%d])\t%s:%d\n",
		nxi, nxi?nxi->nx_id:0, nxi?atomic_read(&nxi->nx_usecnt):0,
		_file, _line);
	atomic_inc(&nxi->nx_usecnt);
	return nxi;
}


#define	free_nx_info(nxi)	\
	call_rcu(&nxi->nx_rcu, rcu_free_nx_info, nxi);

#define put_nx_info(i)	__put_nx_info(i,__FILE__,__LINE__)

static inline void __put_nx_info(struct nx_info *nxi, const char *_file, int _line)
{
	if (!nxi)
		return;
	nxdprintk("put_nx_info(%p[#%d.%d])\t%s:%d\n",
		nxi, nxi?nxi->nx_id:0, nxi?atomic_read(&nxi->nx_usecnt):0,
		_file, _line);
	if (atomic_dec_and_test(&nxi->nx_usecnt))
		free_nx_info(nxi);
}


#define set_nx_info(p,i) __set_nx_info(p,i,__FILE__,__LINE__)

static inline void __set_nx_info(struct nx_info **nxp, struct nx_info *nxi,
	const char *_file, int _line)
{
	BUG_ON(*nxp);
	if (!nxi)
		return;
	nxdprintk("set_nx_info(%p[#%d.%d.%d])\t%s:%d\n",
		nxi, nxi?nxi->nx_id:0,
		nxi?atomic_read(&nxi->nx_usecnt):0,
		nxi?atomic_read(&nxi->nx_refcnt):0,
		_file, _line);
	atomic_inc(&nxi->nx_refcnt);
	*nxp = __get_nx_info(nxi, _file, _line);
}

#define	clr_nx_info(p)	__clr_nx_info(p,__FILE__,__LINE__)

static inline void __clr_nx_info(struct nx_info **nxp,
	const char *_file, int _line)
{
	struct nx_info *nxo = *nxp;

	if (!nxo)
		return;
	nxdprintk("clr_nx_info(%p[#%d.%d.%d])\t%s:%d\n",
		nxo, nxo?nxo->nx_id:0,
		nxo?atomic_read(&nxo->nx_usecnt):0,
		nxo?atomic_read(&nxo->nx_refcnt):0,
		_file, _line);
	*nxp = NULL;
	wmb();
	if (nxo && atomic_dec_and_test(&nxo->nx_refcnt))
		unhash_nx_info(nxo);
	__put_nx_info(nxo, _file, _line);
}


#define task_get_nx_info(i)	__task_get_nx_info(i,__FILE__,__LINE__)

static __inline__ struct nx_info *__task_get_nx_info(struct task_struct *p,
	const char *_file, int _line)
{
	struct nx_info *nxi;
	
	task_lock(p);
	nxi = __get_nx_info(p->nx_info, _file, _line);
	task_unlock(p);
	return nxi;
}

#define nx_verify_info(p,i)	\
	__nx_verify_info((p)->nx_info,i,__FILE__,__LINE__)

static __inline__ void __nx_verify_info(
	struct nx_info *ipa, struct nx_info *ipb,
	const char *_file, int _line)
{
	if (ipa == ipb)
		return;
	printk(KERN_ERR "ip bad assumption (%p==%p) at %s:%d\n",
		ipa, ipb, _file, _line);
}


#define nx_task_nid(t)	((t)->nid)

#define nx_current_nid() nx_task_nid(current)

#define nx_check(c,m)	__nx_check(nx_current_nid(),c,m)

#define nx_weak_check(c,m)	((m) ? nx_check(c,m) : 1)

#undef nxdprintk
#define nxdprintk(x...)


#define __nx_flags(v,m,f)	(((v) & (m)) ^ (f))

#define	__nx_task_flags(t,m,f) \
	(((t) && ((t)->nx_info)) ? \
		__nx_flags((t)->nx_info->nx_flags,(m),(f)) : 0)

#define nx_current_flags() \
	((current->nx_info) ? current->nx_info->nx_flags : 0)

#define nx_flags(m,f)	__nx_flags(nx_current_flags(),(m),(f))


#define nx_current_ncaps() \
	((current->nx_info) ? current->nx_info->nx_ncaps : 0)

#define nx_ncaps(c)	(nx_current_ncaps() & (c))



#define	sock_nx_init(s)	 do {		\
	(s)->sk_nid = 0;		\
	(s)->sk_nx_info = NULL;		\
	} while (0)



#endif
