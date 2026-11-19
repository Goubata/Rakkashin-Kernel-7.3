# Rakkashin Kernel 7.3 for avr

A Linux **7.3** kernel for the Motorola **avr** (XT2611, Snapdragon 8 Gen 5 / SM8845 "canoe", board ALOR) running the OxygenOS 17 port ROM. It is the successor of Rakkashin Kernel 6.12. It carries the Rakkashin features and has KernelSU-Next built in.

- Kernel base: Android Common Kernel **android18-7.3**, plus upstream stable updates (the tree is at **7.3.5**)
- KernelSU-Next **v3.4.2** built into the kernel
- The 6.12 vendor stack (Qualcomm, Motorola and OnePlus drivers) is ported to 7.3 and built as modules

[日本語はこちら](#日本語)

---

## Status

| Area | State |
|---|---|
| Display, touch, touch sample-rate boost | working (phone-verified) |
| Rear / front camera, flashlight | working |
| Fingerprint (jiiov) | working |
| Wi-Fi, Bluetooth (audio) | working |
| 4G data, VoLTE, SMS | working |
| GPS, charging, sensors, NFC | working |
| GPU (KGSL) | working |
| Hardware video codecs | exposed by the video driver (see below); playback not tested on 7.3 yet |
| KernelSU-Next (su / manager / modules) | working |
| **Boot-only install** (boot + vendor_boot, no helper module) | working on 7.3.0 |
| 7.3.5 stable update | built; not yet verified on the phone |
| Reboot from the power menu | **known issue**: can hang in the WLAN driver removal and need a forced restart (being fixed) |

Hardware video codecs (decided by the SoC SKU; formats read from the video driver on the phone): decode H.264 / HEVC / VP9 / AV1 / HEIC up to 8K30 10-bit; encode H.264 / HEVC / HEIC up to 4K60 10-bit. APV is not available on this SKU (Qualcomm's "IRIS4-1P no APV" configuration).

## Rakkashin features

- **Profiles and boosting**: avr_profile profiles, per-app profiles, demand-following caps, Encore FAS, thread boost, launch boost
- **Scheduler and I/O**: PELT multiplier, ADIOS I/O scheduler, Boeffla wakelock blocker
- **Network**: BBR, fq_codel, TCP defaults
- **Memory**: zram (LZ4), tuned vm defaults, fast LZ4 1.9 decode
- **Power**: Sultan-style changes (1 s freezer, unbound RCU GP, allocation-free kernfs/SELinux paths), high-priority devfreq workqueue, binder logging off, dynamic fsync
- **Build**: `-mtune=oryon-1`, ntsync (`/dev/ntsync`)

## Installing

The kernel needs two images: `boot` (the kernel) and `vendor_boot` (a **dual** image: the stock 6.12 ramdisk plus `lib/modules/<7.3 release>/`, so the same vendor_boot boots both 6.12 and 7.3).

1. Back up your current `boot_a` and `vendor_boot_a`.
2. Flash the dual vendor_boot, then the 7.3 boot image:
   ```
   fastboot flash vendor_boot_a vendor_boot-dual.img
   fastboot flash boot_a boot-73.img
   ```
   To try without flashing boot: `fastboot boot boot-73.img` (vendor_boot must already be the dual image).
3. Roll back by flashing your backups.

The current source needs no KernelSU helper module (see `docs/BOOT-ONLY.md`). The older `v7.3-final-20261119` release still needs `avr73-support.zip`; its release notes explain this.

## Source code

| Path | Contents |
|---|---|
| `kernel/` | The full kernel source tree: android18-7.3 + Linux 7.3.5 stable + every avr core change (SCM/TZ wait-queue protocol, SEA fixup, vendor hooks and exports, WALT, KernelSU-Next, boot-only support, …). Build it like any kernel tree. |
| `ports/` | 119 vendor driver ports built as out-of-tree modules: Qualcomm platform (clocks, interconnect, PMIC, Gunyah, IPC, thermal, …), GPU (KGSL), display (msm-drm), camera, EVA, video, audio/DSP, Wi-Fi (qcacld), data/IPA, NFC, Motorola and OnePlus drivers, Rakkashin modules (ADIOS, …). |
| `configs/` | Config fragments merged on top of `gki_defconfig`: `avr-platform`, `rakkashin`, `avr-pmic-glink`, `avr-production` |
| `patches/` | Core changes as standalone patches |
| `scripts/` | Build, apply-patch, audit and image-assembly scripts (`build_production.py`, `assemble_boot_kit.py`, `assemble_vendor_boot_dual.py`, `apply_*.py`, …) |
| `tools/` | KernelSU helper modules used during bring-up (`ksu-avr73-support`, `ksu-avr73-debug`) |
| `manifests/`, `reports/` | Pinned source revisions, module plans and validation reports |
| `docs/` | `BOOT-ONLY.md`, `RELEASE-7.3-final.md`, the phone test plan, and the full work log (`STAGE-LOG-ja.md`) |
| `VALIDATION.md` | Per-stage validation notes (QEMU and phone tests) |

Pinned inputs: Android Common android18-7.3, Motorola SoC/common kernel sources for this firmware, the Rakkashin 6.12 reference, and the OxygenOS port's sync modules. Revisions are listed in `manifests/` and `docs/STAGE-LOG-ja.md`.

### Building

The build is native arm64 with clang/LLD 21, run as root. The scripts were written for an Ubuntu chroot, and `AVR_KERNEL_SRC` points them at `kernel/`.

```
export AVR_KERNEL_SRC=$PWD/kernel BUILD_JOBS=8
python3 scripts/build_production.py --kernel      # Image + in-tree modules (out/prod-gki)
python3 scripts/build_production.py --ports       # all ports against that kernel
python3 scripts/assemble_boot_kit.py --tree prod --set full \
    --defer coresight_funnel,coresight_replicator \
    --extra anc_fps_mmi,adios,ntsync --tag prod
python3 scripts/check_boot_kit.py prod --tree prod
python3 scripts/assemble_vendor_boot_dual.py prod # needs your stock vendor_boot in device-baseline/images/
```
The boot image is `Image` with the cmdline `sched_proxy_exec=0` (required: WALT vs 7.x proxy execution).

## Known limits

- `sched_proxy_exec=0` must stay on the kernel command line.
- Reboot hang (above): under investigation.
- Harmless 7.x warnings: `iommu_set_fault_handler` on DMA domains, `msm_geni_serial` IRQF_ONESHOT.

## Credits

Rakkashin. The kernel sources are GPL-2.0, and so are the Android Common Kernel, Qualcomm, Motorola and OnePlus sources they are based on.

---

## 日本語

Motorola **avr**（XT2611 / Snapdragon 8 Gen 5・SM8845 / ALOR）向けの **Linux 7.3** カーネルです。対象は OxygenOS 17 ポート ROM で、Rakkashin Kernel 6.12 の後継にあたります。Rakkashin の機能をすべて載せていて、KernelSU-Next も内蔵しています。

- **ベース:** Android Common Kernel android18-7.3 に upstream の stable 更新を当てたもの（現在 **7.3.5**）。
- **KernelSU-Next:** v3.4.2 をカーネルに組み込んでいます。
- **ドライバ:** 6.12 のベンダードライバ（Qualcomm、Motorola、OnePlus）を 7.3 に移植して、モジュールとしてビルドしています。

### 動作状況
- **実機で確認済み:**
  - 画面、タッチ
  - カメラ（リア・フロント）、指紋
  - Wi-Fi、Bluetooth
  - 4G、VoLTE、SMS
  - GPS、充電、センサー、NFC
  - GPU
  - KernelSU-Next
- **boot と vendor_boot を書き込むだけで動く構成:** 7.3.0 で確認済みです（補助モジュールは不要）。
- **7.3.5:** ビルドは済んでいますが、実機ではまだ確認していません。
- **既知の問題:** 電源メニューから再起動すると、WLAN ドライバの解除処理で止まることがあります。修正中です。
- **ハードウェア動画コーデック:** 7.3 での再生テストはまだです。ドライバが出している対応形式は次のとおりです。
  - デコード: H.264 / HEVC / VP9 / AV1 / HEIC（最大 8K30 10bit）
  - エンコード: H.264 / HEVC / HEIC（最大 4K60 10bit）
  - APV: この SoC の SKU では使えません。

### 導入
1. 今の `boot_a` と `vendor_boot_a` をバックアップします。
2. デュアル vendor_boot と 7.3 の boot を書き込みます。デュアル vendor_boot は 6.12 と 7.3 のどちらでも起動できます。
3. 戻すときは、バックアップを書き込みます。

いまのソースコードでは、KernelSU の補助モジュールは要りません（`docs/BOOT-ONLY.md`）。古いリリース `v7.3-final-20261119` では `avr73-support.zip` が必要です。

### ソースコード
- **`kernel/`:** カーネルのソースツリー全体です。android18-7.3 と 7.3.5 stable の上に、avr 用のコア変更をすべて入れてあります。
- **`ports/`:** 119 個のベンダードライバ移植です（Qualcomm 基盤、GPU、画面、カメラ、動画、音声、Wi-Fi、データ通信、NFC、Motorola、OnePlus、Rakkashin のモジュール）。
- **`configs/`:** 設定のフラグメントです。
- **`patches/`:** コア変更をパッチの形にしたものです。
- **`scripts/`:** ビルドとイメージ作成のスクリプトです。
- **`tools/`:** 立ち上げ時に使った KernelSU モジュールです。
- **`docs/`:** 文書と作業記録です（`STAGE-LOG-ja.md`）。
- **`VALIDATION.md`:** 検証記録です。

ビルド手順は、英語版の「Building」のコマンドをそのまま使ってください。clang 21 で arm64 ネイティブにビルドします。
