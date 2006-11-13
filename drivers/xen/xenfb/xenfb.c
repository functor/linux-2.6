/*
 * linux/drivers/video/xenfb.c -- Xen para-virtual frame buffer device
 *
 * Copyright (C) 2005-2006
 *
 *      Anthony Liguori <aliguori@us.ibm.com>
 *
 *  Based on linux/drivers/video/q40fb.c
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <asm/hypervisor.h>
#include <xen/evtchn.h>
#include <xen/xenbus.h>
#include <linux/xenfb.h>
#include <linux/kthread.h>

#define XENFB_WIDTH 800
#define XENFB_HEIGHT 600
#define XENFB_DEPTH 32

static int xenfb_fps = 20;
static unsigned long xenfb_mem_len = XENFB_WIDTH * XENFB_HEIGHT * XENFB_DEPTH / 8;

struct xenfb_mapping
{
	struct list_head	next;
	struct vm_area_struct	*vma;
	atomic_t		map_refs;
	int			faults;
	struct xenfb_info	*info;
};

struct xenfb_info
{
	struct task_struct		*kthread;
	wait_queue_head_t		wq;

	unsigned char			*fb;
	struct fb_fix_screeninfo	*fix;
	struct fb_var_screeninfo	*var;
	struct fb_info			*fb_info;
	struct timer_list		refresh;
	int				dirty;
	int				y1, y2;
	int				x1, x2;

	struct semaphore		mm_lock;
	int				nr_pages;
	struct page			**pages;
	struct list_head		mappings;

	unsigned			evtchn;
	int				irq;
	struct xenfb_page		*page;
	unsigned long 			*mfns;
	u32				flags;
};

static void xenfb_do_update(struct xenfb_info *info,
			    int x, int y, int w, int h)
{
	union xenfb_out_event event;
	__u32 prod;

	event.type = XENFB_TYPE_UPDATE;
	event.update.x = x;
	event.update.y = y;
	event.update.width = w;
	event.update.height = h;

	prod = info->page->out_prod;
	if (prod - info->page->out_cons == XENFB_OUT_RING_LEN)
		return;		/* ring buffer full, event lost */
	mb();			/* ensure ring space available */
	XENFB_OUT_RING_REF(info->page, prod) = event;
	wmb();			/* ensure ring contents visible */
	info->page->out_prod = prod + 1;

	notify_remote_via_evtchn(info->evtchn);
}

static int xenfb_queue_full(struct xenfb_info *info)
{
	__u32 cons, prod;

	prod = info->page->out_prod;
	cons = info->page->out_cons;
	return prod - cons == XENFB_OUT_RING_LEN;
}

static void xenfb_update_screen(struct xenfb_info *info)
{
	int y1, y2, x1, x2;
	struct list_head *item;
	struct xenfb_mapping *map;

	if (!(info->flags & XENFB_FLAG_UPDATE))
		return;
	if (xenfb_queue_full(info))
		return;

	y1 = info->y1;
	y2 = info->y2;
	x1 = info->x1;
	x2 = info->x2;
	info->y1 = info->y2 = info->x1 = info->x2 = 0;
	down(&info->mm_lock);
	list_for_each(item, &info->mappings) {
		map = list_entry(item, struct xenfb_mapping, next);
		if (!map->faults)
			continue;
		zap_page_range(map->vma, map->vma->vm_start,
			       map->vma->vm_end - map->vma->vm_start, NULL);
		map->faults = 0;
	}
	up(&info->mm_lock);

	xenfb_do_update(info, x1, y1, x2 - x1, y2 - y1);
}

static int xenfb_thread(void *data)
{
	struct xenfb_info *info = data;

	for (;;) {
		if (kthread_should_stop())
			break;
		if (info->dirty) {
			info->dirty = 0;
			xenfb_update_screen(info);
		}
		wait_event_interruptible(info->wq,
			kthread_should_stop() || info->dirty);
	}
	return 0;
}

static int xenfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	u32 v;

	if (regno > info->cmap.len)
		return 1;

	red   >>= (16 - info->var.red.length);
	green >>= (16 - info->var.green.length);
	blue  >>= (16 - info->var.blue.length);

	v = (red << info->var.red.offset) |
	    (green << info->var.green.offset) |
	    (blue << info->var.blue.offset);

	switch (info->var.bits_per_pixel) {
	case 16:
	case 24:
	case 32:
		((u32 *)info->pseudo_palette)[regno] = v;
		break;
	}
	
	return 0;
}

