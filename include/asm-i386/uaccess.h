#ifndef __i386_UACCESS_H
#define __i386_UACCESS_H

/*
 * User space memory access functions
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/thread_info.h>
#include <linux/prefetch.h>
#include <linux/string.h>
#include <linux/compiler.h>
#include <asm/page.h>

#define VERIFY_READ 0
#define VERIFY_WRITE 1

/*
 * The fs value determines whether argument validity checking should be
 * performed or not.  If get_fs() == USER_DS, checking is performed, with
 * get_fs() == KERNEL_DS, checking is bypassed.
 *
 * For historical reasons, these macros are grossly misnamed.
 */

#define MAKE_MM_SEG(s)	((mm_segment_t) { (s) })


#define KERNEL_DS	MAKE_MM_SEG(0xFFFFFFFFUL)
#define USER_DS		MAKE_MM_SEG(TASK_SIZE)

#define get_ds()	(KERNEL_DS)
#define get_fs()	(current_thread_info()->addr_limit)
#define set_fs(x)	(current_thread_info()->addr_limit = (x))

#define segment_eq(a,b)	((a).seg == (b).seg)

extern long not_a_user_address;
#define check_user_ptr(x) \
	(void) ({ void __user * __userptr = (__typeof__(*(x)) *)&not_a_user_address; __userptr; })

/*
 * movsl can be slow when source and dest are not both 8-byte aligned
 */
#ifdef CONFIG_X86_INTEL_USERCOPY
extern struct movsl_mask {
	int mask;
} ____cacheline_aligned_in_smp movsl_mask;
#endif

#define __addr_ok(addr) ((unsigned long __force)(addr) < (current_thread_info()->addr_limit.seg))

/*
 * Test whether a block of memory is a valid user space address.
 * Returns 0 if the range is valid, nonzero otherwise.
 *
 * This is equivalent to the following test:
 * (u33)addr + (u33)size >= (u33)current->addr_limit.seg
 *
 * This needs 33-bit arithmetic. We have a carry...
 */
#define __range_ok(addr,size) ({ \
	unsigned long flag,sum; \
	check_user_ptr(addr); \
	asm("addl %3,%1 ; sbbl %0,%0; cmpl %1,%4; sbbl $0,%0" \
		:"=&r" (flag), "=r" (sum) \
		:"1" (addr),"g" ((int)(size)),"g" (current_thread_info()->addr_limit.seg)); \
	flag; })

/**
 * access_ok: - Checks if a user space pointer is valid
 * @type: Type of access: %VERIFY_READ or %VERIFY_WRITE.  Note that
 *        %VERIFY_WRITE is a superset of %VERIFY_READ - if it is safe
 *        to write to a block, it is always safe to read from it.
 * @addr: User space pointer to start of block to check
 * @size: Size of block to check
 *
 * Context: User context only.  This function may sleep.
 *
 * Checks if a pointer to a block of memory in user space is valid.
 *
 * Returns true (nonzero) if the memory block may be valid, false (zero)
 * if it is definitely invalid.
 *
 * Note that, depending on architecture, this function probably just
 * checks that the pointer is in the user space range - after calling
 * this function, memory access functions may still return -EFAULT.
 */
#define access_ok(type,addr,size) (likely(__range_ok(addr,size) == 0))

/**
 * verify_area: - Obsolete, use access_ok()
 * @type: Type of access: %VERIFY_READ or %VERIFY_WRITE
 * @addr: User space pointer to start of block to check
 * @size: Size of block to check
 *
 * Context: User context only.  This function may sleep.
 *
 * This function has been replaced by access_ok().
 *
 * Checks if a pointer to a block of memory in user space is valid.
 *
 * Returns zero if the memory block may be valid, -EFAULT
 * if it is definitely invalid.
 *
 * See access_ok() for more details.
 */
static inline int verify_area(int type, const void __user * addr, unsigned long size)
{
	return access_ok(type,addr,size) ? 0 : -EFAULT;
}


/*
 * The exception table consists of pairs of addresses: the first is the
 * address of an instruction that is allowed to fault, and the second is
 * the address at which the program should continue.  No registers are
 * modified, so it is entirely up to the continuation code to figure out
 * what to do.
 *
 * All the routines below use bits of fixup code that are out of line
 * with the main instruction path.  This means when everything is well,
 * we don't even have to jump over them.  Further, they do not intrude
 * on our cache or tlb entries.
 */

