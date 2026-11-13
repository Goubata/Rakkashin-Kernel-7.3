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
  compile into msm_kgsl.o. Final module linking still lacks 40 real dependency
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
