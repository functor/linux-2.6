/*
 *  linux/kernel/vserver/context.c
 *
 *  Virtual Server: Context Support
 *
 *  Copyright (C) 2003-2004  Herbert Pötzl
 *
 *  V0.01  context helper
 *  V0.02  vx_ctx_kill syscall command
 *  V0.03  replaced context_info calls
 *  V0.04  redesign of struct (de)alloc
 *  V0.05  rlimit basic implementation
 *  V0.06  task_xid and info commands
 *  V0.07  context flags and caps
 *  V0.08  switch to RCU based hash
 *
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/vserver.h>
#include <linux/vserver/legacy.h>
#include <linux/vs_base.h>
#include <linux/vs_context.h>
#include <linux/kernel_stat.h>
#include <linux/namespace.h>
#include <linux/rcupdate.h>

#include <asm/errno.h>


/*	__alloc_vx_info()

	* allocate an initialized vx_info struct
	* doesn't make it visible (hash)			*/

static struct vx_info *__alloc_vx_info(xid_t xid)
{
	struct vx_info *new = NULL;
	
	vxdprintk(VXD_CBIT(xid, 0), "alloc_vx_info(%d)*", xid);

	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct vx_info), GFP_KERNEL);
	if (!new)
		return 0;

	memset (new, 0, sizeof(struct vx_info));
	new->vx_id = xid;
	INIT_RCU_HEAD(&new->vx_rcu);
	INIT_HLIST_NODE(&new->vx_hlist);
	atomic_set(&new->vx_refcnt, 0);
	atomic_set(&new->vx_usecnt, 0);

	/* rest of init goes here */
	vx_info_init_limit(&new->limit);
	vx_info_init_sched(&new->sched);
	vx_info_init_cvirt(&new->cvirt);
	vx_info_init_cacct(&new->cacct);

	new->vx_flags = VXF_STATE_SETUP|VXF_STATE_INIT;
	new->vx_bcaps = CAP_INIT_EFF_SET;
	new->vx_ccaps = 0;

	vxdprintk(VXD_CBIT(xid, 0),
		"alloc_vx_info(%d) = %p", xid, new);
	return new;
}

/*	__dealloc_vx_info()

	* final disposal of vx_info				*/

static void __dealloc_vx_info(struct vx_info *vxi)
{
	vxdprintk(VXD_CBIT(xid, 0),
		"dealloc_vx_info(%p)", vxi);

	vxi->vx_hlist.next = LIST_POISON1;
	vxi->vx_id = -1;

	if (vxi->vx_namespace)
		put_namespace(vxi->vx_namespace);
	if (vxi->vx_fs)
		put_fs_struct(vxi->vx_fs);
	
	vx_info_exit_limit(&vxi->limit);
	vx_info_exit_sched(&vxi->sched);
	vx_info_exit_cvirt(&vxi->cvirt);
	vx_info_exit_cacct(&vxi->cacct);
	
	BUG_ON(atomic_read(&vxi->vx_usecnt));
	BUG_ON(atomic_read(&vxi->vx_refcnt));

	kfree(vxi);
}


/*	hash table for vx_info hash */

#define	VX_HASH_SIZE	13

struct hlist_head vx_info_hash[VX_HASH_SIZE];

static spinlock_t vx_info_hash_lock = SPIN_LOCK_UNLOCKED;


static inline unsigned int __hashval(xid_t xid)
{
	return (xid % VX_HASH_SIZE);
}



/*	__hash_vx_info()

	* add the vxi to the global hash table
	* requires the hash_lock to be held			*/

static inline void __hash_vx_info(struct vx_info *vxi)
{
	struct hlist_head *head;
	
	vxdprintk(VXD_CBIT(xid, 4),
		"__hash_vx_info: %p[#%d]", vxi, vxi->vx_id);
	get_vx_info(vxi);
	head = &vx_info_hash[__hashval(vxi->vx_id)];
	hlist_add_head_rcu(&vxi->vx_hlist, head);
}

/*	__unhash_vx_info()

	* remove the vxi from the global hash table
	* requires the hash_lock to be held			*/

static inline void __unhash_vx_info(struct vx_info *vxi)
{
	vxdprintk(VXD_CBIT(xid, 4),
		"__unhash_vx_info: %p[#%d]", vxi, vxi->vx_id);
	hlist_del_rcu(&vxi->vx_hlist);
	put_vx_info(vxi);
}


/*	__lookup_vx_info()

	* requires the rcu_read_lock()
	* doesn't increment the vx_refcnt			*/

