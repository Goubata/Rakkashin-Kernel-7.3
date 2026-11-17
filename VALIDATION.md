# avr / OnePlus port ROM: completion criteria

A successful build is not a runtime pass. No 7.3 kernel has been booted on this
phone yet. No partition has been written. Runtime rows below are all pending.
The current 6.12 environment is the reference for behavior, not a 7.3 test.

| Area | Required verification on 7.3 |
|---|---|
| Boot / storage | Android reaches the launcher; UFS, F2FS, encryption and unlock work |
| Display / touch | Panel modes, brightness, touch, gestures and screen-off wake work |
| GPU | Hardware rendering and sustained 3D loads work without resets |
| CPU / memory | Frequency scaling, scheduler, memory pressure and zram are stable |
| Power / thermal | Charging, battery reporting, thermal throttling and power limits work |
| Suspend | Repeated suspend/resume succeeds; idle drain is compared with 6.12 |
| Cellular | SIM, mobile data, calls, VoLTE and call audio work for the user's carrier |
| Wi-Fi / Bluetooth | Connectivity, reconnection, tethering and Bluetooth audio work |
| Camera | Available cameras, photo/video capture, autofocus and flash work |
| Audio | Speaker, earpiece, microphones, recording and USB/Bluetooth routing work |
| Sensors / location | Rotation, proximity, light, GNSS and sensor services work |
| Biometrics / NFC | Fingerprint unlock and available NFC functions work |
| USB | Charging, data, ADB and OTG work |
| ROM integration | OnePlus port services, SELinux policy and vendor HAL interfaces work |
| Rakkashin features | Root integration and existing kernel control features are ported and tested |
| Reliability | Reboots, repeated unlocks, long workloads and overnight idle show no regressions |

## Evidence so far

- Native 7.3 Image and bundled modules built successfully on the phone.
- 36 Qualcomm foundation / OxygenOS modules built and linked. Exact snapshot:
  reports/boot-core-stage2.json; artifacts/android73-boot-core-stage2/.
- CCF hold-state and IRQ/SCM ports are integrated into the current Image.
- KUnit found unsafe registration-time hardware reads and repeated retention
  release consuming a real consumer's reference. Both were fixed and retested.
- The unmodified 7.3 baseline reproduced a Tasks RCU SMP boot stall. A proxy
  donor was sleeping on a mutex while retained on the runqueue. A local fix
  checks its switched-out state under task/runqueue locks; proxy execution stays
  enabled. This fix is local, not an upstream-reviewed change.
- 29 clock/RCU cases passed on SMP2 and SMP4, including a blocked donor and a
  runnable reader that must remain a holdout. This is targeted regression
  evidence, not a substitute for RCU torture/stress or real-device testing.
- OxygenOS sync module passed 1,283 checks on SMP4 with the current Image.
- Evidence hashes and results: reports/runtime-test-results.json.
- KGSL now links as msm_kgsl.ko with the real Gunyah transport/RM, SYNX,
  IPCLite, hardware-fence and SCM providers. Stage9 has 71 modules with 3,618
  import CRCs matching, and all declared hard/soft dependency names present.
  GPU rendering, DMA/protected-content and firmware operation are unverified.
  The final Image passes 71 KUnit cases across five diskless SMP4 suites;
  reports/gpu-stage9-validation.json records their hashes and exact scope.
  The AVF external module configuration is checked against actual saved
  binaries and vendor configuration. Boost cap and min-level error propagation
  fixes are included. Expiry/concurrency and shutdown lifecycle review remain.
- 32 firmware-matched official repository refs found. A vantage camera source
  with canoe configuration is a candidate; exact avr compatibility is unproven.

## Still required

Storage/encryption, power, IOMMU/interconnect, GPU/display, modem/network, audio,
camera and all runtime rows above remain pending. No efficiency claim is made
until measurements against 6.12 are available. Preparing a physical boot also
requires a concrete restoration plan and authorization for that operation.

## Additional GPU support evidence

- Ten additional support modules built: hwspinlock, SMEM, SoCinfo, LLCC, two
  DCVS modules, three governors and qcom_iommu_util. Stage4 has 46 partial-port
  modules, with all 1,686 recorded import CRCs matching current providers.
- The SCM module implements 11 vendor GPU/DCVS methods. Firmware availability,
  secure memory transport and actual firmware command execution are unverified.
- SMP4 guest loaded 15 support modules, unloaded nine exit-capable drivers and
  retained six permanent providers/dependencies. This does not exercise probes
  for absent Qualcomm hardware.
- Three actual software page-table cases passed for FAST, LPAE and a contiguous
  group with a partial unmap/remap. Hardware DMA/TLB/cache behavior remains open.
- The physical ARM SMMU driver now compiles/links and loads in a guest without
  Qualcomm hardware. Actual controller probe, domain attachment and DMA remain
  pending; symbol resolution does not prove hardware operation.
- SoCinfo firmware offsets/bounds and publication lifetime need a broader audit;
  the vendor LLCC EDAC/perfmon consumers need its explicitly named private layout.

## Stage5 SMMU and secure-buffer evidence

- Core patch `patches/scm-memory-batch-7.3.patch` uses target TZMEM to marshal
  wire descriptors for one atomic firmware batch command. It also fixes the
  native assignment ownership mask for VMIDs 32..63. Actual firmware behavior
  and the SMCI/PIL transport remain unverified.
- ARM SMMU uses the 7.3 attach callback, per-device page-size masks and a flags
  allocator preserving the vendor stage-2 nesting-parent path. Unsupported
  flags/user data are rejected; no nested-child allocator is advertised.
- Logger registrations hold device/group references until unregister. Secure
  descriptors remain CPU-owned until copied to TZMEM; obsolete DMA mappings
  of those descriptors were removed. Driver readiness is cleared on remove.
- Secure-buffer rollback now uses the destination array's actual element count,
  the exact completed region prefix and bounded batches. First-call failure
  no longer divides by zero. Only the assigned prefix gets ownership flags.
- Five actual-code tests passed in SMP4 RAM-only QEMU, mocking only SCM:
  `reports/qemu-secure-buffer-final.log`. Guest SCM ownership is never changed.
- `reports/qemu-smmu-stage5-modules.log`: 19 module loads, 12 unloads, seven
  permanent dependencies retained, with no warnings/panic/stall detected.
- `artifacts/android73-smmu-memory-stage5/`: Image + 49 modules. All 1,912 import
  CRCs and snapshot SHA256 values verified. KGSL remains an object, with ten
  unresolved imports; there is no final GPU .ko or flashable boot image.

## Stage6 system statistics evidence

- The full Motorola sysstats implementation builds without the original
  NONEXPORTED_SYMBOLS zero-statistics option. Native DMA-buffer identification,
  si_swapinfo and NR_VMALLOC provide real accounting. The UAPI is retained.
- GPU callback registration/replacement/unregister now drains SRCU readers.
  Four actual-code KUnit cases cover values, NULL fallback, live memory usage
  and sleeping-reader teardown: reports/qemu-sysstats-final-tests.log.
- The first userspace Netlink test crashed the guest because a family without
  a policy receives NULL info->attrs on 7.3. Setting the family policy and
  validating PID presence fixes it. The failed log is preserved as
  reports/qemu-sysstats-stage6-modules.log; the successful rerun is
  reports/qemu-sysstats-stage6-modules-fixed.log.
- The successful run loads 20 modules, performs four ABI checks (resolve family,
  memory counters, own PID stats, missing-PID rejection), unloads 13 modules
  and retains seven permanent providers. No phone module or partition changed.
- Stage6 artifact: 50 modules, 1,980 matching import CRCs, SHA256 verified.
  GPU modpost still reports eight unresolved symbols (SYNX and PAS retry).
  No final KGSL module, flashable boot.img or physical boot is claimed.

## Unfinished SYNX and hardware-fence provider chain

- All target C sources for synx-driver, ipclite and msm_hw_fence compile into
  aggregate objects. Final modpost fails on eight remoteproc/Gunyah names;
  none is counted in the 50 linked support modules or staged artifacts.
- Exact hardware-fence source is pinned to the firmware's mm-drivers commit;
  SYNX source comes from the user's archive and still needs firmware ABI review.
- Five fence compatibility cases passed in RAM-only SMP4 QEMU, covering ALL/ANY
  before and after callback enabling and repeated-signal error handling:
  reports/qemu-fence-compat-tests.log. This does not test hardware fences or IPC.
- reports/qti-fence-dependencies.json inventories aggregate exports using both
  old __ksymtab and current __export_symbol metadata. Its eight missing names
  agree with actual modpost. There are no synthesized successful providers.

## Stage7 Rakkashin core and scheduler evidence

- Core source commit 4b563b761; replay patch patches/rakkashin-features-7.3.patch.
  Source diff/reverse-apply checks pass. Incremental consolidated native build
  with --gpu-deps --qemu-tests --rakkashin completed successfully; a fresh clean
  checkout and official Kleaf certification remain unverified.
- The final 7.3 Image boots four CPUs with the profile, FAS and wake filters.
  reports/qemu-rakkashin-final-features.log: 122 checks cover all five modes,
  PELT/writeback settings and invalid-input preservation, per-UID foreground
  selection, actual heavy-thread uclamp headroom and restoration, launch boost
  expiry/fork isolation, both ntsync creation ABIs and no fd leak on copyout
  fault, a real file-backed ARM64 uprobe and watchdog events, and FAS root-only
  access. Poll/read checks contribute to this count; it is not 122 features.
- reports/qemu-rakkashin-final-ntsync.log: thirteen upstream cases pass, including
  40,000 mutex-protected non-atomic increments by four threads. The checked -O2
  fixture has explicit compiler fences and checks syscall/thread creation errors.
  The original -O2 test cached its counter before the loop and failed 12/13;
  original -O0 and checked -O2 both pass 13/13. Assembly evidence, hashes and
  failure logs are retained in reports/ntsync-stress-investigation.json.
- reports/qemu-rakkashin-final-wakelock.log: five KUnit cases use real wakeup
  sources and PM activation paths for user/default filters, unmatched, prefix
  and long names. These do not prove phone notification/suspend behavior.
- ADIOS originally generated a slab_mutex/elevator_lock lockdep cycle during
  selection. Slab caches now live at module scope, outside queue locks.
  reports/qemu-rakkashin-final-adios.log: three 4-worker direct-I/O rounds,
  6 MiB total written and read back, depth 64/128/64, scheduler changes and
  unload pass with no warnings. The test disk is guest RAM only, not phone UFS.
- reports/qemu-rakkashin-clock-rcu-memory-regression.log: all 35 existing cases
  pass on the new Image, including proxy-exec Tasks RCU and real guest-zram
  shmem reclaim/readback.
- artifacts/android73-rakkashin-stage7/: 52 modules, 2,095 matching import CRCs;
  all SHA256 values checked. This is a test/partial-port artifact, not boot.img.
- KernelSU-Next's external source/commit is absent from the reference and has
  not been ported. Actual Android services, GPU/firmware, all hardware, SELinux
  enforcing, performance and idle drain remain pending. No partition writes.

## Stage10 notifier ownership

The vendor RM adapter now unwinds partial SCM/hyp and RM/QTVM registration,
unregisters callbacks and drains work on failure/removal, and balances OF node
references. Eight diskless SMP4 cases exercise the production orchestration
helper with real kernel notifier chains, including registration failure and an
in-flight callback. This does not verify physical VM or hypervisor teardown.
The stage10 snapshot has 71 modules with 3,622 import CRCs checked. Base Image
and stage9 test scope remain recorded separately; see gpu-stage10-validation.json.

## Stage13: static DT coverage and UFS PHY modules

The explicit DSP/EVA reference profile plus saved GPU and memory overlays now
has 5,102 nodes with no missing/extra node paths against the saved static DT.
This is not full property, binding, firmware or hardware acceptance: 240 nodes
still have unresolved reference/property differences. Selected DSP/CVP nodes
match within the known comparison encodings; the GPU table matches exactly.
Twenty Python checks cover reference renumbering, equal numeric handles pointing
elsewhere, changed range arguments, memory pool size and display heap targets.

Two firmware-pinned UFS PHY modules build unmodified and pass insertion/removal
in the diskless SMP4 guest, which has no matching hardware. Stage13 has 73
modules / 3,700 import CRC checks. This provides no UFS training, data I/O,
wrapped-key compatibility or encrypted unlock evidence. Host/ICE integration
and physical tests remain required. See `reports/storage-stage13-validation.json`.

## Stage14: vendor ICE V2 and modular SHM Bridge

Stage14 retains the firmware's V2 key commands and uses the target TZMEM pool.
SCM and TZMEM share a module in SHM Bridge mode; both dependency DAGs pass,
73 modules / 3,727 import CRCs agree, and stale standalone TZMEM artifacts are
excluded via Kbuild selection. Eleven native patches reproduce 54 files exactly
from the pinned upstream base. Twenty-four host Python checks pass.

On the new Image, SMP4 diskless guest tests pass: ICE 10, SCM/PAS 13 and secure
buffer 5. Coverage includes production descriptor/slot/retry/readiness helpers,
module insertion/removal and no-firmware/error guards. It excludes real TZMEM
bridge registration, BIST, secure key programming, reset/keytable behavior,
UFS data I/O and encrypted unlock. Those remain acceptance requirements.

## Stage15: emulated UFS I/O and vendor host adaptation

On Image `bef4d9d933dba51faeec9cee4850ce7ed13ba9edf5e475f0b7a83071ae50287d`,
the real native PCI UFS core completed two direct-I/O pattern rounds in both
single-queue and MCQ configurations. Each mode wrote/read 8 MiB, independently
verified the final 4 MiB in a new private backing file, and passed three KUnit
cases for controller/IRQ/queue state. The phone's storage was never attached.

The vendor host C sources compile after API adaptation. Four logging/minidump
exports still prevent final link, so those modules are not in the snapshot.
Stage15 retains 73 linked modules / 3,727 import CRC checks. Core patch replay
now reproduces 12 patches / 56 files. No Qualcomm PHY, real key programming,
encrypted unlock or physical stability/performance/power evidence is implied.

## Stage16: linked Qualcomm UFS host and real diagnostic dependencies

The firmware-pinned SMEM minidump and IPC logging ports provide the four
previously missing exports. The real vendor UFS host and QTI crypto profile
now link; the partial snapshot contains 77 modules / 4,168 import CRC checks.
No saved 6.12 module binary was substituted or re-signed to claim compatibility.

On the new Image, SMP4 guest tests cover five actual symbol-lookup cases and
eight pre-probe registry/IPC cases, including exact 200-user-region capacity,
duplicate rejection, cancellation, invalid names/ranges, decoded log contents
and repeated context creation/destruction. The Qualcomm host and crypto modules
load and unload without a matching physical device. Native PCI UFS SQ and MCQ
again pass three cases each and 8 MiB writes / 8 MiB reads per configuration;
the final 4 MiB private backing pattern is independently checked on the host.
Core patch replay reproduces 13 patches / 58 exact files.

Still unverified: actual SMEM table publication/firmware consumption, live
backend registry removal, panic collection, Qualcomm PHY/host/ICE probe,
encrypted unlock, power/reset transitions, active interrupt/hotplug teardown
and all physical daily-use criteria above. IPC retains memory if unregister
fails and has no deferred reclamation. Its original firmware module has no
module_exit; tests do not claim unload support. See the Stage16 report.

## Stage17: regulator providers and debugfs lifetime

The firmware-pinned RPMh, debug-control and existing logical-regulator drivers
link, giving 80 staged modules and 4,285 matching imports on the Stage16 Image.
All 72 selected regulator provider paths have matching compatible values in
the saved and candidate DTs. This does not validate their bindings/probe/voltage.

An explicitly synthetic QEMU rail executes the real vendor logical driver and
native regulator framework. Five KUnit cases cover balanced nested enables,
voltage constraints/enumeration, modes and load-driven modes. A retained open
voltage descriptor reproduced an actual post-unbind crash; after the fix its
read fails safely. Rebind, consumer-held module reference rejection, final
unbind and unload also pass. Real RPMh hardware is not exercised by that test.
The guest DT overlay is never included in an AVR device tree or snapshot.

Physical RPMh acknowledgement/aggregation/rollback, voltage levels, proxy votes,
suspend, concurrent teardown and all whole-device acceptance remain pending.
A diagnostic consumer holds its regulator provider module until device unbind;
the runner explicitly tests this reference rather than forcing module removal.

## Stage18: Alor interconnect software and candidate DT route

Five additional real modules link; the unchanged Stage16 Image now has an
85-module partial snapshot / 4,405 matching imports. The fixed Motorola common
binding supplies five private vote buckets rather than silently using three.
Nine SMP4 KUnit cases execute actual aggregation, boot floors/reset, performance
tags, local scaling, commit error propagation and native xlate. Diagnostic trace
controls and unload/reload of the three removable libraries pass. Original Alor
and BCM voter modules have no module_exit and remain resident; their expected
unload rejection is tested, not bypassed.

Static source checks find 231 node IDs, no missing link IDs and a maximum of two
BCMs per node (capacity three). The saved DT contains one USB usb-ipa reference
to cnoc_cfg[523], and the saved ELF has no relocation and a zero pointer in that
slot. Its cnoc_main[523] resolves to qhs_ipa. An explicit candidate overlay changes
only that endpoint provider, keeping IDs/other paths intact. Source topology has
a path from MASTER_USB3 to SLAVE_IPA_CFG. All 426 candidate references across 14
providers are in the selected tables; original saved inconsistencies remain in
the report. The one-property repair preserves the saved GPU21 table and memory.

This is no proof of physical USB/IPA, NoC/CRM/RPMh, QoS, firmware or performance.
Voter handle/removal races, static BCM probe rollback, QoS error/vote rollback,
consumer-list concurrency, whole DT/boot/DLKM/ROM alignment and all real-device
acceptance still remain. Candidate DT is separate and not approved for packaging.

## Stage19: SPMI/PMIC providers and QTI diagnostics

Four real vendor modules link, giving 89 modules / 4,567 matching imports on the
unchanged Stage16 Image. IRQ APIs, managed teardown order, mapping publication,
OF/device references, controller names and PMIC metadata lifetime are adapted.
The actual QTI manual unregister used the wrong release key and did not remove
files; a retained-fd test reproduced that and now confirms removal. Custom data
access uses the full debugfs proxy, and only owned files/references are removed.

Five SMP4 KUnit cases execute RAM regmap operations/bounds, mapping validation
and the native SPMI framework against a recording test controller, including
length rejection and status propagation. Actual QTI diagnostic reads/offsets,
readonly rejection, retained-fd rejection, preservation of core regmap files and
four-module unload pass. No physical PMIC register was touched by those tests.
No recording controller/RAM test backend is packaged as a hardware provider.

The strict DT audit deliberately remains failed: SID4 is duplicated in the saved,
running and candidate trees. Reconstructed child order initially selected a
different node than running. Read-only 6.12 sysfs matches the running selection.
An exact raw-subtree permutation preserves running order without changing any
node/property bytes, header/reserve map/strings, GPU21 or memory definitions.
Native DTC accepts it. The duplicate is not hidden or guessed away. Selected DT
shapes include 23 PMIC nodes, max SID13 and 28 IRQ references; these are not
physical probe counts. Native 16-SID child enumeration covers those selected IDs.

Pending: silicon/APID/EE/security ownership, actual transactions/IRQ delivery,
returned mapping consumer ownership, concurrent PMIC/metadata/debug teardown,
duplicate SID resolution, charging/thermal/GPIO dependencies and whole-device
boot, ROM, stability, performance and power acceptance. No flashable image exists.

## Stage20: actual managed regulator unregister

Before the fix, seven logical-rail tests produced five passes and two failures:
the proxy enable vote stayed active and diagnostic files stayed present after
managed unregister. Matching the actual devres release key and freeing with the
list mutex already held fixes both without recursive locking. After the fix all
seven cases, repeated register/unregister, restored diagnostics and the existing
retained-fd/unbind/rebind/reference/unload checks pass. Snapshot remains 89 modules.
This is not evidence of physical rail behavior or fault recovery.

## Stage21: PMIC GPIO callback source and real modules

Two real GPIO/MPP modules link and load/unload on the unchanged Stage16 Image,
for 91 staged modules / 4,649 matching imports. GPIO callbacks use native level
configuration and integer error returns. Physical pin translation is bounded,
missing parent regmap defers probe, and sleeping capability follows that regmap.
The direction functions exactly match pinned native 7.3, including CMOS versus
open-drain/source input-buffer handling. Input sampling preserves the output
request latch rather than overwriting it with the observed line value.

Seven RAM-backed cases compile the actual GPIO callback source in a test-only
translation unit with platform-driver registration disabled. A real gpiolib chip
and regmap exercise indices, values, error propagation, direction transitions,
open-drain readback/request preservation and invalid voltage-source rejection.
They do not exercise production OF probe, SPMI transactions or physical pins.
Test doubles/fixtures are excluded from hardware artifacts. MPP callbacks/probe
are not covered; the selected DT has zero MPP providers. Nine selected GPIO
providers, three qualified GPIO references and seven configuration pin names
have valid nominal indices; valid-hole/subtype/electrical/IRQ acceptance is pending.

Whole-DT validation still fails on the original duplicate SID4. Concurrent cache
access, partial-write recovery, physical PMIC/GPIO/IRQ/suspend and all whole-phone
acceptance remain open. No partition write or physical 7.3 boot was performed.

## Stage22: real ADC and PMIC thermal modules

