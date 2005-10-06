/*
 * Implements the dump driver interface for saving a dump via network
 * interface. 
 *
 * Some of this code has been taken/adapted from Ingo Molnar's netconsole
 * code. LKCD team expresses its thanks to Ingo.
 *
 * Started: June 2002 - Mohamed Abbas <mohamed.abbas@intel.com>
 * 	Adapted netconsole code to implement LKCD dump over the network.
 *
 * Nov 2002 - Bharata B. Rao <bharata@in.ibm.com>
 * 	Innumerable code cleanups, simplification and some fixes.
 *	Netdump configuration done by ioctl instead of using module parameters.
 *
 * Copyright (C) 2001  Ingo Molnar <mingo@redhat.com>
 * Copyright (C) 2002 International Business Machines Corp. 
 *
 *  This code is released under version 2 of the GNU GPL.
 */

#include <net/tcp.h>
#include <net/udp.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/reboot.h>
#include <linux/module.h>
#include <linux/dump.h>
#include <linux/dump_netdev.h>
#include <linux/percpu.h>

#include <asm/unaligned.h>

static int startup_handshake;
static int page_counter;
static struct net_device *dump_ndev;
static struct in_device *dump_in_dev;
static u16 source_port, target_port;
static u32 source_ip, target_ip;
static unsigned char daddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} ;
static spinlock_t dump_skb_lock = SPIN_LOCK_UNLOCKED;
static int dump_nr_skbs;
static struct sk_buff *dump_skb;
static unsigned long flags_global;
static int netdump_in_progress;
static char device_name[IFNAMSIZ];

/*
 * security depends on the trusted path between the netconsole
 * server and netconsole client, since none of the packets are
 * encrypted. The random magic number protects the protocol
 * against spoofing.
 */
static u64 dump_magic;

#define MAX_UDP_CHUNK 1460
#define MAX_PRINT_CHUNK (MAX_UDP_CHUNK-HEADER_LEN)

/*
 * We maintain a small pool of fully-sized skbs,
 * to make sure the message gets out even in
 * extreme OOM situations.
 */
#define DUMP_MAX_SKBS 32

#define MAX_SKB_SIZE \
		(MAX_UDP_CHUNK + sizeof(struct udphdr) + \
				sizeof(struct iphdr) + sizeof(struct ethhdr))

static void
dump_refill_skbs(void)
{
	struct sk_buff *skb;
	unsigned long flags;

	spin_lock_irqsave(&dump_skb_lock, flags);
	while (dump_nr_skbs < DUMP_MAX_SKBS) {
		skb = alloc_skb(MAX_SKB_SIZE, GFP_ATOMIC);
		if (!skb)
			break;
		if (dump_skb)
			skb->next = dump_skb;
		else
			skb->next = NULL;
		dump_skb = skb;
		dump_nr_skbs++;
	}
	spin_unlock_irqrestore(&dump_skb_lock, flags);
}

static struct
sk_buff * dump_get_skb(void)
{
	struct sk_buff *skb;
	unsigned long flags;

	spin_lock_irqsave(&dump_skb_lock, flags);
	skb = dump_skb;
	if (skb) {
		dump_skb = skb->next;
		skb->next = NULL;
		dump_nr_skbs--;
	}
	spin_unlock_irqrestore(&dump_skb_lock, flags);
        
	return skb;
}

/*
 * Zap completed output skbs.
 */
static void
zap_completion_queue(void)
{
	int count;
	unsigned long flags;
	struct softnet_data *sd;

        count=0;
	sd = &__get_cpu_var(softnet_data);
	if (sd->completion_queue) {
		struct sk_buff *clist;
	
		local_irq_save(flags);
		clist = sd->completion_queue;
		sd->completion_queue = NULL;
		local_irq_restore(flags);

		while (clist != NULL) {
			struct sk_buff *skb = clist;
			clist = clist->next;
			__kfree_skb(skb);
			count++;
			if (count > 10000)
				printk("Error in sk list\n");
		}
	}
}

