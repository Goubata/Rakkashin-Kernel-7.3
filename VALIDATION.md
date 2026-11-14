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
