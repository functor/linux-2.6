/*
 *  linux/kernel/vserver/switch.c
 *
 *  Virtual Server: Syscall Switch
 *
 *  Copyright (C) 2003-2004  Herbert Pötzl
 *
 *  V0.01  syscall switch
 *  V0.02  added signal to context
 *  V0.03  added rlimit functions
 *  V0.04  added iattr, task/xid functions
 *
 */

#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/errno.h>

#include <linux/vserver/switch.h>
#include <linux/vinline.h>


static inline int
vc_get_version(uint32_t id)
{
	return VCI_VERSION;
}


#include <linux/vserver/legacy.h>
#include <linux/vserver/context.h>
#include <linux/vserver/network.h>
#include <linux/vserver/namespace.h>
#include <linux/vserver/sched.h>
#include <linux/vserver/limit.h>
#include <linux/vserver/inode.h>
#include <linux/vserver/signal.h>


extern unsigned int vx_debug_switch;


extern asmlinkage long
sys_vserver(uint32_t cmd, uint32_t id, void __user *data)
{

	if (vx_debug_switch)
		printk( "vc: VCMD_%02d_%d[%d], %d\n",
			VC_CATEGORY(cmd), VC_COMMAND(cmd),
			VC_VERSION(cmd), id);

	switch (cmd) {
	case VCMD_get_version:
		return vc_get_version(id);

#ifdef	CONFIG_VSERVER_LEGACY		
	case VCMD_new_s_context:
		return vc_new_s_context(id, data);
	case VCMD_set_ipv4root:
		return vc_set_ipv4root(id, data);
#endif

	case VCMD_task_xid:
		return vc_task_xid(id, data);
	case VCMD_vx_info:
		return vc_vx_info(id, data);

	case VCMD_task_nid:
		return vc_task_nid(id, data);
	case VCMD_nx_info:
		return vc_nx_info(id, data);

	case VCMD_set_namespace:
		return vc_set_namespace(id, data);
	case VCMD_cleanup_namespace:
		return vc_cleanup_namespace(id, data);
	}

	/* those are allowed while in setup too */
	if (!vx_check(0, VX_ADMIN|VX_WATCH) &&
		!vx_flags(VXF_STATE_SETUP,0))
		return -EPERM;

#ifdef	CONFIG_VSERVER_LEGACY
	switch (cmd) {
	case VCMD_set_cflags:
	case VCMD_set_ccaps:
		if (vx_check(0, VX_WATCH))
			return 0;
	}
#endif

	switch (cmd) {
	case VCMD_get_rlimit:
		return vc_get_rlimit(id, data);
	case VCMD_set_rlimit:
		return vc_set_rlimit(id, data);
	case VCMD_get_rlimit_mask:
		return vc_get_rlimit_mask(id, data);
		
	case VCMD_vx_get_vhi_name:
		return vc_get_vhi_name(id, data);
	case VCMD_vx_set_vhi_name:
		return vc_set_vhi_name(id, data);

	case VCMD_set_cflags:
		return vc_set_cflags(id, data);
	case VCMD_get_cflags:
		return vc_get_cflags(id, data);

	case VCMD_set_ccaps:
		return vc_set_ccaps(id, data);
	case VCMD_get_ccaps:
		return vc_get_ccaps(id, data);

	case VCMD_set_nflags:
		return vc_set_nflags(id, data);
	case VCMD_get_nflags:
		return vc_get_nflags(id, data);

	case VCMD_set_ncaps:
		return vc_set_ncaps(id, data);
	case VCMD_get_ncaps:
		return vc_get_ncaps(id, data);

	case VCMD_set_sched:
		return vc_set_sched(id, data);
	}

	/* below here only with VX_ADMIN */
	if (!vx_check(0, VX_ADMIN|VX_WATCH))
		return -EPERM;

	switch (cmd) {
	case VCMD_ctx_kill:
		return vc_ctx_kill(id, data);

#ifdef	CONFIG_VSERVER_LEGACY		
	case VCMD_create_context:
		return vc_ctx_create(id, data);
#endif

	case VCMD_get_iattr:
		return vc_get_iattr(id, data);
	case VCMD_set_iattr:
		return vc_set_iattr(id, data);

	case VCMD_enter_namespace:
		return vc_enter_namespace(id, data);

	case VCMD_ctx_create:
#ifdef	CONFIG_VSERVER_LEGACY		
		if (id == 1) {
			current->xid = 1;
			return 1;
		}
#endif
		return vc_ctx_create(id, data);
	case VCMD_ctx_migrate:
		return vc_ctx_migrate(id, data);

	case VCMD_net_create:
		return vc_net_create(id, data);
	case VCMD_net_migrate:
		return vc_net_migrate(id, data);

	}
	return -ENOSYS;
}

