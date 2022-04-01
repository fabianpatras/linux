/*
 * Deferred Work
 *
 * Exercise #1, #2: simple timer
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>

MODULE_DESCRIPTION("Simple kernel timer");
MODULE_AUTHOR("SO2");
MODULE_LICENSE("GPL");

#define TIMER_TIMEOUT	1
#define TIMER_MESSAGE "hello from the insideeeeeee of timer"

static struct timer_list timer;

static void timer_handler(struct timer_list *tl)
{
	int rc;
	unsigned long expires = jiffies + (TIMER_TIMEOUT) * HZ;

	/* TODO 1: print a message */
	pr_info("Message from timer [%s] timestamp [%lu]\n", TIMER_MESSAGE, jiffies);

	/* TODO 2: rechedule timer */
	if ((rc = mod_timer(&timer, expires))) {
		pr_info("Could not init timer [%d]\n", rc);
	}
}

static int __init timer_init(void)
{
	unsigned long expires = jiffies + (TIMER_TIMEOUT) * HZ;
	int rc = 0;
	pr_info("[timer_init] Init module\n");

	/* TODO 1: initialize timer */
	timer_setup(&timer, &timer_handler, 0);

	/* TODO 1: schedule timer for the first time */
	if ((rc = mod_timer(&timer, expires))) {
		pr_info("Could not init timer [%d]\n", rc);
	}

	return 0;
}

static void __exit timer_exit(void)
{
	pr_info("[timer_exit] Exit module\n");

	/* TODO 1: cleanup; make sure the timer is not running after we exit */
	del_timer_sync(&timer);
}

module_init(timer_init);
module_exit(timer_exit);
