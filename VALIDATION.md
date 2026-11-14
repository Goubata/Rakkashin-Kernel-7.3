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
