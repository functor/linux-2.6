#include <linux/kexec.h>
#include <linux/smp_lock.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#ifdef CONFIG_CRASH_DUMP
#include <asm/crash_dump.h>
#endif

extern unsigned long saved_max_pfn;
extern struct memelfnote memelfnote;
extern int notesize(struct memelfnote *);
extern char *storenote(struct memelfnote *, char *);
extern void elf_kcore_store_hdr(char *, int, int, struct kcore_list *);

#ifdef CONFIG_CRASH_DUMP
extern ssize_t copy_oldmem_page(unsigned long, char *, size_t, int);
extern void __crash_machine_kexec(void);
extern int crash_dump_on;
static inline void crash_machine_kexec(void)
{
	 __crash_machine_kexec();
}
#else
#define crash_machine_kexec()	do { } while(0)
#endif


#if defined(CONFIG_CRASH_DUMP) && defined(CONFIG_PROC_FS)
extern void crash_enable_by_proc(void);
extern void crash_create_proc_entry(void);
#else
#define crash_enable_by_proc() do { } while(0)
#define crash_create_proc_entry() do { } while(0)
#endif
