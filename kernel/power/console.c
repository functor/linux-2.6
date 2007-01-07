/*
 * drivers/power/process.c - Functions for saving/restoring console.
 *
 * Originally from swsusp.
 */

#include <linux/vt_kern.h>
#include <linux/kbd_kern.h>
#include <linux/console.h>
#include "power.h"

extern int console_suspended;

int pm_prepare_console(void)
{
	acquire_console_sem();
	console_suspended = 1;
	system_state = SYSTEM_BOOTING;
	return 0;
}

void pm_restore_console(void)
{
	console_suspended = 0;
	system_state = SYSTEM_RUNNING;
	release_console_sem();
	return;
}
