#if	defined(__KERNEL__) && defined(_VX_INFO_DEF_)

#include <asm/atomic.h>
#include <asm/resource.h>

/* context sub struct */

#define	RLIMIT_OPENFD	12

#define NUM_RLIMITS	16

#define VLIMIT_SOCK	16


struct _vx_limit {
	atomic_t ticks;

	unsigned long rlim[NUM_RLIMITS];	/* Context limit */
	unsigned long rmax[NUM_RLIMITS];	/* Context maximum */
	atomic_t rcur[NUM_RLIMITS];		/* Current value */
	atomic_t lhit[NUM_RLIMITS];		/* Limit hits */
};

static inline void vx_info_init_limit(struct _vx_limit *limit)
{
	int lim;

	for (lim=0; lim<NUM_RLIMITS; lim++) {
		limit->rlim[lim] = RLIM_INFINITY;
		limit->rmax[lim] = 0;
		atomic_set(&limit->rcur[lim], 0);
		atomic_set(&limit->lhit[lim], 0);
	}
}

extern unsigned int vx_debug_limit;

static inline void vx_info_exit_limit(struct _vx_limit *limit)
{
	unsigned long value;
	unsigned int lim;

	if (!vx_debug_limit)
		return;
	for (lim=0; lim<NUM_RLIMITS; lim++) {
		value = atomic_read(&limit->rcur[lim]);
		if (value)
			printk("!!! limit: %p[%d] = %ld on exit.\n",
				limit, lim, value);
	}
}

static inline void vx_limit_fixup(struct _vx_limit *limit)
{
	unsigned long value;
	unsigned int lim;
	
        for (lim=0; lim<NUM_RLIMITS; lim++) {
                value = atomic_read(&limit->rcur[lim]);
                if (value > limit->rmax[lim])
			limit->rmax[lim] = value;
		if (limit->rmax[lim] > limit->rlim[lim])
			limit->rmax[lim] = limit->rlim[lim];
        }
}

#define	VX_LIMIT_FMT	":\t%10d\t%10ld\t%10ld\t%6d\n"

#define	VX_LIMIT_ARG(r)				\
		,atomic_read(&limit->rcur[r])	\
		,limit->rmax[r]			\
		,limit->rlim[r]			\
		,atomic_read(&limit->lhit[r])

static inline int vx_info_proc_limit(struct _vx_limit *limit, char *buffer)
{
	vx_limit_fixup(limit);
	return sprintf(buffer,
		"PROC"	VX_LIMIT_FMT
		"VM"	VX_LIMIT_FMT
		"VML"	VX_LIMIT_FMT
		"RSS"	VX_LIMIT_FMT
		"FILES"	VX_LIMIT_FMT
		"OFD"	VX_LIMIT_FMT
		VX_LIMIT_ARG(RLIMIT_NPROC)
		VX_LIMIT_ARG(RLIMIT_AS)
		VX_LIMIT_ARG(RLIMIT_MEMLOCK)
		VX_LIMIT_ARG(RLIMIT_RSS)
		VX_LIMIT_ARG(RLIMIT_NOFILE)
		VX_LIMIT_ARG(RLIMIT_OPENFD)
		);
}

#else	/* _VX_INFO_DEF_ */
#ifndef _VX_LIMIT_H
#define _VX_LIMIT_H

#include "switch.h"

/*  rlimit vserver commands */

#define VCMD_get_rlimit		VC_CMD(RLIMIT, 1, 0)
#define VCMD_set_rlimit		VC_CMD(RLIMIT, 2, 0)
#define VCMD_get_rlimit_mask	VC_CMD(RLIMIT, 3, 0)

struct  vcmd_ctx_rlimit_v0 {
	uint32_t id;
	uint64_t minimum;
	uint64_t softlimit;
	uint64_t maximum;
};

struct  vcmd_ctx_rlimit_mask_v0 {
	uint32_t minimum;
	uint32_t softlimit;
	uint32_t maximum;
};

#define CRLIM_UNSET		(0ULL)
#define CRLIM_INFINITY		(~0ULL)
#define CRLIM_KEEP		(~1ULL)

#ifdef	__KERNEL__

#include <linux/compiler.h>

extern int vc_get_rlimit(uint32_t, void __user *);
extern int vc_set_rlimit(uint32_t, void __user *);
extern int vc_get_rlimit_mask(uint32_t, void __user *);

struct sysinfo;

void vx_vsi_meminfo(struct sysinfo *);
void vx_vsi_swapinfo(struct sysinfo *);


#endif	/* __KERNEL__ */

#endif	/* _VX_LIMIT_H */
#endif
