# Rakkashin Kernel 7.3.6 for avr

Linux **7.3.6** (Android Common Kernel android18-7.3 + upstream 7.3.6 stable) for Motorola avr (XT2611, SM8845) on the OxygenOS 17 port ROM. It carries the Rakkashin features and has KernelSU-Next v3.4.2 built in. It is a boot-only install, so no helper module is needed.

## What's new since v7.3.5-20261119

- **Linux 7.3.6 stable update** (7.3.5 → 7.3.6, 2,659 upstream commits). Changes already in the Android Common Kernel were not applied twice. The sched_ext / scheduler conflicts were resolved commit by commit.
- **Power-off / reboot hang** (the screen froze, touch stopped and the phone never shut down):
  - Cause: during shutdown, the WLAN driver removal waited forever in cfg80211 `wiphy_unregister` for a leaked open count. That blocked vendor_init, init and the OPLUS shutdown animation in system_server.
  - `wiphy_unregister` now checks every 5 s, reports, and continues after 10 s when no wireless interface is left.
- **Kernel shutdown watchdog:** if the restart or power-off sequence is stuck for 20 s, the kernel logs the device it is stuck on and completes the request.
- Kernel command line: `qcom_dload_mode.download_mode=0`, so a crash restarts normally instead of stopping on the Qualcomm dump screen.

## Files

| File | Partition |
|---|---|
| `boot-736.img` | boot (Image + `sched_proxy_exec=0 qcom_dload_mode.download_mode=0`) |
| `vendor_boot-dual-736.img` | vendor_boot (stock 6.12 ramdisk and DTB + `lib/modules/7.3.6-4k/`, boots both 6.12 and 7.3.6) |

```
fastboot flash vendor_boot_a vendor_boot-dual-736.img
fastboot flash boot_a boot-736.img
```
Back up your current boot_a / vendor_boot_a first.

## 日本語

- Linux 7.3.6 に更新しました。
- **電源を切る・再起動するときに固まる問題を修正しました。**
  - 症状: 画面が止まり、タッチが効かなくなり、電源が落ちない。
  - 原因: 終了処理のとき、WLAN ドライバの解除が cfg80211 の中で永久に待ち続けていました。これで vendor_init、init、OPLUS の終了アニメーションまでが連鎖して止まっていました。
- **カーネル側の安全装置を追加しました。** 終了処理が 20 秒止まったら、止まった場所をログに残して、電源断または再起動を最後までやり切ります。
- クラッシュしたとき、Qualcomm のダンプ画面で止まらずに、普通に再起動するようにしました。
- 書き込む前に、今の boot_a と vendor_boot_a をバックアップしてください。
