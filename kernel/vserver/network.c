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
 *  V0.04  switch to RCU based hash
 *
 */

#include <linux/config.h>
#include <linux/slab.h>
#include <linux/vserver.h>
#include <linux/vs_base.h>
#include <linux/vs_network.h>
#include <linux/rcupdate.h>

#include <asm/errno.h>


/*	__alloc_nx_info()

	* allocate an initialized nx_info struct
	* doesn't make it visible (hash)			*/

static struct nx_info *__alloc_nx_info(nid_t nid)
{
	struct nx_info *new = NULL;
	
	nxdprintk("alloc_nx_info()\n");

	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct nx_info), GFP_KERNEL);
	if (!new)
		return 0;
	
	memset (new, 0, sizeof(struct nx_info));
	new->nx_id = nid;
	INIT_RCU_HEAD(&new->nx_rcu);
	INIT_HLIST_NODE(&new->nx_hlist);
	atomic_set(&new->nx_refcnt, 0);
	atomic_set(&new->nx_usecnt, 0);

	/* rest of init goes here */
	
	nxdprintk("alloc_nx_info() = %p\n", new);
	return new;
}

/*	__dealloc_nx_info()

	* final disposal of nx_info				*/

static void __dealloc_nx_info(struct nx_info *nxi)
{
	nxdprintk("dealloc_nx_info(%p)\n", nxi);

	nxi->nx_hlist.next = LIST_POISON1;
	nxi->nx_id = -1;
	
	BUG_ON(atomic_read(&nxi->nx_usecnt));
	BUG_ON(atomic_read(&nxi->nx_refcnt));

	kfree(nxi);
}


/*	hash table for nx_info hash */

#define	NX_HASH_SIZE	13

struct hlist_head nx_info_hash[NX_HASH_SIZE];

static spinlock_t nx_info_hash_lock = SPIN_LOCK_UNLOCKED;


static inline unsigned int __hashval(nid_t nid)
{
	return (nid % NX_HASH_SIZE);
}



/*	__hash_nx_info()

	* add the nxi to the global hash table
	* requires the hash_lock to be held			*/

static inline void __hash_nx_info(struct nx_info *nxi)
{
	struct hlist_head *head;
	
	nxdprintk("__hash_nx_info: %p[#%d]\n", nxi, nxi->nx_id);
	get_nx_info(nxi);
	head = &nx_info_hash[__hashval(nxi->nx_id)];
	hlist_add_head_rcu(&nxi->nx_hlist, head);
}

/*	__unhash_nx_info()

	* remove the nxi from the global hash table
	* requires the hash_lock to be held			*/

static inline void __unhash_nx_info(struct nx_info *nxi)
{
	nxdprintk("__unhash_nx_info: %p[#%d]\n", nxi, nxi->nx_id);
	hlist_del_rcu(&nxi->nx_hlist);
	put_nx_info(nxi);
}


/*	__lookup_nx_info()

	* requires the rcu_read_lock()
	* doesn't increment the nx_refcnt			*/

static inline struct nx_info *__lookup_nx_info(nid_t nid)
{
	struct hlist_head *head = &nx_info_hash[__hashval(nid)];
	struct hlist_node *pos;

	hlist_for_each_rcu(pos, head) {
		struct nx_info *nxi =
			hlist_entry(pos, struct nx_info, nx_hlist);

		if (nxi->nx_id == nid) {
			return nxi;
		}
	}
	return NULL;
}


/*	__nx_dynamic_id()

	* find unused dynamic nid
	* requires the hash_lock to be held			*/

static inline nid_t __nx_dynamic_id(void)
{
	static nid_t seq = MAX_N_CONTEXT;
	nid_t barrier = seq;
	
	do {
		if (++seq > MAX_N_CONTEXT)
			seq = MIN_D_CONTEXT;
		if (!__lookup_nx_info(seq))
			return seq;
	} while (barrier != seq);
	return 0;
}

/*	__loc_nx_info()

	* locate or create the requested context
	* get() it and if new hash it				*/

static struct nx_info * __loc_nx_info(int id, int *err)
{
	struct nx_info *new, *nxi = NULL;
	
	nxdprintk("loc_nx_info(%d)\n", id);

	if (!(new = __alloc_nx_info(id))) {
		*err = -ENOMEM;
		return NULL;
	}

	spin_lock(&nx_info_hash_lock);

	/* dynamic context requested */
	if (id == NX_DYNAMIC_ID) {
		id = __nx_dynamic_id();
		if (!id) {
			printk(KERN_ERR "no dynamic context available.\n");
			goto out_unlock;
		}
		new->nx_id = id;
	}
	/* existing context requested */
	else if ((nxi = __lookup_nx_info(id))) {
		/* context in setup is not available */
		if (nxi->nx_flags & VXF_STATE_SETUP) {
			nxdprintk("loc_nx_info(%d) = %p (not available)\n", id, nxi);
			nxi = NULL;
			*err = -EBUSY;
		} else {
			nxdprintk("loc_nx_info(%d) = %p (found)\n", id, nxi);
			get_nx_info(nxi);
			*err = 0;
		}
		goto out_unlock;
	}

