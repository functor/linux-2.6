#ifndef _VX_NETWORK_CMD_H
#define _VX_NETWORK_CMD_H


/* vinfo commands */

#define VCMD_task_nid		VC_CMD(VINFO, 2, 0)

#ifdef	__KERNEL__
extern int vc_task_nid(uint32_t, void __user *);

#endif	/* __KERNEL__ */

#define VCMD_nx_info		VC_CMD(VINFO, 6, 0)

struct	vcmd_nx_info_v0 {
	uint32_t nid;
	/* more to come */
};

#ifdef	__KERNEL__
extern int vc_nx_info(struct nx_info *, void __user *);

#endif	/* __KERNEL__ */

#define VCMD_net_create_v0	VC_CMD(VNET, 1, 0)
#define VCMD_net_create		VC_CMD(VNET, 1, 1)

struct  vcmd_net_create {
	uint64_t flagword;
};

#define VCMD_net_migrate	VC_CMD(NETMIG, 1, 0)

#define VCMD_net_add		VC_CMD(NETALT, 1, 0)
#define VCMD_net_remove		VC_CMD(NETALT, 2, 0)

#ifdef __KERNEL__
#include <linux/in.h>
#include <linux/in6.h>
#else
#include <arpa/inet.h>
#endif /* __KERNEL__ */

struct	vcmd_net_addr_v0 {
	uint16_t type;
	uint16_t count;
	union {
		struct {
			struct in_addr ip[4];
			struct in_addr mask[4];
		};
		struct {
			struct in6_addr ip6;
			uint32_t prefix;
		};
	};
	/* more to come */
};


#ifdef	__KERNEL__
extern int vc_net_create(uint32_t, void __user *);
extern int vc_net_migrate(struct nx_info *, void __user *);

extern int vc_net_add(struct nx_info *, void __user *);
extern int vc_net_remove(struct nx_info *, void __user *);

#endif	/* __KERNEL__ */


/* flag commands */

#define VCMD_get_nflags		VC_CMD(FLAGS, 5, 0)
#define VCMD_set_nflags		VC_CMD(FLAGS, 6, 0)

struct	vcmd_net_flags_v0 {
	uint64_t flagword;
	uint64_t mask;
};

#ifdef	__KERNEL__
extern int vc_get_nflags(struct nx_info *, void __user *);
extern int vc_set_nflags(struct nx_info *, void __user *);

#endif	/* __KERNEL__ */


/* network caps commands */

#define VCMD_get_ncaps		VC_CMD(FLAGS, 7, 0)
#define VCMD_set_ncaps		VC_CMD(FLAGS, 8, 0)

struct	vcmd_net_caps_v0 {
	uint64_t ncaps;
	uint64_t cmask;
};

#ifdef	__KERNEL__
extern int vc_get_ncaps(struct nx_info *, void __user *);
extern int vc_set_ncaps(struct nx_info *, void __user *);

#endif	/* __KERNEL__ */
#endif	/* _VX_CONTEXT_CMD_H */