static void xenfb_timer(unsigned long data)
{
	struct xenfb_info *info = (struct xenfb_info *)data;
	info->dirty = 1;
	wake_up(&info->wq);
}

static void xenfb_refresh(struct xenfb_info *info,
			  int x1, int y1, int w, int h)
{
	int y2, x2;

	y2 = y1 + h;
	x2 = x1 + w;
	if (info->y2 == 0) {
		info->y1 = y1;
		info->y2 = y2;
	}
	if (info->x2 == 0) {
		info->x1 = x1;
		info->x2 = x2;
	}

	if (info->y1 > y1)
		info->y1 = y1;
	if (info->y2 < y2)
		info->y2 = y2;
	if (info->x1 > x1)
		info->x1 = x1;
	if (info->x2 < x2)
		info->x2 = x2;

	if (timer_pending(&info->refresh))
		return;

	mod_timer(&info->refresh, jiffies + HZ/xenfb_fps);
}

static void xenfb_fillrect(struct fb_info *p, const struct fb_fillrect *rect)
{
	struct xenfb_info *info = p->par;

	cfb_fillrect(p, rect);
	xenfb_refresh(info, rect->dx, rect->dy, rect->width, rect->height);
}

static void xenfb_imageblit(struct fb_info *p, const struct fb_image *image)
{
	struct xenfb_info *info = p->par;

	cfb_imageblit(p, image);
	xenfb_refresh(info, image->dx, image->dy, image->width, image->height);
}

static void xenfb_copyarea(struct fb_info *p, const struct fb_copyarea *area)
{
	struct xenfb_info *info = p->par;

	cfb_copyarea(p, area);
	xenfb_refresh(info, area->dx, area->dy, area->width, area->height);
}

static void xenfb_vm_open(struct vm_area_struct *vma)
{
	struct xenfb_mapping *map = vma->vm_private_data;
	atomic_inc(&map->map_refs);
}

static void xenfb_vm_close(struct vm_area_struct *vma)
{
	struct xenfb_mapping *map = vma->vm_private_data;
	struct xenfb_info *info = map->info;

	down(&info->mm_lock);
	if (atomic_dec_and_test(&map->map_refs)) {
		list_del(&map->next);
		kfree(map);
	}
	up(&info->mm_lock);
}

static struct page *xenfb_vm_nopage(struct vm_area_struct *vma,
				    unsigned long vaddr, int *type)
{
	struct xenfb_mapping *map = vma->vm_private_data;
	struct xenfb_info *info = map->info;
	int pgnr = (vaddr - vma->vm_start) >> PAGE_SHIFT;
	struct page *page;
	int y1, y2;

	if (pgnr >= info->nr_pages)
		return NOPAGE_SIGBUS;

	down(&info->mm_lock);
	page = info->pages[pgnr];
	get_page(page);
	map->faults++;

	y1 = pgnr * PAGE_SIZE / info->fix->line_length;
	y2 = (pgnr * PAGE_SIZE + PAGE_SIZE - 1) / info->fix->line_length;
	if (y2 > info->var->yres)
		y2 = info->var->yres;
	xenfb_refresh(info, 0, y1, info->var->xres, y2 - y1);
	up(&info->mm_lock);

	if (type)
		*type = VM_FAULT_MINOR;

	return page;
}

static struct vm_operations_struct xenfb_vm_ops = {
	.open	= xenfb_vm_open,
	.close	= xenfb_vm_close,
	.nopage	= xenfb_vm_nopage,
};

static int xenfb_mmap(struct fb_info *fb_info, struct vm_area_struct *vma)
{
	struct xenfb_info *info = fb_info->par;
	struct xenfb_mapping *map;
	int ret;
	int map_pages;

	down(&info->mm_lock);

	ret = -EINVAL;
	if (!(vma->vm_flags & VM_WRITE))
		goto out;
	if (!(vma->vm_flags & VM_SHARED))
		goto out;
	if (vma->vm_pgoff != 0)
		goto out;

	map_pages = (vma->vm_end - vma->vm_start + PAGE_SIZE-1) >> PAGE_SHIFT;
	if (map_pages > info->nr_pages)
		goto out;

	ret = -ENOMEM;
	map = kmalloc(sizeof(*map), GFP_KERNEL);
	if (map == NULL)
		goto out;
	memset(map, 0, sizeof(*map));

	map->vma = vma;
	map->faults = 0;
	map->info = info;
	atomic_set(&map->map_refs, 1);
	list_add(&map->next, &info->mappings);
	vma->vm_ops = &xenfb_vm_ops;
	vma->vm_flags |= (VM_DONTEXPAND | VM_RESERVED);
	vma->vm_private_data = map;
	ret = 0;

 out:
	up(&info->mm_lock);
	return ret;
}