static inline struct vx_info *__lookup_vx_info(xid_t xid)
{
	struct hlist_head *head = &vx_info_hash[__hashval(xid)];
	struct hlist_node *pos;

	hlist_for_each_rcu(pos, head) {
		struct vx_info *vxi =
			hlist_entry(pos, struct vx_info, vx_hlist);

		if (vxi->vx_id == xid) {
			return vxi;
		}
	}
	return NULL;
}


/*	__vx_dynamic_id()

	* find unused dynamic xid
	* requires the hash_lock to be held			*/

static inline xid_t __vx_dynamic_id(void)
{
	static xid_t seq = MAX_S_CONTEXT;
	xid_t barrier = seq;
	
	do {
		if (++seq > MAX_S_CONTEXT)
			seq = MIN_D_CONTEXT;
		if (!__lookup_vx_info(seq)) {
			vxdprintk(VXD_CBIT(xid, 4),
				"__vx_dynamic_id: [#%d]", seq);
			return seq;
		}
	} while (barrier != seq);
	return 0;
}

/*	__loc_vx_info()

	* locate or create the requested context
	* get() it and if new hash it				*/

static struct vx_info * __loc_vx_info(int id, int *err)
{
	struct vx_info *new, *vxi = NULL;
	
	vxdprintk(VXD_CBIT(xid, 1), "loc_vx_info(%d)*", id);

	if (!(new = __alloc_vx_info(id))) {
		*err = -ENOMEM;
		return NULL;
	}

	spin_lock(&vx_info_hash_lock);

	/* dynamic context requested */
	if (id == VX_DYNAMIC_ID) {
		id = __vx_dynamic_id();
		if (!id) {
			printk(KERN_ERR "no dynamic context available.\n");
			goto out_unlock;
		}
		new->vx_id = id;
	}
	/* existing context requested */
	else if ((vxi = __lookup_vx_info(id))) {
		/* context in setup is not available */
		if (vxi->vx_flags & VXF_STATE_SETUP) {
			vxdprintk(VXD_CBIT(xid, 0),
				"loc_vx_info(%d) = %p (not available)", id, vxi);
			vxi = NULL;
			*err = -EBUSY;
		} else {
			vxdprintk(VXD_CBIT(xid, 0),
				"loc_vx_info(%d) = %p (found)", id, vxi);
			get_vx_info(vxi);
			*err = 0;
		}
		goto out_unlock;
	}

	/* new context requested */
	vxdprintk(VXD_CBIT(xid, 0),
		"loc_vx_info(%d) = %p (new)", id, new);
	__hash_vx_info(get_vx_info(new));
	vxi = new, new = NULL;
	*err = 1;

out_unlock:
	spin_unlock(&vx_info_hash_lock);
	if (new)
		__dealloc_vx_info(new);
	return vxi;
}



/*	exported stuff						*/



void rcu_free_vx_info(struct rcu_head *head)
{
	struct vx_info *vxi = container_of(head, struct vx_info, vx_rcu);
	int usecnt, refcnt;

	BUG_ON(!vxi || !head);

	usecnt = atomic_read(&vxi->vx_usecnt);
	BUG_ON(usecnt < 0);

	refcnt = atomic_read(&vxi->vx_refcnt);
	BUG_ON(refcnt < 0);

	vxdprintk(VXD_CBIT(xid, 3),
		"rcu_free_vx_info(%p): uc=%d", vxi, usecnt);
	if (!usecnt)
		__dealloc_vx_info(vxi);
	else
		printk("!!! rcu didn't free\n");
}

void unhash_vx_info(struct vx_info *vxi)
{
	spin_lock(&vx_info_hash_lock);
	__unhash_vx_info(vxi);
	spin_unlock(&vx_info_hash_lock);
}

/*	locate_vx_info()

	* search for a vx_info and get() it			
	* negative id means current				*/

struct vx_info *locate_vx_info(int id)
{
	struct vx_info *vxi;
	
	if (id < 0) {
		vxi = get_vx_info(current->vx_info);
	} else {
		rcu_read_lock();
		vxi = get_vx_info(__lookup_vx_info(id));
		rcu_read_unlock();
	}
	return vxi;
}

/*	vx_info_is_hashed()

	* verify that xid is still hashed			*/

int vx_info_is_hashed(xid_t xid)
{
	int hashed;

	rcu_read_lock();
	hashed = (__lookup_vx_info(xid) != NULL);
	rcu_read_unlock();
	return hashed;
}

#ifdef	CONFIG_VSERVER_LEGACY

