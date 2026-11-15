# Native IPCC provider for AVR 7.3

Pinned native source: `4dd306a07b2f456fd70ddaa28f4f65640374f202`,
`drivers/mailbox/qcom-ipcc.c`. `scripts/audit_ipcc_sources.py` verifies the
exact source and reviewed patch replay. This is the real IRQ/mailbox driver;
production reads/writes actual MMIO and registers actual Linux controllers.

The external module has symmetric driver registration/unregistration. The
IRQ domain is managed before the mailbox and IRQ resources so devres unwinds
IRQ -> mailbox -> domain, including failed IRQ requests. Removed the original
manual double unregister and unpaired IRQ-wake disable. Mailbox channel records
are preallocated, reservations/shutdown share a spinlock, and duplicate checks
continue past holes. Requests cannot reuse a slot whose former client is still
bound. DT IDs exceeding 16 bits and the reserved all-ones pending value fail
without aliasing another endpoint. Failed DT parsing never puts an uninitialized
node reference.

Build: `python scripts/build_storage_deps.py --ipcc-tests`.
Guest: `python scripts/run_qemu.py --test ipcc --log reports/ipcc-qemu.log`.
Seven KUnit cases exercise actual translation, allocation/reuse, shutdown and
send/mask/unmask callbacks against RAM. A separate production module registers
and unregisters after the test module is removed. No physical IPCC is probed.

Saved/candidate DT validation records two active IPCC providers and the six
SMP2P edges, plus the selected SoCCP state references. Whole-board probe order,
real IRQ delivery, wakeup, active domain-consumer teardown and probe fault
injection remain unverified. This is experimental, not daily-use acceptance.
