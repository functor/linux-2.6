/* _VX_CVIRT_H defined below */

#if	defined(__KERNEL__) && defined(_VX_INFO_DEF_)

#include <linux/utsname.h>
#include <linux/rwsem.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <asm/atomic.h>

/* context sub struct */

struct _vx_cvirt {
	int max_threads;

	struct timespec bias_idle;
	uint64_t bias_jiffies;

	struct new_utsname utsname;
};

struct sock_acc {
	atomic_t count;
	atomic_t total;
};

struct _vx_cacct {
	atomic_t nr_threads;
	int nr_running;

	unsigned long total_forks;

	struct sock_acc sock[5][3];
};


static inline long vx_sock_count(struct _vx_cacct *cacct, int type, int pos)
{
	return atomic_read(&cacct->sock[type][pos].count);
}


static inline long vx_sock_total(struct _vx_cacct *cacct, int type, int pos)
{
	return atomic_read(&cacct->sock[type][pos].total);
}


extern uint64_t vx_idle_jiffies(void);

static inline void vx_info_init_cvirt(struct _vx_cvirt *cvirt)
{
	uint64_t idle_jiffies = vx_idle_jiffies();

	cvirt->bias_jiffies = get_jiffies_64();
	jiffies_to_timespec(idle_jiffies, &cvirt->bias_idle);

	down_read(&uts_sem);
	cvirt->utsname = system_utsname;
	up_read(&uts_sem);
}

static inline void vx_info_exit_cvirt(struct _vx_cvirt *cvirt)
{
	return;
}

static inline void vx_info_init_cacct(struct _vx_cacct *cacct)
{
	int i,j;

	atomic_set(&cacct->nr_threads, 1);
	for (i=0; i<5; i++) {
		for (j=0; j<3; j++) {
			atomic_set(&cacct->sock[i][j].count, 0);
			atomic_set(&cacct->sock[i][j].total, 0);
		}
	}
}

static inline void vx_info_exit_cacct(struct _vx_cacct *cacct)
{
	return;
}

static inline int vx_info_proc_cvirt(struct _vx_cvirt *cvirt, char *buffer)
{
	int length = 0;
	length += sprintf(buffer + length,
		"BiasJiffies:\t%lld\n", (long long int)cvirt->bias_jiffies);
	length += sprintf(buffer + length,
		"SysName:\t%.*s\n"
		"NodeName:\t%.*s\n"
		"Release:\t%.*s\n"
		"Version:\t%.*s\n"
		"Machine:\t%.*s\n"
		"DomainName:\t%.*s\n"
		,__NEW_UTS_LEN, cvirt->utsname.sysname
		,__NEW_UTS_LEN, cvirt->utsname.nodename
		,__NEW_UTS_LEN, cvirt->utsname.release
		,__NEW_UTS_LEN, cvirt->utsname.version
		,__NEW_UTS_LEN, cvirt->utsname.machine
		,__NEW_UTS_LEN, cvirt->utsname.domainname
		);
	return length;
}

static inline int vx_info_proc_cacct(struct _vx_cacct *cacct, char *buffer)
{
	int i,j, length = 0;
	static char *type[] = { "UNSPEC", "UNIX", "INET", "INET6", "OTHER" };

	for (i=0; i<5; i++) {
		length += sprintf(buffer + length,
			"%s:", type[i]);
		for (j=0; j<3; j++) {
			length += sprintf(buffer + length,
				"\t%12lu/%-12lu"
				,vx_sock_count(cacct, i, j)
				,vx_sock_total(cacct, i, j)
				);
		}	
		buffer[length++] = '\n';
	}
	return length;
}

#else	/* _VX_INFO_DEF_ */
#ifndef _VX_CVIRT_H
#define _VX_CVIRT_H

#include "switch.h"

/*  cvirt vserver commands */


#ifdef	__KERNEL__

struct timespec;

void vx_vsi_uptime(struct timespec *uptime, struct timespec *idle);

#endif	/* __KERNEL__ */

#endif	/* _VX_CVIRT_H */
#endif
