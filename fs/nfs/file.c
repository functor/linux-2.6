/*
 *  linux/fs/nfs/file.c
 *
 *  Copyright (C) 1992  Rick Sladkey
 *
 *  Changes Copyright (C) 1994 by Florian La Roche
 *   - Do not copy data too often around in the kernel.
 *   - In nfs_file_read the return value of kmalloc wasn't checked.
 *   - Put in a better version of read look-ahead buffering. Original idea
 *     and implementation by Wai S Kok elekokws@ee.nus.sg.
 *
 *  Expire cache on write to a file by Wai S Kok (Oct 1994).
 *
 *  Total rewrite of read side for new NFS buffer cache.. Linus.
 *
 *  nfs regular file handling functions
 */

#include <linux/time.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/stat.h>
#include <linux/nfs_fs.h>
#include <linux/nfs_mount.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/smp_lock.h>

#include <asm/uaccess.h>
#include <asm/system.h>

#define NFSDBG_FACILITY		NFSDBG_FILE

static int nfs_file_open(struct inode *, struct file *);
static int nfs_file_release(struct inode *, struct file *);
static int  nfs_file_mmap(struct file *, struct vm_area_struct *);
static ssize_t nfs_file_sendfile(struct file *, loff_t *, size_t, read_actor_t, void *);
static ssize_t nfs_file_read(struct kiocb *, char __user *, size_t, loff_t);
static ssize_t nfs_file_write(struct kiocb *, const char __user *, size_t, loff_t);
static int  nfs_file_flush(struct file *);
static int  nfs_fsync(struct file *, struct dentry *dentry, int datasync);
static int nfs_check_flags(int flags);

struct file_operations nfs_file_operations = {
	.llseek		= remote_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
	.aio_read		= nfs_file_read,
	.aio_write		= nfs_file_write,
	.mmap		= nfs_file_mmap,
	.open		= nfs_file_open,
	.flush		= nfs_file_flush,
	.release	= nfs_file_release,
	.fsync		= nfs_fsync,
	.lock		= nfs_lock,
	.sendfile	= nfs_file_sendfile,
	.check_flags	= nfs_check_flags,
};

struct inode_operations nfs_file_inode_operations = {
	.permission	= nfs_permission,
	.getattr	= nfs_getattr,
	.setattr	= nfs_setattr,
};

/* Hack for future NFS swap support */
#ifndef IS_SWAPFILE
# define IS_SWAPFILE(inode)	(0)
#endif

static int nfs_check_flags(int flags)
{
	if ((flags & (O_APPEND | O_DIRECT)) == (O_APPEND | O_DIRECT))
		return -EINVAL;

	return 0;
}

/*
 * Open file
 */
static int
nfs_file_open(struct inode *inode, struct file *filp)
{
	struct nfs_server *server = NFS_SERVER(inode);
	int (*open)(struct inode *, struct file *);
	int res;

	res = nfs_check_flags(filp->f_flags);
	if (res)
		return res;

	lock_kernel();
	/* Do NFSv4 open() call */
	if ((open = server->rpc_ops->file_open) != NULL)
		res = open(inode, filp);
	unlock_kernel();
	return res;
}

static int
nfs_file_release(struct inode *inode, struct file *filp)
{
	return NFS_PROTO(inode)->file_release(inode, filp);
}

/*
 * Flush all dirty pages, and check for write errors.
 *
 */
static int
nfs_file_flush(struct file *file)
{
	struct inode	*inode = file->f_dentry->d_inode;
	int		status;

	dfprintk(VFS, "nfs: flush(%s/%ld)\n", inode->i_sb->s_id, inode->i_ino);

	if ((file->f_mode & FMODE_WRITE) == 0)
		return 0;
	lock_kernel();
	/* Ensure that data+attribute caches are up to date after close() */
	status = nfs_wb_all(inode);
	if (!status) {
		status = file->f_error;
		file->f_error = 0;
		if (!status)
			__nfs_revalidate_inode(NFS_SERVER(inode), inode);
	}
	unlock_kernel();
	return status;
}

static ssize_t
nfs_file_read(struct kiocb *iocb, char __user * buf, size_t count, loff_t pos)
{
	struct dentry * dentry = iocb->ki_filp->f_dentry;
	struct inode * inode = dentry->d_inode;
	ssize_t result;

#ifdef CONFIG_NFS_DIRECTIO
	if (iocb->ki_filp->f_flags & O_DIRECT)
		return nfs_file_direct_read(iocb, buf, count, pos);
#endif

	dfprintk(VFS, "nfs: read(%s/%s, %lu@%lu)\n",
		dentry->d_parent->d_name.name, dentry->d_name.name,
		(unsigned long) count, (unsigned long) pos);

	result = nfs_revalidate_inode(NFS_SERVER(inode), inode);
	if (!result)
		result = generic_file_aio_read(iocb, buf, count, pos);
	return result;
}

static ssize_t
nfs_file_sendfile(struct file *f
			if (IS_GETLK(cmd))
				status = LOCK_USE_CLNT;
			goto out_ok;
		}
	}

	/*
	 * No BSD flocks over NFS allowed.
	 * Note: we could try to fake a POSIX lock request here by
	 * using ((u32) filp | 0x80000000) or some such as the pid.
	 * Not sure whether that would be unique, though, or whether
	 * that would break in other places.
	 */
	if (!fl->fl_owner || !(fl->fl_flags & FL_POSIX))
		return -ENOLCK;

	/*
	 * Flush all pending writes before doing anything
	 * with locks..
	 */
	status = filemap_fdatawrite(filp->f_mapping);
	down(&inode->i_sem);
	status2 = nfs_wb_all(inode);
	if (!status)
		status = status2;
	up(&inode->i_sem);
	status2 = filemap_fdatawait(filp->f_mapping);
	if (!status)
		status = status2;
	if (status < 0)
		return status;

	lock_kernel();
	status = NFS_PROTO(inode)->lock(filp, cmd, fl);
	unlock_kernel();
	if (status < 0)
		return status;
	
	status = 0;

	/*
	 * Make sure we clear the cache whenever we try to get the lock.
	 * This makes locking act as a cache coherency point.
	 */
 out_ok:
	if ((IS_SETLK(cmd) || IS_SETLKW(cmd)) && fl->fl_type != F_UNLCK) {
		filemap_fdatawrite(filp->f_mapping);
		down(&inode->i_sem);
		nfs_wb_all(inode);      /* we may have slept */
		up(&inode->i_sem);
		filemap_fdatawait(filp->f_mapping);
		nfs_zap_caches(inode);
	}
	return status;
}
