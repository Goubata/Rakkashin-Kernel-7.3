#!/usr/bin/env python3
"""Restore the android16-6.12 android_vh_try_fixup_sea call in arm64 do_sea().

mem_hooks.ko (phone, first stage, permanent) registers android_vh_try_fixup_sea so
that a synchronous external abort taken in kernel mode (not an EL1 instruction
abort) is recovered through the exception table (e.g. a uaccess or *_nofault
access that hits secure-owned memory) instead of killing the kernel. 7.3 declares
the hook but neither calls nor exports it. 6.12 called it first in do_sea().
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
        raise SystemExit(f'{rel}: anchor count {s.count(old)}')
    p.write_text(s.replace(old, new))


edit('arch/arm64/mm/fault.c', '''	const struct fault_info *inf;
	unsigned long siaddr;

	inf = esr_to_fault_info(esr);

	if (user_mode(regs) && apei_claim_sea(regs) == 0) {''', '''	const struct fault_info *inf;
	unsigned long siaddr;
	bool can_fixup = false;

	/* ANDROID: 6.12 parity: vendor may allow exception-table fixup of SEAs */
	trace_android_vh_try_fixup_sea(far, esr, regs, &can_fixup);
	if (can_fixup && fixup_exception(regs, esr))
		return 0;

	inf = esr_to_fault_info(esr);

	if (user_mode(regs) && apei_claim_sea(regs) == 0) {''')
p = SRC / 'drivers/android/vendor_hooks.c'
s = p.read_text()
if 'EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_try_fixup_sea);' not in s:
    anchor = 'EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_die_kernel_fault);\n'
    if anchor not in s:
        raise SystemExit('vendor_hooks.c: no die_kernel_fault export anchor')
    p.write_text(s.replace(anchor, anchor + 'EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_try_fixup_sea);\n', 1))
print('applied try_fixup_sea hook to', SRC)
