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
 *
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/vserver/context.h>
#include <linux/vserver/legacy.h>
#include <linux/vinline.h>
#include <linux/kernel_stat.h>
#include <linux/namespace.h>

#include <asm/errno.h>


/*  system functions */


LIST_HEAD(vx_infos);

spinlock_t vxlist_lock
	__cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;


/*
 *	struct vx_info allocation and deallocation
 */

static struct vx_info *alloc_vx_info(int id)
{
	struct vx_info *new = NULL;
	
	vxdprintk("alloc_vx_info(%d)\n", id);
	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct vx_info), GFP_KERNEL);
	if (!new)
		return 0;

	memset (new, 0, sizeof(struct vx_info));
	new->vx_id = id;
	INIT_LIST_HEAD(&new->vx_list);
	/* rest of init goes here */
	
	vx_info_init_limit(&new->limit);
	vx_info_init_sched(&new->sched);
	vx_info_init_cvirt(&new->cvirt);
	vx_info_init_cacct(&new->cacct);

	new->vx_flags = VXF_STATE_SETUP|VXF_STATE_INIT;
	new->vx_bcaps = CAP_INIT_EFF_SET;
	new->vx_ccaps = 0;

	vxdprintk("alloc_vx_info(%d) = %p\n", id, new);
	return new;
}

void free_vx_info(struct vx_info *vxi)
{
	vxdprintk("free_vx_info(%p)\n", vxi);
	if (vxi->vx_namespace)
		put_namespace(vxi->vx_namespace);
	if (vxi->vx_fs)
		put_fs_struct(vxi->vx_fs);
	
	vx_info_exit_limit(&vxi->limit);
	vx_info_exit_sched(&vxi->sched);
	vx_info_exit_cvirt(&vxi->cvirt);
	vx_info_exit_cacct(&vxi->cacct);
	
	BUG_ON(atomic_read(&vxi->vx_refcount));
	vxi->vx_id = -1;

	kfree(vxi);
}


/*
 *	struct vx_info search by id
 *	assumes vxlist_lock is held
 */

static __inline__ struct vx_info *__find_vx_info(int id)
{
	struct vx_info *vxi;

	list_for_each_entry(vxi, &vx_infos, vx_list)
		if (vxi->vx_id == id)
			return vxi;
	return 0;
}


/*
 *	struct vx_info ref stuff
 */

struct vx_info *find_vx_info(int id)
{
	struct vx_info *vxi;
	
	if (id < 0) {
		vxi = current->vx_info;
		get_vx_info(vxi);
	} else {
		spin_lock(&vxlist_lock);
		if ((vxi = __find_vx_info(id)))
			get_vx_info(vxi);
		spin_unlock(&vxlist_lock);
	}
	return vxi;
}

/*
 *	verify that id is a valid xid
 */

int vx_info_id_valid(int id)
{
	int valid;

	spin_lock(&vxlist_lock);
	valid = (__find_vx_info(id) != NULL);
	spin_unlock(&vxlist_lock);
	return valid;
}


/*
 *	dynamic context id ...
 */

static __inline__ xid_t __vx_dynamic_id(void)
{
	static xid_t seq = MAX_S_CONTEXT;
	xid_t barrier = seq;
	
	do {
		if (++seq > MAX_S_CONTEXT)
			seq = MIN_D_CONTEXT;
		if (!__find_vx_info(seq))
			return seq;
	} while (barrier != seq);
	return 0;
}

static struct vx_info * __foc_vx_info(int id, int *err)
{
	struct vx_info *new, *vxi = NULL;
	
	vxdprintk("foc_vx_info(%d)\n", id);
	if (!(new = alloc_vx_info(id))) {
		*err = -ENOMEM;
		return NULL;
	}

	/* dirty hack until Spectator becomes a cap */
	if (id == 0 || id == 1) {
		*err = -EBUSY;
		return NULL;
	}