94 staged modules / 4,733 matching imports. The firmware-pinned conversion
library, GEN3/GEN4 ADC and PMIC temperature alarm link on the unchanged Image.
Twelve isolated real-code cases cover voltage/resistance vectors, temperature
units, both generations' NTC monotonicity and threshold roundtrips, invalid
arguments/DT parameters, client registration/polarity and queue cleanup with a
worker acquiring the ADC mutex. The real driver loads after the source-including
test module leaves. A missing explicit queue affinity produced a warning and was
fixed in the actual queue factory; strict final checks require no warning.

The saved/candidate DT both contain 35 active ADC channels (20 GEN3, 15 GEN4),
12 temperature alarms and six qualified IIO references covered by these ports.
This is table/specifier coverage, not register access, calibration or overheat
protection evidence. Original conversion tables and generation constants stay
unchanged. Raw client handles, callback/context lifetime, hardware transaction
rollback and alarm trip/suspend fault injection remain open. No physical 7.3
boot or daily-use acceptance exists; duplicate SID4 still fails the whole-DT audit.

## Stage23: alarm fault and repeated-restore regression cases

Twenty isolated cases pass on SMP4 (previous 12 plus eight real alarm-source
cases with RAM regmap/native thermal zones). Four new cases failed on Stage22:
critical-trip cache publication before a failed write, repeated DAC and LITE
stage enumeration starting at index 3, and initialized=true after failed alarm
enable. All four now pass. Tests also cover GEN2 revision shutdown guards, DAC
limits, LITE register-bit preservation and rounding, hysteresis and read errors.
Restore initializes before requesting the IRQ and preserves the DT trigger;
partial multi-trip write rollback, concurrent updates and physical IRQ/PM paths
are not proven. Artifact remains 94 modules / 4,733 matching imports. No physical
7.3 boot, temperature-safety acceptance, partition write or daily-use claim.

## Stage24: consumer and notification lifetime

Twenty-four actual-code cases pass. New cases bind real synthetic platform
supplier/consumer drivers, verify managed-link consumer-first removal and
busy/unbound deferral, check request copying, execute self-disabling callbacks,
and use a blocked callback plus a separate disable kthread to verify external
disable waits for the callback. Only test transports complete RAM requests
immediately. No test provider enters the phone artifacts. Physical consumer
contracts, inactive-record retention, ID policy, hardware rollback and all
physical ADC/charging/thermal acceptance remain open.

## Stage25: native local IPC dependencies and QMI bounds

99 staged modules / 4,975 matching imports include real native QRTR, QRTR-SMD,
QMI, PDR messages and PDR helper. The original Android common base is retained;
15 local patches replay to 60 exact source files. A real truncated-TLV test
failed before QMI frame/TLV/basic/length/string bounds fixes. Six native QMI
cases now pass; 128 packets pass between real local AF_QIPCRTR sockets. Native
modules unload in dependency order. The new Image also passes 24 ADC and 35
clock/RCU regression cases. No remote endpoint or phone block device is exposed.

Firmware-pinned vendor GLINK source compiles but is explicitly a draft excluded
from artifacts until client/provider/receive/removal lifetime issues are fixed
and tested. GLINK transport, remoteproc/PDR services, battery/charging, QMI schema
stress and all physical safety/boot/whole-phone acceptance remain open.

## Stage26: vendor GLINK client callback lifetime

Six actual vendor-source fixture cases pass alongside six native QMI cases and
128 local QRTR packets. Driver init/exit are suppressed only in the test TU;
no physical firmware provider is registered. Per-client SRCU is acquired under
the IDR mutex and drained before free; RX/state callbacks run outside that mutex
and serialize on a callback mutex. Self-unregister returns EDEADLK. Tests cover
input/disconnected-TX handling, dispatch/state, duplicate IDs, RX/state self
unregister and external unregister waiting for an in-flight RX callback.

The real module links and its import CRCs match, but provider/rpmsg references,
init/notifier/RX/child/IPC teardown and physical transport remain unaccepted.
Hardware artifacts stay at Stage25 (99 modules); vendor GLINK is excluded.
Caller TX/unregister serialization and all charging/full-phone criteria remain.

## Stage27: RPMSG ingress and detach

Nine vendor-source cases plus six native QMI cases and 128 local QRTR packets
pass. Removal unpublishes driver data, drains module-lifetime ingress SRCU,
cancels init work and flushes queued RX. Late RX returns ENOTCONN; repeated
remove is harmless. One test holds ingress with no RX work; another blocks a
real client callback, so ingress and work draining are checked independently.
Native RPMSG bus probe/provider lifetime and charging remain unverified. The
vendor module stays excluded from the unchanged 99-module Stage25 artifacts.

## Stage28: retained supplier and managed transport dependency

Ten vendor-source cases plus six native QMI cases pass with 128 QRTR packets.
Supplier lookup holds a device reference; a helper locks/verifies the bound
supplier and channel before installing a managed dependency. A real platform
consumer/supplier pair verifies consumer-first unbind and calls the actual
transport/provider remove functions on software resources. This is the link
helper and teardown, not native RPMSG hardware bus probe. Parent removal now
unpublishes/drains registrations, cancels init, stops notifiers, drains RX,
removes children while IDR/logging live, and destroys IPC last. Managed public
client release and real OF/notifier/firmware behavior remain pending; no new
hardware module is packaged yet.

## Stage29: managed clients and experimental GLINK module

Public probe-time registration installs both dependency and managed release.
Explicit unregister removes the managed action, preventing double free. Active
callback task scopes reject cross-client synchronous unregister as well as self
unregister. Thirteen vendor-source cases plus six native QMI cases and 128 real
local QRTR packets pass; the production module then registers/unregisters with
native platform/RPMSG buses in the hardware-free guest. Hardware probe is not
exercised. Managed platform consumers verify fallback release before supplier
remove, and explicit release followed by managed teardown.

Stage29 now includes the experimental module: 100 modules / 5,054 import CRCs
match. Saved/candidate DT has two covered SOCCP channels and eight active child
services. Native has no alor SOCCP PAS compatible; firmware handover, SMEM
transport, child services, physical OF/notifier/charging behavior, allocation
fault injection, stress and all full-phone criteria remain incomplete.

## Stage30: selected remoteproc transport availability

Two actual-source cases fail before the project error policy (both disabled
transport APIs return NULL and startup reports zero). Both pass after returning
EOPNOTSUPP for NULL edges. Valid edges and ERR_PTR handling are unchanged.
The separate guest does not load production common simultaneously with its
source-including test module. GLINK/QMI 19-case, QRTR 128-packet and production
GLINK registration regressions pass again. Stage30 retains 100 modules / 5,054
matching import CRCs. This is an availability gate, not firmware communication.

The next source review records existing native early-boot attach support for
other SoCCP targets and matching PAS/firmware fields. Alor compatible, regulator
cx/mx versus native supply/domain handling, IRQ, SMEM/state/mailbox/handover and
secure ABI still need implementation/validation. No compatibility alias, phone
partition write, firmware boot or daily-use claim is made.

## Stage31–32: actual GLINK SMEM/control transport

103 staged modules / 5,216 matching imports. Real native GLINK/SSR, SMEM transport
and RPMSG control libraries precede common helpers; actual ELF imports confirm
register/unregister/SSR are not disabled-header paths. Explicit external flags
select these real APIs while avoiding duplicate native core providers.

Eleven guarded-RAM actual-source cases pass, including VERSION/ACK wire vectors,
real control-driver binding, FIFO wrap/padding, notification reserve, invalid
metadata/access and size overflow, unsupported-feature exclusion and removing a
blocked response worker. Pre-fix tests reproduce index errors and cancel-before-
abort deadlock. Only the doorbell is recorded; physical shared memory is absent.
Related SMP4 regressions pass: remoteproc2, PMIC/QMI19, SSR/Gunyah10 and QRTR128
packets. Real transport modules register/unregister after the source test leaves.

Five active DT edges retain semantic providers/arguments despite phandle numeric
renumbering. Three SMEM IDs and 15 common wire IDs match pinned vendor source.
Vendor command10/zero-copy is not implemented or advertised; complete protocol,
partition19/coherency/IRQ/mailbox/SoCCP/firmware and all phone criteria remain open.


## Stage33–35: actual IPCC/SMP2P and drained SMEM-state callbacks

105 real staged modules / 5,330 matching import CRCs; dependencies and hard/soft
graph are complete and acyclic within this partial stage. Image #22 hash:
`ea9dbf362caed8eb6cadb22f6a37991352be826dff839b4ae301f46a5831da8e`. Native source checkpoint
`74a125597e880ab50dcf372c3b9a2cb082e11d10` replays as 16 patches / 61 exact files.

Seven IPCC callback cases use RAM registers; eight SMP2P cases use real callback
sources, RAM shared entries, a recording regmap bus and the built-in state API.
They do not probe hardware. Production modules also register/unregister separately.
Physical IRQ/MMIO, wakeup, failed hardware probe unwind and active IRQ consumers
remain unverified despite the reviewed devres/receiver cleanup fixes.

Three tests use actual native state register/get/update/unregister APIs, real
synthetic DT client references and concurrent tasks. The original Image fails
all three: unregister returns during a callback, an orphan can be acquired anew,
and bit32 is accepted. New Image passes all three: unpublish under list mutex,
SRCU callback drain, retained client handle orphan rejection and range checking.
Unregister is sleepable and must run outside all state update callbacks. Clients
must retain/get/put their handles normally; this is not arbitrary-use-after-put
protection or a self-unregister API.

New Image regressions pass state3 + IPCC7 + SMP2P8 + GLINK11 + PMIC/QMI19 +
SSR/Gunyah10 + regulators7 = 65 cases, plus QRTR128 packets and 24 DT host tests.
Saved and candidate retain semantic references for two IPCC and six SMP2P
providers and the four SoCCP state bits. Vendor/native C shared-item layouts match;
long WPSS outbound names retain vendor wire truncation and V1/V2 SSR history
behavior. Full firmware/IRQ/protocol equivalence remains unproven.

Still pending: Alor-specific SoCCP rails/PAS and early attach, remote SMEM peer,
all actual remote services, vendor extensions, whole-board DT, packaged boot and
DLKM/DTBO/policy acceptance, controlled physical boot and daily-use/efficiency
measurements. No partition write or flashable image is claimed. Evidence:
`reports/state-ipc-stage35-validation.json`.


## Stage37: Q6V5 common, real sysmon/QMI and AOSS QMP

108 modules / 5,480 matching import CRCs; all hard/soft dependencies present and
acyclic. Existing Image #22 and candidate DT/GPU input remain unchanged.
ELF imports explicitly select real QMP/sysmon/state/QMI functions rather than
disabled-header paths. The requirements table's selected helper exports are now
all present; an Alor PAS driver is still not implemented.

Eleven SMP4 guest cases pass: real state updates and completion notifications,
stop assertion/clear error propagation, optional IRQ absence, ACK parsing,
detach rejection, actual QMI/sysmon subdevice lifecycle, actual remoteproc
ancestor association/duplicate rejection, concurrent source ingress vs provider
removal, 1,000 post-removal rejections and subsequent consumer removal. Three
production modules register/unregister separately after the combined test exits.
No physical QMP, sysmon peer, IRQ or firmware boot is covered. IRQ/QMP resource
cleanup changes are source-reviewed; probe fault injection remains pending.

Endpoint ingress now reads the association inside SRCU; provider/consumer
removal unpublishes and drains it. Request serialization prevents sends through
a removed endpoint. This does not prove full transport endpoint lifetime or
arbitrary self-removal from callbacks. Normal framework client ownership and
remoteproc subdevice serialization are still required. Native QMP firmware
protocol, hardware/late-peer/lifecycle stress, Alor rails/PAS/ELF64/secure ABI,
whole-board image/ROM/policy and daily-use acceptance remain open. Evidence:
`reports/remoteproc-services-stage37-validation.json`.

## Stage39: counted Alor ARC rail integration, draft only

The draft now has an actual Alor match/descriptor and cx/mx ARC requests from
saved DT. Eleven descriptor values match pinned vendor data, including PAS51,
DTB65, crash656, minidump24 and ELF64 metadata. No unrelated proxy power-domain
names are imposed. Native generic cx/px loading is bypassed for Alor.

Eight SMP4 tests exercise the exact counted rail helper with test-only recording
regulator calls: all six forward and six release operations can fail, recovery
retains failed operations, a simultaneous primary/rollback failure preserves
both errors, dirty/double enables are refused, repeated release is harmless,
positive set_load returns are valid, invalid inputs touch nothing, and two
workers each attempt 200 toggles without unbalanced recorded enable counts.
This does not test the actual regulator framework, ARC hardware or physical
recovery from failed regulator operations. Those remain required.

The real PAS/MDT/PIL modules register/unregister separately in QEMU; 132 imports
match provider CRCs. No matching Alor device/firmware is exercised. The new
driver remains excluded from the Stage37 108-module artifact. Its compatible
would enable early attach/auto boot on an actual matching board, so successful
link/registration is not sufficient boot approval or an accepted device image.
Clock/IRQ/handover and authenticated DTB/main image cleanup, firmware memory/
resource-table/ELF64 loading, secure ABI and real provider/remote-service/suspend
acceptance remain open. Evidence: `reports/pas-alor-stage39-validation.json`.

## Stage40: read-only live SoCCP firmware identity

Live SoCCP is attached. The existing ROM supplies firmware under
`/vendor/soccp_firmware/image/`, configured in vendor ueventd. Both dedicated
soccp slots are FAT12 volumes. Independently parsed volume files have exactly
the same hashes as the mounted main/DTB files. Both inputs are ELF32, machine
243/EM_RISCV; the vendor ELF64 dump-metadata selection does not make the firmware
input ELF64. No firmware conversion is justified by that descriptor field.

Selected ELF header geometry, inline segments, hash-segment presence and saved
reserved-region bounds agree with native loader prerequisites. Predicted load
spans are 3,596,288 and 86,016 bytes within 6MiB and 256KiB reservations.
This is a selected static audit, not execution of the actual native loader,
secure authentication, early-attach validation or physical firmware restart.

ROM init enables force_sysfs_fallback=1, the live value is 1 and ignore fallback
is 0; vendor ueventd includes the SoCCP directory. Native USER_HELPER is enabled.
The port still needs mount/init/request-timing and actual firmware fallback
validation. Read-only scripts did not mount, extract, modify, boot or authenticate
firmware. Evidence: `reports/soccp-firmware-stage40-validation.json`. Next: exact
firmware through the actual native loader into guarded RAM without secure calls,
then remaining PAS lifecycle/secure ABI and physical boot acceptance.

## Stage41–42: exact SoCCP inputs through the actual native MDT loader

Captured mounted files are hash-locked to the Stage40 independent FAT/file audit;
blobs stay in ignored local artifacts, not Git. Native get_size, read_metadata
and load_no_init APIs run in SMP4 QEMU. Tests compare the entire 6MiB/256KiB RAM
regions (load segments, BSS zeros and unchanged gaps), both 32-byte guards,
relocation base and exact header/hash metadata. No authentication/reset/PAS boot
API is called. This proves the selected CPU parsing/copy path, not secure ABI,
actual physical memory mapping, signature acceptance or device firmware reload.

The original metadata implementation fails a padded-backing regression: a header
length beyond fw.size is accepted. The reproducer intentionally keeps backing
storage larger so no outside-allocation access is needed. New checks require
metadata to include the table and fit advertised input before memcpy, validate
nonnull inputs/output and ELF32/LE, check total size, and widen file/address
additions before arithmetic. The final seven loader cases plus eight rail
regressions pass. Wider malformed/split-input and allocation stress, full PAS
clock/IRQ/handover/authenticated image error ownership, ROM fallback/mount timing,
secure firmware and all physical/whole-board criteria remain open. The Alor draft
stays outside the packaged Stage37 108-module artifact. Evidence:
`reports/soccp-loader-stage42-validation.json`.


## Stage43: owned proxy resources and deferred handover

Eight recorded-callback helper cases pass, including all six acquire/release
failure positions, simultaneous failures, pending/late handover, retry, no double
release and concurrent handover across 200 cycles. ARC8 and actual-input MDT7
regressions also pass: 23 SMP4 cases. Production wrappers call actual APIs;
physical clocks/regulators/IRQ/authentication and driver fault paths are not
exercised by the helper backend. Pending handover is acted on only after main
authentication returns success. Secure main/DTB ownership/error cleanup remains
a separate known implementation gap. Draft excluded from packaged 108 modules.
Evidence: `reports/pas-proxy-stage43-validation.json`.

## Stage44: secure image ownership and critical failure-stop policy

Seven recorded shutdown/unmap helper cases pass: main-before-DTB stop, refusal
to unmap any active/uncertain image, retained ownership on main/DTB errors,
retry without repeating confirmed shutdown, mapping-only cleanup, nonzero secure
status and 200 repeated cycles. Proxy8/rail8/actual-input MDT7 regressions pass:
30 SMP4 cases. Production driver integration is built/registered but its actual
auth/load/stop fault paths are not exercised by these helper tests.

The driver now records authentication attempt state, quiesces a successfully
started DTB on subsequent main failure, tracks owned mappings and marks attached
images for later stop. Separately requested DTB source firmware is released after
synchronous copying; the main source remains borrowed from remoteproc.

Alor critical authentication/reset failures and unconfirmed secure shutdowns
panic before active resources can be freed, following the pinned Motorola PAS
failure-stop policy. This is an intentional kernel-stop policy, not a successful
fallback or tested physical shutdown. The panic branch itself was not executed.
Native noncritical foreign-board terminal-error teardown and partial IOMMU unmap
are not accepted. Full driver failure injection, secure-world ABI, physical
attach/restart, all board services/remaining drivers and complete software-ready
boot artifacts still remain. Draft excluded from packaged 108 modules. Evidence:
`reports/pas-images-stage44-validation.json`.

## Stage45: preserve the stock Alor remoteproc name source

Pinned Motorola allocates remoteproc with pdev->name; the observed name is
`a3380000.remoteproc-soccp`. Alor now retains that platform-name source rather
than the native sysmon string `soccp`; the sysmon service string is unchanged.
Other descriptors keep native naming. Build, CRC and descriptor/source-flow
checks pass. No actual probe/daemon test or full ROM interface-equivalence claim
is made. Evidence: `reports/pas-name-stage45-validation.json`.


## Stage46–48: actual PAS methods and remoteproc framework

The original DA-to-VA conversion fails the +4GiB/SIZE_MAX regression (8/9
cases pass). Replaced int narrowing and additive bounds checks with u64 offset
and subtraction bounds. Final 13 cases pass with SMP4: actual driver error
paths, startup failure matrix, retained ownership/retry, no-IOMMU Alor path,
real remoteproc firmware requests and two boot/shutdown cycles, detached attach
and an explicit second boot after not-ready fallback. Hardware IRQ/clock/rail/
secure/MDT primitives are recording replacements only in the test translation
unit. Firmware bytes are parsed by the real core; this is not firmware execution.

Two separate guests execute real kernel panic branches for critical DTB auth
and main shutdown failure, requiring exact armed/state/panic/termination markers
and absence of cleanup. The phone was not rebooted or flashed. The harness now
loads providers before the test and removes the dependent test first. All four
production/test module import tables match current exports. Reproduction:

```sh
python scripts/build_storage_deps.py --pas-driver-tests
python scripts/run_qemu.py --test pas-driver --cpus 4 --log reports/pas-driver.log
python scripts/run_qemu.py --test pas-driver --pas-critical auth --cpus 4 --log reports/pas-auth.log
python scripts/run_qemu.py --test pas-driver --pas-critical shutdown --cpus 4 --log reports/pas-shutdown.log
```

Evidence: `reports/pas-driver-stage48-validation.json`. Exact firmware inputs
remain ignored local artifacts. Full probe, GLINK/sysmon children, ROM firmware
timing, removal errors and physical firmware ABI are not proven. The not-ready
case explicitly calls boot a second time; it does not prove automatic fallback.
The draft stays excluded from packaged 108 modules. Software readiness is false.


## Stage49: pinned native PMIC RTC

Unmodified native RTC driver matches saved/candidate qcom,pmk8350-rtc and the
pinned Motorola seven-field register table. Eight SMP4 cases use actual RTC
callbacks over real regmap with a RAM bus: decoding/offset, carry rollover, alarm
round trip, error propagation, no-storage time-write rejection, direct write and
actual RTC alarm timer work notification. The initial 7/8 failure was a test
expectation of synchronous irq_data without arming a timer; corrected via
rtc_set_alarm/flush_work, with work drained before regmap teardown. No production
RTC modification was necessary. Physical PMIC alarm delivery and wake are untested.

Stage49 partial snapshot contains 109 modules with matching import CRCs, complete
hard/soft dependencies and an acyclic combined graph. It excludes Alor PAS draft
and remains non-flashable. Selected AVR DT has no allow-set-time/offset cell/UEFI
property, so no-storage set_time failure agrees with stock. Native optional UEFI
and drift-offset persistence differ from factory outside this selected path.
See reports/rtc-stage49-validation.json. Software readiness remains false.


## Stage50: factory SDAM and native NVMEM integration

Motorola driver and stock module/NVMEM naming/configuration are preserved.
The original length predicate incorrectly accepts SIZE_MAX by addition wrap
(7/8 cases); subtraction bounds fix yields 8/8 SMP4 cases. Actual native NVMEM
and regmap cores run RAM-only read/write, boundary guards, three read-only
register/spanning-write rejection, single-byte PBS exceptions and bus errors.
Huge-size reproducer calls the predicate only, never a bus/copy API.