static void
dump_send_skb(struct net_device *dev, const char *msg, unsigned int msg_len,
		reply_t *reply)
{
	int once = 1;
	int total_len, eth_len, ip_len, udp_len, count = 0;
	struct sk_buff *skb;
	struct udphdr *udph;
	struct iphdr *iph;
	struct ethhdr *eth; 

	udp_len = msg_len + HEADER_LEN + sizeof(*udph);
	ip_len = eth_len = udp_len + sizeof(*iph);
	total_len = eth_len + ETH_HLEN;

repeat_loop:
	zap_completion_queue();
	if (dump_nr_skbs < DUMP_MAX_SKBS)
		dump_refill_skbs();

	skb = alloc_skb(total_len, GFP_ATOMIC);
	if (!skb) {
		skb = dump_get_skb();
		if (!skb) {
			count++;
			if (once && (count == 1000000)) {
				printk("possibly FATAL: out of netconsole "
					"skbs!!! will keep retrying.\n");
				once = 0;
			}
			dev->poll_controller(dev);
			goto repeat_loop;
		}
	}

	atomic_set(&skb->users, 1);
	skb_reserve(skb, total_len - msg_len - HEADER_LEN);
	skb->data[0] = NETCONSOLE_VERSION;

	put_unaligned(htonl(reply->nr), (u32 *) (skb->data + 1));
	put_unaligned(htonl(reply->code), (u32 *) (skb->data + 5));
	put_unaligned(htonl(reply->info), (u32 *) (skb->data + 9));

	memcpy(skb->data + HEADER_LEN, msg, msg_len);
	skb->len += msg_len + HEADER_LEN;

	udph = (struct udphdr *) skb_push(skb, sizeof(*udph));
	udph->source = source_port;
	udph->dest = target_port;
	udph->len = htons(udp_len);
	udph->check = 0;

	iph = (struct iphdr *)skb_push(skb, sizeof(*iph));

	iph->version  = 4;
	iph->ihl      = 5;
	iph->tos      = 0;
	iph->tot_len  = htons(ip_len);
	iph->id       = 0;
	iph->frag_off = 0;
	iph->ttl      = 64;
	iph->protocol = IPPROTO_UDP;
	iph->check    = 0;
	iph->saddr    = source_ip;
	iph->daddr    = target_ip;
	iph->check    = ip_fast_csum((unsigned char *)iph, iph->ihl);

	eth = (struct ethhdr *) skb_push(skb, ETH_HLEN);

	eth->h_proto = htons(ETH_P_IP);
	memcpy(eth->h_source, dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest, daddr, dev->addr_len);

	count=0;
repeat_poll:
	spin_lock(&dev->xmit_lock);
	dev->xmit_lock_owner = smp_processor_id();

	count++;


	if (netif_queue_stopped(dev)) {
		dev->xmit_lock_owner = -1;
		spin_unlock(&dev->xmit_lock);

		dev->poll_controller(dev);
		zap_completion_queue();


		goto repeat_poll;
	}

	dev->hard_start_xmit(skb, dev);

	dev->xmit_lock_owner = -1;
	spin_unlock(&dev->xmit_lock);
}

static unsigned short
udp_check(struct udphdr *uh, int len, unsigned long saddr, unsigned long daddr,
	       	unsigned long base)
{
	return csum_tcpudp_magic(saddr, daddr, len, IPPROTO_UDP, base);
}

static int
udp_checksum_init(struct sk_buff *skb, struct udphdr *uh,
			     unsigned short ulen, u32 saddr, u32 daddr)
{
	if (uh->check == 0) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else if (skb->ip_summed == CHECKSUM_HW) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		if (!udp_check(uh, ulen, saddr, daddr, skb->csum))
			return 0;
		skb->ip_summed = CHECKSUM_NONE;
	}
	if (skb->ip_summed != CHECKSUM_UNNECESSARY)
		skb->csum = csum_tcpudp_nofold(saddr, daddr, ulen,
				IPPROTO_UDP, 0);
	/* Probably, we should checksum udp header (it should be in cache
	 * in any case) and data in tiny packets (< rx copybreak).
	 */
	return 0;
}

static __inline__ int
__udp_checksum_complete(struct sk_buff *skb)
{
	return (unsigned short)csum_fold(skb_checksum(skb, 0, skb->len,
				skb->csum));
}