struct exception_table_entry
{
	unsigned long insn, fixup;
};

extern int fixup_exception(struct pt_regs *regs);

/*
 * These are the main single-value transfer routines.  They automatically
 * use the right size if we just have the right pointer type.
 *
 * This gets kind of ugly. We want to return _two_ values in "get_user()"
 * and yet we don't want to do any pointers, because that is too much
 * of a performance impact. Thus we have a few rather ugly macros here,
 * and hide all the ugliness from the user.
 *
 * The "__xxx" versions of the user access functions are versions that
 * do not verify the address space, that must have been done previously
 * with a separate "access_ok()" call (this is used when we do multiple
 * accesses to the same area of user memory).
 */

extern void __get_user_1(void);
extern void __get_user_2(void);
extern void __get_user_4(void);

#define __get_user_x(size,ret,x,ptr) \
	__asm__ __volatile__("call __get_user_" #size \
		:"=a" (ret),"=d" (x) \
		:"0" (ptr))

extern int get_user_size(unsigned int size, void *val, const void *ptr);
extern int put_user_size(unsigned int size, const void *val, void *ptr);
extern int zero_user_size(unsigned int size, void *ptr);
extern int copy_str_fromuser_size(unsigned int size, void *val, const void *ptr);
extern int strlen_fromuser_size(unsigned int size, const void *ptr);


# define indirect_get_user(x,ptr)					\
({	int __ret_gu,__val_gu;						\
	__typeof__(ptr) __ptr_gu = (ptr);				\
	__ret_gu = get_user_size(sizeof(*__ptr_gu), &__val_gu,__ptr_gu) ? -EFAULT : 0;\
	(x) = (__typeof__(*__ptr_gu))__val_gu;				\
	__ret_gu;							\
})
#define indirect_put_user(x,ptr)					\
({									\
	__typeof__(*(ptr)) *__ptr_pu = (ptr), __x_pu = (x);		\
	put_user_size(sizeof(*__ptr_pu), &__x_pu, __ptr_pu) ? -EFAULT : 0; \
})
#define __indirect_put_user indirect_put_user
#define __indirect_get_user indirect_get_user

#define indirect_copy_from_user(to,from,n) get_user_size(n,to,from)
#define indirect_copy_to_user(to,from,n) put_user_size(n,from,to)

#define __indirect_copy_from_user indirect_copy_from_user
#define __indirect_copy_to_user indirect_copy_to_user

#define indirect_strncpy_from_user(dst, src, count) \
		copy_str_fromuser_size(count, dst, src)

extern int strlen_fromuser_size(unsigned int size, const void *ptr);
#define indirect_strnlen_user(str, n) strlen_fromuser_size(n, str)
#define indirect_strlen_user(str) indirect_strnlen_user(str, ~0UL >> 1)

extern int zero_user_size(unsigned int size, void *ptr);

#define indirect_clear_user(mem, len) zero_user_size(len, mem)
#define __indirect_clear_user clear_user

/* Careful: we have to cast the result to the type of the pointer for sign reasons */
/**
 * get_user: - Get a simple variable from user space.
 * @x:   Variable to store result.
 * @ptr: Source address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple variable from user space to kernel
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and the result of
 * dereferencing @ptr must be assignable to @x without a cast.
 *
 * Returns zero on success, or -EFAULT on error.
 * On error, the variable @x is set to zero.
 */
#define direct_get_user(x,ptr)						\
({	int __ret_gu,__val_gu;						\
	check_user_ptr(ptr);						\
	switch(sizeof (*(ptr))) {					\
	case 1:  __get_user_x(1,__ret_gu,__val_gu,ptr); break;		\
	case 2:  __get_user_x(2,__ret_gu,__val_gu,ptr); break;		\
	case 4:  __get_user_x(4,__ret_gu,__val_gu,ptr); break;		\
	default: __get_user_x(X,__ret_gu,__val_gu,ptr); break;		\
	}								\
	(x) = (__typeof__(*(ptr)))__val_gu;				\
	__ret_gu;							\
})

extern void __put_user_bad(void);

/**
 * put_user: - Write a simple value into user space.
 * @x:   Value to copy to user space.
 * @ptr: Destination address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple value from kernel space to user
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and @x must be assignable
 * to the result of dereferencing @ptr.
 *
 * Returns zero on success, or -EFAULT on error.
 */
