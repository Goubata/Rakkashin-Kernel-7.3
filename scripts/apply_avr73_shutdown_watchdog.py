#!/usr/bin/env python3
"""7.3 reboot/power-off hang: shutdown watchdog.

A power-menu reboot or power-off on avr could hang for good after userspace was
gone (display frozen, nothing more in the log), so a forced reset was needed. This
arms a 20 s timer when the kernel starts the restart / power-off sequence (before the
reboot notifiers, which run vendor callbacks). If the sequence is still running when
it fires, it prints the device whose shutdown callback is running, the blocked tasks
and every CPU's backtrace, then completes the request itself: machine_power_off() for
a power-off, emergency_restart() otherwise. The phone does not hang any more, and the
reason is kept in pstore (warm reset).
"""
from pathlib import Path
import sys

K = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'kernel')


def edit(rel, old, new):
    p = K / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old, new))


edit('drivers/base/core.c', '''void device_shutdown(void)
{''', '''/* ANDROID: avr: device whose shutdown callback is running (shutdown watchdog report) */
struct device *avr_shutdown_cur_dev;

void device_shutdown(void)
{''')
edit('drivers/base/core.c', '''		if (dev->class && dev->class->shutdown_pre) {''', '''		WRITE_ONCE(avr_shutdown_cur_dev, dev);
		if (dev->class && dev->class->shutdown_pre) {''')
edit('drivers/base/core.c', '''		device_unlock(dev);
		if (parent)
			device_unlock(parent);

		put_device(dev);
		put_device(parent);

		spin_lock(&devices_kset->list_lock);
	}''', '''		device_unlock(dev);
		if (parent)
			device_unlock(parent);

		WRITE_ONCE(avr_shutdown_cur_dev, NULL);
		put_device(dev);
		put_device(parent);

		spin_lock(&devices_kset->list_lock);
	}''')

edit('kernel/reboot.c', '''void kernel_restart_prepare(char *cmd)
{
	blocking_notifier_call_chain(&reboot_notifier_list, SYS_RESTART, cmd);''', '''/*
 * ANDROID: avr shutdown watchdog: never hang a restart / power-off. If the kernel
 * shutdown sequence (reboot notifiers + device_shutdown()) takes longer than 20 s,
 * report where it is stuck and finish the request.
 */
extern struct device *avr_shutdown_cur_dev;

static bool avr_shutdown_wdt_poweroff;

static void avr_shutdown_wdt_fn(struct timer_list *t)
{
	struct device *dev = READ_ONCE(avr_shutdown_cur_dev);

	pr_emerg("reboot: shutdown stuck for 20 s (state %d) in %s %s, forcing it\\n",
		 system_state, dev ? dev_driver_string(dev) : "notifiers/core",
		 dev ? dev_name(dev) : "");
	show_state_filter(TASK_UNINTERRUPTIBLE);
	trigger_all_cpu_backtrace();
	pr_emerg("reboot: shutdown watchdog: completing %s\\n",
		 avr_shutdown_wdt_poweroff ? "power-off" : "restart");
	if (avr_shutdown_wdt_poweroff && pm_power_off)
		machine_power_off();
	emergency_restart();
}
static DEFINE_TIMER(avr_shutdown_wdt, avr_shutdown_wdt_fn);

static void avr_shutdown_wdt_arm(bool poweroff)
{
	avr_shutdown_wdt_poweroff = poweroff;
	mod_timer(&avr_shutdown_wdt, jiffies + 20 * HZ);
}

void kernel_restart_prepare(char *cmd)
{
	avr_shutdown_wdt_arm(false);
	blocking_notifier_call_chain(&reboot_notifier_list, SYS_RESTART, cmd);''')
edit('kernel/reboot.c', '''static void kernel_shutdown_prepare(enum system_states state)
{
	blocking_notifier_call_chain(&reboot_notifier_list,''', '''static void kernel_shutdown_prepare(enum system_states state)
{
	avr_shutdown_wdt_arm(state == SYSTEM_POWER_OFF);
	blocking_notifier_call_chain(&reboot_notifier_list,''')
edit('kernel/reboot.c', '''#include <linux/uaccess.h>
''', '''#include <linux/uaccess.h>
#include <linux/nmi.h>
#include <linux/pm.h>
#include <linux/sched/debug.h>
#include <linux/timer.h>
''')
print('applied shutdown watchdog to', K)