static __inline__
int udp_checksum_complete(struct sk_buff *skb)
{
	return skb->ip_summed != CHECKSUM_UNNECESSARY &&
		__udp_checksum_complete(skb);
}

int new_req = 0;
static req_t req;

static int
dump_rx_hook(struct sk_buff *skb)
{
	int proto;
	struct iphdr *iph;
	struct udphdr *uh;
	__u32 len, saddr, daddr, ulen;
	req_t *__req;

	/* 
	 * First check if were are dumping or doing startup handshake, if
	 * not quickly return.
	 */
	if (!netdump_in_progress)
		return NET_RX_SUCCESS;

	if (skb->dev->type != ARPHRD_ETHER)
		goto out;

	proto = ntohs(skb->mac.ethernet->h_proto);
	if (proto != ETH_P_IP)
		goto out;

	if (skb->pkt_type == PACKET_OTHERHOST)
		goto out;

	if (skb_shared(skb))
		goto out;

	 /* IP header correctness testing: */
	iph = (struct iphdr *)skb->data;
	if (!pskb_may_pull(skb, sizeof(struct iphdr)))
		goto out;

	if (iph->ihl < 5 || iph->version != 4)
		goto out;

	if (!pskb_may_pull(skb, iph->ihl*4))
		goto out;

	if (ip_fast_csum((u8 *)iph, iph->ihl) != 0)
		goto out;

	len = ntohs(iph->tot_len);
	if (skb->len < len || len < iph->ihl*4)
		goto out;

	saddr = iph->saddr;
	daddr = iph->daddr;
	if (iph->protocol != IPPROTO_UDP)
		goto out;

	if (source_ip != daddr)
		goto out;

	if (target_ip != saddr)
		goto out;

	len -= iph->ihl*4;
	uh = (struct udphdr *)(((char *)iph) + iph->ihl*4);
	ulen = ntohs(uh->len);

	if (ulen != len || ulen < (sizeof(*uh) + sizeof(*__req)))
		goto out;

	if (udp_checksum_init(skb, uh, ulen, saddr, daddr) < 0)
		goto out;

	if (udp_checksum_complete(skb))
		goto out;

	if (source_port != uh->dest)
		goto out;

	if (target_port != uh->source)
		goto out;

	__req = (req_t *)(uh + 1);
	if ((ntohl(__req->command) != COMM_GET_MAGIC) &&
	    (ntohl(__req->command) != COMM_HELLO) &&
	    (ntohl(__req->command) != COMM_START_WRITE_NETDUMP_ACK) &&
	    (ntohl(__req->command) != COMM_START_NETDUMP_ACK) &&
	    (memcmp(&__req->magic, &dump_magic, sizeof(dump_magic)) != 0))
		goto out;

	req.magic = ntohl(__req->magic);
	req.command = ntohl(__req->command);
	req.from = ntohl(__req->from);
	req.to = ntohl(__req->to);
	req.nr = ntohl(__req->nr);
	new_req = 1;
out:
	return NET_RX_DROP;
}

static void
dump_send_mem(struct net_device *dev, req_t *req, const char* buff, size_t len)
{
	int i;

	int nr_chunks = len/1024;
	reply_t reply;
	
	reply.nr = req->nr;
	reply.info = 0;

        if ( nr_chunks <= 0)
		 nr_chunks = 1;
	for (i = 0; i < nr_chunks; i++) {
		unsigned int offset = i*1024;
		reply.code = REPLY_MEM;
		reply.info = offset;
                dump_send_skb(dev, buff + offset, 1024, &reply);
	}
}

/*
 * This function waits for the client to acknowledge the receipt
 * of the netdump startup reply, with the possibility of packets
 * getting lost. We resend the startup packet if no ACK is received,
 * after a 1 second delay.
 *
 * (The client can test the success of the handshake via the HELLO
 * command, and send ACKs until we enter netdump mode.)
 */