	spin_lock(&vxlist_lock);

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
	else if ((vxi = __find_vx_info(id))) {
		/* context in setup is not available */
		if (vxi->vx_flags & VXF_STATE_SETUP) {
			vxdprintk("foc_vx_info(%d) = %p (not available)\n", id, vxi);
			vxi = NULL;
			*err = -EBUSY;
		} else {
			vxdprintk("foc_vx_info(%d) = %p (found)\n", id, vxi);
			get_vx_info(vxi);
			*err = 0;
		}
		goto out_unlock;
	}

	/* new context requested */
	vxdprintk("foc_vx_info(%d) = %p (new)\n", id, new);
	atomic_set(&new->vx_refcount, 1);
	list_add(&new->vx_list, &vx_infos);
	vxi = new, new = NULL;
	*err = 1;

out_unlock:
	spin_unlock(&vxlist_lock);
	if (new)
		free_vx_info(new);
	return vxi;
}


struct vx_info *find_or_create_vx_info(int id)
{
	int err;

	return __foc_vx_info(id, &err);
}


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
	const unsigned long *obptr, *cbptr;
	int count, total;

	spin_lock(&files->file_lock);
	obptr = files->open_fds->fds_bits;
	cbptr = files->close_on_exec->fds_bits;
	count = files->max_fds / (sizeof(unsigned long) * 8);
	for (total = 0; count > 0; count--) {
		if (*obptr)
			total += hweight_long(*obptr);
		obptr++;
	/*	if (*cbptr)
			total += hweight_long(*cbptr);
		cbptr++; */
	}
	spin_unlock(&files->file_lock);
	return total;
}

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

/*
 *	migrate task to new context
 *	gets vxi, puts old_vxi on change
 */

int vx_migrate_task(struct task_struct *p, struct vx_info *vxi)
{
	struct vx_info *old_vxi = task_get_vx_info(p);
	int ret = 0;
	
	if (!p || !vxi)
		BUG();

	vxdprintk("vx_migrate_task(%p,%p[#%d.%d)\n", p, vxi,
		vxi->vx_id, atomic_read(&vxi->vx_refcount));
	if (old_vxi == vxi)
		goto out;

	if (!(ret = vx_migrate_user(p, vxi))) {
		task_lock(p);
		if (old_vxi) {
			atomic_dec(&old_vxi->cacct.nr_threads);
			atomic_dec(&old_vxi->limit.res[RLIMIT_NPROC]);
		}		
		atomic_inc(&vxi->cacct.nr_threads);
		atomic_inc(&vxi->limit.res[RLIMIT_NPROC]);
		atomic_add(vx_nofiles_task(p), &vxi->limit.res[RLIMIT_NOFILE]);
		atomic_add(vx_openfd_task(p), &vxi->limit.res[RLIMIT_OPENFD]);
		set_vx_info(&p->vx_info, vxi);
		p->xid = vxi->vx_id;
		vx_mask_bcaps(p);
		task_unlock(p);

		put_vx_info(old_vxi);
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

	vxi = find_vx_info(id);
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
        // int ret = -ENOMEM;
	struct vx_info *new_vxi;
	int ret;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if ((xid >= MIN_D_CONTEXT) && (xid != VX_DYNAMIC_ID))
		return -EINVAL;

	if (xid < 1)
		return -EINVAL;

	new_vxi = __foc_vx_info(xid, &ret);
	if (!new_vxi)
		return ret;
	if (!(new_vxi->vx_flags & VXF_STATE_SETUP)) {
		ret = -EEXIST;
		goto out_put;
	}

	ret = new_vxi->vx_id;
	vx_migrate_task(current, new_vxi);
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

	vxi = find_vx_info(id);
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

	vxi = find_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vc_data.flagword = vxi->vx_flags;

	// vc_data.mask = ~0UL;
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

	vxi = find_vx_info(id);
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

	vxi = find_vx_info(id);
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

	vxi = find_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vxi->vx_bcaps &= vc_data.bcaps;
	vxi->vx_ccaps = vx_mask_flags(vxi->vx_ccaps,
		vc_data.ccaps, vc_data.cmask);
	put_vx_info(vxi);
	return 0;
}

#include <linux/module.h>

EXPORT_SYMBOL_GPL(free_vx_info);
EXPORT_SYMBOL_GPL(vxlist_lock);

