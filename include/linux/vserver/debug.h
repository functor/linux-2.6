#ifndef _VX_DEBUG_H
#define _VX_DEBUG_H


extern unsigned int vx_debug_switch;
extern unsigned int vx_debug_xid;
extern unsigned int vx_debug_nid;
extern unsigned int vx_debug_net;
extern unsigned int vx_debug_limit;
extern unsigned int vx_debug_dlim;
extern unsigned int vx_debug_cvirt;


#define	VXD_CBIT(n,m)	(vx_debug_ ## n & (1 << (m)))
#define	VXD_CMIN(n,m)	(vx_debug_ ## n > (m))
#define	VXD_MASK(n,m)	(vx_debug_ ## n & (m))

// #define	VXD_HERE	__FILE__, __LINE__


#ifdef	CONFIG_VSERVER_DEBUG

#define	VX_LOGLEVEL	"vxD: "

#define vxdprintk(c,f,x...)					\
	do {							\
		if (c)						\
			printk(VX_LOGLEVEL f "\n", x);		\
	} while (0)	

#define vxlprintk(c,f,x...)					\
	do {							\
		if (c)						\
			printk(VX_LOGLEVEL f " @%s:%d\n", x);	\
	} while (0)	

#else

#define vxdprintk(x...)	do { } while (0)
#define vxlprintk(x...)	do { } while (0)

#endif



#endif /* _VX_DEBUG_H */
