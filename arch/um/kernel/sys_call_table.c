/* 
 * Copyright (C) 2000 Jeff Dike (jdike@karaya.com)
 * Licensed under the GPL
 */

#include "linux/config.h"
#include "linux/unistd.h"
#include "linux/sys.h"
#include "linux/swap.h"
#include "linux/syscalls.h"
#include "linux/sysctl.h"
#include "asm/signal.h"
#include "sysdep/syscalls.h"
#include "kern_util.h"

#ifdef CONFIG_NFSD
#define NFSSERVCTL sys_nfsservctl
#else
#define NFSSERVCTL sys_ni_syscall
#endif

#define LAST_GENERIC_SYSCALL __NR_vserver

#if LAST_GENERIC_SYSCALL > LAST_ARCH_SYSCALL
#define LAST_SYSCALL LAST_GENERIC_SYSCALL
#else
#define LAST_SYSCALL LAST_ARCH_SYSCALL
#endif

extern syscall_handler_t sys_fork;
extern syscall_handler_t sys_execve;
extern syscall_handler_t um_time;
extern syscall_handler_t um_mount;
extern syscall_handler_t um_stime;
extern syscall_handler_t sys_ptrace;
extern syscall_handler_t sys_pipe;
extern syscall_handler_t sys_olduname;
extern syscall_handler_t sys_sigaction;
extern syscall_handler_t sys_sigsuspend;
extern syscall_handler_t old_readdir;
extern syscall_handler_t sys_uname;
extern syscall_handler_t sys_ipc;
extern syscall_handler_t sys_sigreturn;
extern syscall_handler_t sys_clone;
extern syscall_handler_t sys_rt_sigreturn;
extern syscall_handler_t sys_rt_sigaction;
extern syscall_handler_t sys_sigaltstack;
extern syscall_handler_t sys_vfork;
extern syscall_handler_t sys_mmap2;
extern syscall_handler_t sys_timer_create;
extern syscall_handler_t old_mmap_i386;
extern syscall_handler_t old_select;
extern syscall_handler_t sys_modify_ldt;
extern syscall_handler_t sys_rt_sigsuspend;
extern syscall_handler_t sys_vserver;

