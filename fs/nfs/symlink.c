/*
 *  linux/fs/nfs/symlink.c
 *
 *  Copyright (C) 1992  Rick Sladkey
 *
 *  Optimization changes Copyright (C) 1994 Florian La Roche
 *
 *  Jun 7 1999, cache symlink lookups in the page cache.  -DaveM
 *
 *  nfs symlink handling code
 */

#define NFS_NEED_XDR_TYPES
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/sunrpc/clnt.h>
#include <linux/nfs.h>
#include <linux/nfs2.h>
#include <linux/nfs_fs.h>
#include <linux/pagemap.h>
#include <linux/stat.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/smp_lock.h>
#include <linux/namei.h>

/* Symlink caching in the page cache is even more simplistic
 * and straight-forward than readdir caching.
 */
static int nfs_symlink_filler(struct inode *inode, struct page *page)
{
	int error;

	/* We place the length at the beginning of the page,
	 * in host byte order, followed by the string.  The
	 * XDR response verification will NULL terminate it.
	 */
	lock_kernel();
	error = NFS_PROTO(inode)->readlink(inode, page);
	unlock_kernel();
	if (error < 0)
		goto error;
	SetPageUptodate(page);
	unlock_page(page);
	return 0;

error:
	SetPageError(page);
	unlock_page(page);
	return -EIO;
}

enum {
	Page_Offset = (PAGE_SIZE - sizeof(void *)) / 4
};

static int nfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	struct inode *inode = dentry->d_inode;
	struct page *page;
	u32 *p;

	page = ERR_PTR(nfs_revalidate_inode(NFS_SERVER(inode), inode));
	if (page)
		goto read_failed;
	page = read_cache_page(&inode->i_data, 0,
				(filler_t *)nfs_symlink_filler, inode);
	if (IS_ERR(page))
		goto read_failed;
	if (!PageUptodate(page))
		goto getlink_read_error;
	p = kmap(page);
	if (*p > Page_Offset * 4 - 1 - 4)
		goto too_long;
	*(struct page **)(p + Page_Offset) = page;

	nd_set_link(nd, (char *)(p+1));
	return 0;

too_long:
	kunmap(page);
	page_cache_release(page);
	page = ERR_PTR(-ENAMETOOLONG);
	goto read_failed;
getlink_read_error:
	page_cache_release(page);
	page = ERR_PTR(-EIO);
read_failed:
	nd_set_link(nd, (char*)page);
	return 0;
}

static void nfs_put_link(struct dentry *dentry, struct nameidata *nd)
{
	u32 *s = (u32 *)nd_get_link(nd);
	if (!IS_ERR(s)) {
		struct page *page = *(struct page **)(s + Page_Offset - 1);
		kunmap(page);
		page_cache_release(page);
	}
}

/*
 * symlinks can't do much...
 */
struct inode_operations nfs_symlink_inode_operations = {
	.readlink	= generic_readlink,
	.follow_link	= nfs_follow_link,
	.put_link	= nfs_put_link,
	.getattr	= nfs_getattr,
	.setattr	= nfs_setattr,
};
