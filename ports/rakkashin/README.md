# Rakkashin features on Android 7.3

Reference: Goubata/Rakkashin-Kernel,
7e0d0a6daa4959e047f650513a0a243d3e2565aa.
`reference/` preserves original source for comparison; it is not a second build
entry point. The replayable core change is patches/rakkashin-features-7.3.patch.
Use scripts/build_native.py --gpu-deps --qemu-tests --rakkashin.

Implemented core paths:

- Five avr_profile modes, per-UID profiles, top-app cpuset attach notifications,
  smart/demand/sustained controls, energy-model caps, frame learning and power
  controls. The original sysfs ABI is retained. Hardware frequency/temperature/
  power behavior is not verified by a guest without the phone's devices.
- Launch boost from successful root-to-app setresuid, delayed uclamp restoration
  and reset-on-fork behavior. Global UIDs are captured before commit_creds.
- Foreground heavy-thread headroom boosting, original module parameters and
  clamp restoration. The profile/cpuset connection is in the real core.
- PELT clock scaling and multiplier 1/2/4. Sysctl and kernel requests share one
  writer mutex; sysctl parsing uses local storage so unvalidated values are not
  published. The original veto hook is declared/exported in the 7.3 scheduler
  hook translation unit.
- Encore FAS ABI 3, real uprobes and hrtimer callbacks with the Rakkashin frame
  notifications. Uprobe API 3 is selected for 7.3.
- Original Boeffla wakeup-source controls, including default list and hard-wake
  semantics. RCU hot-path lookup remains; administrative list writes roll back
  on allocation failure and sysfs reads serialize with writers.
- Native 7.3 ntsync plus the early Rakkashin semaphore-create ioctl. Legacy copy
  out occurs before fd publication, so a failed copy does not leak a descriptor.
- Native BBR/fq_codel defaults, power-efficient workqueues, suspend-sync control
  and LZ4 zram selection through configs/rakkashin.fragment. Their presence is
  not a performance/power result; zram remains a rebuilt module at this stage.

ADIOS 3.3.0 is a separate real module under adios/. It uses the target queue-wide
shallow-depth API and core-owned elevator allocation. Slab caches are created
at module init, not under a queue's elevator lock; a guest test exposed the
original circular lock dependency. Per-queue mempools and scheduler behavior
remain. Three 4-worker direct-I/O read/write/compare rounds, depth changes,
scheduler changes and module unload passed with lockdep enabled.

Evidence lives in reports/ and VALIDATION.md. Failures are retained, including
ADIOS locking and the original optimized ntsync test. The ntsync driver was not
changed to make its stress test pass: unmodified -O0 passes, and a checked -O2
variant adds compiler fences around kernel-mutex critical sections and verifies
all wait/unlock errors. It preserves the non-atomic counter, four threads and
10,000 iterations each. All thirteen cases pass in both diagnostic/fixed runs.

Still pending:

- KernelSU-Next: the reference contains only a drivers/kernelsu symlink to an
  external ../../KernelSU-Next/kernel tree, without the source or its commit.
  It has not been ported; the version label does not claim KernelSU is present.
- KGSL final linking, secure firmware and SYNX providers, full GPU/CPU/thermal
  interaction and review of the original Boost cap/concurrency behavior.
- Android rk_gpud/control-module service installation and SELinux validation.
  Original helper source/scripts are retained; no script has run on the phone.
- Physical boot, app launch, frame pacing, all hardware, suspend/power and
  comparisons against 6.12. QEMU is not evidence that these passed.

After feature validation, continue the user's requested module/boot-image work:
use the actual boot/vendor_boot/vendor_dlkm/system_dlkm/DTBO data and live DT as
inputs, rebuild module dependency/order manifests, review vendor DTS against
7.3 bindings and exact avr hardware, and prepare file-based images and recovery
instructions. Source-missing modules need binary/ABI/firmware analysis and real
implementations. Preserve the port ROM's SELinux policy as the baseline and
identify changes from device labels and actual AVC denials. No partition writes
or physical reboot is authorized by this build workflow.
