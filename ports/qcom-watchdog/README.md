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

Stage85 stores the exact owned minidump descriptor, removes it after stopping
watchdog work, and frees the SoC allocation through a native devres action only
when no dump registration remains. If removal fails, retain the actual payload
allocation so the registered physical address stays valid. A failed add does not
permit removing another owner's same-name record. ENOENT confirms absence;
repeated cleanup is inert. Retained payloads have no automatic reclamation path.

Twenty-one guest cases include seven new lifetime cases. Two use the actual
pending/unready native minidump provider, including an injected remove failure;
others cover recorded ownership failures, duplicates, absent records and actual
devres probe-allocation release. This does not exercise initialized SMEM tables,
full watchdog teardown, callbacks or physical reset. Full thread/IRQ/timer/sysfs/
notifier/PM/error-unwind still needs review. Factory IRQ-stat/die-notifier variants
and retained payload reclamation remain pending. Artifacts stay separate from
the Stage57 main bundle.


Stage86 tracks IRQ/notifier/timer/sysfs ownership separately from the hardware
counter's enabled state. Every SPI-path initialization failure unwinds acquired
resources; removal also works after suspend or permanent disable. Use stop-aware
worker waits and serialized hardware access, native timer shutdown, an atomic
sysfs group/readiness guard and singleton registration locking. Keep the platform
IRQ mapping owned by its platform/domain instead of disposing it in this driver.

Thirty guest cases pass. Nine new cases compile the full core with exported names
renamed only and recorded hardware/secure operations. Native private IRQ actions,
notifier chains, sysfs, timers, CPU IPIs, thread creation/petting and stop are real.
Check ten registration/hardware failure points and thread creation failure, plus
paused/disabled/unready/user-pet-wait removal and repeated registration. Native
IRQ reclamation and absence from notifier/sysfs registries are checked directly.
This does not prove real bark/bite, PPI, firmware, exhaustive concurrency or PM
hardware-error recovery. Retained dump reclamation remains pending.
