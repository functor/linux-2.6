/*
 *  linux/kernel/vserver/network.c
 *
 *  Virtual Server: Network Support
 *
 *  Copyright (C) 2003-2004  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext V0.05
 *  V0.02  cleaned up implementation
 *  V0.03  added equiv nx commands
 *
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/vserver/network.h>
#include <linux/ninline.h>

#include <asm/errno.h>


LIST_HEAD(nx_infos);

spinlock_t nxlist_lock
	__cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;


/*
 *	struct nx_info allocation and deallocation
 */

static struct nx_info *alloc_nx_info(void)
{
	struct nx_info *new = NULL;
	
	nxdprintk("alloc_nx_info()\n");
	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct nx_info), GFP_KERNEL);
	if (!new)
		return 0;
	
	memset (new, 0, sizeof(struct nx_info));
	/* rest of init goes here */
	
	nxdprintk("alloc_nx_info() = %p\n", new);
	return new;
}

void free_nx_info(struct nx_info *nxi)
{
	nxdprintk("free_nx_info(%p)\n", nxi);
	kfree(nxi);
}

struct nx_info *create_nx_info(void)
{
	struct nx_info *new;
	static int gnid = 1;
	
	nxdprintk("create_nx_info()\n");
	if (!(new = alloc_nx_info()))
		return 0;

	spin_lock(&nxlist_lock);

	/* new ip info */
	atomic_set(&new->nx_refcount, 1);
	new->nx_id = gnid++;
	list_add(&new->nx_list, &nx_infos);

	spin_unlock(&nxlist_lock);
	return new;
}


/*
 *	struct nx_info search by id
 *	assumes nxlist_lock is held
 */

static __inline__ struct nx_info *__find_nx_info(int id)
{
	struct nx_info *nxi;

	list_for_each_entry(nxi, &nx_infos, nx_list)
		if (nxi->nx_id == id)
			return nxi;
	return 0;
}


/*
 *	struct nx_info ref stuff
 */

struct nx_info *find_nx_info(int id)
{
	struct nx_info *nxi;
	
	if (id < 0) {
		nxi = current->nx_info;
		get_nx_info(nxi);
	} else {
		spin_lock(&nxlist_lock);
		if ((nxi = __find_nx_info(id)))
			get_nx_info(nxi);
		spin_unlock(&nxlist_lock);
	}
	return nxi;
}

/*
 *      verify that id is a valid nid
 */

int nx_info_id_valid(int id)
{
	int valid;
	
	spin_lock(&nxlist_lock);
	valid = (__find_nx_info(id) != NULL);
	spin_unlock(&nxlist_lock);
	return valid;
}


/*
 *	dynamic context id ...
 */

static __inline__ nid_t __nx_dynamic_id(void)
{
	static nid_t seq = MAX_N_CONTEXT;
	nid_t barrier = seq;
	
	do {
		if (++seq > MAX_N_CONTEXT)
			seq = MIN_D_CONTEXT;
		if (!__find_nx_info(seq))
			return seq;
	} while (barrier != seq);
	return 0;
}

static struct nx_info * __foc_nx_info(int id, int *err)
{
	struct nx_info *new, *nxi = NULL;
	
	nxdprintk("foc_nx_info(%d)\n", id);
	// if (!(new = alloc_nx_info(id))) {
	if (!(new = alloc_nx_info())) {
		*err = -ENOMEM;
		return NULL;
	}

	spin_lock(&nxlist_lock);

	/* dynamic context requested */
	if (id == IP_DYNAMIC_ID) {
		id = __nx_dynamic_id();
		if (!id) {
			printk(KERN_ERR "no dynamic context available.\n");
			goto out_unlock;
		}
		new->nx_id = id;
	}
	/* existing context requested */
	else if ((nxi = __find_nx_info(id))) {
		/* context in setup is not available */
		if (nxi->nx_flags & VXF_STATE_SETUP) {
			nxdprintk("foc_nx_info(%d) = %p (not available)\n", id, nxi);
			nxi = NULL;
			*err = -EBUSY;
		} else {
			nxdprintk("foc_nx_info(%d) = %p (found)\n", id, nxi);
			get_nx_info(nxi);
			*err = 0;
		}
		goto out_unlock;
	}

	/* new context requested */
	nxdprintk("foc_nx_info(%d) = %p (new)\n", id, new);
	atomic_set(&new->nx_refcount, 1);
	list_add(&new->nx_list, &nx_infos);
	nxi = new, new = NULL;
	*err = 1;

out_unlock:
	spin_unlock(&nxlist_lock);
	if (new)
		free_nx_info(new);
	return nxi;
}


struct nx_info *find_or_create_nx_info(int id)
{
	int err;

	return __foc_nx_info(id, &err);
}

/*
 *	migrate task to new network
 */

int nx_migrate_task(struct task_struct *p, struct nx_info *nxi)
{
	struct nx_info *old_nxi = task_get_nx_info(p);
	int ret = 0;
	
	if (!p || !nxi)
		BUG();

	nxdprintk("nx_migrate_task(%p,%p[#%d.%d)\n", p, nxi,
		nxi->nx_id, atomic_read(&nxi->nx_refcount));
	if (old_nxi == nxi)
		goto out;

	task_lock(p);
	set_nx_info(&p->nx_info, nxi);
	p->nid = nxi->nx_id;
	task_unlock(p);

	put_nx_info(old_nxi);
out:
	put_nx_info(old_nxi);
	return ret;
}


#include <linux/netdevice.h>
#include <linux/inetdevice.h>

