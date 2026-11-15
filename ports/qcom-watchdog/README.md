# AVR watchdog / SDEI drafts

Pinned factory watchdog core, SoC register driver and SDEI helper are built
against native 7.3 timer, IRQ, minidump and firmware APIs. A required core patch
exports four existing native SDEI event APIs and adds the exact factory secure
watchdog-disable transaction (boot service, command 7, one argument equal to 1).
Missing SCM defers; this is not a substitute that reports firmware success.

The saved DT has one qcom,msm-watchdog, named wdt-base, 11s requested bark and
9360ms pet intervals, and two level-high GIC SPIs. Native IRQ trigger configuration
is preserved. There are no active qcom,sdei / arm,sdei-1.0 DT nodes in the saved
inputs; none are invented. SDEI remains a real watchdog module dependency.
Defaults come from pinned factory code / Alor reference configuration, not an
oscillator measurement or proof of the prior binary's programmed bark counter.

Per-device register maps avoid cross-instance overrides. Validate IO error
pointers, aligned resource offsets, nonzero rates and checked 32-bit tick range.
Use real timer APIs and return secure-disable errors. Pin the SDEI module while
callbacks exist, drain EINPROGRESS unregister and retain the pin on permanent
errors. A bound SDEI provider requires successful unbind before unload. Recovery
of retained registrations after failed removal remains unresolved.

Reproduce build_watchdog.py --tests, audit_watchdog_dt.py and QEMU mode watchdog.
Fourteen cases cover exact register helpers with actual raw writes to private
RAM, full SDEI source with recorded firmware APIs and native module pins, and
an exact selected SCM method with recorded transport. Firmware/PSCI/HVC calls
are never executed by those tests. Production modules load in the guest and the
unbound SoC driver unloads. The factory core has no module_exit; it and its
provider imports remain until guest shutdown. No full watchdog probe or actual
bark/bite/panic/reset, physical IRQ or suspend/CPU-ping proof is claimed.

The minidump record is not yet removed on watchdog teardown, and full thread/
IRQ/timer/sysfs/error-unwind still needs review. Factory IRQ-stat diagnostics and
minidump die-notifier variants are not enabled in this initial draft. These are
acceptance blockers. Artifacts remain separate from the Stage57 main bundle.
