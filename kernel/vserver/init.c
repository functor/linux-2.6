/*
 *  linux/kernel/init.c
 *
 *  Virtual Server Init
 *
 *  Copyright (C) 2004  Herbert Pötzl
 *
 *  V0.01  basic structure
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/vserver.h>
#include <linux/init.h>
#include <linux/module.h>

int	vserver_register_sysctl(void);
void	vserver_unregister_sysctl(void);


static int __init init_vserver(void)
{
	int ret = 0;

	vserver_register_sysctl();
	return ret;
}


static void __exit exit_vserver(void)
{

	vserver_unregister_sysctl();
	return;
}


module_init(init_vserver);
module_exit(exit_vserver);