#define direct_put_user(x,ptr)						\
  __put_user_check((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))


/**
 * __get_user: - Get a simple variable from user space, with less checking.
 * @x:   Variable to store result.
 * @ptr: Source address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple variable from user space to kernel
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and the result of
 * dereferencing @ptr must be assignable to @x without a cast.
 *
 * Caller must check the pointer with access_ok() before calling this
 * function.
 *
 * Returns zero on success, or -EFAULT on error.
 * On error, the variable @x is set to zero.
 */
#define __direct_get_user(x,ptr) \
  __get_user_nocheck((x),(ptr),sizeof(*(ptr)))


/**
 * __put_user: - Write a simple value into user space, with less checking.
 * @x:   Value to copy to user space.
 * @ptr: Destination address, in user space.
 *
 * Context: User context only.  This function may sleep.
 *
 * This macro copies a single simple value from kernel space to user
 * space.  It supports simple types like char and int, but not larger
 * data types like structures or arrays.
 *
 * @ptr must have pointer-to-simple-variable type, and @x must be assignable
 * to the result of dereferencing @ptr.
 *
 * Caller must check the pointer with access_ok() before calling this
 * function.
 *
 * Returns zero on success, or -EFAULT on error.
 */
#define __direct_put_user(x,ptr) \
  __put_user_nocheck((__typeof__(*(ptr)))(x),(ptr),sizeof(*(ptr)))

#define __put_user_nocheck(x,ptr,size)				\
({								\
	long __pu_err;						\
	__put_user_size((x),(ptr),(size),__pu_err,-EFAULT);	\
	__pu_err;						\
})


#define __put_user_check(x,ptr,size)					\
({									\
	long __pu_err = -EFAULT;					\
	__typeof__(*(ptr)) *__pu_addr = (ptr);				\
	might_sleep();						\
	if (access_ok(VERIFY_WRITE,__pu_addr,size))			\
		__put_user_size((x),__pu_addr,(size),__pu_err,-EFAULT);	\
	__pu_err;							\
})							

#define __put_user_u64(x, addr, err)				\
	__asm__ __volatile__(					\
		"1:	movl %%eax,0(%2)\n"			\
		"2:	movl %%edx,4(%2)\n"			\
		"3:\n"						\
		".section .fixup,\"ax\"\n"			\
		"4:	movl %3,%0\n"				\
		"	jmp 3b\n"				\
		".previous\n"					\
		".section __ex_table,\"a\"\n"			\
		"	.align 4\n"				\
		"	.long 1b,4b\n"				\
		"	.long 2b,4b\n"				\
		".previous"					\
		: "=r"(err)					\
		: "A" (x), "r" (addr), "i"(-EFAULT), "0"(err))

#ifdef CONFIG_X86_WP_WORKS_OK

#define __put_user_size(x,ptr,size,retval,errret)			\
do {									\
	retval = 0;							\
	check_user_ptr(ptr);						\
	switch (size) {							\
	case 1: __put_user_asm(x,ptr,retval,"b","b","iq",errret);break;	\
	case 2: __put_user_asm(x,ptr,retval,"w","w","ir",errret);break; \
	case 4: __put_user_asm(x,ptr,retval,"l","","ir",errret); break;	\
	case 8: __put_user_u64((__typeof__(*ptr))(x),ptr,retval); break;\
	  default: __put_user_bad();					\
	}								\
} while (0)

#else

#define __put_user_size(x,ptr,size,retval,errret)			\
do {									\
	__typeof__(*(ptr)) __pus_tmp = x;				\
	retval = 0;							\
									\
	if(unlikely(__copy_to_user_ll(ptr, &__pus_tmp, size) != 0))	\
		retval = errret;					\
} while (0)

#endif
struct __large_struct { unsigned long buf[100]; };
#define __m(x) (*(struct __large_struct *)(x))

/*
 * Tell gcc we read from memory instead of writing: this is because
 * we do not write to any memory gcc knows about, so there are no
 * aliasing issues.
 */
#define __put_user_asm(x, addr, err, itype, rtype, ltype, errret)	\
	__asm__ __volatile__(						\
		"1:	mov"itype" %"rtype"1,%2\n"			\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"3:	movl %3,%0\n"					\
		"	jmp 2b\n"					\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		"	.align 4\n"					\
		"	.long 1b,3b\n"					\
		".previous"						\
		: "=r"(err)						\
		: ltype (x), "m"(__m(addr)), "i"(errret), "0"(err))


