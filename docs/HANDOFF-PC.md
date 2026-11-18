# Moving to the PC (Windows + WSL2): handover

This document is for continuing the AVR (moto XT2611, canoe/alor) android18-7.3 kernel port on the PC, with Claude Code running in WSL2.
Snapshot as of 2026-11-18. The latest details are in the newest Stage sections of `README.md` and `VALIDATION.md`, and in `git log`.

## The user's rules (always follow)
- Reply in **Japanese**. Keep progress updates honest and frequent, with % estimates.
- **Never make sound** (including in audio tests).
- **Never write to partitions (flash), reboot, or `fastboot boot` without asking first.**
  - So far `fastboot boot` (RAM boot, which writes nothing) has been run by the user from the PC.
  - Writing `vendor_boot_a` happens only after the user says "OK to write" each time.
  - On the PC, confirm up front how far the user wants to give blanket approval.
- Never call something done or fixed without verifying it on the phone. Keep moving autonomously ("つづけて").
- Commit identity: `git -c user.name=Codex -c user.email=codex@localhost commit` (add a Co-Authored-By line).

## Goal
Make the 7.3 kernel usable every day on the phone, at least on par with 6.12 (Rakkashin 6.12.113). That means everything works: boot, modem, display, GPU (the user's undervolt table), camera, audio, FBE, sensors, charging, thermal, suspend and KernelSU-Next. After that, tune performance, power and heat.

## Phone state (as of handover)
- Running kernel: Rakkashin 6.12.113 (built-in KernelSU-Next v3.4.2 and zram)
- Bootloader is actually **unlocked (orange)**; the props spoof green. Virtual A/B; slot a is active.
- **`vendor_boot_a` = dual v9** (stock 6.12 ramdisk + `lib/modules/7.3.0-4k/`). SHA256 `eb8a8c38…f1b4`
  - The stock image is `device-baseline/images/vendor_boot_a.img` (fc456214…ae9e), also on the user's PC as `STOCK-vendor_boot_a.img`.
  - Restore: `fastboot flash vendor_boot_a STOCK-vendor_boot_a.img`
- Test boot image: `artifacts/boot-kit-prod-full/boot-73-debug-v10.img`
  - prod kernel #3, `initcall_debug`. Also present on the phone as `/sdcard/Download/avr73/`.
- Diagnostic KSU module `/data/adb/modules/avr73-debug/`
  - No-op on 6.12. Only on 7.3 it does:
    - `setprop vendor.all.modules.ready 1`
    - loads synx, kgsl and msm_drm by hand
    - turns off OPLUS logd flow control (restored automatically on 6.12)
    - records logs to `/data/local/tmp/avr73/<boot>/`
  - Source is in `tools/ksu-avr73-debug/`.
  - The **netd1shot rescue (`rescue1shot`) is built and tested, but the user stopped the install, so it is NOT on the phone.**

## How far 7.3 has got (phone tests 1-18, `reports/phone-tests/`)
1. First stage done: 489 modules autoload in the full set.
2. Android second stage reached:
   - SELinux and FBE (fscrypt_init_user0) OK
   - apexd, vold, keystore, KSU OK
   - synx, kgsl and msm_drm load fine by hand
   - USB gadget up
3. **Current blocker:** the network daemon's self-test `netd1shot` fails, and init reboots with `reboot,netd1shot-fail` (about 15 s in). adbd never starts.
   - netd's error message is unknown because logd keeps no logs on 7.3 (cause unknown; see below).
   - Next step: rescue netd1shot with `rescue1shot` (rewrite the failing exit to exit 0, keep the boot alive, get adb), then debug live.
     - Alternatively, run netd under the tracer (`sctrace`, with `SCTRACE_SOCKETS`).

### Fixes so far (in the order the phone tests found them)
- fastboot boot ignores the boot.img ramdisk → dual vendor_boot (`scripts/assemble_vendor_boot_dual.py`)
- TZ deadlock → vendor SCM WAITQ (WAKE+ACK) + multi-SMC (`patches/scm-waitq-vendor-7.3.patch`, `scm-multicall-7.3.patch`)
- spmi-pmic-arb v8 drvdata
- qcom_edac (LLCC vendor struct)
- in-tree coresight funnel SEA → deferred
- synx: CONFIG_EXTENSIBLE_GLCOREDATA (as in canoe.bzl)
- vendor.all.modules.ready (when vendor_dlkm stays 6.12)
- SELinux firmware_load (`patches/selinux-load-compat-7.3.patch`, back to 6.12 semantics)
- KernelSU-Next v3.4.2 built in (`patches/kernelsu-next-7.3.patch`)

### Known open items
- netd1shot (above)
- logd: with 7.3 it receives messages (`logcat -S` Total>0) but keeps 0. Turning OPLUS flowctrl off doesn't help. AF_UNIX credentials are fine.
- Config parity: 7.3 has these but they are off. Enabling them changes struct layouts → rebuild all ports and rewrite vendor_boot.
  - `MEMCG_V1`, `GPIO_CDEV_V1`, `NET_CLS_CGROUP`, `CGROUP_NET_CLASSID`, `BLK_WBT`, `RCU_NOCB_CPU_DEFAULT_ALL`, `PROC_MEM_ALWAYS_FORCE`, `GACT_PROB` (see the end of test 15 in VALIDATION)
- synx: kit uses `--defer synx_driver` (loaded by hand on the phone); can go back to autoload now.
- coresight / stm vendor modules not ported (debug only).
- Kernel WARNs: spmi transaction failed, `__setup_irq`, `iommu_set_fault_handler` (msm_drm, kgsl).
- vendor_dlkm / system_dlkm are still 6.12; the final install needs a policy decision (replace partitions, or load everything from vendor_boot).

## Building on the PC (WSL2)
Scripts on the phone assume the chroot `~/.andronix/distros/ubuntu/rootfs` and `su -c chroot` (with binds `/work/avr-src` = android-7.3 and `/work/avr-port` = this repo).
On the PC, a native build is enough. Adapt as follows:
- Recommended: in WSL2 Ubuntu 24.04+, put everything under `~/avr/` with the same layout: `avr-kernel-7.3/` and `avr-upstream-audit/`.
- Packages:
  `sudo apt install -y build-essential bc bison flex libssl-dev libelf-dev dwarves python3 git rsync lz4 cpio device-tree-compiler qemu-system-arm clang lld llvm`
  - On the phone the toolchain was **clang 21.1.8**. Use the same major version where possible: `apt.llvm.org`, `llvm-21`.
- In `scripts/build_native.py`, `build_production.py`, `stage_tool.py`, `run_qemu.py` and the other `chroot`/`su -c` call sites, give a native mode via an environment variable (for example `AVR_NATIVE=1`, which means `/work/avr-src` → the real path).
  - One way: create the symlinks `/work/avr-src` and `/work/avr-port` with sudo. Then only the `chroot ROOTFS` prefix needs removing.
- After a rebuild, run the regression (`scripts/regress.sh TAG`) and the production build (`scripts/build_production.py --kernel` then `--ports`). Then go back to the kit:
  - `scripts/assemble_boot_kit.py --tree prod --set full --defer coresight_funnel,coresight_replicator --tag prod-full`
  - `scripts/check_boot_kit.py prod-full --tree prod`
  - `scripts/assemble_vendor_boot_dual.py prod-full`

## Testing from the PC (phone connected over USB)
- Calling Windows `adb.exe` / `fastboot.exe` (platform-tools) from WSL is the simplest.
  - Example: `alias adb=/mnt/c/platform-tools/adb.exe`
- Shell on 6.12: `adb shell su -c '...'` (KSU grants root; allow in the manager on first use).
- Test cycle (RAM boot, no writes):
  1. `adb reboot bootloader`
  2. `fastboot boot boot-73-debug-v10.img`
  3. Wait.
  4. (If it reboots itself or hangs) the phone comes back to 6.12.
  5. `adb wait-for-device`
  6. Collect `/data/local/tmp/avr73/latest/` and `/sys/fs/pstore/*`.
- A vendor_boot write (with permission):
  1. Check the image hash.
  2. `fastboot flash vendor_boot_a <img>`
  3. Check that 6.12 still boots.
- If it hangs: hold power + volume down to force a restart (6.12 comes back). pstore survives a warm reset.
