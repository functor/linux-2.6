/*
 * linux/drivers/input/keyboard/xenkbd.c -- Xen para-virtual input device
 *
 * Copyright (C) 2005
 *
 *      Anthony Liguori <aliguori@us.ibm.com>
 *
 *  Based on linux/drivers/input/mouse/sermouse.c
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/input.h>
#include <asm/hypervisor.h>
#include <xen/evtchn.h>
#include <xen/xenbus.h>
#include <linux/xenkbd.h>

struct xenkbd_device
{
	struct input_dev *dev;
	struct xenkbd_info *info;
	unsigned evtchn;
	int irq;
};

static irqreturn_t input_handler(int rq, void *dev_id, struct pt_regs *regs)
{
	struct xenkbd_device *dev = dev_id;
	struct xenkbd_info *info = dev ? dev->info : 0;
	static int button_map[3] = { BTN_RIGHT, BTN_MIDDLE, BTN_LEFT };
	__u32 cons, prod;

	prod = info->in_prod;
	rmb();			/* ensure we see ring contents up to prod */
	for (cons = info->in_cons; cons != prod; cons++) {
		union xenkbd_in_event *event;
		event = &XENKBD_IN_RING_REF(info, cons);
	
		switch (event->type) {
		case XENKBD_TYPE_MOTION:
			input_report_rel(dev->dev, REL_X, event->motion.rel_x);
			input_report_rel(dev->dev, REL_Y, event->motion.rel_y);
			break;
		case XENKBD_TYPE_BUTTON:
			if (event->button.button < 3)
				input_report_key(dev->dev,
						 button_map[event->button.button],
						 event->button.pressed);
			break;
		case XENKBD_TYPE_KEY:
			input_report_key(dev->dev, event->key.keycode, event->key.pressed);
			break;
		}
	}
	input_sync(dev->dev);
	mb();			/* ensure we got ring contents */
	info->in_cons = cons;
	notify_remote_via_evtchn(dev->evtchn);

	return IRQ_HANDLED;
}

static struct xenkbd_device *xenkbd_dev;

int __init xenkbd_init(void)
{
	int ret = 0;
	int i;
	struct xenkbd_device *dev;
	struct input_dev *input_dev;
	struct evtchn_alloc_unbound alloc_unbound;
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

	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!dev || !input_dev)
		return -ENOMEM;

	dev->dev = input_dev;
	dev->info = (void *)__get_free_page(GFP_KERNEL);
	if (!dev->info) {
		ret = -ENOMEM;
		goto error;
	}

	alloc_unbound.dom = DOMID_SELF;
	alloc_unbound.remote_dom = 0;
	ret = HYPERVISOR_event_channel_op(EVTCHNOP_alloc_unbound,
					  &alloc_unbound);
	if (ret)
		goto error_freep;
	dev->evtchn = alloc_unbound.port;
	ret = bind_evtchn_to_irqhandler(dev->evtchn, input_handler, 0,
					"xenkbd", dev);
	if (ret < 0)
		goto error_freep;
	dev->irq = ret;

	input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REL);
	input_dev->keybit[LONG(BTN_MOUSE)] = BIT(BTN_LEFT) | BIT(BTN_RIGHT);
	input_dev->relbit[0] = BIT(REL_X) | BIT(REL_Y);

	/* FIXME not sure this is quite right */
	for (i = 0; i < 256; i++)
		set_bit(i, input_dev->keybit);

	input_dev->name = "Xen Virtual Keyboard/Mouse";

	input_register_device(input_dev);

 again:
	ret = xenbus_transaction_start(&xbt);
	if (ret)
		goto error_unreg;
	ret = xenbus_printf(xbt, "vkbd", "page-ref", "%lu",
			    virt_to_mfn(dev->info));
	if (ret)
		goto error_xenbus;
	ret = xenbus_printf(xbt, "vkbd", "event-channel", "%u",
			    dev->evtchn);
	if (ret)
		goto error_xenbus;
	ret = xenbus_transaction_end(xbt, 0);
	if (ret) {
		if (ret == -EAGAIN)
			goto again;
		goto error_unreg;
	}

	dev->info->in_cons = dev->info->in_prod = 0;
	dev->info->out_cons = dev->info->out_prod = 0;
	xenkbd_dev = dev;

	return ret;

	
 error_xenbus:
	xenbus_transaction_end(xbt, 1);
 error_unreg:
	input_unregister_device(input_dev);
	unbind_from_irqhandler(dev->irq, dev);
 error_freep:
	free_page((unsigned long)dev->info);
 error:
	kfree(dev);
	return ret;
}

static void __exit xenkbd_cleanup(void)
{
	input_unregister_device(xenkbd_dev->dev);
	unbind_from_irqhandler(xenkbd_dev->irq, xenkbd_dev);
	free_page((unsigned long)xenkbd_dev->info);
	kfree(xenkbd_dev);
	xenkbd_dev = NULL;
}

module_init(xenkbd_init);
module_exit(xenkbd_cleanup);

MODULE_LICENSE("GPL");