	/* new context requested */
	nxdprintk("loc_nx_info(%d) = %p (new)\n", id, new);
	__hash_nx_info(get_nx_info(new));
	nxi = new, new = NULL;
	*err = 1;

out_unlock:
	spin_unlock(&nx_info_hash_lock);
	if (new)
		__dealloc_nx_info(new);
	return nxi;
}



/*	exported stuff						*/




void rcu_free_nx_info(void *obj)
{
	struct nx_info *nxi = obj;
	int usecnt, refcnt;

	usecnt = atomic_read(&nxi->nx_usecnt);
	BUG_ON(usecnt < 0);

	refcnt = atomic_read(&nxi->nx_refcnt);
	BUG_ON(refcnt < 0);

	if (!usecnt)
		__dealloc_nx_info(nxi);
	else
		printk("!!! rcu didn't free\n");
}

void unhash_nx_info(struct nx_info *nxi)
{
	spin_lock(&nx_info_hash_lock);
	__unhash_nx_info(nxi);
	spin_unlock(&nx_info_hash_lock);
}

/*	locate_nx_info()

	* search for a nx_info and get() it			
	* negative id means current				*/

struct nx_info *locate_nx_info(int id)
{
	struct nx_info *nxi;
	
	if (id < 0) {
		nxi = get_nx_info(current->nx_info);
	} else {
		rcu_read_lock();
		nxi = get_nx_info(__lookup_nx_info(id));
		rcu_read_unlock();
	}
	return nxi;
}

/*	nx_info_is_hashed()

	* verify that nid is still hashed			*/

int nx_info_is_hashed(nid_t nid)
{
	int hashed;

	rcu_read_lock();
	hashed = (__lookup_nx_info(nid) != NULL);
	rcu_read_unlock();
	return hashed;
}

#ifdef	CONFIG_VSERVER_LEGACY

struct nx_info *locate_or_create_nx_info(int id)
{
	int err;

	return __loc_nx_info(id, &err);
}

struct nx_info *create_nx_info(void)
{
	struct nx_info *new;
	int err;
	
	nxdprintk("create_nx_info()\n");
	if (!(new = __loc_nx_info(NX_DYNAMIC_ID, &err)))
		return NULL;
	return new;
}


#endif

#ifdef	CONFIG_PROC_FS

#define hlist_for_each_rcu(pos, head) \
        for (pos = (head)->first; pos && ({ prefetch(pos->next); 1;}); \
		pos = pos->next, ({ smp_read_barrier_depends(); 0;}))

int get_nid_list(int index, unsigned int *nids, int size)
{
	int hindex, nr_nids = 0;

	rcu_read_lock();
	for (hindex = 0; hindex < NX_HASH_SIZE; hindex++) {
		struct hlist_head *head = &nx_info_hash[hindex];
		struct hlist_node *pos;

		hlist_for_each_rcu(pos, head) {
			struct nx_info *nxi;

			if (--index > 0)
				continue;

			nxi = hlist_entry(pos, struct nx_info, nx_hlist);
			nids[nr_nids] = nxi->nx_id;			
			if (++nr_nids >= size)
				goto out;
		}
	}
out:
	rcu_read_unlock();
	return nr_nids;
}
#endif


/*
 *	migrate task to new network
 */

int nx_migrate_task(struct task_struct *p, struct nx_info *nxi)
{
	struct nx_info *old_nxi;
	int ret = 0;
	
	if (!p || !nxi)
		BUG();

	nxdprintk("nx_migrate_task(%p,%p[#%d.%d.%d])\n",
		p, nxi, nxi->nx_id,
		atomic_read(&nxi->nx_usecnt),
		atomic_read(&nxi->nx_refcnt));

	old_nxi = task_get_nx_info(p);
	if (old_nxi == nxi)
		goto out;

	task_lock(p);
	/* should be handled in set_nx_info !! */
	if (old_nxi)
		clr_nx_info(&p->nx_info);
	set_nx_info(&p->nx_info, nxi);
	p->nid = nxi->nx_id;
	task_unlock(p);

	/* obsoleted by clr/set */
	// put_nx_info(old_nxi);
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
	if (nxi && ifa)
		return __addr_in_nx_info(ifa->ifa_address, nxi);
	return 1;
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

	nxi = locate_nx_info(id);
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

	new_nxi = __loc_nx_info(nid, &ret);
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

	nxi = locate_nx_info(id);
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

	nxi = locate_nx_info(id);
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

	nxi = locate_nx_info(id);
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

	nxi = locate_nx_info(id);
	if (!nxi)
		return -ESRCH;

	vc_data.flagword = nxi->nx_flags;

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

	nxi = locate_nx_info(id);
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

	nxi = locate_nx_info(id);
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

	nxi = locate_nx_info(id);
	if (!nxi)
		return -ESRCH;

	nxi->nx_ncaps = vx_mask_flags(nxi->nx_ncaps,
		vc_data.ncaps, vc_data.cmask);
	put_nx_info(nxi);
	return 0;
}


#include <linux/module.h>

EXPORT_SYMBOL_GPL(rcu_free_nx_info);
EXPORT_SYMBOL_GPL(nx_info_hash_lock);
EXPORT_SYMBOL_GPL(unhash_nx_info);