static struct fb_ops xenfb_fb_ops = {
	.owner		= THIS_MODULE,
	.fb_setcolreg	= xenfb_setcolreg,
	.fb_fillrect	= xenfb_fillrect,
	.fb_copyarea	= xenfb_copyarea,
	.fb_imageblit	= xenfb_imageblit,
	.fb_mmap	= xenfb_mmap,
};

static irqreturn_t xenfb_event_handler(int rq, void *dev_id,
				       struct pt_regs *regs)
{
	struct xenfb_info *info = dev_id;
	__u32 cons, prod;

	prod = info->page->in_prod;
	rmb();			/* ensure we see ring contents up to prod */
	for (cons = info->page->in_cons; cons != prod; cons++) {
		union xenfb_in_event *event;
		event = &XENFB_IN_RING_REF(info->page, cons);

		switch (event->type) {
		case XENFB_TYPE_SET_EVENTS:
			info->flags = event->set_events.flags;
			break;
		}
	}
	mb();			/* ensure we're done with ring contents */
	info->page->in_cons = cons;
	notify_remote_via_evtchn(info->evtchn);

	return IRQ_HANDLED;
}

static unsigned long vmalloc_to_mfn(void *address)
{
	return pfn_to_mfn(vmalloc_to_pfn(address));
	//return arbitrary_virt_to_machine(address) >> PAGE_SHIFT;
}

static struct xenfb_info *xenfb_info;

