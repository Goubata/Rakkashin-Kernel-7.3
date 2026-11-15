# Native SMP2P state provider for AVR 7.3

Pinned source: native `4dd306a07b2f456fd70ddaa28f4f65640374f202`,
`drivers/soc/qcom/{smp2p.c,trace-smp2p.h}`. Audit/replay:
`scripts/audit_smp2p_sources.py`. The production module uses the real SMEM,
mailbox, IRQ-domain and built-in SMEM-state APIs. No successful mock provider is
substituted in the production path.

Port changes bound fixed shared arrays and string reads, validate shared item
sizes/header ranges, reject excess outbound slots, restrict updates to their
mask, and drain the receiver before removing its domains/mailbox. Existing
vendor long outbound names retain the same strscpy(16) wire truncation: this
matters for the active `wlan_ep_power_save` DT entry. Restart history is reset
only on a changed restart bit for V2, preserving the pinned vendor V1 behavior.

Requires `patches/smem-state-lifetime-7.3.patch` in the native core: unpublishes
orphan states and drains update callbacks before provider-private memory can
be released. Unregister must run in sleepable context outside state callbacks.
Client handles still need their normal get/put ownership.

Build: `python scripts/build_storage_deps.py --smp2p-tests --smem-state-tests`.
Guests: `scripts/run_qemu.py --test smp2p` and `--test smem-state`, both with
`--log ...`. Eight SMP2P cases cover real callbacks on RAM/regmap, the built-in
state registration API, version/features, SSR histories, IRQ line-state reads,
array/name bounds and actual outbound registration. Three separate tests use
real DT client lookups and native state APIs, including concurrent callback
removal. A separate production SMP2P module registers/unregisters without any
Qualcomm device probe in QEMU.

Selected saved/candidate DT references and the pinned vendor/native C wire
layout match. This does not establish full firmware or vendor-driver
compatibility. Physical SMEM contents, IRQ/IPCC delivery, early attach,
suspend/wakeup, consumer/domain lifetime stress and all remote services remain
pending. No physical 7.3 boot or daily-use acceptance exists.
