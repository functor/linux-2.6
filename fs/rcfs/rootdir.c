/* 
 * fs/rcfs/rootdir.c 
 *
 * Copyright (C)   Vivek Kashyap,   IBM Corp. 2004
 *           
 * 
 * Functions for creating root directories and magic files 
 * for classtypes and classification engines under rcfs
 *
 * Latest version, more details at http://ckrm.sf.net
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

/*
 * Changes
 *
 * 08 April 2004
 *        Created.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/namespace.h>
#include <linux/dcache.h>
#include <linux/seq_file.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/smp_lock.h>
#include <linux/backing-dev.h>
#include <linux/parser.h>

#include <asm/uaccess.h>

#include <linux/rcfs.h>

rbce_eng_callback_t rcfs_eng_callbacks = {
	NULL, NULL
};

int rcfs_register_engine(rbce_eng_callback_t * rcbs)
{
	if (!rcbs->mkdir || rcfs_eng_callbacks.mkdir) {
		return -EINVAL;
	}
	rcfs_eng_callbacks = *rcbs;
	rcfs_engine_regd++;
	return 0;
}

EXPORT_SYMBOL_GPL(rcfs_register_engine);

int rcfs_unregister_engine(rbce_eng_callback_t * rcbs)
{
	if (!rcbs->mkdir || !rcfs_eng_callbacks.mkdir ||
	    (rcbs->mkdir != rcfs_eng_callbacks.mkdir)) {
		return -EINVAL;
	}
	rcfs_eng_callbacks.mkdir = NULL;
	rcfs_eng_callbacks.rmdir = NULL;
	rcfs_engine_regd--;
	return 0;
}

EXPORT_SYMBOL(rcfs_unregister_engine);

/*
 * rcfs_mkroot
 * Create and return a "root" dentry under /rcfs. 
 * Also create associated magic files 
 *
 * @mfdesc: array of rcfs_magf describing root dir and its magic files
 * @count: number of entries in mfdesc
 * @core:  core class to be associated with root
 * @rootde: output parameter to return the newly created root dentry
 */

int rcfs_mkroot(struct rcfs_magf *mfdesc, int mfcount, struct dentry **rootde)
{
	int sz;
	struct rcfs_magf *rootdesc = &mfdesc[0];
	struct dentry *dentry;
	struct rcfs_inode_info *rootri;

	if ((mfcount < 0) || (!mfdesc))
		return -EINVAL;

	rootdesc = &mfdesc[0];
	printk(KERN_DEBUG "allocating classtype root <%s>\n", rootdesc->name);
	dentry = rcfs_create_internal(rcfs_rootde, rootdesc, 0);

	if (!dentry) {
		printk(KERN_ERR "Could not create %s\n", rootdesc->name);
		return -ENOMEM;
	}
	rootri = RCFS_I(dentry->d_inode);
	sz = strlen(rootdesc->name) + strlen(RCFS_ROOT) + 2;
	rootri->name = kmalloc(sz, GFP_KERNEL);
	if (!rootri->name) {
		printk(KERN_ERR "Error allocating name for %s\n",
		       rootdesc->name);
		rcfs_delete_internal(dentry);
		return -ENOMEM;
	}
	snprintf(rootri->name, sz, "%s/%s", RCFS_ROOT, rootdesc->name);
	if (rootdesc->i_fop)
		dentry->d_inode->i_fop = rootdesc->i_fop;
	if (rootdesc->i_op)
		dentry->d_inode->i_op = rootdesc->i_op;

	/* set output parameters */
	*rootde = dentry;

	return 0;
}

EXPORT_SYMBOL_GPL(rcfs_mkroot);

int rcfs_rmroot(struct dentry *rootde)
{
	struct rcfs_inode_info *ri;

	if (!rootde)
		return -EINVAL;

	rcfs_clear_magic(rootde);
	ri = RCFS_I(rootde->d_inode);
	kfree(ri->name);
	ri->name = NULL;
	rcfs_delete_internal(rootde);
	return 0;
}

EXPORT_SYMBOL_GPL(rcfs_rmroot);

int rcfs_register_classtype(ckrm_classtype_t * clstype)
{
	int rc;
	struct rcfs_inode_info *rootri;
	struct rcfs_magf *mfdesc;

	if (genmfdesc[clstype->mfidx] == NULL) {
		return -ENOMEM;
	}

	clstype->mfdesc = (void *)genmfdesc[clstype->mfidx]->rootmf;
	clstype->mfcount = genmfdesc[clstype->mfidx]->rootmflen;

	mfdesc = (struct rcfs_magf *)clstype->mfdesc;

	/* rcfs root entry has the same name as the classtype */
	strncpy(mfdesc[0].name, clstype->name, RCFS_MAGF_NAMELEN);

	rc = rcfs_mkroot(mfdesc, clstype->mfcount,
			 (struct dentry **)&(clstype->rootde));
	if (rc)
		return rc;
	rootri = RCFS_I(((struct dentry *)(clstype->rootde))->d_inode);
	rootri->core = clstype->default_class;
	clstype->default_class->name = rootri->name;
	ckrm_core_grab(clstype->default_class);

	/* Create magic files under root */
	if ((rc = rcfs_create_magic(clstype->rootde, &mfdesc[1],
				    clstype->mfcount - 1))) {
		kfree(rootri->name);
		rootri->name = NULL;
		rcfs_delete_internal(clstype->rootde);
		return rc;
	}
	return rc;
}

EXPORT_SYMBOL_GPL(rcfs_register_classtype);

int rcfs_deregister_classtype(ckrm_classtype_t * clstype)
{
	int rc;

	rc = rcfs_rmroot((struct dentry *)clstype->rootde);
	if (!rc) {
		clstype->default_class->name = NULL;
		ckrm_core_drop(clstype->default_class);
	}
	return rc;
}

EXPORT_SYMBOL_GPL(rcfs_deregister_classtype);

#ifdef CONFIG_CKRM_TYPE_TASKCLASS
extern struct rcfs_mfdesc tc_mfdesc;
#endif

#ifdef CONFIG_CKRM_TYPE_SOCKETCLASS
extern struct rcfs_mfdesc sock_mfdesc;
#endif

/* Common root and magic file entries.
 * root name, root permissions, magic file names and magic file permissions 
 * are needed by all entities (classtypes and classification engines) existing 
 * under the rcfs mount point
 *
 * The common sets of these attributes are listed here as a table. Individual 
 * classtypes and classification engines can simple specify the index into the 
 * table to initialize their magf entries. 
 */

struct rcfs_mfdesc *genmfdesc[CKRM_MAX_CLASSTYPES] = {
#ifdef CONFIG_CKRM_TYPE_TASKCLASS
	&tc_mfdesc,
#else
	NULL,
#endif
#ifdef CONFIG_CKRM_TYPE_SOCKETCLASS
	&sock_mfdesc,
#else
	NULL,
#endif

};
