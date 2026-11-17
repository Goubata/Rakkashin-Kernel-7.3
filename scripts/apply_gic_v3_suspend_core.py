#!/usr/bin/env python3
"""Restore the android16-6.12 GICv3 suspend hook and exports on 7.3.

msm_show_resume_irq.ko (loaded on the phone, qcom,show-resume-irqs) saves GIC
distributor/redistributor state through the restricted vendor hook
android_vh_gic_v3_suspend (called from a GICv3 syscore suspend callback) and,
on resume from hibernation or mem_sleep=deep (PM_SUSPEND_MEM), re-initialises
the GIC with the exported gic_v3_dist_init(), gic_v3_cpu_init() and
gic_v3_dist_wait_for_rwp() before restoring the saved enables/pending/config.
The phone defaults to s2idle but offers "deep", so without this a deep suspend
on 7.3 would lose interrupt configuration. 6.12 ACK provided all of it;
android18-7.3 has none. gic_dist_init() loses __init (it only touches
__ro_after_init data and non-init helpers), as on 6.12.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')


def edit(rel, old, new, count=1):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != count:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old, new))


H = 'include/trace/hooks/gic_v3.h'
edit(H, '''struct irq_data;
struct cpumask;''', '''struct irq_data;
struct cpumask;
struct gic_chip_data;''')
edit(H, '''#endif /* _TRACE_HOOK_GIC_V3_H */''', '''/* ANDROID: 6.12 parity: called from the GICv3 syscore suspend callback */
DECLARE_HOOK(android_vh_gic_v3_suspend,
	TP_PROTO(struct gic_chip_data *gd, int *ret),
	TP_ARGS(gd, ret));

#endif /* _TRACE_HOOK_GIC_V3_H */''')

edit('drivers/android/vendor_hooks.c', 'EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_gic_v3_set_affinity);\n',
     'EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_gic_v3_set_affinity);\nEXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_gic_v3_suspend);\n')

G = 'drivers/irqchip/irq-gic-v3.c'
edit(G, 'static void __init gic_dist_init(void)', 'static void gic_dist_init(void)')
edit(G, '''static void gic_cpu_pm_init(void)
{
	cpu_pm_register_notifier(&gic_cpu_pm_notifier_block);
}

#else
static inline void gic_cpu_pm_init(void) { }
#endif /* CONFIG_CPU_PM */
''', '''static void gic_cpu_pm_init(void)
{
	cpu_pm_register_notifier(&gic_cpu_pm_notifier_block);
}

#else
static inline void gic_cpu_pm_init(void) { }
#endif /* CONFIG_CPU_PM */

/* ANDROID: 6.12 parity for vendor deep-suspend/hibernation GIC restore */
void gic_v3_dist_init(void)
{
	gic_dist_init();
}
EXPORT_SYMBOL_GPL(gic_v3_dist_init);

void gic_v3_cpu_init(void)
{
	gic_cpu_init();
}
EXPORT_SYMBOL_GPL(gic_v3_cpu_init);

void gic_v3_dist_wait_for_rwp(void)
{
	gic_dist_wait_for_rwp();
}
EXPORT_SYMBOL_GPL(gic_v3_dist_wait_for_rwp);

#ifdef CONFIG_PM
static int gic_v3_suspend(void *data)
{
	int ret = 0;

	trace_android_vh_gic_v3_suspend(&gic_data, &ret);
	return ret;
}

static const struct syscore_ops gic_syscore_ops = {
	.suspend = gic_v3_suspend,
};

static struct syscore gic_syscore = {
	.ops = &gic_syscore_ops,
};

static void gic_syscore_init(void)
{
	register_syscore(&gic_syscore);
}
#else
static inline void gic_syscore_init(void) { }
#endif
''')
edit(G, '''	gic_cpu_pm_init();

	if (gic_dist_supports_lpis()) {''', '''	gic_cpu_pm_init();
	gic_syscore_init();

	if (gic_dist_supports_lpis()) {''')
s = (SRC / G).read_text()
if '#include <linux/syscore_ops.h>' not in s:
    s = s.replace('#include <linux/slab.h>\n', '#include <linux/slab.h>\n#include <linux/syscore_ops.h>\n', 1)
    (SRC / G).write_text(s)
h = SRC / 'include/linux/irqchip/arm-gic-v3.h'
t = h.read_text()
decl = '''/* ANDROID: exported for vendor GIC restore after deep suspend/hibernation */
void gic_v3_dist_init(void);
void gic_v3_cpu_init(void);
void gic_v3_dist_wait_for_rwp(void);
'''
if decl not in t:
    i = t.index('#ifndef __ASSEMBLER__')
    j = t.index('\n#endif\n', i)
    h.write_text(t[:j + 1] + '\n' + decl + t[j + 1:])
print('applied GICv3 suspend hook/exports to', SRC)
