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
- KGSL user ZIP staged with custom interfaces retained; all C sources now
  compile into msm_kgsl.o. Final module linking still lacks 11 real dependency
  symbols. The shmem and thermal API migrations compile. Six memory cases
  (35 total with clock/RCU) passed on SMP4, including guest-zram swap and data
  read-back. This does not prove GPU DMA or protected-content behavior.
  Dependency migration and boost cap/concurrency review are outstanding. See
  ports/kgsl/PORTING.md; do not install this unfinished source build.
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
