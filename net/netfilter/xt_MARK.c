/* This is a module which is used for setting the NFMARK field of an skb. */

/* (C) 1999-2001 Marc Boucher <marc@mbsi.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/checksum.h>
#include <net/route.h>
#include <net/inet_hashtables.h>

#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_MARK.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Marc Boucher <marc@mbsi.ca>");
MODULE_DESCRIPTION("ip[6]tables MARK modification module");
MODULE_ALIAS("ipt_MARK");
MODULE_ALIAS("ip6t_MARK");

static inline u_int16_t
get_dst_port(struct ip_conntrack_tuple *tuple)
{
	switch (tuple->dst.protonum) {
	case IPPROTO_GRE:
		/* XXX Truncate 32-bit GRE key to 16 bits */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)
		return tuple->dst.u.gre.key;
#else
		return htons(ntohl(tuple->dst.u.gre.key));
#endif  
	case IPPROTO_ICMP:
		/* Bind on ICMP echo ID */
		return tuple->src.u.icmp.id;
	case IPPROTO_TCP:
		return tuple->dst.u.tcp.port;
	case IPPROTO_UDP:
		return tuple->dst.u.udp.port;
	default:
		return tuple->dst.u.all;
	}
}

static inline u_int16_t
get_src_port(struct ip_conntrack_tuple *tuple)
{
	switch (tuple->dst.protonum) {
	case IPPROTO_GRE:
		/* XXX Truncate 32-bit GRE key to 16 bits */
		return htons(ntohl(tuple->src.u.gre.key));
	case IPPROTO_ICMP:
		/* Bind on ICMP echo ID */
		return tuple->src.u.icmp.id;
	case IPPROTO_TCP:
		return tuple->src.u.tcp.port;
	case IPPROTO_UDP:
		return tuple->src.u.udp.port;
	default:
		return tuple->src.u.all;
	}
}

static unsigned int
target_v0(struct sk_buff **pskb,
	  const struct net_device *in,
	  const struct net_device *out,
	  unsigned int hooknum,
	  const struct xt_target *target,
	  const void *targinfo)
{
	const struct xt_mark_target_info *markinfo = targinfo;

	if((*pskb)->mark != markinfo->mark)
		(*pskb)->mark = markinfo->mark;

	return XT_CONTINUE;
}

static unsigned int
target_v1(struct sk_buff **pskb,
	  const struct net_device *in,
	  const struct net_device *out,
	  unsigned int hooknum,
	  const struct xt_target *target,
	  const void *targinfo)
{
	const struct xt_mark_target_info_v1 *markinfo = targinfo;
	int mark = -1;

	switch (markinfo->mode) {
	case XT_MARK_SET:
		mark = markinfo->mark;
		break;
		
	case XT_MARK_AND:
		mark = (*pskb)->mark & markinfo->mark;
		break;
		
	case XT_MARK_OR:
		mark = (*pskb)->mark | markinfo->mark;
		break;

	case XT_MARK_COPYXID: 
		{
		enum ip_conntrack_info ctinfo;
		struct sock *connection_sk;
		int dif;

		struct ip_conntrack *ct = ip_conntrack_get((*pskb), &ctinfo);
		extern struct inet_hashinfo tcp_hashinfo;
		enum ip_conntrack_dir dir;

		if (!ct) 
			break;
		dir= CTINFO2DIR(ctinfo);
		u_int32_t src_ip = ct->tuplehash[dir].tuple.src.ip;
		u_int16_t src_port = get_src_port(&ct->tuplehash[dir].tuple);

		u_int32_t ip;
		u_int16_t port;

		dif = ((struct rtable *)(*pskb)->dst)->rt_iif;
		ip = ct->tuplehash[dir].tuple.dst.ip;
		port = get_dst_port(&ct->tuplehash[dir].tuple);

		if ((*pskb)->sk) 
			connection_sk = (*pskb)->sk;
		else {
			connection_sk = inet_lookup(&tcp_hashinfo, src_ip, src_port, ip, port, dif);
		}

		if (connection_sk) {
			/* XXX:
			connection_sk->sk_peercred.gid = connection_sk->sk_peercred.uid = ct->xid[dir];
			ct->xid[!dir]=connection_sk->sk_xid;
			*/
			connection_sk->sk_peercred.gid = connection_sk->sk_peercred.uid = connection_skb->sk_xid;
			if (connection_sk->sk_xid != 0) 
				mark = connection_sk->sk_xid;
			if (connection_sk != (*pskb)->sk)
				sock_put(connection_sk);
		}
		}
		break;
	}

	if((*pskb)->mark != mark && mark != -1)
		(*pskb)->mark = mark;

	return XT_CONTINUE;
}


