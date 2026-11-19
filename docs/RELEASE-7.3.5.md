# Rakkashin Kernel 7.3.5 for avr

Linux **7.3.5** (Android Common Kernel android18-7.3 + upstream 7.3.5 stable) for Motorola avr (XT2611, SM8845) on the OxygenOS 17 port ROM. It carries the Rakkashin features and has KernelSU-Next v3.4.2 built in.

## What's new since v7.3-final-20261119

- **Linux 7.3.5 stable update** (7.3.0 → 7.3.5). The f2fs fixes are already in the Android Common Kernel and were not applied twice.
- **Boot-only install: no KernelSU helper module needed any more.** Write boot + vendor_boot and you're done. `avr73-support.zip` is no longer used.
  - GPU, display, camera, EVA, fingerprint, ADIOS and ntsync load from vendor_boot.
  - The kernel handles the netd BPF API level, `vendor.all.modules.ready`, `/dev/ntsync` permissions and the OPLUS boot monitor.
- **Fingerprint:** only the jiiov driver is loaded. Loading goodix too broke the jiiov HAL.
- **Reboot hang, partly fixed:**
  - cfg80211 no longer waits forever in `wiphy_unregister` at shutdown.
  - The display now turns its CRTCs off before the SMMU shuts down.
  - The SMMU no longer runs the fault dump or BUG() during shutdown.

## Known issue

- **Reboot from the power menu still hangs**, so a forced restart (hold power + volume down) is needed. Normal use, sleep and charging are not affected. The cause is still being fixed.
- `sched_proxy_exec=0` must stay on the kernel command line.

## Files

| File | Partition |
|---|---|
| `boot-735.img` | boot (Image + `sched_proxy_exec=0`) |
| `vendor_boot-dual-735.img` | vendor_boot (stock 6.12 ramdisk + `lib/modules/7.3.5-4k/`, boots both 6.12 and 7.3.5) |

```
fastboot flash vendor_boot_a vendor_boot-dual-735.img
fastboot flash boot_a boot-735.img
```
Back up your current boot_a / vendor_boot_a first. If you used `avr73-support` before, disable it.

## 日本語

- Linux 7.3.5 に更新しました。
- **KernelSU の補助モジュールは要らなくなりました。** boot と vendor_boot を書き込むだけで動きます。
- 指紋認証を修正しました。
- 再起動で固まる問題は、一部だけ修正しました（WLAN の解除処理と、画面・SMMU の終了処理）。
- **既知の問題:** 電源メニューから再起動すると、まだ固まります。そのときは電源と音量下を長押しして強制再起動してください。普段の使用、スリープ、充電には影響ありません。
- 書き込む前に、今の boot_a と vendor_boot_a をバックアップしてください。avr73-support を使っていた人は無効にしてください。
