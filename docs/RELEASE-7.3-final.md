# Rakkashin Kernel 7.3 for avr — final build (2026-11-19)

Android Common Kernel **android-7.3** (Linux 7.3) for Motorola avr (XT2611, SM8845
canoe) on the OxygenOS 17 port ROM, with the Rakkashin Kernel features and
KernelSU-Next.

## 日本語

avr 向けの Linux 7.3 カーネルです（Rakkashin Kernel の機能と KernelSU-Next を搭載）。

実機で確認済み:
- 画面、タッチ、タッチサンプリングレートブースト
- リア・フロントカメラ、フラッシュライト
- 指紋認証
- Wi-Fi、Bluetooth（音声）
- 4G データ通信、VoLTE、SMS
- GPS、充電、センサー、NFC
- KernelSU-Next（su / マネージャ / モジュール）

Rakkashin の機能:
- **プロファイルとブースト**: avr_profile の各プロファイル、アプリ別プロファイル、需要追従キャップ、Encore FAS、スレッドブースト、起動ブースト
- **スケジューラ・I/O**: PELT 倍率、ADIOS I/O スケジューラ、Boeffla wakelock blocker
- **ネットワーク**: BBR、fq_codel、TCP の既定値
- **メモリ**: zram（LZ4）、vm の既定値（page-cluster 0、watermark 50/0、vfs_cache_pressure 50、vmstat 20 秒、writeback 15 秒）、LZ4 1.9 の高速デコード
- **省電力**: Sultan 系（freezer 1 秒、RCU の unbound GP、kernfs/SELinux のメモリ確保なし処理）、devfreq の高優先度 wq、binder ログ無効、dynamic fsync
- **ビルド**: `-mtune=oryon-1`、ntsync

## Files (GitHub release assets)

| file | partition | notes |
|---|---|---|
| `boot-73-rakkashin-final.img` | boot | Image + `sched_proxy_exec=0` |
| `vendor_boot-dual-full-v17.img` | vendor_boot | stock 6.12 ramdisk + `lib/modules/7.3.0-4k/` (boots both kernels) |
| `avr73-support.zip` | KernelSU module | needed on 7.3: netd BPF API level, late GPU/display/camera/fingerprint/ADIOS/ntsync modules, boot monitor; does nothing on 6.12 |

Try first without flashing: `fastboot boot boot-73-rakkashin-final.img`
(vendor_boot must already be the dual image). Keep your current boot.img to roll back.

## Sources

- `kernel/` — the full kernel source (android-7.3 base snapshot + all port commits)
- `ports/` — vendor/Motorola/Rakkashin driver ports built as modules
- `patches/` + `reports/core-patch-checkpoint.json` — every core change as a patch
- `scripts/` — build, boot-kit and vendor_boot assembly
- `tools/ksu-avr73-support/`, `tools/ksu-avr73-debug/` — runtime support / bring-up tools

## Known limits

- `sched_proxy_exec=0` is required (WALT vs 7.x proxy execution).
- The modem restart (SSR) path was not re-tested after the qrtr fix.
- Harmless 7.x warnings: iommu_set_fault_handler on DMA domains, msm_geni_serial IRQF_ONESHOT.
