#!/usr/bin/env python3
"""Restore the android16-6.12 thermal vendor hooks on 7.3.

qti_thermal_vendor_hooks.ko (loaded on the phone) registers
android_vh_thermal_pm_notify_suspend so thermal zones whose sensors have wake
capable interrupts keep running across system suspend (6.12 thermal_core skipped
thermal_zone_pm_prepare/complete and the "suspended" marking of late zones when the
hook set irq_wakeable), and android_vh_enable_thermal_genl_check to filter
thermal netlink events. android18-7.3 has neither the header nor the call sites.
Call sites/semantics follow 6.12 thermal_core.c / thermal_netlink.c exactly.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')


def edit(rel, old, new):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old, new))


h = SRC / 'include/trace/hooks/thermal.h'
if not h.exists():
    h.write_text('''/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM thermal

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_THERMAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_THERMAL_H

#include <trace/hooks/vendor_hooks.h>

/* ANDROID: android16-6.12 parity */
DECLARE_HOOK(android_vh_enable_thermal_genl_check,
	TP_PROTO(int event, int tz_id, int *enable_thermal_genl),
	TP_ARGS(event, tz_id, enable_thermal_genl));

struct thermal_zone_device;
DECLARE_HOOK(android_vh_thermal_pm_notify_suspend,
	     TP_PROTO(struct thermal_zone_device *tz, int *irq_wakeable),
	     TP_ARGS(tz, irq_wakeable));

#endif /* _TRACE_HOOK_THERMAL_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
''')

V = 'drivers/android/vendor_hooks.c'
edit(V, '#include <trace/hooks/cgroup.h>\n', '#include <trace/hooks/cgroup.h>\n#include <trace/hooks/thermal.h>\n')
s = (SRC / V).read_text()
if 'android_vh_thermal_pm_notify_suspend' not in s:
    s = s.rstrip('\n') + '\nEXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_enable_thermal_genl_check);\nEXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_thermal_pm_notify_suspend);\n'
    (SRC / V).write_text(s)

C = 'drivers/thermal/thermal_core.c'
edit(C, '''	tz->state &= ~TZ_STATE_FLAG_INIT;
	/*
	 * If system suspend or resume is in progress at this point, the
	 * new thermal zone needs to be marked as suspended because
	 * thermal_pm_notify() has run already.
	 */
	if (thermal_pm_suspended)
		tz->state |= TZ_STATE_FLAG_SUSPENDED;
''', '''	tz->state &= ~TZ_STATE_FLAG_INIT;
	/*
	 * If system suspend or resume is in progress at this point, the
	 * new thermal zone needs to be marked as suspended because
	 * thermal_pm_notify() has run already.
	 */
	if (thermal_pm_suspended) {
		int irq_wakeable = 0;

		/* ANDROID: 6.12 parity, wake-capable zones keep running */
		trace_android_vh_thermal_pm_notify_suspend(tz, &irq_wakeable);
		if (!irq_wakeable)
			tz->state |= TZ_STATE_FLAG_SUSPENDED;
	}
''')
edit(C, '''	thermal_pm_suspended = true;

	list_for_each_entry(tz, &thermal_tz_list, node)
		thermal_zone_pm_prepare(tz);
''', '''	thermal_pm_suspended = true;

	list_for_each_entry(tz, &thermal_tz_list, node) {
		int irq_wakeable = 0;

		/* ANDROID: 6.12 parity */
		trace_android_vh_thermal_pm_notify_suspend(tz, &irq_wakeable);
		if (irq_wakeable)
			continue;

		thermal_zone_pm_prepare(tz);
	}
''')
edit(C, '''	thermal_pm_suspended = false;

	list_for_each_entry(tz, &thermal_tz_list, node)
		thermal_zone_pm_complete(tz);
''', '''	thermal_pm_suspended = false;

	list_for_each_entry(tz, &thermal_tz_list, node) {
		int irq_wakeable = 0;

		/* ANDROID: 6.12 parity */
		trace_android_vh_thermal_pm_notify_suspend(tz, &irq_wakeable);
		if (irq_wakeable)
			continue;

		thermal_zone_pm_complete(tz);
	}
''')
s = (SRC / C).read_text()
if '#include <trace/hooks/thermal.h>' not in s:
    # before CREATE_TRACE_POINTS (thermal_trace.h), as in 6.12
    s = s.replace('#include <linux/suspend.h>\n', '#include <linux/suspend.h>\n#include <trace/hooks/thermal.h>\n', 1)
    (SRC / C).write_text(s)

N = 'drivers/thermal/thermal_netlink.c'
edit(N, '''	struct sk_buff *msg;
	int ret = -EMSGSIZE;
	void *hdr;

	if (!thermal_group_has_listeners(THERMAL_GENL_EVENT_GROUP))
		return 0;
''', '''	struct sk_buff *msg;
	int ret = -EMSGSIZE;
	void *hdr;
	int enable_thermal_genl = 1;

	/* ANDROID: 6.12 parity */
	trace_android_vh_enable_thermal_genl_check(event, p->tz_id, &enable_thermal_genl);
	if (!enable_thermal_genl)
		return 0;

	if (!thermal_group_has_listeners(THERMAL_GENL_EVENT_GROUP))
		return 0;
''')
s = (SRC / N).read_text()
if '#include <trace/hooks/thermal.h>' not in s:
    s = s.replace('#include "thermal_core.h"\n', '#include "thermal_core.h"\n#include <trace/hooks/thermal.h>\n', 1)
    (SRC / N).write_text(s)
print('applied thermal vendor hooks to', SRC)