static inline int __addr_in_nx_info(u32 addr, struct nx_info *nxi)
{
	int i, nbip;

	nbip = nxi->nbipv4;
	for (i=0; i<nbip; i++)
		if (nxi->ipv4[i] == addr)
			return 1;
	return 0;
}

int ifa_in_nx_info(struct in_ifaddr *ifa, struct nx_info *nxi)
{
	if (!nxi)
		return 1;
	
	return __addr_in_nx_info(ifa->ifa_address, nxi);
}

int dev_in_nx_info(struct net_device *dev, struct nx_info *nxi)
{
	struct in_device *in_dev = __in_dev_get(dev);
	struct in_ifaddr **ifap = NULL;
	struct in_ifaddr *ifa = NULL;

	if (!nxi)
		return 1;
	if (!in_dev)
		return 0;

	for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
		ifap = &ifa->ifa_next) {
		if (__addr_in_nx_info(ifa->ifa_address, nxi))
			return 1;
	}
	return 0;
}




/* vserver syscall commands below here */

/* taks nid and nx_info functions */

#include <asm/uaccess.h>


int vc_task_nid(uint32_t id, void __user *data)
{
        nid_t nid;

        if (id) {
                struct task_struct *tsk;

                if (!vx_check(0, VX_ADMIN|VX_WATCH))
                        return -EPERM;

                read_lock(&tasklist_lock);
                tsk = find_task_by_pid(id);
                nid = (tsk) ? tsk->nid : -ESRCH;
                read_unlock(&tasklist_lock);
        }
        else
                nid = current->nid;
        return nid;
}


int vc_nx_info(uint32_t id, void __user *data)
{
	struct nx_info *nxi;
	struct vcmd_nx_info_v0 vc_data;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;
	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RESOURCE))
		return -EPERM;

	nxi = find_nx_info(id);
	if (!nxi)
		return -ESRCH;

	vc_data.nid = nxi->nx_id;
	put_nx_info(nxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


/* network functions */

int vc_net_create(uint32_t nid, void __user *data)
{
        // int ret = -ENOMEM;
	struct nx_info *new_nxi;
	int ret;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if ((nid >= MIN_D_CONTEXT) && (nid != VX_DYNAMIC_ID))
		return -EINVAL;

	if (nid < 1)
		return -EINVAL;

	new_nxi = __foc_nx_info(nid, &ret);
	if (!new_nxi)
		return ret;
	if (!(new_nxi->nx_flags & VXF_STATE_SETUP)) {
		ret = -EEXIST;
		goto out_put;
	}

	ret = new_nxi->nx_id;
	nx_migrate_task(current, new_nxi);
out_put:
	put_nx_info(new_nxi);
	return ret;
}


int vc_net_migrate(uint32_t id, void __user *data)
{
	struct nx_info *nxi;
	
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	nxi = find_nx_info(id);
	if (!nxi)
		return -ESRCH;
	nx_migrate_task(current, nxi);
	put_nx_info(nxi);
	return 0;
}

int vc_net_add(uint32_t id, void __user *data)
{
	struct nx_info *nxi;
	struct vcmd_net_nx_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	nxi = find_nx_info(id);
	if (!nxi)
		return -ESRCH;

	// add ip to net context here
	put_nx_info(nxi);
	return 0;
}

int vc_net_remove(uint32_t id, void __user *data)
{
	struct nx_info *nxi;
	struct vcmd_net_nx_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	nxi = find_nx_info(id);
	if (!nxi)
		return -ESRCH;

	// rem ip from net context here
	put_nx_info(nxi);
	return 0;
}



int vc_get_nflags(uint32_t id, void __user *data)
{
	struct nx_info *nxi;
	struct vcmd_net_flags_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	nxi = find_nx_info(id);
	if (!nxi)
		return -ESRCH;

	vc_data.flagword = nxi->nx_flags;

	// vc_data.mask = ~0UL;
	/* special STATE flag handling */
	vc_data.mask = vx_mask_flags(~0UL, nxi->nx_flags, IPF_ONE_TIME);

	put_nx_info(nxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_nflags(uint32_t id, void __user *data)
{
	struct nx_info *nxi;
	struct vcmd_net_flags_v0 vc_data;
	uint64_t mask, trigger;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	nxi = find_nx_info(id);
	if (!nxi)
		return -ESRCH;

	/* special STATE flag handling */
	mask = vx_mask_mask(vc_data.mask, nxi->nx_flags, IPF_ONE_TIME);
	trigger = (mask & nxi->nx_flags) ^ (mask & vc_data.flagword);
	// if (trigger & IPF_STATE_SETUP)

	nxi->nx_flags = vx_mask_flags(nxi->nx_flags,
		vc_data.flagword, mask);
	put_nx_info(nxi);
	return 0;
}

int vc_get_ncaps(uint32_t id, void __user *data)
{
	struct nx_info *nxi;
	struct vcmd_net_caps_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	nxi = find_nx_info(id);
	if (!nxi)
		return -ESRCH;

	vc_data.ncaps = nxi->nx_ncaps;
	vc_data.cmask = ~0UL;
	put_nx_info(nxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_ncaps(uint32_t id, void __user *data)
{
	struct nx_info *nxi;
	struct vcmd_net_caps_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	nxi = find_nx_info(id);
	if (!nxi)
		return -ESRCH;

	nxi->nx_ncaps = vx_mask_flags(nxi->nx_ncaps,
		vc_data.ncaps, vc_data.cmask);
	put_nx_info(nxi);
	return 0;
}


#include <linux/module.h>

EXPORT_SYMBOL_GPL(free_nx_info);
EXPORT_SYMBOL_GPL(nxlist_lock);

