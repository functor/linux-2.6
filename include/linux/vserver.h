#ifndef _LINUX_VSERVER_H
#define _LINUX_VSERVER_H

#include <linux/vserver/context.h>
#include <linux/vserver/network.h>

extern long vs_reboot(unsigned int, void *);

#define hlist_for_each_rcu(pos, head) \
	for (pos = (head)->first; pos && ({ prefetch(pos->next); 1;}); \
		pos = pos->next, ({ smp_read_barrier_depends(); 0;}))

#endif
