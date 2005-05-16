#ifndef _VX_LIMIT_H
#define _VX_LIMIT_H

#ifdef	__KERNEL__

struct sysinfo;

void vx_vsi_meminfo(struct sysinfo *);
void vx_vsi_swapinfo(struct sysinfo *);

#define VXD_RLIMIT(r,l)		(VXD_CBIT(limit, (l)) && ((r) == (l)))

#define NUM_LIMITS	20

#define VLIMIT_NSOCK	16

extern const char *vlimit_name[NUM_LIMITS];

#endif	/* __KERNEL__ */
#endif	/* _VX_LIMIT_H */
