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