Read-only stock metadata acquisition opens NVMEM O_RDONLY and reads exactly one
SDAM_SIZE byte at 0x44 per provider; all 12 have 128-byte data windows. Saved and
candidate providers, 13 fixed cells and 18 resolved direct references agree;
all fixed cells fit. Legacy bit/bits spelling is retained and only geometry is
audited. No live data cell or PBS register is accessed. Stock config.size is
left unset (native upstream differs); no new sysfs aperture is invented.

Partial Stage50 snapshot has 110 modules, all imports match, hard/soft
dependencies present and combined graph acyclic. Alor PAS draft remains excluded.
Physical candidate probe, all SDAM consumers, suspend and complete board/ROM/boot
readiness remain unverified/incomplete. reports/sdam-stage50-validation.json.


## Stage51: PMIC power/resin input and PON parent

Three native modules provide real reboot-mode, PON parent child population and
PMIC key callbacks. Selected Gen3 descriptors match factory status bits/names/
capabilities; both stock wake defaults are enabled, so native resin default is
adjusted to true. Saved/candidate two key nodes, key codes, IRQ tuples and the
0x1300 HLOS / 0x800 PBS bases agree. Stock /proc input and wake reads are recorded
without injection. Native suspend IRQ-wake semantics differ from vendor IRQ
free/re-request and need physical acceptance.

Eight SMP4 cases use actual input handler/core and regmap with a RAM-only bus:
power and volume-down press/release, release-only synthetic pair, duplicate
press filtering, read errors, pending debounce skip, PBS debounce and reboot-mode
mask/error propagation. Initial 4/8 failure came from test id.flags=0 (driver_info
is no longer a match-table sentinel); corrected EV_KEY flags and open-handle
requirement yield 8/8. Production input matching and Rakkashin matching already
use nonzero flags; neither required that test correction.

Partial Stage51 has 113 modules, matching import CRCs and complete acyclic hard/
soft dependencies. It excludes Alor PAS draft and is not a flashable boot image.
Physical key IRQ/wake, other input devices/ROM consumers, reboot actions and full
software readiness remain pending. reports/power-key-stage51-validation.json.


## Stage52: actual panel notification API and callback drain

Factory panel_event_notifier is ported with its three real exported APIs.
Original unregister returns while a controlled callback is still blocked
(6/7 cases pass); the reproducer retains callback data, avoiding a real UAF.
The fix unpublishes under the registry mutex, SRCU-drains without that mutex,
and blocks slot reuse with EBUSY while retiring. Final 8 SMP4 cases pass,
including callback registration of another client, drain/reuse, routing,
all five clients and invalid cookies/NULL notifications. Callback self-unregister
is forbidden: unregister may sleep, callers release data only after return and
must unregister each cookie once. Full consumer audit remains necessary.

Archived stock static client array is 160 bytes (five ARM64 entries); the
optional two-client MOT extension is absent. Selected header declarations,
payloads and enum values agree with factory. This is not whole-driver ABI or
physical display/touch acceptance. Stage52 partial snapshot contains 114 modules
with matching import CRCs, complete hard/soft dependencies and an acyclic graph.
It excludes Alor PAS draft and remains non-flashable/software-readiness false.
Evidence: reports/panel-event-stage52-validation.json.


## Stage53: actual Motorola relay and sensor class APIs

Nine SMP4 cases pass: blocking payload, atomic-context atomic notifications,
type/device isolation, invalid argument rejection, metadata callback registering
another notifier type, client drain, provider removal plus absent-provider
unregister drain/reprobe, real sensor registration and invalid sensor arguments.
These exercise real exported modules and actual platform probe/remove, not copied
test implementations. Relay is fully initialized before singleton publication,
unpublished/SRCU-drained before free, and never holds a global mutex across
notification callbacks. Native chain recursion rules still apply; modification
of a chain from its own callback and callback unregister are forbidden.

Sensor registration uses device attribute groups rather than ignoring individual
attribute failures. The native device core supplies rollback; forced group failure
and full sysfs/calibration operations are not tested. Sequential unregister clears
the device/list linkage; concurrent unregister is not accepted. Header ABI and
all 20 attribute names/modes/callbacks are preserved by selected static comparison.

Saved/running stock DT has no relay node; live stock driver has no bound device.
A new optional candidate adds exactly /soc/mmi-relay using factory compatible.
Every existing node property/order, reserved map and saved 21-level GPU table is
unchanged. Actual OF population/ROM consumers remain pending. The partial Stage53
bundle has 116 modules and candidate.dtb, all imports match and hard/soft deps are
present/acyclic. Whole-board DT (including existing SID4 conflict), boot packaging,
Goodix/other drivers and physical/software-readiness gates remain open.
Evidence: reports/moto-touch-deps-stage53-validation.json.


## Stage54: linked Goodix/THP and info component drafts

Four modules link to real providers with matching import CRCs. Factory nine
build flags and header UAPI are preserved; only TUI prototypes are added to
the Goodix header. Native string/trace/prototype adaptations, ten-byte custom
info bounds and pen unregister/free correction are recorded in replay patches.
SMEM now checks real allocation/get errors and exact size before copy.
Archived GET_EVENTS method is exactly mov w0,wzr; ret (8 bytes), matching
the original no-payload source behavior. It is not event acquisition proof.

Eight SMP4 tests include actual annotation source append methods with RAM-only
globals: short/long strings, terminator capacity, SIZE_MAX offsets, persistent
text bounds, nonpersistent fallback, NULL format and four concurrent writers.
No physical probe/mapping/minidump/SMEM operation runs in these tests. Test TU
disables driver init and minidump probe branches; production forces real APIs.

These are explicitly unaccepted drafts outside the main 116-module artifact.
Known annotation probe import/publication/unwind/remove/minidump mapping issues,
info init failure handling and full touch open/ioctl/compat/remove/probe lifetime
remain. Physical SPI/IRQ/display/power/firmware/gesture/stylus and daemon/ROM
acceptance also remain. No all-functions/software-readiness claim is made.
See reports/goodix-touch-stage54-validation.json and draft artifacts directory.


## Stage55: firmware DT population and real relay probe

A synthetic QEMU firmware tree supplies a simple-bus child with exactly the
candidate relay compatible/status properties. Native OF creates the platform
device and the real provider driver binds. One SMP4 KUnit case locates that
OF-created device (no direct platform registration in this test), checks driver
data and round-trips registration/payload/unregister through actual exported APIs.
Provider/test unload and guest powerdown succeed without warnings. Selected
candidate/guest node properties and parent bus compatible are byte-identical.

This closes the selected synthetic OF population/probe gap left by Stage53;
actual AVR boot timing, full DT and consumers still require acceptance. Module
counts remain main116 + unaccepted drafts4, with all Stage54 implementation
gaps and whole-board/ROM/boot gates open. Evidence:
reports/moto-touch-of-stage55-validation.json. Reproduce using
`python scripts/build_goodix_touch.py --relay-of-tests` and
`python scripts/run_qemu.py --test moto-touch-of --cpus 4 --log reports/relay-of.log`.


## Stage56: annotate provider lifetime and no-map RAM

A fully owned context is published only after all resources succeed. Old-boot
restore is clamped to trusted mapped capacity/local size with NUL termination;
runtime writes cannot expand past trusted capacity via a corrupted header.
Source header fields and selected stock/candidate 4KiB no-map reservation plus
8KiB local buffer agree. The generic truncation test uses a smaller local buffer
than stock; stock geometry alone did not demonstrate that restore overran it.
Use paired memremap/memunmap instead of invented struct-page/linear-map aliases.

Removal drains proc callbacks and API locks, unpublishes, unregisters minidump
and only then frees backing. Unconfirmed removal retains backing, retried at
next probe/exit. Terminal module exit failure intentionally keeps backing until
reboot; bounded retention is the safety policy, not leak-free terminal teardown.

21 SMP4 cases pass: 11 exact source probe/remove/API cases over real OF/proc/
reserved QEMU RAM with recorded minidump faults, eight source format regressions
and two tests around actual production module/native pending minidump APIs.
The latter uses firmware DT auto-binding, real proc read/write, a duplicate-name
check while registered and successful name reuse after actual module unload.
No minidump firmware backend or physical AVR memory is accessed. Initial helper
symbol_get was rejected for a non-GPLONLY export; test corrected via driver data/
proc checks, production export semantics unchanged.

Only annotate is promoted: partial Stage56 has 117 modules/5,695 matching imports
and complete acyclic hard/soft deps. Three info/touch drafts remain excluded.
Physical/cache/initialized-firmware acceptance, low-memory/proc failure injection,
info init/SMEM, Goodix/remaining drivers and complete DT/boot/ROM gates remain.
Evidence: reports/mmi-annotate-stage56-validation.json. Reproduce with
`python scripts/build_goodix_touch.py --annotate-tests --annotate-driver-tests --annotate-native-tests`
and run QEMU modes mmi-annotate, mmi-annotate-driver and mmi-annotate-native.


## Stage57: independently deferred Motorola metadata and immutable bootargs

Fixed mmi_info initialization ignoring errors: four software platform endpoints
now own independent storage/RAM/unit/boot lifetimes. SMEM defers RAM/unit while
boot metadata and DT selectors stay available. Boot getters are initialized
before exported APIs become visible. Device registration failure unwinds in
reverse order, proc/sysfs callbacks drain before backing is released.

DDR error pointers/NULL/short items are rejected before dereference; sysfs reads
an owned snapshot. Unit SMEM remains version 3 and exactly 1024 bytes; public
headers remain unchanged since Stage56, with endpoint names/modes preserved.
Bootargs use immutable cached values, exact keys, native next_arg for cmdline
and bounded bootconfig parsing. Selected OF strings/lists hold node references.

15 SMP4 cases pass: 13 exact-source cases with recorded SMEM/error injection
and native framework behavior, plus two against the production module with real
unready SMEM. The latter confirms storage/boot binding while RAM/unit defer,
public getters/selector and a real storage sysfs read. Shell checks verify two
module unloads with one reload leave no proc/sysfs endpoints. These tests do not
initialize real modem/SMEM firmware or prove physical supplier-readiness timing.

Partial Stage57 contains 118 modules/5,750 matching import CRCs; all hard/soft
module dependencies are present and acyclic. Same-name coverage is 104/544;
that is an inventory measure, not functional completion. Two Goodix/THP drafts
remain excluded, as do three Alor PAS drafts. Whole-board DT/boot/ROM, remaining
mandatory drivers and physical all-function acceptance are still incomplete.

Evidence: reports/mmi-info-stage57-validation.json. Reproduce with:
`python scripts/build_goodix_touch.py --info-tests --info-native-tests`, then
QEMU modes `mmi-info` and `mmi-info-native` with `--cpus 4`. These guests have
no phone disks or network. No phone partitions were written.


## Stage58: Goodix multi-SPI bounds and error propagation (draft)

The production multi-SPI ioctl used fixed 5120-byte TX/RX buffers without
checking aggregate descriptor lengths. Reject empty/over-capacity descriptor
counts, zero-length transfers and totals beyond that factory capacity before
payload access. kcalloc bounds array allocation; allocation failures now return
-ENOMEM, user-copy failures -EFAULT. Stop receive copies at the first fault.
Wakeup setup, SPI and restoration errors now propagate; restore is attempted
even after setup failure because spi_setup may already change software state.
The factory wakeup path still has no receive-copy phase; public ioctl layouts
and nine build flags remain unchanged. Descriptor delay behavior is not changed.

22 executable cases compile the exact production function and header structs
against recorded allocation/uaccess/SPI primitives. They cover exact aggregate
capacity, overflow lengths/counts, all four allocation failures, all input-copy
positions, both receive faults, normal/NULL transfers and wakeup/SPI/restore
errors. This is a host source-method test, not native uaccess, full driver
lifetime, daemon compatibility or physical SPI proof. Reproduce with:
`python tests/goodix-spi/run.py` and `python scripts/build_goodix_touch.py`.
Both Goodix/THP modules build and pass import CRC checks, but remain excluded
from the main 118-module Stage57 bundle. Draft binaries are saved separately in
artifacts/moto-touch-drafts-stage58. Evidence is recorded in
reports/goodix-spi-stage58-validation.json. Frame/mmap/TSC message bounds,
open/remove/probe lifetime, physical hardware and whole-board gates remain.


## Stage59: frame slot and TSC message bounds (draft)

Reject negative/oversize/NULL-payload frame submissions before ring mutation,
and invalid ring indices/missing backing under frame_mutex. Request IDs use an
atomic counter across instances. Zero-initialize the entire frame ioctl result;
the formerly uninitialized size field is now zero. Its intended daemon semantics
still require ROM validation; no guessed length ABI is introduced.

Reject TSC lengths larger than the 128-byte source. Version copies fit the
100-byte destination, require a NUL for a full-size payload, clear the previous
tail, and log with a bounded length. Fixed-size command fields retain factory
behavior (ignoring advertised lengths); their daemon compatibility and concurrent
metadata/event publication still need review.

241 host assertions exercise exact production enqueue/get/message functions
and factory header structs with recorded primitives: capacity guards, invalid
ring state, ring wrap/full behavior, ioctl zeroed output/copy errors, all version
lengths 0..99, full terminated/unterminated strings and oversized messages.
These are assertions, not 241 independent hardware/KUnit cases. The production
7.3 modules build and all import CRCs match. Reproduce with
`python tests/goodix-payload/run.py` and `python scripts/build_goodix_touch.py`.
No touch drafts are promoted; Stage57 remains the main 118-module partial
bundle. Mmap, open/remove/probe lifetimes, work/IRQ draining and physical
firmware/daemon/all-function acceptance remain incomplete. Evidence:
reports/goodix-payload-stage59-validation.json.


## Stage60: initialized, bounded, page-referenced Goodix mmap (draft)

Replace unchecked kmalloc backing with checked, zeroed alloc_pages_exact and
paired free_pages_exact. Validate mmap page offsets before shifting and length
with subtraction bounds. vm_insert_page holds each mapped page independently;
VM_MIXEDMAP/DONTEXPAND/DONTDUMP flags bound VMA behavior. Owner release clears
the pointer under frame_mutex and drops only owner references. Existing VMAs
retain page references through split/fork/unmap; this does not fix core/file
lifetime or producer callbacks during full driver removal.

Actual 7.3 SMP4 QEMU runs the exact extracted production mmap/release methods
in a synthetic misc device using native VM APIs. User tests read all 200KiB for
zeroes, exercise full/last-page aliases, reject end/overrun/huge offsets, release
allocation-owner references, reject new maps, split VMA with mprotect, fork and
share child writes, close fd, and independently unmap both aliases. The 204820
assertions mostly check individual bytes, not independent test scenarios.
Synthetic core is static; whole physical driver detach is explicitly untested.

Production touch modules rebuild and pass import CRC/UAPI checks, remaining
unaccepted drafts outside the main Stage57 bundle. Reproduce with
`python scripts/build_goodix_mmap_test.py`, then
`python scripts/run_qemu.py --test goodix-mmap --cpus 4 --timeout 120 --log reports/goodix-mmap-stage60-qemu.log`.
Evidence: reports/goodix-mmap-stage60-validation.json. Partial page-insertion
failure injection, all open/ioctl/remove/probe and producer/work/IRQ lifetimes,
physical daemon/firmware and whole-board completion remain pending.


## Stage61: single-SPI errors and combined SPI regression (draft)

Single-SPI allocation failures now return -ENOMEM and user-copy residuals
-EFAULT. Reject zero-size requests; suspended non-gesture factory behavior
still returns success without accessing userspace. The host fixture compiles
both exact production SPI methods and factory structs: 34 recorded-primitive
cases cover the prior 22 multi-SPI cases and 12 single-SPI success/error paths.
No physical/native-uaccess/daemon proof is inferred. Reproduce with
`python tests/goodix-spi/run.py --stage 61` and the normal touch builder.
Production modules link and import CRCs/UAPI checks pass. Main bundle remains
Stage57, touch drafts unaccepted. Evidence:
reports/goodix-spi-stage61-validation.json.


## Stage62: ESD initialization order and failed read handling (draft)

Initialize esd_work before publishing the misc endpoint, including disabled
configurations. The prior order allowed ioctl/sysfs use before initialization.
After SPI read failure, do not inspect the unfilled ESD byte or power-cycle on
its value; retain the scheduled retry policy. Ten exact-source branch assertions
with recorded primitives cover timeout/EIO, heartbeat writes, power-cycle branch,
suspend/disabled conditions and disabled-config initialization. An executable
source check asserts init precedes misc_register. Production modules rebuild and
CRC/UAPI checks pass. Reproduce: `python tests/goodix-esd/run.py`.
Concurrent enable/disable/requeue, synchronous remove draining, native workqueue
execution and power-cycle failure handling are still pending. Evidence:
reports/goodix-esd-stage62-validation.json. No draft promotion or physical proof.


## Stage63: low-level SPI and frame wire bounds (draft)

Check private wire buffer lengths before adding read/write protocol overhead,
reject missing platform/core before dereference, and allocate maximum advertised
frame capacity plus 16 protocol bytes (previously 8KiB+16 despite 10KiB frames).
Get-frame now checks tdev/buf before access, header minimum, packaged ring slot
maximum, address addition overflow and frame-body read errors. Header-only frames
need no zero-length SPI read. Sum header bytes as u8 explicitly.

67 exact-source assertions with recorded SPI exercise both chip read formats,
wire buffer boundary/oversize/NULL requests, all frame lengths below the 14-byte
header, header-only and maximum packaged frames, oversize, header/body timeout,
checksum, missing core and suspended transport. Guard bytes stay intact. The
fixture extracts protocol constants from the pinned source; no hardware model
or physical firmware proof is inferred. Reproduce:
`python tests/goodix-wire/run.py` and the normal touch builder.
Production modules build and CRC/UAPI checks pass. The enlarged private buffer
and maximum-frame behavior still need daemon/physical acceptance. Draft remains
excluded; main partial bundle is Stage57. Evidence:
reports/goodix-wire-stage63-validation.json.


## Stage64: read-only current SPMI selection evidence

Read current 6.12 sysfs device OF links/driver bindings, with no register access
or bind/unbind. All observed main-bus PMIC nodes match the first active node for
each SID in the saved running DT; no saved first node is absent. SID4 selects
pm8010@4 and has no driver symlink, while the same-bus pmd802x@4 alternative is
not separately enumerated. This confirms current enumeration order only. It
does not prove either SID4 subtree is functional or that deleting either is
valid. The candidate already preserves saved child order. Duplicate-SID and
required PMIC subtree/probe acceptance remain unresolved for 7.3; do not mark
the whole-board gate passed. Reproduce read-only with
`python scripts/audit_spmi_runtime_selection.py` on this phone. Evidence:
reports/spmi-runtime-stage64.json. No DT or partition modification occurred.


## Stage65: AVR M31 eUSB2 and PMIC repeater drafts

Import firmware-pinned repeater framework, PMIC eUSB2 driver and actual AVR M31
PHY from qcom-soc 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1. All three modules
link against real 7.3 dependencies (101 matching import CRCs), without replacing
hardware operations. Disabled DWC3 header fallbacks are made static inline;
these drivers use flag constants, not the disabled DWC3 function fallbacks.

Managed repeater acquisition previously allocated empty struct storage then
released it as a repeater. Store exact acquired device/module refs instead;
release does not dereference a provider descriptor. Check NULL/unbound nodes,
reject duplicate entries, use consistent IRQ-safe locking and bound repeated
remove. Unmanaged getters retain factory borrowed semantics; forced provider
unbind versus active consumers remains unresolved, so none are promoted.

Five actual production-framework SMP4 QEMU cases exercise native devres groups,
device/module refcount return, repeated managed acquires, invalid/disabled nodes,
duplicate/remove/re-add and required/optional missing phandles. Real platform
registration supplies the synthetic provider; no PMIC/USB IO is attempted.

Selected DT checks pass: AVR M31 node references the enabled SID7 PMIC repeater,
required clock/reset/supply targets are active, named MMIO tuples and override
pairs are valid. Normalize phandles to paths before comparing; all other USB
node property bytes and clock/reset arguments match the saved running DT.
No USB node changes are needed for these same-binding ports; whole-board gate
remains unaccepted. Reproduce `python scripts/build_usb_repeater.py --tests`,
QEMU mode `usb-repeater`, and `python scripts/audit_usb_eusb2_dt.py`.
Evidence: reports/usb-repeater-stage65-validation.json. Three USB drafts are
saved outside the main 118-module Stage57 bundle. PMIC/M31 callbacks, supplier
lifetime, vendor DWC3/SuperSpeed/charging and physical USB acceptance remain.


## Stage66: PMIC repeater failed-IO and readiness handling

PMIC get_version no longer returns an unfilled stack byte on failed register
read. Reset returns masked-write errors; init-status polling propagates read
failure and returns -ETIMEDOUT when RPTR_OK never arrives. Initialize the user
parameter mutex before publishing the provider to consumers. Tuning writes and
regulator rollback still require review; this is not full PMIC acceptance.

Seven SMP4 QEMU cases execute exact production source callbacks on a real
regmap backed by private RAM: successful/failed version, reset mask set/clear
and write failure, readiness success/read failure/timeout. Factory driver
registration is disabled only in the test translation unit, so no actual PMIC
is bound. All three production USB modules rebuild with 101 matching imports.
Reproduce `python scripts/build_usb_repeater.py --pmic-tests` and QEMU mode
`usb-repeater-pmic`. Evidence: reports/usb-repeater-stage66-validation.json.
Three drafts are saved separately, without promotion to the main bundle or
physical USB/cable/suspend/charging claims. Override/sysfs parsing, tuning writes,
M31 lifetime/MMIO, forced provider removal, DWC3/SuperSpeed and whole-board
DT/boot/DLKM/SELinux remain incomplete.