static int __init xenfb_probe(void)
{
	struct xenfb_info *info;
	int i, ret;
	struct fb_info *fb_info;
	struct evtchn_alloc_unbound alloc_unbound;
	struct evtchn_close close;
	struct xenbus_transaction xbt;

        /* Nothing to do if running in dom0. */
        if (is_initial_xendomain())
		return -ENODEV;
#if 1
        /* if we're not set up to use graphics mode, then don't initialize */
	if (xenbus_scanf(XBT_NIL, "console", "use_graphics", "%d", &ret) < 0)
		return -ENODEV;
	if (ret == 0)
		return -ENODEV;
#endif

	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	memset(info, 0, sizeof(*info));

	INIT_LIST_HEAD(&info->mappings);

	info->fb = vmalloc(xenfb_mem_len);
	if (info->fb == NULL)
		goto error;
	memset(info->fb, 0, xenfb_mem_len);
	info->nr_pages = (xenfb_mem_len + PAGE_SIZE - 1) >> PAGE_SHIFT;
	info->pages = kmalloc(sizeof(struct page*)*info->nr_pages, GFP_KERNEL);
	if (info->pages == NULL)
		goto error_vfree;
	for (i = 0; i < info->nr_pages; i++)
		info->pages[i] = vmalloc_to_page(info->fb + i * PAGE_SIZE);

	fb_info = framebuffer_alloc(sizeof(u32) * 256, NULL);
	// FIXME sizeof(struct xenfb_info)
	if (fb_info == NULL)
		goto error_kfree;

	info->mfns = vmalloc(sizeof(unsigned long) * info->nr_pages);
	/* set up shared page */
	info->page = (void *)__get_free_page(GFP_KERNEL);
	if (!info->page)
		goto error_kfree;
	/* set up event channel */
	alloc_unbound.dom = DOMID_SELF;
	alloc_unbound.remote_dom = 0;
	ret = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound,
					  &alloc_unbound);
	if (ret)
		goto error_freep;
	info->evtchn = alloc_unbound.port;

	for (i = 0; i < info->nr_pages; i++)
		info->mfns[i] = vmalloc_to_mfn(info->fb + i * PAGE_SIZE);
	info->page->pd[0] = vmalloc_to_mfn(info->mfns);
	info->page->pd[1] = 0;
	info->page->width = XENFB_WIDTH;
	info->page->height = XENFB_HEIGHT;
	info->page->depth = XENFB_DEPTH;
	info->page->line_length = (info->page->depth / 8) * info->page->width;
	info->page->mem_length = xenfb_mem_len;
	info->page->in_cons = info->page->in_prod = 0;
	info->page->out_cons = info->page->out_prod = 0;

	ret = bind_evtchn_to_irqhandler(info->evtchn, xenfb_event_handler,
					0, "xenfb", info);
	if (ret < 0) {
		close.port = info->evtchn;
		HYPERVISOR_event_channel_op(EVTCHNOP_close, &close);
		goto error_freep;
	}

	info->irq = ret;
	xenfb_info = info;

	fb_info->pseudo_palette = fb_info->par;
	fb_info->par = info;
	fb_info->screen_base = info->fb;

	memset(&fb_info->var, 0, sizeof(fb_info->var));
	memset(&fb_info->fix, 0, sizeof(fb_info->fix));

	fb_info->fbops = &xenfb_fb_ops;
	fb_info->var.xres_virtual = fb_info->var.xres = info->page->width;
	fb_info->var.yres_virtual = fb_info->var.yres = info->page->height;
	fb_info->var.bits_per_pixel = info->page->depth;

	fb_info->var.red = (struct fb_bitfield){16, 8, 0};
	fb_info->var.green = (struct fb_bitfield){8, 8, 0};
	fb_info->var.blue = (struct fb_bitfield){0, 8, 0};

	fb_info->var.activate = FB_ACTIVATE_NOW;
	fb_info->var.height = -1;
	fb_info->var.width = -1;
	fb_info->var.vmode = FB_VMODE_NONINTERLACED;

	fb_info->fix.visual = FB_VISUAL_TRUECOLOR;
	fb_info->fix.line_length = info->page->line_length;
	fb_info->fix.smem_start = 0;
	fb_info->fix.smem_len = xenfb_mem_len;
	strcpy(fb_info->fix.id, "xen");
	fb_info->fix.type = FB_TYPE_PACKED_PIXELS;
	fb_info->fix.accel = FB_ACCEL_NONE;

	fb_info->flags = FBINFO_FLAG_DEFAULT;

	fb_alloc_cmap(&fb_info->cmap, 256, 0);

	info->fb_info = fb_info;
	info->fix = &fb_info->fix;
	info->var = &fb_info->var;

	init_MUTEX(&info->mm_lock);
	init_waitqueue_head(&info->wq);
	init_timer(&info->refresh);
	info->refresh.function = xenfb_timer;
	info->refresh.data = (unsigned long)info;

	info->kthread = kthread_run(xenfb_thread, info, "xenfb thread");
	if (IS_ERR(info->kthread))
		goto error_unbind;

	ret = register_framebuffer(fb_info);
	if (ret)
		goto error_unbind;

 again:
	ret = xenbus_transaction_start(&xbt);
	if (ret)
		goto error_unreg;
	ret = xenbus_printf(xbt, "vfb", "page-ref", "%lu",
			    virt_to_mfn(info->page));
	// FIXME grant tables?
	if (ret)
		goto error_xenbus;
	ret = xenbus_printf(xbt, "vfb", "event-channel", "%u",
			    info->evtchn);
	if (ret)
		goto error_xenbus;
	ret = xenbus_transaction_end(xbt, 0);
	if (ret) {
		if (ret == -EAGAIN)
			goto again;
		goto error_unreg;
	}

	return 0;

 error_xenbus:
	xenbus_transaction_end(xbt, 1);
 error_unreg:
	unregister_framebuffer(fb_info);
 error_unbind:
	unbind_from_irqhandler(info->irq, info);
	// FIXME do we have to stop info->kthread?
 error_freep:
	free_page((unsigned long)info->page);
 error_kfree:
	kfree(info->pages);
 error_vfree:
	vfree(info->fb);
 error:
	kfree(info);
	xenfb_info = NULL;

	return -ENODEV;
}

static int __init xenfb_init(void)
{
	return xenfb_probe();
}

static void __exit xenfb_cleanup(void)
{
	struct xenfb_info *info = xenfb_info;

	unregister_framebuffer(info->fb_info);
	unbind_from_irqhandler(info->irq, info);
	free_page((unsigned long)info->page);
	kfree(info->pages);
	vfree(info->fb);
	kfree(info);
	xenfb_info = NULL;
}

module_init(xenfb_init);
module_exit(xenfb_cleanup);

MODULE_LICENSE("GPL");
