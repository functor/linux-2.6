/*
 *	fs/proc/vmcore.c Interface for accessing the crash
 * 				 dump from the system's previous life.
 * 	Heavily borrowed from fs/proc/kcore.c
 *	Created by: Hariprasad Nellitheertha (hari@in.ibm.com)
 *	Copyright (C) IBM Corporation, 2004. All rights reserved
 */

#include <linux/config.h>
#include <linux/mm.h>
#include <linux/proc_fs.h>
#include <linux/user.h>
#include <linux/a.out.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/highmem.h>
#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/crash_dump.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/* This is to re-use the kcore header creation code */
static struct kcore_list vmcore_mem;

static int open_vmcore(struct inode * inode, struct file * filp)
{
	return 0;
}

static ssize_t read_vmcore(struct file *,char __user *,size_t, loff_t *);

#define BACKUP_START CRASH_BACKUP_BASE
#define BACKUP_END CRASH_BACKUP_BASE + CRASH_BACKUP_SIZE
#define REG_SIZE sizeof(elf_gregset_t)

struct file_operations proc_vmcore_operations = {
	.read		= read_vmcore,
	.open		= open_vmcore,
};

struct proc_dir_entry *proc_vmcore;

struct memelfnote
{
	const char *name;
	int type;
	unsigned int datasz;
	void *data;
};

static size_t get_vmcore_size(int *nphdr, size_t *elf_buflen)
{
	size_t size;

	/* We need 1 PT_LOAD segment headers
	 * In addition, we need one PT_NOTE header
	 */
	*nphdr = 2;
	size = (size_t)(saved_max_pfn << PAGE_SHIFT);

	*elf_buflen =	sizeof(struct elfhdr) +
			(*nphdr + 2)*sizeof(struct elf_phdr) +
			3 * sizeof(struct memelfnote) +
			sizeof(struct elf_prstatus) +
			sizeof(struct elf_prpsinfo) +
			sizeof(struct task_struct);
	*elf_buflen = PAGE_ALIGN(*elf_buflen);
	return size + *elf_buflen;
}

/*
 * Reads a page from the oldmem device from given offset.
 */
static ssize_t read_from_oldmem(char *buf, size_t count,
			     loff_t *ppos, int userbuf)
{
	unsigned long pfn;
	size_t read = 0;

	pfn = (unsigned long)(*ppos / PAGE_SIZE);

	if (pfn > saved_max_pfn) {
		read = -EINVAL;
		goto done;
	}

	count = (count > PAGE_SIZE) ? PAGE_SIZE : count;

	if (copy_oldmem_page(pfn, buf, count, userbuf)) {
		read = -EFAULT;
		goto done;
	}

	*ppos += count;
done:
	return read;
}

/*
 * store an ELF crash dump header in the supplied buffer
 * nphdr is the number of elf_phdr to insert
 */
static void elf_vmcore_store_hdr(char *bufp, int nphdr, int dataoff)
{
	struct elf_prstatus prstatus;	/* NT_PRSTATUS */
	struct memelfnote notes[1];
	char reg_buf[REG_SIZE];
	loff_t reg_ppos;
	char *buf = bufp;

	vmcore_mem.addr = (unsigned long)__va(0);
	vmcore_mem.size = saved_max_pfn << PAGE_SHIFT;
	vmcore_mem.next = NULL;

	/* Re-use the kcore code */
	elf_kcore_store_hdr(bufp, nphdr, dataoff, &vmcore_mem);
	buf += sizeof(struct elfhdr) + 2*sizeof(struct elf_phdr);

	/* set up the process status */
	notes[0].name = "CORE";
	notes[0].type = NT_PRSTATUS;
	notes[0].datasz = sizeof(struct elf_prstatus);
	notes[0].data = &prstatus;

	memset(&prstatus, 0, sizeof(struct elf_prstatus));

	/* 1 - Get the registers from the reserved memory area */
	reg_ppos = BACKUP_END + CRASH_RELOCATE_SIZE;
	read_from_oldmem(reg_buf, REG_SIZE, &reg_ppos, 0);
	elf_core_copy_regs(&prstatus.pr_reg, (struct pt_regs *)reg_buf);
	buf = storenote(&notes[0], buf);
}

/*
 * read from the ELF header and then the crash dump
 */
static ssize_t read_vmcore(
struct file *file, char __user *buffer, size_t buflen, loff_t *fpos)
{
	ssize_t acc = 0;
	size_t size, tsz;
	size_t elf_buflen;
	int nphdr;
	unsigned long start;

	tsz =  get_vmcore_size(&nphdr, &elf_buflen);
	proc_vmcore->size = size = tsz + elf_buflen;
	if (buflen == 0 || *fpos >= size) {
		goto done;
	}

	/* trim buflen to not go beyond EOF */
	if (buflen > size - *fpos)
		buflen = size - *fpos;

	/* construct an ELF core header if we'll need some of it */
	if (*fpos < elf_buflen) {
		char * elf_buf;

		tsz = elf_buflen - *fpos;
		if (buflen < tsz)
			tsz = buflen;
		elf_buf = kmalloc(elf_buflen, GFP_ATOMIC);
		if (!elf_buf) {
			acc = -ENOMEM;
			goto done;
		}
		memset(elf_buf, 0, elf_buflen);
		elf_vmcore_store_hdr(elf_buf, nphdr, elf_buflen);
		if (copy_to_user(buffer, elf_buf + *fpos, tsz)) {
			kfree(elf_buf);
			acc = -EFAULT;
			goto done;
		}
		kfree(elf_buf);
		buflen -= tsz;
		*fpos += tsz;
		buffer += tsz;
		acc += tsz;

		/* leave now if filled buffer already */
		if (buflen == 0) {
			goto done;
		}
	}

	start = *fpos - elf_buflen;
	if ((tsz = (PAGE_SIZE - (start & ~PAGE_MASK))) > buflen)
		tsz = buflen;

	while (buflen) {
		unsigned long p;
		loff_t pdup;

		if ((start < 0) || (start >= size))
			if (clear_user(buffer, tsz)) {
				acc = -EFAULT;
				goto done;
			}

		/* tsz contains actual len of dump to be read.
		 * buflen is the total len that was requested.
		 * This may contain part of ELF header. start
		 * is the fpos for the oldmem region
		 * If the file position corresponds to the second
		 * kernel's memory, we just return zeroes
		 */
		p = start;
		if ((p >= BACKUP_START) && (p < BACKUP_END)) {
			if (clear_user(buffer, tsz)) {
				acc = -EFAULT;
				goto done;
			}

			goto read_done;
		} else if (p < CRASH_RELOCATE_SIZE)
			p += BACKUP_END;

		pdup = p;
		if (read_from_oldmem(buffer, tsz, &pdup, 1)) {
			acc = -EINVAL;
			goto done;
		}

read_done:
		buflen -= tsz;
		*fpos += tsz;
		buffer += tsz;
		acc += tsz;
		start += tsz;
		tsz = (buflen > PAGE_SIZE ? PAGE_SIZE : buflen);
	}

done:
	return acc;
}
