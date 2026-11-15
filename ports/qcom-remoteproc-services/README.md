# Native Q6V5, sysmon and AOSS QMP services

Source pin `4dd306a07b2f456fd70ddaa28f4f65640374f202`; source files and reviewed
port changes replay through `scripts/audit_remoteproc_services_sources.py`.
The three production modules are `qcom_q6v5.ko`, `qcom_sysmon.ko` and
`qcom_aoss.ko`. Explicit build declarations select actual sysmon and QMP APIs;
Q6V5's ELF imports reference the real QMP/sysmon exports, and sysmon imports the
actual native QMI helpers. Core providers are disabled to avoid duplicate symbols.

Q6V5 deinit drains its five successfully initialized IRQs before private state
can disappear. An ICC acquisition failure drops the acquired QMP reference.
Crash messages are bounded by the actual SMEM size. Stop assertion/clear errors
propagate; an unsuccessful assertion no longer waits for an impossible ack.

Sysmon releases its managed shutdown IRQ before freeing its callback data after
QMI initialization failure or explicit removal. A missing optional shutdown IRQ
returns false without issuing an invalid IRQ query. Association changes use the
sysmon-list mutex plus the request mutex. Teardown unpublishes endpoint private
data and drains SRCU callbacks before freeing the sysmon object. Ingress rereads
the current endpoint association inside SRCU, ignoring a possibly stale callback
argument. Requests check endpoint presence after taking their mutex. Duplicate
associations fail with EBUSY. Transport endpoint ownership and remoteproc
subdevice call serialization still follow their native framework contracts.

Build: `python scripts/build_storage_deps.py --remoteproc-services-tests`.
Guest: `python scripts/run_qemu.py --test remoteproc-services --cpus 4 --log reports/remoteproc-services-qemu.log`.
Eleven cases include native state API calls, stop errors/ack/clear, completions,
missing optional IRQ, real QMI/sysmon subdevice initialization, actual ancestor
lookup/association, parallel ingress vs provider removal, 1,000 post-removal
rejections and later consumer removal. Production modules register/unregister
separately after the combined source test is unloaded. The QMP module is real,
but no QMP firmware exchange or physical probe is exercised.

This is experimental and packaged only as part of the partial port. Hardware
IRQ/resource cleanup fault injection, live transport endpoint races, QMP shared
memory/firmware, remote sysmon/QMI exchange, suspend and Alor-specific PAS/rail/
firmware acceptance remain open. No physical 7.3 boot or daily-use claim is made.
