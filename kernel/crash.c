/*
 *	kernel/crash.c - Memory preserving reboot related code.
 *
 *	Created by: Hariprasad Nellitheertha (hari@in.ibm.com)
 *	Copyright (C) IBM Corporation, 2004. All rights reserved
 */

#include <linux/smp_lock.h>
#include <linux/kexec.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/bootmem.h>
#include <linux/highmem.h>
#include <linux/crash_dump.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#ifdef CONFIG_PROC_FS
/*
 * Enable kexec reboot upon panic; for dumping
 */
static ssize_t write_crash_dump_on(struct file *file, const char __user *buf,
					size_t count, loff_t *ppos)
{
	if (count) {
		if (get_user(crash_dump_on, buf))
			return -EFAULT;
	}
	return count;
}

static struct file_operations proc_crash_dump_on_operations = {
	.write = write_crash_dump_on,
};

extern struct file_operations proc_vmcore_operations;
extern struct proc_dir_entry *proc_vmcore;

void crash_enable_by_proc(void)
{
	struct proc_dir_entry *entry;

	entry = create_proc_entry("kexec-dump", S_IWUSR, NULL);
	if (entry)
		entry->proc_fops = &proc_crash_dump_on_operations;
}

void crash_create_proc_entry(void)
{
	if (dump_enabled) {
		proc_vmcore = create_proc_entry("vmcore", S_IRUSR, NULL);
		if (proc_vmcore) {
			proc_vmcore->proc_fops = &proc_vmcore_operations;
			proc_vmcore->size =
			(size_t)(saved_max_pfn << PAGE_SHIFT);
		}
	}
}

#endif /* CONFIG_PROC_FS */

void __crash_machine_kexec(void)
{
	struct kimage *image;

	if ((!crash_dump_on) || (crashed))
		return;

	image = xchg(&kexec_crash_image, 0);
	if (image) {
		crashed = 1;
		printk(KERN_EMERG "kexec: opening parachute\n");
		crash_dump_stop_cpus();
		crash_dump_save_registers();

	/* If we are here to do a crash dump, save the memory from
	 * 0-640k before we copy over the kexec kernel image.  Otherwise
	 * our dump will show the wrong kernel entirely.
	 */
		crash_relocate_mem();

		machine_kexec(image);
	} else {
		printk(KERN_EMERG "kexec: No kernel image loaded!\n");
	}
}

/*
 * Copy a page from "oldmem". For this page, there is no pte mapped
 * in the current kernel. We stitch up a pte, similar to kmap_atomic.
 */
ssize_t copy_oldmem_page(unsigned long pfn, char *buf,
				size_t csize, int userbuf)
{
	void *page, *vaddr;

	if (!csize)
		return 0;

	page = kmalloc(PAGE_SIZE, GFP_KERNEL);

	vaddr = kmap_atomic_pfn(pfn, KM_PTE0);
	copy_page(page, vaddr);
	kunmap_atomic(vaddr, KM_PTE0);

	if (userbuf) {
		if (copy_to_user(buf, page, csize)) {
			kfree(page);
			return -EFAULT;
		}
	} else
		memcpy(buf, page, csize);
	kfree(page);

	return 0;
}
