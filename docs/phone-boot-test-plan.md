# 7.3 カーネル 実機起動テスト計画（書き込みなし RAM ブート）

この手順は **パーティションへの書き込みを一切しない** 方式です。実行前に必ずユーザーの許可を得ます。

## 方式

- `fastboot boot boot-ramtest.img` で 7.3 カーネルを RAM から一度だけ起動します。
- boot-ramtest.img = 7.3 Image（本番設定: lockdep なし、zram 内蔵）
  + ramdisk（実機 init_boot の generic ramdisk そのまま + `lib/modules/<7.3 release>/`）。
- AOSP の first_stage_init は、`/lib/modules/<uname -r>/` が存在するとそこだけからモジュールを読みます。
  そのため、フラッシュ済み vendor_boot の 6.12 モジュールは無視されます。
  DTB、cmdline、bootconfig はフラッシュ済み vendor_boot のものがそのまま使われます。
- vendor_dlkm / system_dlkm（6.12）は変更しません。vendor init がそれらを読もうとしても、
  7.3 カーネルが vermagic 不一致で拒否するだけです（エラーログが出るだけ）。
- 実機は orange（アンロック済み）を /proc/bootconfig で確認済みです（プロパティの green は偽装）。

## 2 段階

1. `boot-kit-first`：first stage の 125 モジュールだけを入れます
   （UFS、クロック、PMIC、SCM、pinctrl、Gunyah など）。
   - 期待：Android の起動処理が進み、adb が使えるところまで。
   - 画面、音、Wi-Fi はまだ出ません（それらは vendor_dlkm 側）。
2. `boot-kit-full`：実機で読み込まれている全モジュール（約 496）を first stage で読みます。
   - 期待：画面、タッチ、GPU、モデム、Wi-Fi、音（※音のテストはしない。音を出さない）。

## 失敗したときの戻し方（何も書き込んでいないので再起動だけで戻ります）

- モジュール読み込みが 1 つでも失敗すると、init は LOG(FATAL) を出してブートローダーに戻ります。
  - `fastboot reboot` で通常の 6.12 Rakkashin が起動します。
- 起動が止まった場合は、電源 + 音量下を長押しして強制再起動します。
  - 通常どおり 6.12 Rakkashin が起動します（RAM ブートは一度きり）。
- 6.12 に戻ったら、読み取り専用で次のログを回収します。
  - `/sys/fs/pstore/console-ramoops-0`（7.3 のカーネルログ。ramoops は DT 0xae000000、console 256KB）。
  - `pstore.compress=none` なので平文です。

## 実行手順（ユーザー許可後）

```
adb reboot bootloader
fastboot boot artifacts/boot-kit-<tag>/boot-ramtest.img
# 成功時: adb shell dmesg > reports/phone-73-dmesg-<tag>.txt
# 失敗時: fastboot reboot → 6.12 で /sys/fs/pstore/console-ramoops-0 を回収
```

## 事前検証（QEMU・静的）

- `scripts/check_boot_kit.py`：キット内の全モジュールについて、import の CRC、依存、vermagic を静的に照合します。
- `tests/boot-kit/qemu_android_init.py`：実機 init_boot の Android init を 7.3 上で QEMU 起動します。
  リリース別ディレクトリの選択、modules.dep の依存読み込み、first stage mount への到達を確認します。