static int
dump_handshake(struct dump_dev *net_dev)
{
	char tmp[200];
	reply_t reply;
	int i, j;

	if (startup_handshake) {
		sprintf(tmp, "NETDUMP start, waiting for start-ACK.\n");
		reply.code = REPLY_START_NETDUMP;
		reply.nr = 0;
		reply.info = 0;
	} else {
		sprintf(tmp, "NETDUMP start, waiting for start-ACK.\n");
		reply.code = REPLY_START_WRITE_NETDUMP;
		reply.nr = net_dev->curr_offset;
		reply.info = net_dev->curr_offset;
	}
	
	/* send 300 handshake packets before declaring failure */
	for (i = 0; i < 300; i++) {
		dump_send_skb(dump_ndev, tmp, strlen(tmp), &reply);

		/* wait 1 sec */
		for (j = 0; j < 10000; j++) {
			udelay(100);
			dump_ndev->poll_controller(dump_ndev);
			zap_completion_queue();
			if (new_req)
				break;
		}

		/* 
		 * if there is no new request, try sending the handshaking
		 * packet again
		 */
		if (!new_req)
			continue;

		/* 
		 * check if the new request is of the expected type,
		 * if so, return, else try sending the handshaking
		 * packet again
		 */
		if (startup_handshake) {
			if (req.command == COMM_HELLO || req.command ==
				COMM_START_NETDUMP_ACK) {
				return 0;
			} else {
				new_req = 0;
				continue;
			}
		} else {
			if (req.command == COMM_SEND_MEM) {
				return 0;
			} else {
				new_req = 0;
				continue;
			}
		}
	}
	return -1;
}

static ssize_t
do_netdump(struct dump_dev *net_dev, const char* buff, size_t len)
{
	reply_t reply;
	char tmp[200];
	ssize_t  ret = 0;
	int repeatCounter, counter, total_loop;
	
	netdump_in_progress = 1;

	if (dump_handshake(net_dev) < 0) {
		printk("network dump failed due to handshake failure\n");
		goto out;
	}

	/*
	 * Ideally startup handshake should be done during dump configuration,
	 * i.e., in dump_net_open(). This will be done when I figure out
	 * the dependency between startup handshake, subsequent write and
	 * various commands wrt to net-server.
	 */
	if (startup_handshake)
		startup_handshake = 0;

        counter = 0;
	repeatCounter = 0;
	total_loop = 0;
	while (1) {
                if (!new_req) {
			dump_ndev->poll_controller(dump_ndev);
			zap_completion_queue();
		}
		if (!new_req) {
			repeatCounter++;

			if (repeatCounter > 5) {
				counter++;
				if (counter > 10000) {
					if (total_loop >= 100000) {
						printk("Time OUT LEAVE NOW\n");
						goto out;
					} else {
						total_loop++;
						printk("Try number %d out of "
							"10 before Time Out\n",
							total_loop);
					}
				}
				mdelay(1);
				repeatCounter = 0;
			}	
			continue;
		}
		repeatCounter = 0;
		counter = 0;
		total_loop = 0;
		new_req = 0;
		switch (req.command) {
		case COMM_NONE:
			break;

		case COMM_SEND_MEM:
			dump_send_mem(dump_ndev, &req, buff, len);
			break;

		case COMM_EXIT:
                case COMM_START_WRITE_NETDUMP_ACK:
			ret = len;
			goto out;

		case COMM_HELLO:
			sprintf(tmp, "Hello, this is netdump version "
					"0.%02d\n", NETCONSOLE_VERSION);
			reply.code = REPLY_HELLO;
			reply.nr = req.nr;
                        reply.info = net_dev->curr_offset;
			dump_send_skb(dump_ndev, tmp, strlen(tmp), &reply);
			break;

		case COMM_GET_PAGE_SIZE:
			sprintf(tmp, "PAGE_SIZE: %ld\n", PAGE_SIZE);
			reply.code = REPLY_PAGE_SIZE;
			reply.nr = req.nr;
			reply.info = PAGE_SIZE;
			dump_send_skb(dump_ndev, tmp, strlen(tmp), &reply);
			break;

		case COMM_GET_NR_PAGES:
			reply.code = REPLY_NR_PAGES;
			reply.nr = req.nr;
			reply.info = num_physpages;
                        reply.info = page_counter;
			sprintf(tmp, "Number of pages: %ld\n", num_physpages);
			dump_send_skb(dump_ndev, tmp, strlen(tmp), &reply);
			break;

		case COMM_GET_MAGIC:
			reply.code = REPLY_MAGIC;
			reply.nr = req.nr;
			reply.info = NETCONSOLE_VERSION;
			dump_send_skb(dump_ndev, (char *)&dump_magic,
					sizeof(dump_magic), &reply);
			break;

		default:
			reply.code = REPLY_ERROR;
			reply.nr = req.nr;
			reply.info = req.command;
			sprintf(tmp, "Got unknown command code %d!\n",
					req.command);
			dump_send_skb(dump_ndev, tmp, strlen(tmp), &reply);
			break;
		}
	}
out:
	netdump_in_progress = 0;
	return ret;
}