## Stage67: counted PMIC override parsing and atomic replacement

Replace duplicated two-pass sysfs parsers with one counted parser. Never scan
past count; validate byte values, complete pairs and at most 254 cells before
allocation/publication. Commit replacement under the same mutex used by init;
errors preserve the previous pointer/count, and exact `none` clears only the
selected host/device sequence. This fixes stale-pointer/free risks when a final
bad token was discovered after the old buffer was released. The u8 counters
remain representable, so existing show output stays within one page. DT override
arrays are checked for the same count/pair/byte limits. Sysfs names/modes and
valid reg/value ordering are preserved; low-memory/concurrency stress is pending.

Eleven SMP4 QEMU cases pass: the seven native regmap cases plus nonterminated
counted input, invalid replacement preservation, independent host/device clear,
254-cell capacity, excess/empty/oversize rejection and output guard checks.
Production modules rebuild and import CRCs match; selected saved/candidate DT
checks pass with normalized phandle targets. Reproduce the PMIC test builder
and QEMU mode `usb-repeater-pmic`; run the selected DT auditor with
`--report reports/usb-eusb2-stage67-dt.json` for current DT bounds. Evidence:
reports/usb-repeater-stage67-validation.json. No promotion, physical USB proof
or whole-board completion is implied. Three drafts remain separate from Stage57.


## Stage68: GENI DMA/I2C/SPI drafts and native wrapper mapping API

Import pinned GPI DMA, I2C and SPI source plus factory common/header/trace files.
Keep non-exported helpers static, use external trace include paths, and assert
35 overlapping native register constants match rather than overriding silently.
SPI chip-select scanning uses actual array bounds and the four-CS hardware limit.
All three modules link with 323 matching imports, without dummy DMA/bus methods.

Factory common diagnostics cast parent private data to struct geni_se even
though it is struct geni_wrapper. Add native geni_se_get_wrapper_base borrowed
mapping API and use it without private-layout assumptions. Keep the parent
bound during access. Native source is now 93a77327a (full SHA in the report),
with a rebuilt Image/modules; full replay passes 17 patches/63 exact files.
This new API requires the matching Image; old Stage57 Image lacks it.

Four SMP4 QEMU cases bind the actual native wrapper to reserved guest RAM using
fixed clocks, then test new accessor/null behavior, hardware-version read,
common diagnostic writes/read and neighbor canary, missing wrapper data, and
SPI chip-select helper using the real native spi_device layout. All three
controller modules register/unregister, but their physical probes/transfers are
not exercised. Selected DT checks pass for 3 wrappers, 3 GPI controllers,
8 I2C and 1 SPI controllers: clock/DMA/IOMMU/ICC/pinctrl/memory references and
factory five-cell DMA tuples resolve, and normalized properties match saved DT.
An initial audit assumed six cells from an obsolete log string; corrected to
parse the factory REQ_OF_DMA_ARGS=5 definition and actual se-clk lookup name.
No DT values were changed to satisfy the check.

Reproduce `python scripts/build_geni_bus.py --tests`, QEMU mode `geni-bus`, and
`python scripts/audit_geni_bus_dt.py`. New Image/three drafts are archived in
artifacts/geni-drafts-stage68 separately from the main Stage57 bundle.
Evidence: reports/geni-bus-stage68-validation.json. DMA rings/descriptor/IRQ
lifecycle, physical transfers, other mandatory drivers and whole-board gates
remain incomplete; no draft promotion or daily-use acceptance is claimed.


## Stage69: GPI ring geometry, arithmetic and backing ownership

Widen elements*element_size before multiplication, reject zero/one-slot and
non-power-of-two element geometry, bound rounded length to the u32 ring field,
and use defined power-of-two rounding. Reject reallocating a configured ring.
Use subtraction bounds for private SMEM arena limits. Free only an actually
owned coherent allocation; SMEM backing and repeated release never call the
DMA free API with a NULL/unowned buffer. SMEM bump-arena reclamation and real
firmware mapping remain unresolved; this is not complete SMEM support.

Six SMP4 QEMU cases include exact production GPI source, with registration
disabled only in the test translation unit. Native dma_alloc_coherent/free,
not allocation mocks, exercise alignment/zeroing, 65-element rounding, duplicate
allocation/idempotent free, reserved-slot/full/wrap behavior, recycle/translation
and geometry/overflow rejection. A separate private synthetic SMEM arena tests
carving/bounds/owner-free; no real shared firmware memory is accessed.
Reproduce `python scripts/build_geni_bus.py --ring-tests` and QEMU mode
`geni-ring`. All three production modules rebuild and import CRCs match.
Evidence: reports/geni-ring-stage69-validation.json. Drafts remain excluded
from the main bundle; descriptor/IRQ/actual transfer and whole-board gates remain.


## Stage70: preflight complete GPI scatterlists before ring mutation

Before MMIO or ring writes, validate configured/native TRE geometry, ring cursor
range/alignment, nonempty SG list, actual pages/CPU addresses, complete TRE-sized
lengths and aggregate remaining capacity using subtraction bounds. This prevents
truncated length accounting, aggregate wrap and partially written rings when a
later SG entry is invalid. Preserve previous num_tre metadata on rejection.
Prep still requires caller serialization and stable input buffers; no new
concurrency/completion guarantee is claimed.

Ten SMP4 QEMU cases pass: the six native coherent-DMA ring regressions and four
SG cases. Invalid lists intentionally have no channel-state mapping and must
reject before MMIO; cover missing/empty/short/zero/page-less entries, a later
malformed entry, huge lengths/list counts, unchanged ring/metadata. Valid input
uses private channel-state RAM and native vchan_init/vchan_tx_prep, confirms
copied TRE bytes/position/metadata and descriptor allocation; cleanup drains
native virtual descriptors/tasklet. No doorbell, hardware transfer, IRQ or
completion is simulated or claimed. Reproduce the GENI ring builder and QEMU
mode `geni-ring`. Production modules rebuild with matching imports.
Evidence: reports/geni-sg-stage70-validation.json. Drafts are saved with their
matching Image separately from Stage57. Full probe/remove/concurrency, physical
bus/peripheral behavior and remaining drivers/whole-board gates stay incomplete.


## Stage71: bounded GPI completion pointers and event cursors

Validate aligned DMA addresses inside configured rings before translating to
CPU pointers or changing descriptors. Check immediate event index/channel and
8-byte data capacity. Combine both Q2SPI pointer fields (40 bits) rather than
truncating to ptr_l. Validate hardware event cursors before scanning/re-scanning;
stale scans return errors on invalid targets, propagated by reset before freeing
descriptors. Missing generic client callback returns before reading channel
state. These checks do not establish descriptor ownership under concurrent
termination, bounded live-producer scheduling or complete cookie reporting.

Fifteen SMP4 cases pass: ten ring/SG regressions plus valid completion/wrap,
malformed address/channel preservation, immediate index/length rejection and
valid payload callback, full Q2SPI high-bit translation, event-cursor rejection
and selective stale marking. Native coherent allocations and virtual descriptor
queues are real; channel/event registers are private RAM. The 40-bit Q2SPI IOVA
is synthetic metadata, not proof of physical 40-bit DMA. All three production
modules rebuild with matching import CRCs. Reproduce GENI ring builder/QEMU
mode `geni-ring`. Evidence: reports/geni-events-stage71-validation.json.
Drafts remain excluded. Actual IRQ/STOP/RESET/concurrency, callback lifetime,
physical bus/peripheral and whole-board acceptance remain pending.


## Stage72: completed-descriptor ownership and native DMA callbacks

Detach the exact issued descriptor under vc.lock before client callback, update
its native cookie and hold private ownership until callback returns. This avoids
freeing a new head descriptor if a reentrant client drains/requeues work. Cookie
retirement occurs before waking clients. Use native callback selection/invocation
so result callbacks take precedence, receive DMA_TRANS_ABORTED for error events,
and legacy callbacks may use NULL parameters. Populate Motorola callback metadata
only for the legacy callback form; keep existing shared/unlock silent-retirement
branch choices. Completed cookie means retired work, while callback_result
reports success/error; no new DMA_ERROR polling status is claimed.

Nineteen SMP4 cases pass, including all earlier ring/SG/event regressions plus
cookie completion before legacy callback, native result-callback error delivery
with both callback fields present, reentrant drain/requeue preserving the new
issued descriptor and subsequent completion, and NULL-parameter legacy callback.
Initial 18-case run was rejected for test-side unlocked vchan_next_desc warnings;
fixed test locking and made the callback SG source heap-backed. Final checks
retain strict warning rejection and pass without WARN. These are native software
DMA/descriptor tests on private RAM, not real IRQ/termination timing.
Reproduce GENI ring builder/QEMU mode `geni-ring`. All production modules link
with matching imports. Evidence: reports/geni-completion-stage72-validation.json.
Peer-channel abort/cancellation, shared multi-descriptor modes, STOP/RESET/IRQ
concurrency, physical buses/peripherals and whole-board completion remain pending.
Drafts are archived separately from Stage57; no deployment promotion occurred.


## Stage73: SPI clock target width and divider field limits

Validate nonzero rate and positive sampling before entering native GENI clock
matching. Widen checked target multiplication and compute ceiling division using
quotient/remainder rather than overflow-prone addition. Reject divisors outside
the native CLK_DIV_MSK field before register programming. Requests previously
accepted with an unrepresentable divider now return -ERANGE; this is intentional
instead of programming truncated/reserved bits. No DT clock/rate values changed.

Seven SMP4 cases pass: four native wrapper/chip-select regressions and three
clock cases. The exact production clock method is extracted into a reduced
private-field fixture using real geni_se/CCF objects and a native 19.2MHz fixed
clock. Cover zero/negative sampling, zero rate, 400kHz one/two-sample division,
a target multiplication that formerly wrapped to zero, and valid/invalid divider
field limits. The enormous test request demonstrates arithmetic behavior only;
it does not claim that physical SPI can achieve that request. This does not
exercise the complete SPI driver probe or actual signal timing.
Reproduce `python scripts/build_geni_bus.py --tests` and QEMU mode `geni-bus`.
Production modules build with matching imports; drafts stay outside Stage57.
Evidence: reports/geni-spi-clock-stage73-validation.json. Actual AVR clock table,
waveforms, FIFO/DMA/peripheral/PM and whole-board acceptance remain pending.


## Stage74: transactional SPI message DMA mapping

Initialize every transfer DMA field to DMA_MAPPING_ERROR before attempting the
first mapping. If any RX/TX mapping fails, unwind all earlier acquired mappings,
including the current transfer's RX when TX fails. Clear each ownership token
after release so later cleanup cannot double-unmap. Skip zero-length/control
transfers; use dma_mapping_error rather than treating address zero as unused.
This requires the normal SPI-core prepare/unprepare sequencing; it does not
support calling prepare twice on a still-mapped successful message or changing
transfer buffers midway through a message.

Five SMP4 cases compile exact production map/unmap methods with real native
spi_message/spi_transfer structures. Native DMA map/unmap uses private linear
RAM, with wrappers injecting each of six failure positions and tracking lease
size/direction/ownership. Cover success and repeated cleanup, all rollback
positions, retry after failure and zero-length/unidirectional transfers. The
zero-IOVA case is explicitly recorded-only and performs no DMA API calls with
physical address zero. No AVR IOMMU/cache/actual-transfer proof is claimed.

Reproduce `python scripts/build_geni_bus.py --map-tests` and QEMU mode `geni-map`.
Production modules rebuild and import CRC checks pass. Evidence:
reports/geni-map-stage74-validation.json. Drafts remain outside Stage57. Full
prepare failure PM/sequencer-lock unwind, remove/suspend lifetime, physical
transfers and remaining mandatory drivers/whole-board gates remain incomplete.

## Stage75: SPI message preparation failure unwind

Track the shared-message reference (including the first reference inherited from
prepare_transfer_hardware) and a successfully acquired sequencer lock. On failed
message preparation, release the lock while power is held, then release only the
references owned by this message. SPI core does not call unprepare_message when
prepare_message fails. Preserve setup=true after consuming the first inherited
reference so a queued retry acquires a fresh one. Treat runtime resume failure
as a returned error rather than issuing WARN_ON_ONCE. Nonshared hardware-phase
references remain owned by their caller.

Eight SMP4 tests pass using exact production prepare/unprepare methods, native
runtime-PM callbacks/refcounts and native SPI objects. Lock/mode/FIFO/DMA actions
are recorded, not physical controller operations. Cover lock, map, FIFO, mode
and native resume failures; retry; inherited first-message ownership; borrowed
nonshared reference; and fallback local resume failure unwind. No warning or
panic occurs. Reproduce build_geni_bus.py --pm-tests and QEMU mode geni-pm.
Three draft modules rebuild with all 323 imports matching. Evidence:
reports/geni-pm-stage75-validation.json. Draft artifacts are separate from the
Stage57 main bundle. Hardware prepare failure unwind, full suspend/remove/VM
ownership, actual AVR transfers and whole-board acceptance remain pending.

## Stage76: hardware preparation PM ownership and SPI-core teardown

Stop immediately when active-pin selection fails instead of logging and
continuing with a shadowed error. Track locally acquired pins/PM and unwind on
runtime resume or master-setup error. Restore selected sleep pins before the
successful PM reference is released; keep the original failure if sleep-pin
restoration also fails. Ordinary resume failures return errors without WARN.
Guard unprepare_transfer_hardware using the existing preparation-progress flag:
the native SPI core can call it after failed preparation, when no reference is
owned. Repeated hardware unprepare no longer releases a second reference.

Nineteen SMP4 cases pass without WARN/panic, including the previous eight.
Nine new direct cases cover disabled PM, pin failure/retry, native PM resume
failure, recorded setup failure/unwind, cleanup-error preservation, nonshared
and shared phase handoffs, shared setup failure and LE client ownership. Two
more register a native SPI controller and child, then invoke real spi_sync:
hardware-prepare failure followed by retry, and message-map failure followed by
retry. The real core's noqueue failure cleanup is exercised. Actual pinctrl,
master initialization, DMA map and wire transfers are recorded in this fixture.
Master setup is a clean-result stub: production partial DMA/channel allocation,
premature setup publication and hardware/firmware discovery are NOT validated
or repaired here. Continuous queued failure recovery and full autosuspend,
remove/deep-sleep/VM ownership remain pending, as does successful fallback
message-reference pairing.

Reproduce build_geni_bus.py --pm-tests and QEMU mode geni-pm (19 cases).
The earlier 17-case preliminary run is retained; the final run includes native
SPI-core coverage. All 323 production imports match. Evidence:
reports/geni-pm-stage76-validation.json. Drafts remain outside Stage57; no real
AVR 7.3 boot or physical-function acceptance and no partition writes occurred.

## Stage77: fail-closed SPI master DMA initialization

The full master-setup method previously continued after missing channels or
allocation failures, sometimes returning success and marking setup complete;
configuration failures left devres allocations behind, and a zero QUP hardware
version could fail after setup had already been published. Keep newly acquired
TX/RX channels and GSI allocations local until all initialization succeeds.
Return request errors (NULL becomes -ENODEV), -ENOMEM for allocation failure,
original configuration errors, and -ENXIO for a missing hardware version. Release
all newly acquired channels/allocations on failure without publishing their
pointers. Move setup publication to the successful tail; retain the deliberate
shared first-message handoff flag and LE common-register bypass.

Nine SMP4 cases compile the entire exact production master-setup method. Native
SPI objects, real devres allocation/free and native dmaengine_slave_config
callback dispatch are used; channel request/release, configuration results,
MMIO/firmware values, IPC context and peripheral setup are explicitly recorded.
Exercise both request positions, NULL channel, both allocation positions, both
configuration positions, zero version, cleanup and retry; shared handoff,
nonshared no-lock allocation, FIFO and LE validation/bypass. Assertions check
no setup/resource publication during configuration and no retained resource
leases after each injected failure. No warning or panic occurs. This validates
source ownership only, not real channel shutdown, hardware register rollback,
interrupt synchronization, firmware loading or full driver probe/remove.

Reproduce build_geni_bus.py --setup-tests and QEMU mode geni-setup. All 323
production imports match. Evidence: reports/geni-setup-stage77-validation.json.
Draft artifacts are separate from Stage57. Continuous queued recovery after
hardware prepare failure, autosuspend/remove/deep-sleep/VM integration, physical
AVR transactions and all remaining whole-board acceptance gates are pending.

## Stage78: native-derived AVR TSENS v2 driver and selected DT coverage

Saved AVR DT uses seven qcom,tsens-v2 controllers, not the legacy tsens26xx
family. The initial legacy compile attempt is retained as rejected investigation;
its incompatible driver is not part of this port. Build pinned native 7.3 TSENS
as an external qcom_tsens module, preserving native thermal APIs/regmap/IRQ/devres.
Bound DT sensor counts by the hardware field count and explicit hardware-ID
array length. Retain the pinned factory generic-v2 204000 mC trip range instead
of the native default 120000 mC. This does not change any DT trip or calibration.

Selected DT checks pass for all seven controllers / 49 declared sensors / 49
thermal zones: generic compatible, indexed TM/SROT resources and field coverage,
native uplow/critical IRQ names, inherited active GIC provider, level-high SPI
specifiers, complete in-range sensor ID references, trip range and normalized
saved/candidate equality. No DT was modified. This is not full schema validation,
cooling-device binding proof or whole-board acceptance.

Eight SMP4 tests pass without WARN/panic. Compile the complete production source
with only platform-driver registration disabled in the test module and separate
exact translation units for native variant data. Use native regmap and regmap
fields over private scripted RAM. Cover all 4096 signed 12-bit codes, all 16 slots,
stable invalid-sample fallback, unstable -EAGAIN, later valid sample, read errors,
factory range, mask isolation and signed IRQ-threshold decoding. The separate
production qcom_tsens module also registers/unregisters in this guest. Actual
probe/MMIO, calibration/firmware state, interrupt delivery, critical thermal
protection, watchdog and suspend/remove are not validated. Factory persistent
temperature diagnostics and native setter error propagation remain pending.

Reproduce build_tsens.py --tests, audit_tsens_dt.py and QEMU mode tsens. Module
imports match. Evidence: reports/tsens-stage78-validation.json. Artifact is a
separate draft; main Stage57 bundle remains 118 modules. No physical 7.3 boot,
partition writes or whole-board daily-use acceptance occurred.

## Stage79: TSENS threshold-window register failures

Return register errors from native-derived interrupt mask helpers. In set_trips,
check the initial snapshot, mask both boundaries during updates, check both
threshold writes and interrupt reenables, and return the first error. On error,
try to mask both boundaries; do not overwrite the original errno. A permanent
bus fault cannot guarantee masking or rollback. Other IRQ-handler callers still
need an error-handling review; their previously ignored return values are not
claimed repaired by this callback change.

Eleven SMP4 cases pass without WARN/panic: the previous eight plus a registered
native tripless-zone object invoking exact production set_trips. Cover signed
and factory-range/clamped thresholds, all 29 transient register operation fault
positions, masking after failure and manual callback retry, and permanent read/
write failure without false success. This is not thermal-core automatic retry:
the native core caches requested boundaries before invoking set_trips and skips
identical requests subsequently. Framework-level recovery remains unresolved and
is an explicit daily-use blocker. No atomic hardware rollback is claimed.

Evidence: reports/tsens-stage79-validation.json. Build with build_tsens.py --tests
and QEMU mode tsens (11 cases). Production imports match; no DT values changed.
Artifacts stay separate from Stage57. Physical thermal control and whole-board
acceptance remain pending; no physical boot or partition writes occurred.

## Stage80: actual native thermal core retries failed threshold windows

Reproduced a skipped same-window retry on Image #23 with the actual running
thermal_zone_set_trips function, resolved through the existing native diagnostic
symbol provider and called with a real zone lock held. This is not an extracted
or substitute core function. The original Image passes 13/14 cases and fails
the same-window retry assertions: no register access, old thresholds and masks
still set. Preserve the rejected baseline and strict log-check failure.