#define __get_user_nocheck(x,ptr,size)				\
({								\
	long __gu_err, __gu_val;				\
	__get_user_size(__gu_val,(ptr),(size),__gu_err,-EFAULT);\
	(x) = (__typeof__(*(ptr)))__gu_val;			\
	__gu_err;						\
})

extern long __get_user_bad(void);

#define __get_user_size(x,ptr,size,retval,errret)			\
do {									\
	retval = 0;							\
	check_user_ptr(ptr);						\
	switch (size) {							\
	case 1: __get_user_asm(x,ptr,retval,"b","b","=q",errret);break;	\
	case 2: __get_user_asm(x,ptr,retval,"w","w","=r",errret);break;	\
	case 4: __get_user_asm(x,ptr,retval,"l","","=r",errret);break;	\
	default: (x) = __get_user_bad();				\
	}								\
} while (0)

#define __get_user_asm(x, addr, err, itype, rtype, ltype, errret)	\
	__asm__ __volatile__(						\
		"1:	mov"itype" %2,%"rtype"1\n"			\
		"2:\n"							\
		".section .fixup,\"ax\"\n"				\
		"3:	movl %3,%0\n"					\
		"	xor"itype" %"rtype"1,%"rtype"1\n"		\
		"	jmp 2b\n"					\
		".previous\n"						\
		".section __ex_table,\"a\"\n"				\
		"	.align 4\n"					\
		"	.long 1b,3b\n"					\
		".previous"						\
		: "=r"(err), ltype (x)					\
		: "m"(__m(addr)), "i"(errret), "0"(err))


unsigned long __must_check __copy_to_user_ll(void __user *to, const void *from, unsigned long n);
unsigned long __must_check __copy_from_user_ll(void *to, const void __user *from, unsigned long n);

/*
 * Here we special-case 1, 2 and 4-byte copy_*_user invocations.  On a fault
 * we return the initial request size (1, 2 or 4), as copy_*_user should do.
 * If a store crosses a page boundary and gets a fault, the x86 will not write
 * anything, so this is accurate.
 */

/**
 * __copy_to_user: - Copy a block of data into user space, with less checking.
 * @to:   Destination address, in user space.
 * @from: Source address, in kernel space.
 * @n:    Number of bytes to copy.
 *
 * Context: User context only.  This function may sleep.
 *
 * Copy data from kernel space to user space.  Caller must check
 * the specified block with access_ok() before calling this function.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 */
static inline unsigned long __must_check
__direct_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	if (__builtin_constant_p(n)) {
		unsigned long ret;

		switch (n) {
		case 1:
			__put_user_size(*(u8 *)from, (u8 __user *)to, 1, ret, 1);
			return ret;
		case 2:
			__put_user_size(*(u16 *)from, (u16 __user *)to, 2, ret, 2);
			return ret;
		case 4:
			__put_user_size(*(u32 *)from, (u32 __user *)to, 4, ret, 4);
			return ret;
		}
	}
	return __copy_to_user_ll(to, from, n);
}

/**
 * __copy_from_user: - Copy a block of data from user space, with less checking.
 * @to:   Destination address, in kernel space.
 * @from: Source address, in user space.
 * @n:    Number of bytes to copy.
 *
 * Context: User context only.  This function may sleep.
 *
 * Copy data from user space to kernel space.  Caller must check
 * the specified block with access_ok() before calling this function.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 *
 * If some data could not be copied, this function will pad the copied
 * data to the requested size using zero bytes.
 */
static inline unsigned long __must_check
__direct_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	if (__builtin_constant_p(n)) {
		unsigned long ret;

		switch (n) {
		case 1:
			__get_user_size(*(u8 *)to, from, 1, ret, 1);
			return ret;
		case 2:
			__get_user_size(*(u16 *)to, from, 2, ret, 2);
			return ret;
		case 4:
			__get_user_size(*(u32 *)to, from, 4, ret, 4);
			return ret;
		}
	}
	return __copy_from_user_ll(to, from, n);
}

/**
 * copy_to_user: - Copy a block of data into user space.
 * @to:   Destination address, in user space.
 * @from: Source address, in kernel space.
 * @n:    Number of bytes to copy.
 *
 * Context: User context only.  This function may sleep.
 *
 * Copy data from kernel space to user space.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 */