static int
dump_validate_config(void)
{
	source_ip = dump_in_dev->ifa_list->ifa_local;
	if (!source_ip) {
		printk("network device %s has no local address, "
				"aborting.\n", device_name);
		return -1;
	}

#define IP(x) ((unsigned char *)&source_ip)[x]
	printk("Source %d.%d.%d.%d", IP(0), IP(1), IP(2), IP(3));
#undef IP

	if (!source_port) {
		printk("source_port parameter not specified, aborting.\n");
		return -1;
	}
	printk(":%i\n", source_port);
	source_port = htons(source_port);

	if (!target_ip) {
		printk("target_ip parameter not specified, aborting.\n");
		return -1;
	}

#define IP(x) ((unsigned char *)&target_ip)[x]
	printk("Target %d.%d.%d.%d", IP(0), IP(1), IP(2), IP(3));
#undef IP

	if (!target_port) {
		printk("target_port parameter not specified, aborting.\n");
		return -1;
	}
	printk(":%i\n", target_port);
	target_port = htons(target_port);

	printk("Target Ethernet Address %02x:%02x:%02x:%02x:%02x:%02x",
		daddr[0], daddr[1], daddr[2], daddr[3], daddr[4], daddr[5]);

	if ((daddr[0] & daddr[1] & daddr[2] & daddr[3] & daddr[4] & 
				daddr[5]) == 255)
		printk("(Broadcast)");
	printk("\n");
	return 0;
}

/*
 * Prepares the dump device so we can take a dump later. 
 * Validates the netdump configuration parameters.
 *
 * TODO: Network connectivity check should be done here.
 */
static int
dump_net_open(struct dump_dev *net_dev, unsigned long arg)
{
	int retval = 0;

	/* get the interface name */
	if (copy_from_user(device_name, (void *)arg, IFNAMSIZ))
		return -EFAULT;

	if (!(dump_ndev = dev_get_by_name(device_name))) {
		printk("network device %s does not exist, aborting.\n",
				device_name);
		return -ENODEV;
	}

	if (!dump_ndev->poll_controller) {
		printk("network device %s does not implement polling yet, "
				"aborting.\n", device_name);
		retval = -1; /* return proper error */
		goto err1;
	}

	if (!(dump_in_dev = in_dev_get(dump_ndev))) {
		printk("network device %s is not an IP protocol device, "
				"aborting.\n", device_name);
		retval = -EINVAL;
		goto err1;
	}

	if ((retval = dump_validate_config()) < 0)
		goto err2;

	net_dev->curr_offset = 0;
	printk("Network device %s successfully configured for dumping\n",
			device_name);
	return retval;
err2:
	in_dev_put(dump_in_dev);
err1:
	dev_put(dump_ndev);	
	return retval;
}

/*
 * Close the dump device and release associated resources
 * Invoked when unconfiguring the dump device.
 */
static int
dump_net_release(struct dump_dev *net_dev)
{
	if (dump_in_dev)
		in_dev_put(dump_in_dev);
	if (dump_ndev)
		dev_put(dump_ndev);
	return 0;
}

/*
 * Prepare the dump device for use (silence any ongoing activity
 * and quiesce state) when the system crashes.
 */
static int
dump_net_silence(struct dump_dev *net_dev)
{
	netpoll_set_trap(1);
	local_irq_save(flags_global);
	dump_ndev->rx_hook = dump_rx_hook;
        startup_handshake = 1;
	net_dev->curr_offset = 0;
	printk("Dumping to network device %s on CPU %d ...\n", device_name,
			smp_processor_id());
	return 0;
}