#if 0
struct vx_info *alloc_vx_info(xid_t xid)
{
	return __alloc_vx_info(xid);
}
#endif

struct vx_info *locate_or_create_vx_info(int id)
{
	int err;

	return __loc_vx_info(id, &err);
}

#endif

#ifdef	CONFIG_PROC_FS

#define hlist_for_each_rcu(pos, head) \
        for (pos = (head)->first; pos && ({ prefetch(pos->next); 1;}); \
		pos = pos->next, ({ smp_read_barrier_depends(); 0;}))

int get_xid_list(int index, unsigned int *xids, int size)
{
	int hindex, nr_xids = 0;

	rcu_read_lock();
	for (hindex = 0; hindex < VX_HASH_SIZE; hindex++) {
		struct hlist_head *head = &vx_info_hash[hindex];
		struct hlist_node *pos;

		hlist_for_each_rcu(pos, head) {
			struct vx_info *vxi;

			if (--index > 0)
				continue;

			vxi = hlist_entry(pos, struct vx_info, vx_hlist);
			xids[nr_xids] = vxi->vx_id;			
			if (++nr_xids >= size)
				goto out;
		}
	}
out:
	rcu_read_unlock();
	return nr_xids;
}
#endif

int vx_migrate_user(struct task_struct *p, struct vx_info *vxi)
{
	struct user_struct *new_user, *old_user;
	
	if (!p || !vxi)
		BUG();
	new_user = alloc_uid(vxi->vx_id, p->uid);
	if (!new_user)
		return -ENOMEM;

	old_user = p->user;
	if (new_user != old_user) {
		atomic_inc(&new_user->processes);
		atomic_dec(&old_user->processes);
		p->user = new_user;
	}
	free_uid(old_user);
	return 0;
}

void vx_mask_bcaps(struct task_struct *p)
{
	struct vx_info *vxi = p->vx_info;

	p->cap_effective &= vxi->vx_bcaps;
	p->cap_inheritable &= vxi->vx_bcaps;
	p->cap_permitted &= vxi->vx_bcaps;
}


#include <linux/file.h>

static inline int vx_nofiles_task(struct task_struct *tsk)
{
	struct files_struct *files = tsk->files;
	const unsigned long *obptr;
	int count, total;

	spin_lock(&files->file_lock);
	obptr = files->open_fds->fds_bits;
	count = files->max_fds / (sizeof(unsigned long) * 8);
	for (total = 0; count > 0; count--) {
		if (*obptr)
			total += hweight_long(*obptr);
		obptr++;
	}
	spin_unlock(&files->file_lock);
	return total;
}

#if 0

static inline int vx_openfd_task(struct task_struct *tsk)
{
	struct files_struct *files = tsk->files;
	const unsigned long *bptr;
	int count, total;

	spin_lock(&files->file_lock);
	bptr = files->open_fds->fds_bits;
	count = files->max_fds / (sizeof(unsigned long) * 8);
	for (total = 0; count > 0; count--) {
		if (*bptr)
			total += hweight_long(*bptr);
		bptr++;
	}
	spin_unlock(&files->file_lock);
	return total;
}

#endif

/*
 *	migrate task to new context
 *	gets vxi, puts old_vxi on change
 */

int vx_migrate_task(struct task_struct *p, struct vx_info *vxi)
{
	struct vx_info *old_vxi;
	int ret = 0;
	
	if (!p || !vxi)
		BUG();

	old_vxi = task_get_vx_info(p);
	if (old_vxi == vxi)
		goto out;

	vxdprintk(VXD_CBIT(xid, 5),
		"vx_migrate_task(%p,%p[#%d.%d])", p, vxi,
		vxi->vx_id, atomic_read(&vxi->vx_usecnt));

	if (!(ret = vx_migrate_user(p, vxi))) {
		int nofiles;

		task_lock(p);
		// openfd = vx_openfd_task(p);
		nofiles = vx_nofiles_task(p);

		if (old_vxi) {
			atomic_dec(&old_vxi->cacct.nr_threads);
			atomic_dec(&old_vxi->limit.rcur[RLIMIT_NPROC]);
			atomic_sub(nofiles, &old_vxi->limit.rcur[RLIMIT_NOFILE]);
			// atomic_sub(openfd, &old_vxi->limit.rcur[RLIMIT_OPENFD]);
		}		
		atomic_inc(&vxi->cacct.nr_threads);
		atomic_inc(&vxi->limit.rcur[RLIMIT_NPROC]);
		atomic_add(nofiles, &vxi->limit.rcur[RLIMIT_NOFILE]);
		// atomic_add(openfd, &vxi->limit.rcur[RLIMIT_OPENFD]);

		vxdprintk(VXD_CBIT(xid, 5),
			"moved task %p into vxi:%p[#%d]",
			p, vxi, vxi->vx_id);

		/* should be handled in set_vx_info !! */
		if (old_vxi)
			clr_vx_info(&p->vx_info);
		set_vx_info(&p->vx_info, vxi);
		p->xid = vxi->vx_id;
		vx_mask_bcaps(p);
		task_unlock(p);

		/* obsoleted by clr/set */
		// put_vx_info(old_vxi);
	}
out:
	put_vx_info(old_vxi);
	return ret;
}