static inline unsigned long __must_check
direct_copy_to_user(void __user *to, const void *from, unsigned long n)
{
	might_sleep();
	if (access_ok(VERIFY_WRITE, to, n))
		n = __direct_copy_to_user(to, from, n);
	return n;
}

/**
 * copy_from_user: - Copy a block of data from user space.
 * @to:   Destination address, in kernel space.
 * @from: Source address, in user space.
 * @n:    Number of bytes to copy.
 *
 * Context: User context only.  This function may sleep.
 *
 * Copy data from user space to kernel space.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero.
 *
 * If some data could not be copied, this function will pad the copied
 * data to the requested size using zero bytes.
 */
static inline unsigned long __must_check
direct_copy_from_user(void *to, const void __user *from, unsigned long n)
{
	might_sleep();
	if (access_ok(VERIFY_READ, from, n))
		n = __direct_copy_from_user(to, from, n);
	else
		memset(to, 0, n);
	return n;
}

long strncpy_from_user(char *dst, const char __user *src, long count);
long __strncpy_from_user(char *dst, const char __user *src, long count);

/**
 * strlen_user: - Get the size of a string in user space.
 * @str: The string to measure.
 *
 * Context: User context only.  This function may sleep.
 *
 * Get the size of a NUL-terminated string in user space.
 *
 * Returns the size of the string INCLUDING the terminating NUL.
 * On exception, returns 0.
 *
 * If there is a limit on the length of a valid string, you may wish to
 * consider using strnlen_user() instead.
 */

long direct_strncpy_from_user(char *dst, const char *src, long count);
long __direct_strncpy_from_user(char *dst, const char *src, long count);
#define direct_strlen_user(str) direct_strnlen_user(str, ~0UL >> 1)
long direct_strnlen_user(const char *str, long n);
unsigned long direct_clear_user(void *mem, unsigned long len);
unsigned long __direct_clear_user(void *mem, unsigned long len);

extern int indirect_uaccess;

#ifdef CONFIG_X86_UACCESS_INDIRECT

/*
 * Return code and zeroing semantics:

 __clear_user          0                      <-> bytes not done
 clear_user            0                      <-> bytes not done
 __copy_to_user        0                      <-> bytes not done
 copy_to_user          0                      <-> bytes not done
 __copy_from_user      0                      <-> bytes not done, zero rest
 copy_from_user        0                      <-> bytes not done, zero rest
 __get_user            0                      <-> -EFAULT
 get_user              0                      <-> -EFAULT
 __put_user            0                      <-> -EFAULT
 put_user              0                      <-> -EFAULT
 strlen_user           strlen + 1             <-> 0
 strnlen_user          strlen + 1 (or n+1)    <-> 0
 strncpy_from_user     strlen (or n)          <-> -EFAULT

 */

#define __clear_user(mem,len) __indirect_clear_user(mem,len)
#define clear_user(mem,len) indirect_clear_user(mem,len)
#define __copy_to_user(to,from,n) __indirect_copy_to_user(to,from,n)
#define copy_to_user(to,from,n) indirect_copy_to_user(to,from,n)
#define __copy_from_user(to,from,n) __indirect_copy_from_user(to,from,n)
#define copy_from_user(to,from,n) indirect_copy_from_user(to,from,n)
#define __get_user(val,ptr) __indirect_get_user(val,ptr)
#define get_user(val,ptr) indirect_get_user(val,ptr)
#define __put_user(val,ptr) __indirect_put_user(val,ptr)
#define put_user(val,ptr) indirect_put_user(val,ptr)
#define strlen_user(str) indirect_strlen_user(str)
#define strnlen_user(src,count) indirect_strnlen_user(src,count)
#define strncpy_from_user(dst,src,count) \
			indirect_strncpy_from_user(dst,src,count)

#else

#define __clear_user __direct_clear_user
#define clear_user direct_clear_user
#define __copy_to_user __direct_copy_to_user
#define copy_to_user direct_copy_to_user
#define __copy_from_user __direct_copy_from_user
#define copy_from_user direct_copy_from_user
#define __get_user __direct_get_user
#define get_user direct_get_user
#define __put_user __direct_put_user
#define put_user direct_put_user
#define strlen_user direct_strlen_user
#define strnlen_user direct_strnlen_user
#define strncpy_from_user direct_strncpy_from_user

#endif /* CONFIG_X86_UACCESS_INDIRECT */

#endif /* __i386_UACCESS_H */