syscall_handler_t *sys_call_table[] = {
	[ __NR_restart_syscall ] = (syscall_handler_t *) sys_restart_syscall,
	[ __NR_exit ] (syscall_handler_t *) sys_exit,
	[ __NR_fork ] (syscall_handler_t *) sys_fork,
	[ __NR_read ] = (syscall_handler_t *) sys_read,
	[ __NR_write ] = (syscall_handler_t *) sys_write,

	/* These three are declared differently in asm/unistd.h */
	[ __NR_open ] = (syscall_handler_t *) sys_open,
	[ __NR_close ] = (syscall_handler_t *) sys_close,
	[ __NR_waitpid ] = (syscall_handler_t *) sys_waitpid,
	[ __NR_creat ] (syscall_handler_t *) sys_creat,
	[ __NR_link ] (syscall_handler_t *) sys_link,
	[ __NR_unlink ] (syscall_handler_t *) sys_unlink,
	[ __NR_execve ] = (syscall_handler_t *) sys_execve,

	/* declared differently in kern_util.h */
	[ __NR_chdir ] (syscall_handler_t *) sys_chdir,
	[ __NR_time ] = um_time,
	[ __NR_mknod ] (syscall_handler_t *) sys_mknod,
	[ __NR_chmod ] (syscall_handler_t *) sys_chmod,
	[ __NR_lchown ] (syscall_handler_t *) sys_lchown16,
	[ __NR_break ] (syscall_handler_t *) sys_ni_syscall,
	[ __NR_oldstat ] (syscall_handler_t *) sys_stat,
	[ __NR_lseek ] = (syscall_handler_t *) sys_lseek,
	[ __NR_getpid ] (syscall_handler_t *) sys_getpid,
	[ __NR_mount ] = um_mount,
	[ __NR_umount ] (syscall_handler_t *) sys_oldumount,
	[ __NR_setuid ] (syscall_handler_t *) sys_setuid16,
	[ __NR_getuid ] (syscall_handler_t *) sys_getuid16,
	[ __NR_stime ] = um_stime,
	[ __NR_ptrace ] (syscall_handler_t *) sys_ptrace,
	[ __NR_alarm ] (syscall_handler_t *) sys_alarm,
	[ __NR_oldfstat ] (syscall_handler_t *) sys_fstat,
	[ __NR_pause ] (syscall_handler_t *) sys_pause,
	[ __NR_utime ] (syscall_handler_t *) sys_utime,
	[ __NR_stty ] (syscall_handler_t *) sys_ni_syscall,
	[ __NR_gtty ] (syscall_handler_t *) sys_ni_syscall,
	[ __NR_access ] (syscall_handler_t *) sys_access,
	[ __NR_nice ] (syscall_handler_t *) sys_nice,
	[ __NR_ftime ] (syscall_handler_t *) sys_ni_syscall,
	[ __NR_sync ] (syscall_handler_t *) sys_sync,
	[ __NR_kill ] (syscall_handler_t *) sys_kill,
	[ __NR_rename ] (syscall_handler_t *) sys_rename,
	[ __NR_mkdir ] (syscall_handler_t *) sys_mkdir,
	[ __NR_rmdir ] (syscall_handler_t *) sys_rmdir,

	/* Declared differently in asm/unistd.h */
	[ __NR_dup ] = (syscall_handler_t *) sys_dup,
	[ __NR_pipe ] (syscall_handler_t *) sys_pipe,
	[ __NR_times ] (syscall_handler_t *) sys_times,
	[ __NR_prof ] (syscall_handler_t *) sys_ni_syscall,
	[ __NR_brk ] (syscall_handler_t *) sys_brk,
	[ __NR_setgid ] (syscall_handler_t *) sys_setgid1
	[ __NR_getdents64 ] (syscall_handler_t *) sys_getdents64,
	[ __NR_fcntl64 ] (syscall_handler_t *) sys_fcntl64,
	[ 223 ] (syscall_handler_t *) sys_ni_syscall,
	[ __NR_gettid ] (syscall_handler_t *) sys_gettid,
	[ __NR_readahead ] (syscall_handler_t *) sys_readahead,
	[ __NR_setxattr ] (syscall_handler_t *) sys_setxattr,
	[ __NR_lsetxattr ] (syscall_handler_t *) sys_lsetxattr,
	[ __NR_fsetxattr ] (syscall_handler_t *) sys_fsetxattr,
	[ __NR_getxattr ] (syscall_handler_t *) sys_getxattr,
	[ __NR_lgetxattr ] (syscall_handler_t *) sys_lgetxattr,
	[ __NR_fgetxattr ] (syscall_handler_t *) sys_fgetxattr,
	[ __NR_listxattr ] (syscall_handler_t *) sys_listxattr,
	[ __NR_llistxattr ] (syscall_handler_t *) sys_llistxattr,
	[ __NR_flistxattr ] (syscall_handler_t *) sys_flistxattr,
	[ __NR_removexattr ] (syscall_handler_t *) sys_removexattr,
	[ __NR_lremovexattr ] (syscall_handler_t *) sys_lremovexattr,
	[ __NR_fremovexattr ] (syscall_handler_t *) sys_fremovexattr,
	[ __NR_tkill ] (syscall_handler_t *) sys_tkill,
	[ __NR_sendfile64 ] (syscall_handler_t *) sys_sendfile64,
	[ __NR_futex ] (syscall_handler_t *) sys_futex,
	[ __NR_sched_setaffinity ] (syscall_handler_t *) sys_sched_setaffinity,
	[ __NR_sched_getaffinity ] (syscall_handler_t *) sys_sched_getaffinity,
	[ __NR_set_thread_area ] (syscall_handler_t *) sys_ni_syscall,
	[ __NR_get_thread_area ] (syscall_handler_t *) sys_ni_syscall,
	[ __NR_io_setup ] (syscall_handler_t *) sys_io_setup,
	[ __NR_io_destroy ] (syscall_handler_t *) sys_io_destroy,
	[ __NR_io_getevents ] (syscall_handler_t *) sys_io_getevents,
	[ __NR_io_submit ] (syscall_handler_t *) sys_io_submit,
	[ __NR_io_cancel ] (syscall_handler_t *) sys_io_cancel,
	[ __NR_fadvise64 ] (syscall_handler_t *) sys_fadvise64,
	[ 251 ] (syscall_handler_t *) sys_ni_syscall,
	[ __NR_exit_group ] (syscall_handler_t *) sys_exit_group,
	[ __NR_lookup_dcookie ] (syscall_handler_t *) sys_lookup_dcookie,
	[ __NR_epoll_create ] (syscall_handler_t *) sys_epoll_create,
	[ __NR_epoll_ctl ] (syscall_handler_t *) sys_epoll_ctl,
	[ __NR_epoll_wait ] (syscall_handler_t *) sys_epoll_wait,
        [ __NR_remap_file_pages ] (syscall_handler_t *) sys_remap_file_pages,
        [ __NR_set_tid_address ] (syscall_handler_t *) sys_set_tid_address,
	[ __NR_timer_create ] (syscall_handler_t *) sys_timer_create,
	[ __NR_timer_settime ] (syscall_handler_t *) sys_timer_settime,
	[ __NR_timer_gettime ] (syscall_handler_t *) sys_timer_gettime,
	[ __NR_timer_getoverrun ] (syscall_handler_t *) sys_timer_getoverrun,
	[ __NR_timer_delete ] (syscall_handler_t *) sys_timer_delete,
	[ __NR_clock_settime ] (syscall_handler_t *) sys_clock_settime,
	[ __NR_clock_gettime ] (syscall_handler_t *) sys_clock_gettime,
	[ __NR_clock_getres ] (syscall_handler_t *) sys_clock_getres,
	[ __NR_clock_nanosleep ] (syscall_handler_t *) sys_clock_nanosleep,
	[ __NR_statfs64 ] (syscall_handler_t *) sys_statfs64,
	[ __NR_fstatfs64 ] (syscall_handler_t *) sys_fstatfs64,
	[ __NR_tgkill ] (syscall_handler_t *) sys_tgkill,
	[ __NR_utimes ] (syscall_handler_t *) sys_utimes,
	[ __NR_fadvise64_64 ] (syscall_handler_t *) sys_fadvise64_64,
	[ __NR_vserver ] (syscall_handler_t *) sys_vserver,

	ARCH_SYSCALLS
	[ LAST_SYSCALL + 1 ... NR_syscalls ] = 
	        (syscall_handler_t *) sys_ni_syscall
};

/*
 * Overrides for Emacs so that we follow Linus's tabbing style.
 * Emacs will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
