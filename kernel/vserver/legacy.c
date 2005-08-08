/*
 *  linux/kernel/vserver/legacy.c
 *
 *  Virtual Server: Legacy Funtions
 *
 *  Copyright (C) 2001-2003  Jacques Gelinas
 *  Copyright (C) 2003-2004  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext.c V0.05
 *
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/namespace.h>
#include <linux/vserver/legacy.h>
#include <linux/vserver/namespace.h>
#include <linux/vserver.h>
#include <linux/vs_base.h>
#include <linux/vs_context.h>
#include <linux/vs_network.h>

#include <asm/errno.h>
#include <asm/uaccess.h>



static int vx_set_initpid(struct vx_info *vxi, int pid)
{
	if (vxi->vx_initpid)
		return -EPERM;

	vxi->vx_initpid = pid;
	return 0;
}

int vc_new_s_context(uint32_t ctx, void __user *data)
{
	int ret = -ENOMEM;
	struct vcmd_new_s_context_v1 vc_data;
	struct vx_info *new_vxi;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	/* legacy hack, will be removed soon */
	if (ctx == -2) {
		/* assign flags and initpid */
		if (!current->vx_info)
			return -EINVAL;
		ret = 0;
		if (vc_data.flags & VX_INFO_INIT)
			ret = vx_set_initpid(current->vx_info, current->tgid);
		if (ret == 0) {
			/* We keep the same vx_id, but lower the capabilities */
			current->vx_info->vx_bcaps &= (~vc_data.remove_cap);
			// current->cap_bset &= (~vc_data.remove_cap);
			ret = vx_current_xid();
			current->vx_info->vx_flags |= vc_data.flags;
		}
		return ret;
	}

	if (!vx_check(0, VX_ADMIN) ||
		!capable(CAP_SYS_ADMIN) || vx_flags(VX_INFO_PRIVATE, 0))
		return -EPERM;

	/* ugly hack for Spectator */
	if (ctx == 1) {
		current->xid = 1;
		return 0;
	}

	if (((ctx > MAX_S_CONTEXT) && (ctx != VX_DYNAMIC_ID)) ||
		(ctx == 0))
		return -EINVAL;

	if ((ctx == VX_DYNAMIC_ID) || (ctx < MIN_D_CONTEXT))
		new_vxi = locate_or_create_vx_info(ctx);
	else
		new_vxi = locate_vx_info(ctx);

	if (!new_vxi)
		return -EINVAL;
	new_vxi->vx_flags &= ~(VXF_STATE_SETUP|VXF_STATE_INIT);

	ret = vx_migrate_task(current, new_vxi);
	if (ret == 0) {
		current->vx_info->vx_bcaps &= (~vc_data.remove_cap);
		// current->cap_bset &= (~vc_data.remove_cap);
		new_vxi->vx_flags |= vc_data.flags;
		if (vc_data.flags & VX_INFO_INIT)
			vx_set_initpid(new_vxi, current->tgid);
		if (vc_data.flags & VX_INFO_NAMESPACE)
			vx_set_namespace(new_vxi,
				current->namespace, current->fs);
		if (vc_data.flags & VX_INFO_NPROC)
			new_vxi->limit.rlim[RLIMIT_NPROC] =
				current->signal->rlim[RLIMIT_NPROC].rlim_max;
		ret = new_vxi->vx_id;
	}
	put_vx_info(new_vxi);
	return ret;
}


extern struct nx_info *create_nx_info(void);

/*  set ipv4 root (syscall) */

int vc_set_ipv4root(uint32_t nbip, void __user *data)
{
	int i, err = -EPERM;
	struct vcmd_set_ipv4root_v3 vc_data;
	struct nx_info *new_nxi, *nxi = current->nx_info;

	if (nbip < 0 || nbip > NB_IPV4ROOT)
		return -EINVAL;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	if (!nxi || nxi->ipv4[0] == 0 || capable(CAP_NET_ADMIN))
		// We are allowed to change everything
		err = 0;
	else if (nxi) {
		int found = 0;

		// We are allowed to select a subset of the currently
		// installed IP numbers. No new one allowed
		// We can't change the broadcast address though
		for (i=0; i<nbip; i++) {
			int j;
			__u32 nxip = vc_data.nx_mask_pair[i].ip;
			for (j=0; j<nxi->nbipv4; j++) {
				if (nxip == nxi->ipv4[j]) {
					found++;
					break;
				}
			}
		}
		if ((found == nbip) &&
			(vc_data.broadcast == nxi->v4_bcast))
			err = 0;
	}
	if (err)
		return err;

	new_nxi = create_nx_info();
	if (!new_nxi)
		return -EINVAL;

	new_nxi->nbipv4 = nbip;
	for (i=0; i<nbip; i++) {
		new_nxi->ipv4[i] = vc_data.nx_mask_pair[i].ip;
		new_nxi->mask[i] = vc_data.nx_mask_pair[i].mask;
	}
	new_nxi->v4_bcast = vc_data.broadcast;
	// current->nx_info = new_nxi;
	if (nxi) {
		printk("!!! switching nx_info %p->%p\n", nxi, new_nxi);
		clr_nx_info(&current->nx_info);
	}
	nx_migrate_task(current, new_nxi);
	// set_nx_info(&current->nx_info, new_nxi);
	// current->nid = new_nxi->nx_id;
	put_nx_info(new_nxi);
	return 0;
}


