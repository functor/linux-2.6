#ifndef _VX_VS_DLIMIT_H
#define _VX_VS_DLIMIT_H


// #define VX_DEBUG

#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>

#include "vserver/context.h"
#include "vserver/dlimit.h"

#if defined(VX_DEBUG)
#define vxdprintk(x...) printk("vxd: " x)
#else
#define vxdprintk(x...)
#endif


#define get_dl_info(i)	__get_dl_info(i,__FILE__,__LINE__)

static inline struct dl_info *__get_dl_info(struct dl_info *dli,
	const char *_file, int _line)
{
	if (!dli)
		return NULL;
	vxdprintk("get_dl_info(%p[#%d.%d])\t%s:%d\n",
		dli, dli?dli->dl_xid:0, dli?atomic_read(&dli->dl_usecnt):0,
		_file, _line);
	atomic_inc(&dli->dl_usecnt);
	return dli;
}


#define	free_dl_info(i)	\
	call_rcu(&i->dl_rcu, rcu_free_dl_info);

#define put_dl_info(i)	__put_dl_info(i,__FILE__,__LINE__)

static inline void __put_dl_info(struct dl_info *dli, const char *_file, int _line)
{
	if (!dli)
		return;
	vxdprintk("put_dl_info(%p[#%d.%d])\t%s:%d\n",
		dli, dli?dli->dl_xid:0, dli?atomic_read(&dli->dl_usecnt):0,
		_file, _line);
	if (atomic_dec_and_test(&dli->dl_usecnt))
		free_dl_info(dli);
}


extern int vx_debug_dlimit;

#define	__dlimit_char(d)	((d)?'*':' ')

static inline int __dl_alloc_space(struct super_block *sb,
	xid_t xid, dlsize_t nr, const char *file, int line)
{
	struct dl_info *dli = NULL;
	int ret = 0;

	if (nr == 0)
		goto out;
	dli = locate_dl_info(sb, xid);
	if (!dli)
		goto out;

	spin_lock(&dli->dl_lock);
	ret = (dli->dl_space_used + nr > dli->dl_space_total);
	if (!ret)
		dli->dl_space_used += nr;
	spin_unlock(&dli->dl_lock);
	put_dl_info(dli);
out:
	if (vx_debug_dlimit)
		printk("ALLOC (%p,#%d)%c %lld bytes (%d)@ %s:%d\n",
			sb, xid, __dlimit_char(dli), nr, ret, file, line);
	return ret;
}

static inline void __dl_free_space(struct super_block *sb,
	xid_t xid, dlsize_t nr, const char *file, int line)
{
	struct dl_info *dli = NULL;

	if (nr == 0)
		goto out;
	dli = locate_dl_info(sb, xid);
	if (!dli)
		goto out;

	spin_lock(&dli->dl_lock);
	dli->dl_space_used -= nr;
	spin_unlock(&dli->dl_lock);
	put_dl_info(dli);
out:
	if (vx_debug_dlimit)
		printk("FREE  (%p,#%d)%c %lld bytes @ %s:%d\n",
			sb, xid, __dlimit_char(dli), nr, file, line);
}

static inline int __dl_alloc_inode(struct super_block *sb,
	xid_t xid, const char *file, int line)
{
	struct dl_info *dli;
	int ret = 0;

	dli = locate_dl_info(sb, xid);
	if (!dli)
		goto out;

	spin_lock(&dli->dl_lock);
	ret = (dli->dl_inodes_used >= dli->dl_inodes_total);
	if (!ret)
		dli->dl_inodes_used++;
	spin_unlock(&dli->dl_lock);
	put_dl_info(dli);
out:
	if (vx_debug_dlimit)
		printk("ALLOC (%p,#%d)%c inode (%d)@ %s:%d\n",
			sb, xid, __dlimit_char(dli), ret, file, line);
	return ret;
}

static inline void __dl_free_inode(struct super_block *sb,
	xid_t xid, const char *file, int line)
{
	struct dl_info *dli;

	dli = locate_dl_info(sb, xid);
	if (!dli)
		goto out;

	spin_lock(&dli->dl_lock);
	dli->dl_inodes_used--;
	spin_unlock(&dli->dl_lock);
	put_dl_info(dli);
out:
	if (vx_debug_dlimit)
		printk("FREE  (%p,#%d)%c inode @ %s:%d\n",
			sb, xid, __dlimit_char(dli), file, line);
}



#define DLIMIT_ALLOC_BLOCK(sb, xid, nr) \
	__dl_alloc_space(sb, xid, \
		((dlsize_t)(nr)) << (sb)->s_blocksize_bits, \
		__FILE__, __LINE__ )

#define DLIMIT_FREE_BLOCK(sb, xid, nr) \
	__dl_free_space(sb, xid, \
		((dlsize_t)(nr)) << (sb)->s_blocksize_bits, \
		__FILE__, __LINE__ )

#define DLIMIT_ALLOC_INODE(sb, xid) \
	__dl_alloc_inode(sb, xid, __FILE__, __LINE__ )

#define DLIMIT_FREE_INODE(sb, xid) \
	__dl_free_inode(sb, xid, __FILE__, __LINE__ )


#define	DLIMIT_ADJUST_BLOCK(sb, xid, fb, rb)


#else
#warning duplicate inclusion
#endif