static int
checkentry_v0(const char *tablename,
	      const void *entry,
	      const struct xt_target *target,
	      void *targinfo,
	      unsigned int hook_mask)
{
	struct xt_mark_target_info *markinfo = targinfo;

	if (markinfo->mark > 0xffffffff) {
		printk(KERN_WARNING "MARK: Only supports 32bit wide mark\n");
		return 0;
	}
	return 1;
}

static int
checkentry_v1(const char *tablename,
	      const void *entry,
	      const struct xt_target *target,
	      void *targinfo,
	      unsigned int hook_mask)
{
	struct xt_mark_target_info_v1 *markinfo = targinfo;

	if (markinfo->mode != XT_MARK_SET
	    && markinfo->mode != XT_MARK_AND
	    && markinfo->mode != XT_MARK_OR
	    && markinfo->mode != XT_MARK_COPYXID) {
		printk(KERN_WARNING "MARK: unknown mode %u\n",
		       markinfo->mode);
		return 0;
	}
	if (markinfo->mark > 0xffffffff) {
		printk(KERN_WARNING "MARK: Only supports 32bit wide mark\n");
		return 0;
	}
	return 1;
}

#ifdef CONFIG_COMPAT
struct compat_xt_mark_target_info_v1 {
	compat_ulong_t	mark;
	u_int8_t	mode;
	u_int8_t	__pad1;
	u_int16_t	__pad2;
};

static void compat_from_user_v1(void *dst, void *src)
{
	struct compat_xt_mark_target_info_v1 *cm = src;
	struct xt_mark_target_info_v1 m = {
		.mark	= cm->mark,
		.mode	= cm->mode,
	};
	memcpy(dst, &m, sizeof(m));
}

static int compat_to_user_v1(void __user *dst, void *src)
{
	struct xt_mark_target_info_v1 *m = src;
	struct compat_xt_mark_target_info_v1 cm = {
		.mark	= m->mark,
		.mode	= m->mode,
	};
	return copy_to_user(dst, &cm, sizeof(cm)) ? -EFAULT : 0;
}
#endif /* CONFIG_COMPAT */

static struct xt_target xt_mark_target[] = {
	{
		.name		= "MARK",
		.family		= AF_INET,
		.revision	= 0,
		.checkentry	= checkentry_v0,
		.target		= target_v0,
		.targetsize	= sizeof(struct xt_mark_target_info),
		.table		= "mangle",
		.me		= THIS_MODULE,
	},
	{
		.name		= "MARK",
		.family		= AF_INET,
		.revision	= 1,
		.checkentry	= checkentry_v1,
		.target		= target_v1,
		.targetsize	= sizeof(struct xt_mark_target_info_v1),
#ifdef CONFIG_COMPAT
		.compatsize	= sizeof(struct compat_xt_mark_target_info_v1),
		.compat_from_user = compat_from_user_v1,
		.compat_to_user	= compat_to_user_v1,
#endif
		.table		= "mangle",
		.me		= THIS_MODULE,
	},
	{
		.name		= "MARK",
		.family		= AF_INET6,
		.revision	= 0,
		.checkentry	= checkentry_v0,
		.target		= target_v0,
		.targetsize	= sizeof(struct xt_mark_target_info),
		.table		= "mangle",
		.me		= THIS_MODULE,
	},
};

static int __init xt_mark_init(void)
{
	return xt_register_targets(xt_mark_target, ARRAY_SIZE(xt_mark_target));
}

static void __exit xt_mark_fini(void)
{
	xt_unregister_targets(xt_mark_target, ARRAY_SIZE(xt_mark_target));
}

module_init(xt_mark_init);
module_exit(xt_mark_fini);