Patch native thermal_trip.c to cache only successful windows. On failure use
reversed cached limits to invalidate every valid next request, including a
return to the old window or an unbounded window. Rebuild Image/modules (#24).
All exported native symbol CRCs remain unchanged; no public structure/API layout
changed. Core checkpoint/replay now includes 18 patches / 64 exact files.

The same guest tests pass 14/14 without WARN/panic on the updated Image. Three
actual-core cases cover failed same-window retry, old/full-range requests after
failure, and suppression of an already successful duplicate. The previous full
TSENS source/regmap tests, including all 29 transient callback fault positions,
remain covered. Test registration/zone/MMIO values are private to the guest.

Evidence: reports/thermal-window-stage80-validation.json. The separate artifact
includes the required new Image; source-module import CRCs still pass. This fixes
cached-success bookkeeping, not retry scheduling: the next request may retry,
but no autonomous timer is added for a failed interrupt-only zone. Permanent
bus-fault masking, physical thermal protection and all remaining whole-board
acceptance remain pending. Stage57 is unchanged; no partition writes occurred.

## Stage81: exact successful SPI message PM ownership through cleanup

Persist only references acquired/inherited by a successful prepare_message and
its successfully acquired bus lock. Release those references in unprepare_message,
including a successful nonshared fallback resume. Release an acquired reference
even if runtime PM was subsequently disabled; current PM status is not an
ownership token. Preserve the hardware-phase/client references. Reject duplicate
prepare while the original message remains prepared, and make repeated/late
unprepare inert. These fields rely on normal SPI-core message-pump serialization.

Twenty-five SMP4 cases pass without WARN/panic. The previous 19 include native
registered SPI-core spi_sync failure/retry coverage. Six new cases cover fallback
success/reference balance, PM disabled before cleanup, native autosuspend,
duplicate prepare ownership, late cleanup after failed preparation and LE client
ownership. The test uses real native PM/SPI objects and callbacks, with recorded
hardware mode/lock/map/setup actions. Production map/unmap and full master setup
have separate Stage74/77 evidence; this fixture does not join them into physical
controller transfer acceptance.

Reproduce build_geni_bus.py --pm-tests and QEMU mode geni-pm (25 cases). All 323
production imports match the current core. Evidence:
reports/geni-pm-stage81-validation.json. Draft archive includes matching Image
#24; Stage57 remains unchanged. Full hardware-phase lifecycle, continuous queued
failure recovery, physical DMA/SPI and all remaining board gates are pending.
No physical 7.3 boot or partition writes occurred.

## Stage82: native queued SPI hardware-phase recovery

When hardware preparation fails, the native SPI worker may leave its queue busy
and invoke prepare_message for subsequent messages without another hardware
callback. If the driver's phase-progress flag is clear, explicitly prepare the
hardware phase before message setup (LE remains client-managed). Successful
normal phases are preserved. Also release the nonshared hardware-owned PM
reference on unprepare even if runtime PM was disabled after acquisition.

Reproduce both bugs on the old source: 25/27 cases pass; the queued case misses
its second setup and continues with setup=false, and disabled-PM hardware cleanup
leaves a reference. The same suite passes 27/27 after the fix without WARN/panic.
Three messages are submitted via actual spi_async while holding the native SPI
io_mutex to stage them in the same queue; the first setup fails once, the next
reestablishes hardware preparation and two following messages complete. Flush
the real native worker before checking zero PM references and idle state. The
fixture uses real PM/SPI objects but records master initialization, pins and
transfers, so physical recovery/full master setup is not claimed.

Builder extraction now selects definitions rather than forward declarations and
orders exact hardware methods before the message methods in the test header.
Evidence: reports/geni-pm-stage82-validation.json. Reproduce build_geni_bus.py
--pm-tests and QEMU mode geni-pm (27 cases). Three production modules rebuild
with 323 matching imports. Full suspend/remove/VM/DMA/firmware and whole-board
acceptance remain pending. Stage57 stays unchanged; no partition writes occurred.

## Stage83: native-derived AVR TRNG driver and DT resource audit

Build pinned native 7.3 qcom-rng as an external draft matching the saved active
qcom,trng node at /soc/rng@10c3000. Verify one TM resource covering data/status,
the native true-random match entry, and the absent optional core-clock path.
Selected saved/candidate normalized DT properties are equal. No DT was modified;
this is a resource-shape audit, not full schema or entropy acceptance.

Honor hwrng wait=false without sleeping, make zero-length requests inert,
return copied bytes if a later FIFO wait fails, bound size_t requests before
narrowing to an int-returning read, and reject nonempty NULL buffers. Preserve
native registration/entropy quality and clock APIs. Eleven SMP4 cases pass
without WARN/panic using exact full source, recorded FIFO MMIO, native polling
and native absent/fixed-rate clock APIs. Cover tails 1..65 and unaligned guards,
empty/nonblocking/partial/delayed FIFO, blocking timeout, copied prefix, a zero
word, oversized request and clock lifetime. Actual production driver registration
and unload also pass in the guest. No synthetic entropy provider is registered
and no recorded bytes are fed to the kernel entropy pool.

Reproduce build_rng.py --tests, audit_rng_dt.py and QEMU mode rng (11 cases).
Imports match the current core. Evidence: reports/rng-stage83-validation.json.
Physical source/firmware/entropy health and quality, probe/remove/suspend and
whole-board daily-use acceptance remain pending. Draft archive is separate from
Stage57; no phone MMIO/partition writes or physical 7.3 boot occurred.

## Stage84: watchdog/SDEI drafts and real native firmware dependencies

Add pinned qcom_wdt_core, qcom_soc_wdt and qcom_sdei drafts. Expose four existing
native SDEI event APIs as GPL exports and add the exact factory SCM secure-watchdog
transaction with an unready-SCM deferral guard. Native patch replay now covers
19 patches / 65 exact files. Rebuild Image (#25) and native firmware modules;
all preexisting exported CRCs remain unchanged, with exactly five new exports.
Use the matching native SCM module, not its older artifact lacking the new API.

Use native timer APIs; retain OF IRQ trigger type instead of forcing rising edge.
Validate negative IRQ errors/time intervals and read saved DT bark/pet requests.
Make register maps per-device, reject mapped error pointers and invalid resource
offsets/rates, check wide time multiplication and hardware counter bounds. Return
secure-disable failures. Unregister PM notifier on normal removal/enable failure
and clear failed publication. SDEI retains a real native module pin until all
native event registrations are gone, retries EINPROGRESS, retains ownership on
unregister failure and propagates reset status. It does not claim firmware success
through a missing-provider stub. Native signal macro equality is asserted.

Selected DT audit passes for the saved watchdog's named MMIO, native clock default,
11s requested bark / 9360ms pet, counter limits, level-high GIC specs and inherited
active interrupt parent. Saved/candidate semantics match. No active SDEI node
exists in these saved inputs, so none is added. Read-only live 6.12 binding and
pet interval observations are separately recorded; they are not 7.3 hardware proof.

Fourteen SMP4 cases pass without WARN/panic: six exact register-helper cases use
actual raw IO writes only to private guest RAM; eight compile full SDEI source
with recorded firmware APIs, native module pins/CPU calls and a selected native
SCM method with recorded transport. Cover resource isolation, counter overflow,
all four registration/enable failures, retry, EINPROGRESS, failed unregister
pinning, firmware IDs/arguments/status and missing SCM. No actual HVC/PSCI/reset
was issued by tests. All three production modules load; the unbound SoC driver
unloads. Factory watchdog core has no module_exit and remains with providers
until the private guest shuts down. All 102 module imports match.

Evidence: reports/watchdog-stage84-validation.json. Reproduce build_watchdog.py
--tests, audit_watchdog_dt.py and QEMU mode watchdog. The artifact includes required
Image/native firmware modules and remains separate from Stage57. Full controller
probe/IRQ/thread/timer/sysfs/PM/hotplug, actual panic/reset/firmware, minidump record
removal/lifetime, retained SDEI recovery and factory IRQ-stat/die-notifier variants
remain unresolved. No daily-use acceptance, physical boot or partition writes.

## Stage85: watchdog minidump record and allocation ownership

Store the exact zero-initialized minidump descriptor after a successful add and
track whether this instance owns a registration. Remove that record after the
watchdog work is stopped during remove. Do not remove records after a failed add;
reject duplicate adds and make repeated cleanup inert. ENOENT confirms absence.
Replace automatic devm allocation with an owned allocation and native devres
release action: free on normal/probe-error cleanup only when no dump registration
remains. If provider removal fails, retain the actual payload allocation rather
than leave its physical dump address dangling. MMIO/device mappings are not held
alive by this retained flat payload. Automatic reclamation after later backend
recovery is not implemented; retention is an explicit memory tradeoff.

Twenty-one SMP4 cases pass without WARN/panic. The previous 14 remain covered.
Seven new cases compile exact dump and free methods, use native allocation/devres,
and include two cases with the real pending/unready minidump provider (normal
remove and injected remove failure). Also cover failed/duplicate add, ownership,
repeated cleanup, ENOENT, retained descriptor contents and native probe-allocation
release. Recorded backend failures are distinguished from the real API. This is
not initialized SMEM/firmware-table acceptance or complete controller teardown.

Evidence: reports/watchdog-stage85-validation.json. Reproduce build_watchdog.py
--tests and QEMU mode watchdog (21 cases). Production modules rebuild/import CRCs
match and the artifact retains matching Image/native firmware modules. Remaining
watchdog IRQ/thread/timer/notifier/sysfs/PM/hotplug paths, retained-record recovery,
actual firmware reset and whole-board acceptance remain unresolved. Stage57 is
unchanged; no physical watchdog operations, 7.3 phone boot or partition writes.

## Stage86: full watchdog core ownership and native cleanup

Separate ownership flags from enabled: suspend may stop the counter while IRQs
and notifiers still exist. Initialize locks/waits/both timers before publishing
callbacks, populate all PPI storage slots and unwind each acquired resource on
init failure. Check timeout, notifier, hardware and sysfs registration errors.
Removal closes/drains the sysfs group, stops hardware activity, unregisters owned
IRQ/PM/CPU-PM/panic/die/restart callbacks regardless of counter state, shuts down
timers and wakes/stops the worker. Keep OF/platform IRQ mappings with their owner.
Track and serialize singleton registration; repeated removal is inert.

Use one atomic sysfs group preserving optional file visibility, readiness gates
and serialized writes. A permanently disabled controller cannot restart from a
PM callback or user-pet write. Serialize worker/PM/sysfs hardware operations with
the same mutex. Worker waits observe stop in both timer and userspace-pet waits,
prevent rearming after shutdown and take a stable online-CPU snapshot for IPI.

Thirty SMP4 cases pass without WARN/panic. Nine new cases compile the complete
production core, rename exported names for test isolation and record hardware/
secure operations. Use native private IRQ descriptors/chip/actions, real notifier
chains/sysfs/timers/kthread/CPU calls and real pending minidump APIs. Cover actual
petting, ten init failure points, thread creation error, suspend-then-remove,
permanent disable/resume, secure-disable error, unready sysfs, userspace-pet wait
stop and repeated register/remove. Verify IRQ can be requested again and actual
notifier/sysfs registries have no remaining registration, not just local flags.
No bark IRQ, panic, physical watchdog or firmware reset is triggered.

Evidence: reports/watchdog-stage86-validation.json. Reproduce build_watchdog.py
--tests and QEMU mode watchdog (30 cases). Draft modules rebuild and import CRCs
match. Platform probe/MMIO/physical IRQ/firmware, PPI and factory diagnostic
variants, hardware-error recovery and exhaustive concurrent caller/unbind/PM
races remain unverified. Retained dump/SDEI recovery and whole-board acceptance
are pending. Stage57 is unchanged; no phone partition writes or 7.3 phone boot.

## Stage87: BCL SoC depletion sensor and native OF thermal links

Port pinned factory bcl_soc.c to native 7.3 thermal/power-supply/cpufreq APIs.
Replace the global device and per-CPU static requests with per-device ownership.
Acquire battery/USB references once with managed puts and return probe deferral
when unavailable. Register the native thermal zone; gate readiness until notifier
registration succeeds. On teardown close notifications, drain immediate/delayed
work and remove owned QoS requests before the managed zone and supply references
are released. Keep callbacks outside the driver mutex when entering thermal core.

Preserve the 100-capacity depletion metric, absent-battery result, cached trend,
high-threshold gating and opt-in boot cap (default INT_MAX). Reject invalid
presence/capacity and propagate property errors without fabricating a healthy
value. Retain a requested cap on unknown supply state. Acquire one request and
policy reference per shared policy, rollback partial acquisition and release
only owned requests. No fake secure/thermal/vendor success callbacks are added.

Twenty SMP4 cases pass with no WARN/panic/skip. The complete production source
is compiled into the test translation unit with only policy lookup, selected
faults and generic-zone selection intercepted. Native power_supply registry,
get_property, notifier, work, devres and QoS requests execute; init/read helpers
not exported by the native core are extracted verbatim from the pinned base
with local names. Physical CPU policy/hardware is never registered. All 101
valid capacities, invalid/error values, missing suppliers, zone/notifier failure,
threshold/trend behavior, actual changed notifications, repeated probe/remove,
shared policy ownership, partial rollback and boot cap decisions are exercised.

Two additional cases select the actual devm_thermal_of_zone_register path using
a private guest DT with saved AVR scalar semantics. Verify zero-cell sensor,
90/95 passive depletion trips, 100ms passive delay, absent polling-delay=0 and
three actual private native cooling-device bindings only on the 95 trip, with
bounds 0..1/0..4/0..4. Test does not assume DT child enumeration order. Repeated
OF zone registration/removal is exercised. Cooling callbacks only record state.
Production bcl_soc.ko separately loads/unloads unbound in this guest.

Saved running DT and Stage53 candidate preserve the same selected provider,
consumer, all trip scalars and normalized trip/cooling references. No AVR DT is
modified. Audit the compiled guest scalar fixture against the saved semantics.
This is selected binding/shape evidence, not whole-board schema/provider proof.

Evidence: reports/bcl-soc-stage87-validation.json. Reproduce with
scripts/build_bcl_soc.py --tests; scripts/run_qemu.py --test bcl-soc --cpus 4
--timeout 120 --log reports/bcl-soc-stage87-qemu.log; and
scripts/audit_bcl_soc_dt.py --guest. The draft has 35 matching import CRCs and
retains matching Image#25/config/native exports. Stage57 stays at 118 modules.
Physical supplier/charging/CPU/cooling, forced supplier unbind, late-supplier
probe wakeup, exhaustive PM/unbind races and whole-board boot/DLKM/SELinux
acceptance remain pending. No phone partition writes or physical 7.3 boot.

## Stage88: BCL recovery after late power-supply class registration

Reproduce a startup gap with real native driver binding: register BCL while
battery/USB are absent, then register the actual native private power-supply
class devices. After their changed work and native deferred probes drain, BCL
remains unbound; an explicit native driver_attach binds immediately. A supplier
class event is not itself a guarantee of a successful supplier-driver bind.

Add a module-level named-supply notifier and native work. A missing name sets
an atomic retry-demand flag; events queue work only while that demand exists.
Work atomically consumes the flag and invokes native driver_attach, which
serializes probes/skips bound devices and preserves genuine probe failures.
Missing suppliers restore the demand on each failed attempt. There is no timer
polling, shared per-device supply pointer, fake provider or core API export.
Register the notifier after the driver then queue an initial attempt to cover
the missed-event window. On exit unregister the notifier, drain retry work and
only then unregister the platform driver. Handle registration/attach errors.

Twenty-five SMP4 cases pass without WARN/panic/skip. The previous twenty remain.
Five new native driver-core cases cover the no-hook regression, battery-then-USB
OF binding, repeated missing-USB retries without battery reference leaks,
module-notifier failure after real binding with complete rollback, and queued
retry teardown before late supplies appear. Native class registry, notifiers,
driver binding, devres, OF thermal, QoS and work execute; CPU policy references,
selected errors and physical cooling operations retain the recorded scopes.

Evidence: reports/bcl-soc-stage88-validation.json. Build with build_bcl_soc.py
--tests and run QEMU mode bcl-soc, SMP4 (25 cases). audit_bcl_soc_dt.py --guest
--output reports/bcl-soc-stage88-dt.json preserves Stage87's historical report.
The new draft retains matching Image#25/config/symbols and 39 matching imports;
Stage57 remains unchanged at 118 modules. Physical behavior, supplier forced
unbind, userspace force-unbind policy, exhaustive concurrent teardown/PM races,
remaining drivers and whole-board acceptance remain pending. No phone writes.

## Stage89: BCL initial supply event window and PMIC DT dependency audit

Twenty-six SMP4 BCL SoC cases pass without WARN/panic/skip. The new case
publishes both real supplies and drains their changed events between the initial
missing-supply probe and the module supplier notifier registration. Initial
retry work closes this window and completes actual native platform binding.
Production Image#25 and bcl_soc.ko are byte-identical to the Stage88 artifact.
The fixture remains private; physical supply/thermal/CPU hardware is not modeled.

Audit pinned qti_bcl_pmic5.c, qti_bcl_common.h and qti_bcl_stats.c at factory
037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1 against saved/candidate AVR DT. One
hundred checks per input pass for two active providers, eleven declared zones,
sixteen passive trips, six cooling maps, native-style inherited four-cell SPMI
IRQ routes, sensor enum IDs and normalized NVMEM/cooling/trip references. Keep
the disabled third provider and all selected saved semantics unchanged.

Both active saved nodes use qcom,bcl-v5, selecting pmih010x_data in factory
source. Do not guess a replacement compatible for the PM8550 node or reinterpret
calibration. Parent has one address cell and zero size cells; the raw second reg
cell 0x100 is not evidence of a native resource extent. The factory helper uses
the first address. The SDAM cell is byte0x5d bit5; its physical value is unknown.
DT declares standard IDs0/1 and alternative 2S IDs8/9; factory NVMEM selects one
pair. Event IDs5/6/7 and voltage/current metrics are not degree-temperature units.

This audit does not port or prove PMIC BCL operation. Required implementation
includes native thermal API/layout, error propagation from NVMEM/IRQ/zone/write,
per-device resource ownership and probe/remove/freeze/restore rollback. Factory
notifier callback is assigned after registration, device allocation/count globals
can become stale, and debugfs/IPC lifetime needs repair. Preserve real register
semantics and validate calibration/endianness/threshold arithmetic before hardware.

Evidence: reports/bcl-stage89-validation.json and reports/bcl-pmic-stage89-dt.json.
Reproduce build_bcl_soc.py --tests, QEMU mode bcl-soc --cpus4 (26 cases), and
audit_bcl_pmic_dt.py. No board DT changes, phone partition writes or physical
7.3 boot. Main Stage57 bundle remains unchanged; whole-board acceptance pending.

## Stage90: full-source PMIC BCL module draft

Build pinned factory qti_bcl_pmic5.c/common header/stats as bcl_pmic5.ko against
actual native 7.3 thermal private layout/accessors and the existing real IPC log
provider. Remove the vendor thermal change_mode dependency; native thermal core
owns zone mode. Do not fabricate a provider or successful callback. Preserve the
ten sensor IDs, IRQ-name table, compatible matches, descriptor registers/scales
and BCL constants. Selected saved/candidate DT contract checks pass for both
active nodes, eleven declarations, sixteen trip nodes and six cooling maps.
Both current sensor pairs are declared; NVMEM selects the pair actually used.
No DT or GPU tables are modified by this stage.

Replace stale allocation/count globals with per-device storage. Initialize all
sensor locks and stats back-pointers before callbacks. Propagate revision,
NVMEM, IRQ/zone and monitor/BPM enable failures. Native OF absence ENODEV remains
optional where a sensor has no consumer. Assign the notifier callback before
registration. On failed/normal teardown close callback publication, remove the
owned debugfs subtree, drain owned IRQ actions and thermal polling, then destroy
the log context. Retain a shared debugfs root only while devices own children;
use device names for diagnostic subtrees. Keep platform IRQ mappings with their
owner. Track wake ownership and retain IRQ actions across freeze/restore instead
of freeing invalid/unowned IRQs. Acquire wake references before rearming on
restore and rollback newly acquired references on the tested wake-enable error.
Frozen threshold updates return EBUSY so native thermal core can retry later.

Serialize ADC access per sensor and only publish successful callback samples.
Widen intermediate arithmetic and clamp CCM requests before narrowing. Preserve
factory no-reading/cache behavior. Decode BPM's eleven wire bytes explicitly LE
into a local snapshot, attempt CLR after failed reads, and only commit caller
output/timestamp after successful HOLD/read/CLR. This is output consistency, not
a guarantee of hardware recovery if the bus rejects CLR. Propagate peer PMIC
notifier errors to voltage-trip callers. A primary write can already have
succeeded when a peer fails; cross-chip rollback is not claimed.

Twenty-two SMP4 cases pass with PROVE_LOCKING and no WARN/panic/skip. Tests
compile both complete production C files, suppress only platform-driver macro
registration, and use real native regmap, generic thermal zone, private IRQ
actions/wake bookkeeping, notifier and debugfs registries. RAM bus/chip operations
record data/errors; no physical SPMI, PMIC IRQ line or wake delivery is modeled.
Production module separately loads/unloads unbound with real dependency modules.
Cover signed/voltage samples and selected legacy scaling, caches and read errors,
threshold programming/ranges/write errors, CCM narrowing, all thirteen BPM bus
failure positions and snapshot decoding, revision errors, stop/frozen gates,
IRQ ownership/reusability/wake enable failure, repeated freeze/restore and wake
rollback, two debugfs subtrees, BPM reset errors and peer threshold fanout/retry.

The first guest had positive assertions but was rejected by the strict checker
because lockdep detected same-class sensor lock nesting. Remove unused auxiliary
ADC reads from the level callback; the IRQ handler already collects them, and
they do not contribute to the level metric. This removes nesting and redundant
bus work. The final fresh guest passes with lockdep active; its errors from
fault injection are expected register-operation errors, not hidden warnings.

Evidence: reports/bcl-pmic-stage90-validation.json. Reproduce
scripts/build_bcl_pmic.py --tests; scripts/run_qemu.py --test bcl-pmic --cpus 4
--timeout 120 --log reports/bcl-pmic-stage90-qemu.log; and
scripts/audit_bcl_pmic_dt.py --port --output reports/bcl-pmic-stage90-dt.json.
Draft includes unchanged matching Image#25/config/native exports, the new module
and four existing providers (5 modules, 288 matching imports; new module 60).
Stage57 remains unchanged at 118 modules; this draft is not promoted or flashable.

Still pending: real OF/platform probe with NVMEM/SPMI routing and full partial-
probe fault integration, initial physical events, calibration and monitor/BPM
policy, actual IRQ/firmware/cooling behavior, disable-wake/cleanup error recovery,
open-debugfs-file teardown stress and exhaustive notifier/IRQ/thermal/PM/forced-
unbind races. Hardware variants/ROM diagnostic paths and initialized firmware
IPC/minidump state are not accepted by these tests. Whole-board drivers, DT
schema, boot/vendor_boot/dtbo/DLKM/SELinux and daily-use acceptance remain pending.
No phone partition writes, 7.3 phone boot or physical BCL operations performed.

## Stage91: native OF/NVMEM/probe/IRQ integration and thermal compatibility

Thirty-six SMP4 cases pass without WARN/panic/skip: the previous 22 plus 14 native
probe cases. Compile the complete production source in a separate test module;
register a private compatible to the same factory descriptor, and a real native
RAM-backed supplier platform driver. It creates the actual regmap and NVMEM
provider in probe. Do not allocate device resources before driver probing or
bypass firmware/device-link checks. Late supplier-driver binding automatically
retries the consumer. Probe uses actual OF thermal registration, SDAM byte 0x5d
bit 5 lookup, IRQ resources/wake actions, IPC logging and debugfs. Both standard
and 2S bits yield six registered zones from eight PMIH declarations. No native
successful-operation stub is installed; selected zone errors are injected only
before calling the real API.

Cover eleven failure conditions: two NVMEM reads, four revision reads, three
zone registrations, monitor enable and BPM enable after notifier registration.
Check native registries are empty and IRQs can be requested again after rollback.
Release/rebind the actual device three times. Dispatch a private IRQ through the
native core into the real BCL thread, observe trip/IRQ disabling, clear the RAM
status and update thermal to re-arm it. The physical IRQ chip/SPMI is not modeled.
Verify OF voltage trip indices 2800/2600/2300 program offsets 0x48/0x49/0x4a as
56/14/2. Production Image#25 and bcl_pmic5.ko remain identical to Stage90.

The new native OF tests reject both saved pmih010x-2s-ibat-lvl0/1 names (21
characters): the actual 7.3 thermal type array is 20 bytes including NUL. Also,
ordered fixture comparison reveals that the Stage53 candidate has 39 trip sibling
groups in a different order from the saved running tree. This matters because
BCL voltage trip indices select different comparator registers, and userspace
can address trip indices. Keep the kernel thermal ABI unchanged. Build a new,
unpromoted DT file from Stage53: restore those 39 entire sibling groups to saved
order and rename only the two overlength zones to pmih010x-2s-ibat0/1. Preserve
all physical property values, unrelated node order, reservations, strings table
and 21 GPU levels. No trip-child set lacks a saved counterpart. Verify with native
DTC for parse/build only (its inherited binding warnings remain, and no full
DT-schema acceptance is claimed) and compare the private fixture's eight ordered PMIH zone definitions,
provider ID/flags, polling and NVMEM bit to this new candidate.

Read-only live inspection still shows normal current-zone names and nine BCL-
related types on 6.12 with SELinux Enforcing. Do not infer the actual bit value
from this snapshot. Vendor thermal HAL SHA256 dc4b68fa0103ca2cefec856b7eef90977f44d6edbfa7f373d4dfcf0d5db0237f
contains the old 2S lvl0 string at file offset0x6239; lvl1 is not a literal in this
binary. Disassembly at 0x1e5cc..0x1e630 identifies the address and an inlined
short-string size tag at 0x1e610. A literal-only replacement is insufficient.
Prepare a local draft that changes the literal and its length tag (six changed
bytes) while retaining ELF layout/headers/relocations. The original build-id note
remains, so use the new SHA256 to identify the modified file. Do not execute or
install the modified HAL. A static native ARM64 harness executes only seven
selected original/candidate constructor instructions on private buffers. Both
cases verify names, tagged lengths, terminator and surrounding guards. This is
not execution of the complete constructor/control flow, a proof of every indirect
reference or acceptance of the Android service. Full binary/ROM integration and
DT pairing remain unverified; the DT-name candidate is not independently ready
for deployment in 2S mode.

Evidence: reports/bcl-stage91-validation.json. Build tests with build_bcl_pmic.py
--tests and run QEMU mode bcl-pmic --cpus 4 (36 cases); build_bcl_name_dt.py and
audit_bcl_probe_fixture.py reproduce DT preparation/audit. prepare_bcl_thermal_hal.py
requires the pinned dump (or the included original file) and prepares the exact
local draft and constructor.S. Compile tests/bcl-pmic/thermal-hal-constructor.c
with that assembly in the native arm64 Linux rootfs to reproduce the two-case
selected-instruction check. Artifacts: dt-bcl-name-stage91 and
bcl-thermal-hal-stage91. These are review drafts, not flashable boot packages.
No partition/vendor file writes or phone 7.3 boot. Physical PMIC/calibration,
full service behavior, forced supplier removal/concurrent teardown/hardware
errors, SID4 and remaining board drivers/DT/boot/DLKM/SELinux acceptance are pending.

## Stage92: fallible wake references and native regmap supplier lifetime

Forty-four SMP4 cases pass without WARN/panic/skip: the previous 36 plus six wake/
PM fault cases and two native supplier-unbind cases. Inspect actual native private
IRQ descriptor wake_depth as well as driver flags and recorded IRQ-chip balances.
The native core restores wake_depth to 1 if disable_irq_wake fails. Clear driver
ownership only on successful disable. On threshold disable failure preserve both
armed action and wake ownership, return the error, and allow retry. A failed
rearm no longer clears trigger statistics before wake acquisition succeeds.

Freeze records all original IRQ actions before a fallible step. A tested partial
wake-drop failure restores the complete original set. Restore skips retained
wake references, records only references acquired by the current attempt, and
unwinds only that set. If rollback cannot drop one reference, keep its ownership
flag; a later retry must not increase native depth from 1 to 2. Wake-enable and
monitor-write failures retain retry state. Quiesce reports a failed wake release
and keeps the flag for a subsequent cleanup attempt while its mapping remains
alive; the transient-error case demonstrates that retry. This is not guaranteed
recovery from permanent hardware failure during final removal: remove is void,
and no persistent owner/retry facility is implemented after context destruction.

A parent device relationship alone did not protect the shared regmap from driver
unbind. In the before-link guest the BCL consumer stayed bound while its supplier
removed a map whose mutex was held by the actual IRQ thread; strict validation
rejected failed assertions and a mutex_destroy WARN. Add a native managed
DL_FLAG_AUTOREMOVE_CONSUMER link to the regmap-owning parent before using its
resources. The driver core now unbinds/drains the consumer before supplier remove,
and owns link cleanup. No caller-side deletion of a managed link, runtime-PM
reference manipulation or dependency-check bypass is introduced.

Three native supplier release/rebind cycles pass (normal native consumer attach
is explicitly used if intentional unbind requires it). The controlled concurrency
case blocks the real threaded IRQ inside a RAM bus read, starts supplier removal
in a native kthread, and observes BCL stopping while regmap/NVMEM remain alive and
removal is incomplete. Releasing the gate lets teardown finish with no remaining
zone/notifier/debugfs/IRQ/wake ownership. The IRQ checks stopping again after its
sleeping status read; no later bus read begins after the test stop marker. All
fixture exit paths release the gate before waiting for the removal task. The bus
and IRQ-chip operations remain recorded private hardware; physical SPMI/IRQ/reset
behavior and exhaustive races are not accepted by this test.

Evidence: reports/bcl-pmic-stage92-validation.json. Reproduce build_bcl_pmic.py
--tests and QEMU mode bcl-pmic --cpus 4 (44 cases). audit_bcl_pmic_dt.py --port
--output reports/bcl-pmic-stage92-dt.json and audit_bcl_probe_fixture.py --output
reports/bcl-probe-stage92-dt.json preserve the historical reports. Selected
calibration/ID/order/scalar contracts remain intact. Image#25 is unchanged; draft
contains five modules with 290 matching imports (new BCL module 62). Main Stage57
bundle stays unchanged at 118. Stage91 DT/HAL candidates are unchanged and remain
unaccepted; no full modified HAL execution or phone 7.3 boot is performed.

Permanent wake-release failure, concurrent probe/PM/forced-unbind/hotplug variants,
physical calibration/charging/cooling/firmware, full HAL/ROM and DT-schema work,
SID4 and remaining board drivers/boot/vendor_boot/dtbo/DLKM/SELinux/daily-use
acceptance remain pending. No phone partition or vendor file writes.

## Stage93: QMI modem/CDSP thermal mitigation draft

New ports/qcom-qmi-cooling driver uses the exact pinned factory generated QMI
protocol tables and whitelist with a rewritten native lifecycle. Desired state
and acknowledged state are separate: negative transport/remote errors invalidate
confirmation; identical unconfirmed targets retransmit. Explicit peer endpoints
and epochs discard replies after peer loss. Validate all base/extended list
bounds/flags/terminated IDs and contradictory duplicate limits before publishing
capabilities. Changing a maximum replaces the native cooling device and clamps
the desired state. Never store ERR_PTR cooling pointers or hold the state/IO mutex
across native cooling registration/unregistration. Drain own work and transactions
before releasing QMI; cleanup is idempotent and state belongs to each device.

Fifteen private ARM64 QEMU cases use the full driver methods and real native QMI
encoding/decoding, QRTR socket requests/responses and thermal cdev registration.
The server replies are recorded local firmware behavior, not physical modem/CDSP
validation. Tests cover golden bytes, 32-byte wire strings/truncation, malformed
lists and contradictory duplicates, repeated transactions, ACK/rejection/timeout,
identical-request retry, peer-loss stale ACK, offline replay, old-peer deletion,
reset, changed max/clamp, unsupported/no-ACK registration and repeated cleanup.
Only automatic platform-driver registration is suppressed in the source-included
test module; the production module also registers/unregisters unbound in the guest.
The real OF probe, name-service discovery and physical firmware paths remain
unaccepted, as do exhaustive concurrency/PM/unbind variants.

The initial local fixture used port0 from an unbound QMI server; explicitly bind
its native QRTR socket. Integration then exposed the draft's incorrect treatment
of positive qmi_txn_init IDs as errors: native API returns transaction ID >=0 and
requires wait/cancel for every success. Correct the negative-only test and
normalize nonnegative decode completion. The repeated-transaction test inspects
that the native transaction IDR is empty after every completed request. Failed
before/final-named diagnostic logs are retained as failures; only fixed-qemu is
accepted by reports/qmi-cooling-stage93-check.json (15/0/0/15, no WARN/panic/skip).

Reproduce: build_qmi_cooling.py --tests, then run_qemu.py --test qmi-cooling
--log reports/NEW-qmi-cooling.log. Source replay: audit_qmi_cooling_sources.py.
DT audit: audit_qmi_cooling_dt.py; the saved tree and Stage91 candidate retain all
27 QMI declarations and normalized cooling-map references, ignoring renumbered
phandle identities. No DT bytes are changed here. This is not full DT-schema or
runtime binding acceptance. Image#25 is unchanged; the draft has three modules
with 222 matching imports (new driver 42). Stage57 and Stage91 DT/HAL remain
unchanged and unaccepted for daily use. Phone remains 6.12/Enforcing.

report_module_candidate_coverage.py counts 122 same-name 7.3 build candidates
among 544 archived unique names (22.4%). It combines individual drafts and frozen
Stage57; it is not one accepted package, functionality coverage, effort completed
or a daily-use completion percentage. Display/camera/audio/wireless, physical
firmware, remaining board drivers, whole DT/SID4 and boot/vendor_boot/dtbo/DLKM/
SELinux/physical stability remain pending. No phone partition writes.

## Stage94: native OF probe and QMI name-service lifecycle

The private DT fixture reproduces all 27 saved AVR instance IDs, node names, wire
names and #cooling-cells. A container without compatible prevents automatic board
population. Tests explicitly create native platform devices with a private driver
name/compatible; the full production OF parser, probe, managed resource actions,
QMI lookup, worker, thermal registration, removal and native device attach run.
The production driver code and module are unchanged from Stage93.

Two separately bound native QRTR/QMI sockets advertise TMD service instances0 and
0x43. Native QRTR name service performs actual registration/lookup and delivers
new_server/del_server to the production callbacks; new integration cases do not
manually inject those lifecycle events. All27 cdevs obtain advertised max3 and
ACK state0, then send/confirm state2. Firmware values are recorded local replies.
Closing both server sockets makes all27 clients offline; advertising replacement
sockets reconnects all27. Three driver release/attach cycles pass. Two concurrent
platform devices own distinct instance arrays; removing one leaves the other ready.

Native probe rejects duplicate instance IDs, unsupported wire IDs and cooling
arity1 without publishing drvdata. Four targeted failure cases inject only the
first/second QMI handle-init or lookup error. Earlier successful native resources,
callbacks and workers remain real. Wrappers track ownership of each successfully
acquired handle, observe balanced releases after failed probe, then allow the same
platform device to attach and register27 clients before normal balanced teardown.
This does not exhaust allocator failure points or all concurrency/PM variants.

Initial fixture failures are retained: two services on a single QRTR port replaced
one another in the native name service; use distinct endpoint ports. A subsequent
second endpoint bind used node0 instead of the actual local socket node; preserve
native qmi_handle_init identity when binding. The bound-peer diagnostic log passes
22 cases before adding four partial-probe cases. Final log passes26/0/0/26 with no
WARN/panic/skip and normal module unload/powerdown. These corrections apply only to
the test endpoint fixture, not the production driver.

Reproduce build_qmi_cooling.py --tests and run_qemu.py --test qmi-cooling --cpus4
(use separate arguments --cpus 4) --log reports/NEW-qmi.log. Fixture comparison:
audit_qmi_probe_fixture.py. Saved/candidate contract audit remains
 audit_qmi_cooling_dt.py. Evidence: reports/qmi-cooling-stage94-validation.json;
private artifacts: artifacts/qmi-cooling-probe-stage94. Image#25 and the Stage93
production module are byte-identical; Stage57118-module bundle and Stage91 DT/HAL
candidates are unchanged. No phone partition/vendor file writes or phone reboot.

Physical firmware and ROM/HAL consumers, actual thermal-zone cooling-map binding,
exhaustive PM/concurrency/unbind and all remaining board drivers/full DT/SID4/boot/
vendor_boot/dtbo/DLKM/SELinux/daily-use work remain pending. Candidate count stays
122/544, a module-name proxy only; this integration milestone does not imply a
new functional completion percentage or physical daily-use acceptance.

## Stage95: saved cooling maps, native governor and failed-read fix

Core Image#25 ignored get_cur_state errors in step_wise and used an uninitialized
current state to compute mitigation. A private native-governor regression creates
a real OF BCL thermal binding, invalidates the QMI acknowledged state, resets the
instance initialization/target, then calls the actual native governor under the
zone lock. Before the patch it incorrectly initializes the instance and computes
target1; the other26 lifecycle tests pass. Strict checking rejects that guest.

Move the successful current-state read ahead of get_target_state and skip failed
reads before changing target/initialized. Valid-state policy stays intact. New
patch thermal-cooling-read-error-7.3.patch and reviewed native source commit are
in core-patch-checkpoint.json; the exact replay grows to20 patches/66 files.
Build_native.py includes the new patch in fresh builds. Full native Image/modules
rebuild succeeds as Image#26. No new exported interface or thermal struct layout.

The private overlay now projects four saved zones (PMIH BCL levels0/1, socd and
mdmss-0), preserving ordered trip names/temperature/hysteresis/type, passive delay
and eight QMI cooling-map references/limits. Other map providers are omitted and
sensors replaced by separate private recorded inputs; this is not whole-board DT
validation. audit_qmi_thermal_maps.py compares saved and guest declarations with
normalized phandles. Native devm_thermal_of_zone_register creates the actual zones
and native bindings; no mocked bind/governor or substituted successful RPC.

New tests verify eight bindings; native step_wise modem targets8/10/100, then255,
then release0; shared BCL demand retains2 when only the lower-demand zone cools;
CDSP follows saved socd cap4 and releases; RPC rejection preserves unknown state
and requested mitigation until the real driver worker replays successful RPCs.
The failed-read regression passes with the new Image. Local advertised capabilities
are recorded fixture values chosen to cover saved limits, not physical modem
acceptance. QMI32 + BCL SoC26 + PMIC BCL44 =102 cases pass under Image#26 with no
WARN/panic/skip and normal guest powerdown. Targeted existing thermal suites are
run because a native shared governor was changed.

A read-only current6.12 sysfs snapshot sees25 same-name cooling devices among27
saved declarations. modem_vdd/modem_bcl_mtpl_dsc have no same-name entry in that
snapshot; do not infer permanent firmware absence. Selected map bounds all fit
observed maxima. CDSP max8 and several modem/BCL maxima255 exceed the smaller
selected fixture bounds; do not mistake test capability values for an actual
firmware list. Snapshot/current states are running-driver cached values. No
physical thermal settings, partitions, vendor files or phone reboot are changed.

Evidence: reports/qmi-thermal-stage95-validation.json; draft including new Image,
three production modules, private test module/DT and hashes:
artifacts/qmi-thermal-draft-stage95. This is not a flashable boot image or a promoted
main bundle. Stage57 and Stage91 physical DT/HAL candidates remain unchanged.
Reproduce core make Image modules in the pinned chroot, build_qmi_cooling.py
--tests, then QEMU qmi-cooling --cpus 4 (32), bcl-soc (26), bcl-pmic (44).

Pending: all remaining thermal providers/maps and changed-max rebinding corner
cases; two absent snapshot declarations; physical firmware mitigation, calibrated
sensors and ROM/HAL interactions; concurrent PM/hotplug/forced-unbind recovery;
remaining display/camera/audio/wireless/board drivers, full DT/SID4/boot/vendor_boot/
dtbo/DLKM/SELinux and daily stability. No functional completion percentage inferred.

## Stage96: preserve cooling bindings and recover configured limits

Two recorded regressions identify different problems. With the Stage95 driver,
changing LTE advertised max255 to3 unregisters/re-registers the cdev. Both existing
saved DT bindings are lost because explicit bounds8/255 exceed3; the new test fails
while32 previous cases pass. Keep the native cdev and use the real exported
thermal_cooling_device_update outside state/IO mutexes, even if the apply RPC fails.
Native update retains bindings and clamps them, but a second test finds that the
native core permanently overwrites configured bounds with3. After max255 returns,
bounds8/255 and effective mitigation remain3 (33 pass, recovery test fails).

Store the configured resolved lower/upper bounds in native thermal_instance at
initial bind. Recompute effective bounds from those originals for each maximum
change, retaining THERMAL_NO_LIMIT semantics via upper_no_limit. Clamp active
targets while preserving THERMAL_NO_TARGET; reset initialization/update cache when
limits change so a subsequent normal governor pass re-evaluates stable temperature.
The new private fields are appended to the structure. Native Image/modules and
all three affected external test suites are rebuilt. The core checkpoint replays
21 patches/68 exact files; build_native.py includes thermal-binding-limits-7.3.patch.

New Image#27 passes38 QMI/thermal cases and26 BCL SoC plus44 PMIC BCL cases,108 total,
without WARN/panic/skip. Tests retain two native LTE bindings through255->3->255,
restore exact configured8/255 limits, recover255 at stable temperature, handle
255->0->255 twice, and preserve bindings/native max during a rejected apply RPC
until a healthy retry confirms state. Existing32 cases remain covered. The driver
updates native capability bounds even when no current state can be confirmed;
get_cur_state continues returning EAGAIN rather than reporting fabricated success.

A fixture pinned to Stage95's read-only6.12 snapshot advertises25 recorded ceilings
through actual local QMI transport. The two unadvertised IDs get no cdev or valid
capability. An explicit later capability refresh with recorded responses registers
both and sends acknowledged level1. This is a source refresh test, not proof of
unsolicited firmware-list discovery. The25 ceilings are sysfs cached values rather
than a captured modem capability response; do not claim actual firmware behavior.
The two saved nodes have no disabled status and no kernel DT cooling-map references,
but their actual firmware/ROM/HAL consumers and absence reason remain unconfirmed.

Evidence: reports/qmi-thermal-stage96-validation.json; draft with Image#27, three
production modules, private test module/DT and hashes:
artifacts/qmi-thermal-draft-stage96. Source replay includes the revised QMI driver
and unchanged pinned generated wire protocol. audit_qmi_phone_fixture.py pins the
recorded ceiling fixture; existing selected-map/probe DT audits pass unchanged.
Phone remains6.12/Enforcing; no partition/vendor writes, phone DT edits or reboot.
Stage57 and Stage91 physical DT/HAL candidates remain unchanged/unpromoted.

Limits: an initial bind whose advertised maximum is already below an explicit DT
bound still follows native rejection; this change recovers previously valid existing
bindings. Expansion re-evaluates on the next normal governor pass, not a guarantee
of immediate hardware mitigation before that event. Additional governors and
concurrent capability/setter/peer replacement/PM/forced-unbind variants remain to
be tested, as do physical firmware and7.3 boot, remaining drivers/maps/ROM/HAL,
whole DT/schema/SID4/boot/vendor_boot/dtbo/DLKM/SELinux and stable daily operation.

## Stage97: DDR cooling device on native ICC/thermal

Port pinned factory drivers/thermal/qcom/ddr_cdev.c (037aa1c) as ports/qcom-ddr-cooling.
Factory semantics are kept: state0 votes 0, stateN votes freq-table[N-1]*bus-width as a
single peak request on the DT interconnects path. The 7.3 draft validates #cooling-cells,
nonzero width/frequencies, array shape and multiplication overflow, and rejects a NULL
optional ICC path instead of reporting no-op success. Callbacks are serialized; cur_state
is reported only after an acknowledged icc_set_bw, a failed vote returns the error, keeps
get_cur_state at EAGAIN and is retried by a delayed worker that a newer acknowledged target
cancels. devres order releases the ICC path only after the managed thermal cdev has been
unregistered and its callbacks drained; native icc_put sends the zero vote.

First QEMU run failed all14 cases: the private provider had no get_bw, so native ICC keeps
the conservative INT_MAX boot request and teardown never saw users/peak return to zero.
The fixture now records a zero initial hardware vote; driver checks were not weakened.

Image#27 (unchanged, sha 2555cbe4…) passes14 KUnit cases on4 CPUs with real OF/platform/
devres/ICC core/thermal registry: probe, vote3552 and release, idempotent repeat, bound
rejection, failed vote->EAGAIN->retry, pending cancel, pending remove,3x rebind, two-level
table, six malformed DT rejections and initial-vote failure with reattach. No WARN/panic.
23+51 import CRCs match. Source replay lock and patch: reports/ddr-cooling-source-lock.json,
patches/ddr-cooling-7.3.patch.

Saved6.12 DT audit (scripts/audit_ddr_cooling_dt.py): one active node, width4, table<888>,
provider qcom,alor-mc_virt MASTER_LLCC->SLAVE_EBI1, one passive map in zone ddr (100C,
hysteresis5C) selecting state1..1. The private fixture reuses those values and endpoint IDs.
ddr_cdev is in saved vendor.modules.load and loaded on the running6.12 phone.

Evidence: reports/ddr-cooling-stage97-validation.json; draft artifacts/ddr-cooling-draft-stage97
(Image#27 not duplicated). Limits: physical qnoc/RPMh/CRM DDR actuation and the firmware
unit interpretation of3552 are unverified; qnoc-alor was not co-tested with this consumer;
concurrent PM/supplier-unbind variants, whole-board boot, DT/vendor_boot/dtbo/DLKM/SELinux
and daily use remain. No partition writes, phone DT edits or reboot.

## Stage98: GPU devfreq cooling device on native OPP/PM-QoS

Port pinned factory drivers/thermal/qcom/qti_devfreq_cdev.c (037aa1c) as
ports/qti-devfreq-cdev. Factory ABI kept: a type-only cooling device "gpu" (no OF
node), kHz OPP table highest-first, and one DEV_PM_QOS_MAX_FREQUENCY request on the
KGSL device. Source review found factory defects: a missing qcom,devfreq phandle
dereferences NULL, an empty OPP table yields max_state -1, and the GPU device is used
after put_device. The draft rejects those, defers until the GPU device exists and its
driver is bound, holds the GPU reference for the binding, uses a managed device link,
serializes state and orders devres as cdev unregister -> QoS removal -> GPU put.
7.3 devm_thermal_of_cooling_device_register binds dev->of_node, so the non-OF devm
variant is used to keep the factory type-only ABI.

Image#27 (unchanged) passes9 KUnit cases on4 CPUs with real OF/platform/devres/
device-link/OPP/PM-QoS/thermal: the private GPU publishes the saved21-level user
table; max_state20; ceilings891000/72000/1238000 kHz; bound rejection; release
restores the default; coexistence with a KGSL-style request (native min); deferral
until the GPU binds; GPU unbind forces consumer unbind and QoS removal (3 cycles);
consumer rebind3x; missing phandle, empty table and phandle-without-device rejected.
22+43 import CRCs match.

Saved DT (scripts/audit_devfreq_cooling_dt.py): one active node targeting the active
KGSL node, one bin, 21 strictly descending levels equal to the fixture; the12 GPU-node
cooling maps bind KGSL's own OF cdev "kgsl", not this module. Read-only phone6.12
snapshot (reports/thermal-stage98-phone-cdevs.tsv): gpu20, kgsl20, ddr-cdev1.

Evidence: reports/devfreq-cooling-stage98-validation.json, artifacts/devfreq-cooling-
draft-stage98. Limits: real KGSL/GMU enforcement of the ceiling and thermal HAL use of
type gpu are unverified; concurrency stress beyond devres ordering, whole-board boot,
DT/vendor_boot/dtbo/DLKM/SELinux and daily use remain. No partition writes or reboot.

## Stage99: cpu-cluster cooling on native cpufreq/freq-QoS/cpuhp

Port pinned factory drivers/thermal/qcom/qti_cpufreq_cdev.c (037aa1c) as
ports/qti-cpufreq-cdev. Factory ABI kept: one type-only cdev per available child
(type = node name), first qcom,cpus phandle that is a possible CPU, every valid
cpufreq entry including boost, highest first, one FREQ_QOS_MAX request.
Review found factory defects: devm entries stay on a global list after remove
(use-after-free on rebind/hotplug), pending registration work is not cancelled,
concurrent online events can double-add the QoS request, the held policy reference
makes cpufreq driver unregister wait forever, and the table indexes raw entries
assuming no holes. The draft uses a per-device context, a refcounted multi-instance
cpuhp state, a lifecycle mutex separate from callback state, CREATE_POLICY
re-registration and synchronous REMOVE_POLICY teardown, and a sorted de-duplicated
valid-entry table. freq_qos_add_request errors are handled.

First QEMU run passed4/9: the test indexed clusters by array position but fdtoverlay
reverses child order; it now selects by type name. Driver unchanged by that fix.
Image#27 (unchanged) passes9 KUnit cases on4 CPUs with a private cpufreq driver
publishing the phone policy0 (24) and policy6 (21+4 boost) tables: max_state23/24,
ceilings2611200/384000 kHz, release restores the prior aggregate, registration after
late cpufreq, driver unregister/re-register3x, CPU2/3 offline/online, 40 duplicate
online events without WARN, rebind3x, unsorted/invalid/duplicate table, no-match
rejection. Import CRCs match.

Saved DT (scripts/audit_cpufreq_cooling_dt.py): cluster0=CPUs0-5, cluster1=CPUs6-7,
no kernel cooling-map references (userspace HAL by type). Read-only phone sysfs
(reports/cpufreq-stage99-phone-tables.txt): max_state23/24 equal the table sizes.
Evidence: reports/cpufreq-cooling-stage99-validation.json. Limits: SCMI enforcement on
hardware, HAL use, whole-board boot and daily use remain. No partition writes/reboot.

## Stage100: CPU hotplug cooling on native cpuhp/thermal_of

Port pinned factory drivers/thermal/qcom/cpu_hotplug.c (037aa1c) as ports/qti-cpu-hotplug.
Factory ABI kept: one OF cooling device per available child (DT cooling maps bind it),
type qcom,cdev-alias or cpu-hotplugN, max state1, mitigation offlines the CPU and the
online callback vetoes onlining while mitigated (now -EBUSY instead of NOTIFY_BAD).
Review found: INIT_LIST_HEAD on every probe, cdev unregister under the lock its
get_cur_state takes (deadlock), uncancelled reg/exec work, unreferenced child nodes.
The draft uses a per-device context on a global list, a refcounted single-instance
cpuhp state, lifecycle vs callback locks, converging exec work that restores only CPUs
it offlined, and teardown that cancels work, unregisters and restores CPUs.

A multi-instance cpuhp draft vetoed correctly but native rollback hit WARN
kernel/cpu.c:190 (stale last instance), so the single-instance factory shape was kept.
Image#27 (unchanged) passes8 KUnit cases on4 CPUs with real CPU hotplug: alias/default
names and OF node, bound rejection, isolation+veto+restore, unbind restores (3x), late
registration after online, 51-step toggles converge, user-offlined CPU left offline,
private thermal zone cooling-map isolates CPU2 at60C and restores at20C via step_wise,
invalid/duplicate/disabled children skipped. No WARN/panic. Import CRCs match.
Saved DT (scripts/audit_cpu_hotplug_dt.py): 8 children -> CPUs0-7, aliases cpu-hotplugN,
20 cooling-map references; phone cpu-hotplug0-7 max_state1. Evidence:
reports/cpu-hotplug-stage100-validation.json. Limits: physical isolation with vendor
hooks/HAL, suspend interaction, whole-board boot and daily use remain. No writes/reboot.

## Stage101: userspace cooling device (display-fps)

Port pinned factory qti_userspace_cdev.c (037aa1c) as ports/qti-userspace-cdev. The
factory put the device's own node (reference underflow) and kept unreferenced child
pointers; 7.3's devm OF helper binds dev->of_node, so each child is registered with
thermal_of_cooling_device_register and released by devres with its node reference.
Image#27 (unchanged) passes6 KUnit cases: OF identity and max levels16/3, level store
and bound, private zone mirroring the saved three maps where step_wise selects levels
1/2/3 then0, rebind3x, later-child error unwinding, no children rejected. Saved DT:
display-fps max16, maps1/2/3; phone display-fps max_state16. Source replay now uses the
generic scripts/audit_port_sources.py with ports/qti-userspace-cdev/source-spec.json.
Evidence: reports/userspace-cdev-stage101-validation.json. Limits: display HAL use,
boot and daily use remain. No writes/reboot.

## Stage102: GKI dynamic task_struct vendor area (WALT prerequisite)

The phone's sched_walt keeps per-task state through android_task_vendor_data(), backed in
android16-6.12 by CONFIG_GKI_DYNAMIC_TASK_STRUCT_SIZE: the bootloader passes
android_arch_task_struct_size=512 (read-only /proc/cmdline) and the task_struct slab grows
by that amount. android18-7.3 dropped the feature. Restored as in Motorola kernel-common
479a9e4ae: Kconfig.gki options, exported vendor_data_pad for init_task, early_param
setup before setup_arch, inline accessors in sched/task.h and zeroing in dup_task_struct.
android-7.3 commit 81604dc82, patches/dynamic-task-vendor-data-7.3.patch, config fragment.
arch_task_struct_size stays unexported, as in 6.12.

Image#28 boots three ways: =512 passes4/4 (size, init pad, 64 forks zeroed/isolated, slab
cover); no parameter and =4096 (above max, rejected) add no area (3 pass, fork case skipped
by design). Regression on Image#28: smoke, clock35, oplus1283 checks, rakkashin122, fence5,
ntsync13, ddr14, devfreq9, cpufreq9, cpu-hotplug8, userspace6, qmi38, bcl-soc26, bcl-pmic44.
One ntsync stress_wait TCG timeout did not reproduce (2 reruns on #28, 1 on #27: 13/13).
External port CRCs unchanged. Evidence: reports/core-stage102-validation.json,
artifacts/core-stage102. Next: five sched/binder hooks WALT needs, then WALT itself.

## Stage103: sched_walt vendor hooks and C binder (6.12 parity)

sched_walt registers 58 vendor hooks; 48 exist in android18-7.3. Restored the five it
needs on the phone (android-7.3 22a4a79de, patches/walt-core-hooks-7.3.patch,
scripts/apply_walt_core_hooks.py): check_preempt_wakeup_fair in wakeup_preempt_fair
(after idle/policy/sched_delayed filters, before slice/eligibility, as in 6.12),
replace_next_task_fair in pick_task_fair (7.3 folded pick_next_task_fair into it),
find_new_ilb at the top of find_new_ilb (-1 = none, honoured by kick_ilb), and binder
set/restore priority. preemptirq hooks (sched-walt-debug, not loaded on the phone) and
the sched_ext hook are omitted.

android18-7.3 gki_defconfig builds only Rust binder: it has binderfs but none of the
Android RT priority inheritance the 6.12 C driver provides. The platform fragment now
selects ANDROID_BINDER_IPC=y, ANDROID_BINDERFS=y and disables Rust binder.

Image#29: a probe module registered like a vendor module sees the preempt hook for a
real wakeup on a busy CPU (p/pse match), >10 picks with on-rq/cpu-consistent tasks,
and find_new_ilb from a nohz kick (3/3). binder-pi-test mounts binderfs and runs a real
transaction: a SCHED_FIFO 42 client to an INHERIT_RT context manager executes at FIFO 42
and returns to SCHED_OTHER after BC_REPLY; set/restore hooks fire (1/2). Regression:
15 suites pass; 132 port modules' CRCs match. Evidence: reports/core-stage103-validation.json.
Next: port sched_walt itself onto these interfaces.

## Stage104: sched_walt on android18-7.3

Ported the pinned Motorola/Qualcomm sched_walt (canoe: WALT=m, MIDPOINT, MOTO_UNFAIR;
DEBUG/STATS off) as ports/sched-walt (source-spec.json, patches/sched-walt-7.3.patch).
Core (android-7.3 d3ada4994, patches/walt-core-exports-7.3.patch): android_pick_task and
__balance_callbacks, cpu_busy_with_softirqs, __scx_enabled/ext_sched_class exports, vendor
and OEM data back in struct binder_transaction, and android_vh_mutex_add_waiter_pos — the
7.3 mutex queue is first_waiter + a headless ring, so the vendor only selects which waiter
to precede and native __mutex_add_waiter() inserts (the ww_mutex path).

WALT adaptations: syscore/data callbacks, __task_rq_unlock(p), dev_offline_disabled,
CONFIG_SYSCTL, void stop_one_cpu_nowait, explicit WQ_PERCPU, h_nr_running->h_nr_queued
(6.12 alias), binder need_reply == !is_async && !is_reply, update_rq_clock before
detach/attach (7.3 assertions are always on), MOTO timestamp read under the src lock. It
refuses to load when the task vendor area is smaller than walt_task_struct; a first QEMU
boot without android_arch_task_struct_size showed exactly that corruption (neighbour task
refcount underflow). sw_cycle_counter (default off) selects WALT's shipped software counter
because QEMU lacks the phone's qcom,gclk MMIO counters.

Image#30 QEMU, 2 asymmetric clusters, private cpufreq with the phone tables and an energy
model, phone cmdline android_arch_task_struct_size=512 sched_walt.midpoint_freq=2500000:
three consecutive runs pass 6 checks each — 87 /proc/sys/walt entries (phone 87), walt
governor on both policies, big cluster 384000->3436800 kHz under load and back down idle,
12 mixed tasks with 3x CPU3 hotplug and 3x schedutil<->walt round trips, no WALT_BUG/WARN.
Regression: 16 suites pass; 133 port modules' CRCs match.
Open risk: a fixture without CPUFREQ_HAVE_GOVERNOR_PER_POLICY made waltgov_init fail midway
and CFS later oopsed; the phone driver sets the flag but that error path needs an audit.
Not verified: gclk counters, SCMI, perf HAL/thermal_pause/moto_sched, perf/power parity.

## Stage105: proxy-execution donors and sched_walt load balancing

Stage104's open risk was real and not governor specific. With both policies on walt and
20 s of mixed load, ~40% of QEMU runs hit lockdep_assert_rq_held in update_se /
update_load_avg / cpuacct_charge, then put_prev_task_fair h_curr != prev and an Oops in
pick_next_entity. A failed waltgov_init alone did not reproduce (0/2); disabling WALT's
MVP replace hook did not help (1/4); instrumenting set_task_cpu found no running/queued
retarget (0 hits in 13 runs, 2 reproductions). The first warning trace was an idle CPU in
walt_newidle_balance -> walt_lb_pull_tasks -> walt_detach_task -> deactivate_task.

Root cause: android18-7.3 builds SCHED_PROXY_EXEC. Mutex-blocked tasks stay queued and can
be rq->donor while the owner runs on their scheduling context; native can_migrate_task()
rejects task_is_blocked(). WALT targets 6.12 (no proxy exec) and only skipped on-CPU tasks,
so it pulled donors and left the source CPU accounting an entity of another rq.
Fix: walt_proxy_pinned() (blocked_on.lock or rq->donor) in _walt_can_migrate_task and the
storage LB; the active-push path also skips blocked tasks (stopper already requires
TASK_RUNNING). blocked_on.lock is read directly because __sched_proxy_exec is unexported.
After: 12/12 sustained all-walt, 4/4 mixed, 3/3 standard checks+stress, zero WARN/Oops.
Evidence: reports/sched-walt-stage105-validation.json (before/after per-log counts).

## Stage106: thermal_pause on the sched_walt halt path

ports/qti-thermal-pause (source-spec.json). The first build lacked CONFIG_SCHED_WALT, so
linux/sched/walt.h compiled walt_pause_cpus() as a stub returning 0: pauses reported success
while ftrace showed no halt_cpus events and native balancing kept placing tasks on the CPU.
Fixed defines (CRC now imports walt_pause/resume). Factory overlap bug fixed: WALT keeps one
vote bit per client, so resume only CPUs no other paused cdev holds. WALT halt set
RQCF_UPDATED only under CONFIG_SCHED_DEBUG (gone in 7.3, checks always on): now always, and
the clock is updated after relocking. Lifetime fixes as in cpu_hotplug.
Six QEMU runs pass 8 checks each with zero warnings; WALT re-verified (2/2, 4/4 sustained).

## Stage107: moto_sched on android18-7.3

ports/moto-sched from motorola-kernel-modules a24357771 (firmware branch), canoe config
MUTEX/RWSEM inherit + MDPF. Core (eb6d0f515, patches/moto-core-hooks-7.3.patch): binder
transaction finish, PR_SET_VMA name bypass (now in set_anon_vma_name), percpu-rwsem hooks
and start-time wrapper, mutex wait/unlock, rwsem wake/wake_finish/reader steal, a rwsem
waiter-position hook for the 7.3 ring (new head takes over the handoff, as 6.12 did), and
the 6.12 cgroup_threadgroup_rwsem export. 6.12 invoked wait/wake hooks under wait_lock;
moto_sched takes the owner's rq lock there while 7.3 proxy execution nests wait_lock in rq
locks, so those calls now run just after wait_lock is dropped.
moto_sched fixes: resched_task() used task_current() and put_prev_task() on a non-donor
(proxy exec) -> CFS corruption/Oops on the first inheritance test; now task_current_donor().
Reordering moved to position hooks; WALT data via android_task_vendor_data().
WALT: walt_init re-reads the root domain after rebuilding sched domains; the stale pointer
caused an intermittent false WALT_BUG panic.
Image#33: moto_sched 6/6 runs x 7 KUnit (inherit/clear for mutex and rwsem, UX queue jump
for both, RT reader steal, percpu-rwsem, 8-thread storm) with lockdep, plus PR_SET_VMA and
binder PI, no WARN/BUG/circular-locking. sched_walt 3/3, thermal_pause 2/2, 16 regression
suites, 135 module CRCs pass. Evidence: reports/moto-sched-stage107-validation.json.

## Stage108: Qualcomm DMA-BUF heaps and mem_buf on android18-7.3

ports/qcom-dma-heaps from qcom-soc 037aa1c (50 files, source lock reports/qcom-dma-heaps-source-lock.json):
qcom_dma_heaps (system, secure system, CMA, carveout, dynamic page pools, DT parser), mem_buf,
mem_buf_msgq, mem_buf_dev, msm_dma_iommu_mapping, deferred-free-helper, mem-prot, gh_mem_notifier,
gh_rm_heap_manager. Core (6b1cbb295, patches/dma-heap-lcp-7.3.patch): dma_heap_ops.get_pool_size,
dma_heap_try_get_pool_size_kb() and /sys/kernel/dma_heap/total_pools_kb (read by libmeminfo; dropped
in 7.3), and qcom_scm_cfg_pddr_protected_region() (LCP DARE pool in the phone DT) on the SCM TZMEM
pool instead of 6.12 qtee_shmbridge buffers, polling GET_CMD_RESULT while TZ reports PROCESSING.
Port changes: string MODULE_IMPORT_NS + DMA_BUF_HEAP import, free_area_empty() -> list_empty(),
sched_setattr -> sched_setattr_nocheck (not exported on 7.3), relative includes/trace path.
QEMU (Image#34): AVR-shaped qcom,dma-heaps DT (display = reusable CMA, qseecom-ta = no-map carveout).
All six heaps appear (system, qcom,system, qcom,secure-pixel, qcom,secure-non-pixel, qcom,display,
qcom,qseecom-ta); a static userspace test allocates 4 KiB/1 MiB/8 MiB through /dev/dma_heap, mmaps,
writes and verifies under DMA_BUF_IOCTL_SYNC, runs 4 threads x 100 allocations, exhausts and recovers
the carveout, and sees total_pools_kb grow after frees (34824 -> 36872 kB). 3/3 runs, no WARN/BUG.
QEMU is not a Gunyah guest: gunyah.ko refuses to load (correct) and gh_rm_heap_manager needs
hypervisor heap resources, so a test-only stub (tests/dma-heaps/avr-gunyah-stub.c, CRC-identical
exports, never shipped) stands in for those symbols. Not verified: LCP SCM call, Gunyah heap
manager, secure heap assignment to TZ (phone only).
Regression: 15 base suites + task-vendor-data, sched-walt, thermal-pause, moto-sched, dma-heaps
pass; 144 port module CRCs; core patch replay 26 patches / 91 exact files.

## Stage109: Qualcomm TEE transport (si_core, qseecom_proxy, smcinvoke) on android18-7.3

canoe builds smcinvoke_dlkm from securemsm-kernel 43e5f439c in the si_core_xts variant
(CONFIG_QCOM_SI_CORE, +ADCI, +WQ; no MEM_FFA/DOORBELL), matching the phone's module deps
(smcinvoke_dlkm -> si_core_module -> qseecom_proxy, mem_buf_dev). ports/qcom-tee (qcom-soc
037aa1c: si_core + xts mem-object, qseecom_proxy) and ports/securemsm (whole repo, canoe Kbuild).
Core (qcom-scm.ko only; Image stays #34):
- patches/qtee-shmbridge-7.3.patch: <linux/qtee_shmbridge.h> on qcom_tzmem; cached pools
  (is_cached), bridge registry (re-register returns handle + ref; query -EEXIST),
  create_with_vmid with the 6.12 encodings (vmid 0: self-owner bit + RW; vmid: 1 NS VM),
  flush/inv; default 1 MiB..8 MiB cached pool created on first use.
- patches/scm-invoke-7.3.patch: qcom_scm_invoke_smc/_legacy/callback_response with 6.12
  signatures (int32 result, response type, data) on the no-busy-replay SMC path (si_core
  backs off itself).
- patches/tzmem-once-sleepable-7.3.patch: upstream 7.3 runs qcom_tzmem_init() under DO_ONCE
  (once_lock spinlock, IRQs off) while SHM-bridge mode issues sleeping SCM calls; lockdep
  "Invalid wait context" on first probe. Now DO_ONCE_SLEEPABLE.
Port fixes: si_core resets oic->u before RELEASE/RETAIN callbacks (update_msg re-applied the
previous callback's args and failed the release reply with -EINVAL whenever QTEE's message area
was not left untouched); trace_smci.h wait_for_pending_txn stored a cb_txn pointer and printed
through it (7.3 double-dereference WARN; use-after-free in trace output); string import NS.
QEMU method: tests/qcom-tee/prepare_fake_scm.py builds the REAL android-7.3 qcom-scm sources
with only the two SMC call sites redirected to tests/qcom-tee/avr_fake_tz.c (all 96 exports
CRC-identical to the shipped qcom-scm.ko). The fake TZ answers INFO, SHM bridge
enable/create/delete and a miniature QTEE in the real si_core wire format (open, echo,
callback into NS objects via INBOUND_REQ_NEEDED + CB_RSP, output objects, SCM busy x2,
registerWithCredentials releasing the credentials through a callback, RELEASE). Fixture DT
mirrors the phone nodes (qcom_scm, qcom_smcinvoke, qcom_mem_object).
Results (3/3 runs, no WARN/BUG/Invalid wait): KUnit avr_qcom_tee 4/4 (bridge registry and flag
encoding, cached pool growth to 8 MiB + phys/cache maintenance, si_core invoke/callback/
output object/busy/client env/release, 4-thread x 200 open+echo+release storm with all QTEE
objects released); userspace smcinvoke-test via /dev/smcinvoke (root ADCI refusal,
SERVER_REQ + ACCEPT_REQ callback server, NULL-credential refusal, registerWithCredentials,
open/refused open, echo, SIZE_OUT, QTEE->user callback "pong:ping", output object fd, busy,
4 threads x 100, si_core release counters 0/0).
Regression: 21 suites (incl. qcom-tee) + scm, secure, pas-driver, ice, iommu pass; 147 port
module CRCs; core replay 29 patches / 94 files. Not verified: real QTEE ABI, 6.12 multi-SMC
concurrency (not ported; standard SCM calls are serialised), securemsm tz_log/qrng/qce/hdcp/
smmu_proxy/tmecom (not built yet).

## Stage110: download mode, reboot reasons and PON log on android18-7.3

ports/qcom-reboot: qcom-dload-mode, qcom-reboot-reason, pmic-pon-log (MOT_PON_LOG; the phone has
/proc/driver/mot_pon_log) from qcom-soc 037aa1c and moto_reboot_reason from motorola-kernel-modules,
all unmodified (canoe_perf.bzl: POWER_RESET_QCOM_DOWNLOAD_MODE=m +_DEFAULT, *_REBOOT_REASON=m,
QTI_PMIC_PON_LOG=m). qcom_pon/reboot-mode/pwrkey were already ported (qcom-power-key).
Core (patches/scm-dload-7.3.patch, qcom-scm.ko only): exported 6.12 qcom_scm_set_download_mode(enum)
(whole-register write: EDL 0x1, FULL 0x10, MINI 0x20), qcom_scm_get_download_mode(),
qcom_scm_disable_sdi(); "qcom-dload-mode" child device when qcom,dload-mode is present and there is
no /soc/dload_mode node; 6.12 restart handler (priority 130: OEM warm reboot for REBOOT_WARM, rtc/twm
custom resets). Probe/shutdown follow 6.12: download mode only touched when qcom_scm.download_mode
is set, SDI disabled at probe only for qcom,sdi-enabled and always at shutdown (7.3 rewrote the
dump field at every probe/shutdown and disabled SDI at probe).
QEMU (fake-TZ qcom-scm + SCM IO TCSR register, RAM nvmem SDAMs with the phone's restart@48/49
cells and a preset PON FIFO, minidump_enabled stub with the real CRC): dload probe with
name=mini writes 0x20 to tcsr+0x19000; sysfs full/both -> 0x10/0x30, bogus rejected;
download_mode=0 -> 0x0 and one SDI disable; =1 -> 0x30; mot_pon_log shows the PON trigger;
reboot(RESTART2,"bootloader") -> restart handler sees SDAM 0x48=0x02 (qcom reason),
0x49=0x04 (moto bootloader flag), dload 0x0, SDI disabled during shutdown. 3/3 runs.
Regression: 22 suites + scm, secure, pas-driver, ice, iommu; 151 CRCs; core replay 30 patches.
Not verified: real TCSR/SDAM/PON hardware, EDL entry (vendor code keeps EDL disabled: "#if 0").

## Stage111: CPU DVFS transport (CPUCP mailbox, SCMI vendor protocol, scmi-cpufreq) on android18-7.3

Phone: scaling_driver=scmi; CPUs use clocks=<&pdp_scmi_perf 0|1>; pdp_scmi (perf 0x13, 0x80-byte
shmem) and cpucp_scmi (clock 0x14, qcom vendor 0x80, 0x200-byte shmem) over qcom,cpucp-v2 mailboxes.
ports/qcom-cpucp-scmi (qcom-soc 037aa1c): qcom_cpucp, qcom_scmi_vendor, qcom_scmi_client, cpucp_fast,
cpucp_log, cpu_phys_log_map, qcom-cpufreq-hw. clk-scmi: the vendor file is an older upstream copy
than GKI 6.12 -> 7.3 in-tree CONFIG_COMMON_CLK_SCMI=m (module name scmi-clk.ko; phone: clk_scmi).
Port fixes: vendor protocol version from ph->version (xops->version_get removed) with
supported_version=UINT_MAX (6.12 never negotiated); cpufreq-hw boost via .set_boost, no exported
scaling_*_freqs attrs, .remove.
Found with the fake platform and fixed:
- 7.3 SCMI mailbox max-msg-size default 104 (6.12: 128): qcom_scmi_vendor requests 128-byte
  replies -> -ERANGE on every set/get_param. Fix in DT (upstream mechanism):
  ports/device-tree/avr73-fixups.dtso adds arm,max-msg-size=<128> to /soc/qcom,cpucp-scmi only
  (PDP shmem is 0x80 and must stay at 104).
- 7.3 scmi-cpufreq no longer enables boost by default (GKI 6.12 set boost_enabled when turbo OPPs
  exist; phone boots with boost=1, gold max 3801600): restored (patches/scmi-boost-7.3.patch).
- 7.3 cpufreq_boost_set_sw() could never lower cpuinfo.max_freq after boost (table_cpuinfo keeps
  a larger previous max), so boost=0 had no effect: recompute from the table (same patch).
QEMU: tests/qcom-cpucp-scmi/avr-fake-scmi.c registers a mailbox controller and answers base,
perf v3 (phone tables, 30 us, uW power) and vendor 0x80 in the real shmem protocol. Results 3/3:
both SCMI instances probe ('Qualcomm:CPUCP', 'Qualcomm:PDP0'; max-msg 128/104), scmi-cpufreq
policies {0,1} and {2,3} with exactly the phone's scaling_available_frequencies, boost list
3532800..3801600, latency 30000, EM perf domains created; boost=1 at boot like the phone;
performance -> 3801600, boost 0 -> 3436800, boost 1 -> 3801600, scaling_max 2000000 -> 1996800,
policy0 max 1555200 -> LEVEL_SET 1555200; KUnit vendor set/get/start/stop roundtrip via
get_qcom_scmi_device(); qcom_cpucp, cpucp_log, cpu_phys_log_map, qcom-cpufreq-hw, scmi-clk load.
Regression on Image#36: 23 suites + scm, secure, pas-driver, ice, iommu; 158 CRCs; replay 31 patches.
Not verified: CPUCP MMIO mailbox/IRQ, cpucp_fast (needs WALT + CPUCP), perf notifications/
fastchannels support of the real PDP firmware.

## Stage112: sg, icc-clk, qcom_smd, msm_qmp; sdhci-msm/cqhci omitted

ports/qcom-firststage-misc builds modules whose 7.3 Kconfig symbols are not selectable in this
tree (INTERCONNECT_CLK is select-only, RPMSG_QCOM_SMD needs the in-tree QCOM_SMEM we replace by
the smem port, CHR_DEV_SG kept out of the core): sg, icc-clk, qcom_smd from android18-7.3 sources
plus the vendor deltas that matter (sg: scsi_autopm_get/put_device around open/release; icc-clk
and qcom_smd deltas are equivalent on 7.3), msm_qmp from qcom-soc unmodified.
QEMU (UFS host + QEMU UFS LU, as the ufs suite): sg attaches sg0-sg3 (W-LUNs + LU0), SG_IO
INQUIRY on the LU returns QEMU/QEMU HARDDISK, runtime_usage 2 -> 3 while /dev/sg3 is open -> 2
after close; icc-clk, qcom_smd, msm_qmp load. 3/3 runs.
sdhci-msm/cqhci are deliberately omitted (reports/module-set-decisions.json): the AVR's only
SDHC is status=disabled, and the 7.3 in-tree sdhci-msm imports the upstream qcom_ice ABI, which
conflicts with the vendor ICE module used by UFS inline encryption.
Regression: 25 suites incl. firststage-misc; 162 CRCs.

## Stage113: crash-dump table and diagnostic modules on android18-7.3

ports/qcom-debug-diag (qcom-soc 037aa1c): memory_dump_v2, qcom_va_minidump, qcom_logbuf_boot_log,
qcom_logbuf_vendor_hooks, kryo_arm64_edac (PANIC_ON_UE=y), qcom_iommu_debug. debug_symbol is not
built: 7.3 kernel/kallsyms.c already exports debug_symbol_available()/debug_symbol_lookup_name()
(core 3684f35f1; module would double-export) - recorded in reports/module-set-decisions.json.
Core (patches/scm-dumptable-7.3.patch): qcom_scm_assign_dump_table_region() = SIP UTIL 0x03 /
DUMP_TABLE_ASSIGN 0x13 (canoe has no CONFIG_QCOM_SCM_SMCI, so 6.12 used this plain call).
Port fix: nth_page() removed in 7.x -> page + i (va_minidump vmap).
QEMU (Image#38, fake TZ): phone-shaped mem_dump with a mapped, non-reusable dump_mem_region and
static entries (4 CPU contexts + rpmh-sized): the table handed to TZ (0x10b000 bytes) holds a
v2.0 top table with 1 entry whose APPS table is v2.0 with 5 entries; IMEM is left alone when TZ
accepts (driver behaviour). kryo EDAC (phone DT has no kryo-cpu-erp node), va_minidump, logbuf x2
and iommu debug load. 3/3 runs.
Regression: 26 suites + scm, secure, pas-driver, ice, iommu, ufs, debug-symbol, diagnostics; 168
CRCs; core replay 32 patches.

## Stage114: PMU library, msm_performance, CPU vendor hooks, PMUs, gh_vcpu_mgr

ports/qcom-perf-misc (qcom-soc 037aa1c): qcom-pmu-lib, qcom_llcc_pmu, arm_smmu_pmu, msm_performance,
qcom_cpu_vendor_hooks, gh_vcpu_mgr. Port fixes: 7.3 struct syscore/register_syscore;
QCOM_SCM_VMID_GH_RM fallback; pmu_lib reads AMU counters only when ID_AA64PFR0_EL1.AMU != 0
(6.12 relied on the Kconfig; QEMU's CPU has no AMU and AMEVCNTR trapped as undefined).
QEMU integrated suite qcom-perf-misc (Image#38, 2-cluster, android_arch_task_struct_size=512):
Stage111 SCMI DVFS fixture + the phone's qcom,pmu (long counters, events 0x08/0x11/0x17 + AMU
0x1000, no pmu-base), qcom,cpu-vendor-hooks, IMEM kaslr cell and capacity-dmips-mhz 1024/1208
(arch_topology capacity update is what schedules walt_init). Order as on the phone: scmi-cpufreq
policies first, sched-walt loaded later. Results 5/5: walt_init completes (87 /proc/sys/walt),
walt governor set on both SCMI policies; KUnit pmu_lib: events 0x08/0x11/0x17 supported on every
CPU, cycle counts advance (TCG does not count INST_RETIRED without icount); msm_performance
cpu_max_freq 2:1996800 -> policy2 scaling_max 1996800, cpu_min_freq 0:1555200 -> policy0
scaling_min 1555200, both released again; cpu-vendor-hooks and qcom-pmu bound; llcc/smmu PMU load.
Fixture-only: arm,max-rx-timeout-ms=<1000> on the QEMU SCMI nodes and a highpri worker in the fake
firmware (one run timed out at the default 30 ms while WALT's stop_machine stalled TCG).
Regression: 27 suites; 174 CRCs. Not verified: gh_vcpu_mgr (needs Gunyah), PMU hardware on phone.

## Stage115: CPU idle governor, sleep statistics and GICv3 suspend hook

ports/qcom-pm (qcom-soc 037aa1c): qcom_lpm (cpuidle governor qcom-cpu-lpm, as on the phone with
psci_idle), qcom_lpm_monitor, qcom_stats, sys_pm_vx, qcom_cpuss_sleep_stats(_v4), smp2p_sleepstate,
msm_show_resume_irq, boot_stats, msm_show_epoch, sysmon_subsystem_stats. Port fixes: hrtimer_setup()
(hrtimer_init removed), struct syscore, .remove.
Core (patches/gic-v3-suspend-7.3.patch, Image#39): 6.12 ACK android_vh_gic_v3_suspend called from a
GICv3 syscore suspend, gic_dist_init no longer __init, exported gic_v3_dist_init/cpu_init/
dist_wait_for_rwp. msm_show_resume_irq uses them to save and re-init the GIC around
PM_SUSPEND_MEM (mem_sleep=deep, offered by the phone next to the default s2idle) and hibernation.
QEMU (3/3): qcom-cpu-lpm becomes the current cpuidle governor on load; with all PM modules loaded
(boot_stats on an IMEM cell + MPM2 counter at the phone's 32768 Hz) s2idle suspend enters and
resumes on the pl031 RTC alarm (suspend_stats success 1, fail 0), no WARN/BUG.
Not verified in QEMU: PSCI idle states (with cpu-idle-states QEMU TCG stops booting), OSI cluster
and APSS domain states, mem_sleep=deep (QEMU offers only s2idle) and so the GIC restore path.
Regression: 28 suites + scm, secure, pas-driver, ice, iommu, ufs, debug-symbol, diagnostics; 185 CRCs;
core replay.

## Stage116: dwc3-msm, USB PHYs, EUD, redrivers

Phone bindings (running ROM): msm-dwc3 a600000.ssusb (+ core dwc3), msm_m31_eusb2_phy 88e3000.hsphy and
eusb2-repeater (both ported earlier in ports/usb-repeater), msm-usb-ssphy-qmp 88e8000.ssphy,
usb_phy_generic usb_nop_phy, msm-eud; redrivers/fsa4480/wcd_usbss have no I2C devices.
ports/qcom-usb (qcom-soc 037aa1c): dwc3-msm (debug_ipc + core + ops), phy-msm-ssusb-qmp,
phy-msm-snps-eusb2, phy-msm-snps-hs, phy-qcom-emu, phy-qcom-eusb2-repeater, eud, redriver,
nb7vpq904m, ps5169. 7.3 dwc3 core changes handled: no dep->regs (DEPCMD*/DEPCMDPAR*(n) on dwc->regs,
same absolute offsets as 6.12's dep->regs + DWC3_DEPCMD), DWC3_LLUCTL(n), HCS_MAX_PORTS is a mask.
dwc3-msm-ops hooks the core with kretprobes on 14 symbols: all present in 7.3 vmlinux with
6.12-identical signatures and identical dwc3 trace-event prototypes; KUnit avr_dwc3_msm_probes
registers/unregisters a kretprobe on each (3/3). All modules load in QEMU.
Not verified: any USB hardware path (gadget/adb, host, SS PHY, EUD) - phone only. CRC: 195 modules.

## Stage117: charging (PMIC glink clients, Motorola charger) and vendor GLINK signals

Phone bindings: qti_battery_charger, altmode-glink, ucsi_glink, glink_adc, pmic_glink_debug,
qti_battery_debug, glink_pkt (modem AT/DATA/IMS, ADSP apr_apps2), Motorola qti_glink_charger and
mmi,mmi-charger; soc:qcom,glink has no children.
ports/qcom-charging (qcom-soc 037aa1c, 9 modules) and ports/moto-charging (motorola-kernel-modules,
5 modules incl. hall_phone_case, MMI_PHONE_CASE_SUPPORT). 7.3 adaptations: power_supply_config fwnode,
cooling-device cdev_id, UCSI sync_control now returns CCI/MESSAGE_IN (wrapped around the 6.12
synchronous write), const bin_attribute, ida_alloc_max/ida_free, zap_special_vma_range,
alarm_start_timer(relative).
GLINK transport (ports/qcom-glink-transport, Codex's 7.3-based native GLINK): added the 6.12 vendor
client API glink_pkt needs (include/linux/rpmsg/qcom_glink_vendor.h): signals with 6.12 semantics
(NATIVE DSR/CTS/CD/RI <-> TIOCM, remote/local state, signals_cb), rx_done_supported() = false
(zero-copy is not negotiated, so glink_pkt uses its copying path). New KUnit case
vendor_signals_roundtrip: remote NATIVE DSR|CTS|RI -> TIOCM_DSR|CTS|RI + callback(old 0), set DTR|RTS
-> wire cmd 15 / lcid 3 / TIOCM|NATIVE_DTR|NATIVE_CTS, clear RTS keeps DTR. glink-transport 12/12.
QEMU: all 14 charging modules load (3/3); glink-transport, pmic-glink, remoteproc-services, rproc-gate,
pmic-ipc, smp2p pass. Deferred (module-set-decisions.json): glink_probe, qcom_glink_spss (SPSS stage).
Regression: 30 suites + 8 extra; 209 CRCs. Not verified: battery/charging with ADSP firmware.

## Stage118: GKI-equivalent phone modules (Image#40)

Phone-loaded modules whose AVR (qcom-soc) sources equal GKI 6.12 or differ trivially are built from
android18-7.3 in-tree drivers as modules (configs/avr-platform.fragment): tls, hwmon (vendor adds only
devm_hwmon_device_unregister), phy-generic (of_property_read_bool vs present), ledtrig-pattern,
i2c-qcom-geni and spi-geni-qcom (identical; unbound on the phone, vendor *-msm-geni bind). Omitted:
ext4_inode_test, lib_test (GKI tests). pwrseq_core is built in on 7.3. QEMU intree-misc: all 6 load,
phy-generic binds a usb-nop-xceiv fixture node.
Regression fixes: tests/qcom-tee/fake-scm (nested dir) must be rebuilt with every Image (stale CRCs
broke qcom-tee/reboot/debug-diag/perf-misc/pm until rebuilt). moto-sched test race: holding owners
are now confirmed to own the lock before waiters start (rare rwsem_ux_writer_jumps_queue failure,
seen in stage107 and once here); 6/6 after the fix; moto_sched itself unchanged.
Regression: 31 suites + 8 extra pass; 209 CRCs.

## Stage119: display dependencies (mm-drivers, mmrm), sps, remaining securemsm (Image#41)

ports/mm-drivers: msm_ext_display, sync_fence, msm_hfi_core (vendor mm-drivers, canoe defconfigs/bzl)
and msm-mmrm (mmrm-driver tag MMI-W3WWL36.65-11-18). ports/qcom-sps: sps_drv (canoe_perf CONFIG_SPS=m,
NDP_BAM=y). ports/securemsm adds tz_log, qce50, qcedev-mod, qcrypto-msm, hdcp_qseecom, qrng,
smmu_proxy (pvm), tmecom-intf.
7.3 adaptations: dma_fence_array_create() lost signal_on_any -> sync_fence sets num_pending=1 after
create (identical to the 6.12 init; 7.3 callbacks honour num_pending<=0); fence->lock ->
dma_fence_spinlock(); bind lock taken irqsave (lockdep: dma_fence_array signals from hardirq
irq_work; vendor plain spin_lock is a latent deadlock on 6.12 too); dma_fence_enable_sw_signaling ->
dma_fence_enable_signaling; from_timer -> timer_container_of; qcrypto workqueue explicit WQ_PERCPU;
rproc_set_state inline stub (canoe has no SOCCP_V1; phone msm_hfi_core imports nothing from
qcom_q6v5_pas). Core: scm-vendor-misc-7.3.patch restores 11 6.12 SCM calls found from the phone
modules' undefined symbols (tz_log QSEELOG x7, get_sec_dump_state, mem_protect_region_id,
mem_protect_sd_ctrl, smmu_notify_secure_lut); CONFIG_CRYPTO_DRBG=m selects CRYPTO_RNG for qrng
(7.3 GKI selects none). Function symbol sets of msm_hfi_core/sync_fence/msm_ext_display match the
phone modules apart from inlining.
QEMU mm-drivers (3/3): 14 modules load, stdrng registered, /dev/spec_sync userspace test:
GET_VER, num_fences range, ANY signals after the first of two sw_sync fences, ALL only after both,
flags=0 behaves as ANY, double bind refused, invalid fd bind error-signals. Negative control
(num_pending fix disabled): ANY/default fail (reports/mm-stage119-negative-control.log).
scripts/rebuild_all_ports.sh rebuilds every port/test incl. nested fake-scm. Regression: 32 suites +
11 extra pass; 222 CRCs. Not verified: TZ log, crypto engine, PRNG, HDCP, TME on the phone.