int vx_set_init(struct vx_info *vxi, struct task_struct *p)
{
	if (!vxi)
		return -EINVAL;
        if (vxi->vx_initpid)
                return -EPERM;

        vxi->vx_initpid = p->tgid;
	return 0;
}


/* vserver syscall commands below here */

/* taks xid and vx_info functions */

#include <asm/uaccess.h>


int vc_task_xid(uint32_t id, void __user *data)
{
        xid_t xid;

        if (id) {
                struct task_struct *tsk;

                if (!vx_check(0, VX_ADMIN|VX_WATCH))
                        return -EPERM;

                read_lock(&tasklist_lock);
                tsk = find_task_by_pid(id);
                xid = (tsk) ? tsk->xid : -ESRCH;
                read_unlock(&tasklist_lock);
        }
        else
                xid = current->xid;
        return xid;
}


int vc_vx_info(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_vx_info_v0 vc_data;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;
	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RESOURCE))
		return -EPERM;

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vc_data.xid = vxi->vx_id;
	vc_data.initpid = vxi->vx_initpid;
	put_vx_info(vxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


/* context functions */

int vc_ctx_create(uint32_t xid, void __user *data)
{
	struct vx_info *new_vxi;
	int ret;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if ((xid >= MIN_D_CONTEXT) && (xid != VX_DYNAMIC_ID))
		return -EINVAL;

	if (xid < 1)
		return -EINVAL;

	new_vxi = __loc_vx_info(xid, &ret);
	if (!new_vxi)
		return ret;
	if (!(new_vxi->vx_flags & VXF_STATE_SETUP)) {
		ret = -EEXIST;
		goto out_put;
	}

	ret = new_vxi->vx_id;
	vx_migrate_task(current, new_vxi);
	/* if this fails, we might end up with a hashed vx_info */
out_put:
	put_vx_info(new_vxi);
	return ret;
}


int vc_ctx_migrate(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	/* dirty hack until Spectator becomes a cap */
	if (id == 1) {
		current->xid = 1;
		return 0;
	}

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;
	vx_migrate_task(current, vxi);
	put_vx_info(vxi);
	return 0;
}


int vc_get_cflags(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_flags_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vc_data.flagword = vxi->vx_flags;

	/* special STATE flag handling */
	vc_data.mask = vx_mask_flags(~0UL, vxi->vx_flags, VXF_ONE_TIME);

	put_vx_info(vxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_cflags(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_flags_v0 vc_data;
	uint64_t mask, trigger;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;

	/* special STATE flag handling */
	mask = vx_mask_mask(vc_data.mask, vxi->vx_flags, VXF_ONE_TIME);
	trigger = (mask & vxi->vx_flags) ^ (mask & vc_data.flagword);

	if (trigger & VXF_STATE_SETUP)
		vx_mask_bcaps(current);
	if (trigger & VXF_STATE_INIT)
		if (vxi == current->vx_info)
			vx_set_init(vxi, current);

	vxi->vx_flags = vx_mask_flags(vxi->vx_flags,
		vc_data.flagword, mask);
	put_vx_info(vxi);
	return 0;
}

int vc_get_ccaps(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_caps_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vc_data.bcaps = vxi->vx_bcaps;
	vc_data.ccaps = vxi->vx_ccaps;
	vc_data.cmask = ~0UL;
	put_vx_info(vxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_ccaps(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_caps_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi = locate_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vxi->vx_bcaps &= vc_data.bcaps;
	vxi->vx_ccaps = vx_mask_flags(vxi->vx_ccaps,
		vc_data.ccaps, vc_data.cmask);
	put_vx_info(vxi);
	return 0;
}

#include <linux/module.h>

EXPORT_SYMBOL_GPL(rcu_free_vx_info);
EXPORT_SYMBOL_GPL(vx_info_hash_lock);
EXPORT_SYMBOL_GPL(unhash_vx_info);