/*
 * Invoked when dumping is done. This is the time to put things back 
 * (i.e. undo the effects of dump_block_silence) so the device is 
 * available for normal use.
 */
static int
dump_net_resume(struct dump_dev *net_dev)
{
	int indx;
	reply_t reply;
	char tmp[200];

        if (!dump_ndev)
		return (0);

	sprintf(tmp, "NETDUMP end.\n");
	for( indx = 0; indx < 6; indx++) {
		reply.code = REPLY_END_NETDUMP;
		reply.nr = 0;
		reply.info = 0;
		dump_send_skb(dump_ndev, tmp, strlen(tmp), &reply);
	}
	printk("NETDUMP END!\n");
	local_irq_restore(flags_global);
	netpoll_set_trap(0);
	dump_ndev->rx_hook = NULL;
	startup_handshake = 0;
	return 0;
}

/*
 * Seek to the specified offset in the dump device.
 * Makes sure this is a valid offset, otherwise returns an error.
 */
static  int
dump_net_seek(struct dump_dev *net_dev, loff_t off)
{
	/*
	 * For now using DUMP_HEADER_OFFSET as hard coded value,
	 * See dump_block_seekin dump_blockdev.c to know how to
	 * do this properly.
	 */
	net_dev->curr_offset = off;
	return 0;
}

/*
 *
 */
static int
dump_net_write(struct dump_dev *net_dev, void *buf, unsigned long len)
{
	int cnt, i, off;
	ssize_t ret;

	cnt = len/ PAGE_SIZE;

	for (i = 0; i < cnt; i++) {
		off = i* PAGE_SIZE;
		ret = do_netdump(net_dev, buf+off, PAGE_SIZE);
		if (ret <= 0)
			return -1;
		net_dev->curr_offset = net_dev->curr_offset + PAGE_SIZE;
	}
	return len;
}

/*
 * check if the last dump i/o is over and ready for next request
 */
static int
dump_net_ready(struct dump_dev *net_dev, void *buf)
{
	return 0;
}

/*
 * ioctl function used for configuring network dump
 */
static int
dump_net_ioctl(struct dump_dev *net_dev, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case DIOSTARGETIP:
		target_ip = arg;
		break;
	case DIOSTARGETPORT:
		target_port = (u16)arg;
		break;
	case DIOSSOURCEPORT:
		source_port = (u16)arg;
		break;
	case DIOSETHADDR:
		return copy_from_user(daddr, (void *)arg, 6);
		break;
	case DIOGTARGETIP:
	case DIOGTARGETPORT:
	case DIOGSOURCEPORT:
	case DIOGETHADDR:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

struct dump_dev_ops dump_netdev_ops = {
	.open 		= dump_net_open,
	.release	= dump_net_release,
	.silence	= dump_net_silence,
	.resume 	= dump_net_resume,
	.seek		= dump_net_seek,
	.write		= dump_net_write,
	/* .read not implemented */
	.ready		= dump_net_ready,
	.ioctl		= dump_net_ioctl
};

static struct dump_dev default_dump_netdev = {
	.type_name = "networkdev", 
	.ops = &dump_netdev_ops, 
	.curr_offset = 0
};

static int __init
dump_netdev_init(void)
{
        default_dump_netdev.curr_offset = 0;

	if (dump_register_device(&default_dump_netdev) < 0) {
		printk("network dump device driver registration failed\n");
		return -1;
	}
	printk("network device driver for LKCD registered\n");
 
	get_random_bytes(&dump_magic, sizeof(dump_magic));
	return 0;
}

static void __exit
dump_netdev_cleanup(void)
{
	dump_unregister_device(&default_dump_netdev);
}

MODULE_AUTHOR("LKCD Development Team <lkcd-devel@lists.sourceforge.net>");
MODULE_DESCRIPTION("Network Dump Driver for Linux Kernel Crash Dump (LKCD)");
MODULE_LICENSE("GPL");

module_init(dump_netdev_init);
module_exit(dump_netdev_cleanup);
