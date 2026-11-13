#!/usr/bin/env python3
"""Stage the Motorola clock hold-state extension without editing active GKI."""
from pathlib import Path
import difflib
import subprocess

HERE = Path(__file__).resolve().parent
HOME_DIR = HERE.parents[2]
ACK = HOME_DIR/'avr-upstream-audit/android-7.3'
VENDOR = HOME_DIR/'avr-upstream-audit/kernel-common--drivers_clk_clk.c'

def function(source, signature):
    start = source.index(signature)
    brace = source.index('{', start)
    depth = 1
    pos = brace + 1
    while depth:
        if source[pos] == '{': depth += 1
        elif source[pos] == '}': depth -= 1
        pos += 1
    return source[start:pos] + '\n'

def once(source, old, new):
    assert source.count(old) == 1, old
    return source.replace(old, new, 1)

def main():
    original = subprocess.check_output(['git', '-C', str(ACK), 'show', '4dd306a07b2f456fd70ddaa28f4f65640374f202:drivers/clk/clk.c'], text=True)
    vendor = VENDOR.read_text()
    s = once(original, '\tbool\t\t\trpm_enabled;\n',
             '\tbool\t\t\trpm_enabled;\n\tbool\t\t\tneed_sync;\n\tbool\t\t\tboot_enabled;\n')
    for name in ('clk_unprepare_unused_subtree', 'clk_disable_unused_subtree'):
        body = function(s, 'static void __init '+name+'(')
        old = f'\thlist_for_each_entry(child, &core->children, child_node)\n\t\t{name}(child);\n'
        new = old + '\n\tif (dev_has_sync_state(core->dev) &&\n\t    !(core->flags & CLK_DONT_HOLD_STATE))\n\t\treturn;\n'
        s = once(s, body, once(body, old, new))
    release = function(vendor, 'static void clk_unprepare_disable_dev_subtree(')
    release += '\n' + function(vendor, 'void clk_sync_state(') + 'EXPORT_SYMBOL_GPL(clk_sync_state);\n\n'
    release = once(release, '\tclk_core_disable_unprepare(core);',
                   '\t/* Drop only the reference acquired by boot-state retention, once. */\n\tcore->need_sync = false;\n\tcore->boot_enabled = false;\n\tclk_core_disable_unprepare(core);')
    anchor = 'static int clk_core_determine_round_nolock('
    s = once(s, anchor, release + anchor)
    hold = function(vendor, 'static void clk_core_hold_state(')
    hold += '\n' + function(vendor, 'static void __clk_core_update_orphan_hold_state(') + '\n'
    anchor = 'static void clk_core_update_orphan_status('
    s = once(s, anchor, hold + anchor)
    s = once(s, '\t\t\t__clk_recalc_rates(orphan, true, 0);\n',
             '\t\t\t__clk_recalc_rates(orphan, true, 0);\n\t\t\t__clk_core_update_orphan_hold_state(orphan);\n')
    s = once(s, '\tcore->rate = core->req_rate = rate;\n',
             '\tcore->rate = core->req_rate = rate;\n\n\tcore->boot_enabled = clk_core_is_enabled(core);\n')
    s = once(s, '\n\tclk_core_reparent_orphans_nolock();\nout:',
             '\n\tclk_core_hold_state(core);\n\tclk_core_reparent_orphans_nolock();\nout:')
    # In 7.3 this helper is private to driver core. Keep equivalent local logic
    # rather than including drivers/base's private header from the clock core.
    helper = function((ACK/'drivers/base/base.h').read_text(),
                      'static inline bool dev_has_sync_state(')
    helper = helper.replace('dev_has_sync_state', 'clk_dev_has_sync_state')
    s = s.replace('dev_has_sync_state(', 'clk_dev_has_sync_state(')
    s = s.replace('core->boot_enabled = clk_core_is_enabled(core);',\
                  'core->boot_enabled = clk_dev_has_sync_state(core->dev) &&\n\t\t!(core->flags & CLK_DONT_HOLD_STATE) &&\n\t\tclk_core_is_enabled(core);')
    s = once(s, '#include <linux/device.h>\n',
             '#include <linux/device.h>\n#include <linux/device/bus.h>\n#include <linux/device/driver.h>\n')
    s = once(s, 'static DEFINE_SPINLOCK(enable_lock);',
             helper + '\nstatic DEFINE_SPINLOCK(enable_lock);')
    header = subprocess.check_output(['git', '-C', str(ACK), 'show', '4dd306a07b2f456fd70ddaa28f4f65640374f202:include/linux/clk-provider.h'], text=True)
    modified = once(header, '#define CLK_DUTY_CYCLE_PARENT\tBIT(13)\n',
                    '#define CLK_DUTY_CYCLE_PARENT\tBIT(13)\n#define CLK_DONT_HOLD_STATE\tBIT(14) /* Do not hold boot state until sync_state */\n\nvoid clk_sync_state(struct device *dev);\n')
    patch = ''
    for path, old, new in [('drivers/clk/clk.c', original, s), ('include/linux/clk-provider.h', header, modified)]:
        patch += ''.join(difflib.unified_diff(old.splitlines(True), new.splitlines(True), fromfile='a/'+path, tofile='b/'+path))
    (HERE/'hold-state-7.3.patch').write_text(patch)
    (HERE/'clk.c').write_text(s)
    (HERE/'compat.h').write_text('#include <linux/clk-provider.h>\n#define CLK_DONT_HOLD_STATE BIT(14)\nvoid clk_sync_state(struct device *dev);\n')
    print('Staged CCF implementation and header patch; active 7.3 source unchanged')

if __name__ == '__main__':
    main()
