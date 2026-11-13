# avr: Android 7.3 kernel port

Motorola avr（XT2611 / alor、ユーザー作成のOnePlusポートROM）用の移植作業です。端末内で7.3 Imageと付属モジュール、Qualcomm基盤・GPU支援・OnePlus連携の50モジュールをビルドしました。**実機の7.3起動・全機能動作は未確認で、常用版ではありません。**

## 固定入力

| 入力 | コミット等 |
|---|---|
| Android common android18-7.3 | 4dd306a07b2f456fd70ddaa28f4f65640374f202 |
| Motorola SoC W3WWL36.65-11-18 | 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1 |
| Motorola common | 479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4 |
| Rakkashin参照 | 7e0d0a6daa4959e047f650513a0a243d3e2565aa |
| OxygenOSポートの連携モジュール | a7142e82df011d607010b439b9c1cfd7828c076c |
| Motorola graphics | 61a6d039282d94bd2ea2f908cef580e0d1006686 |
| ユーザーのKGSL ZIP SHA256 | 0b65d98f38556912f5fd486535f455b580287c178197fd07cc22b0448c656e1f |

公式manifestの個別SHAは manifests/android-7.3-pinned.xml、32件のファームウェア一致リポジトリは reports/vendor-source-refs.json を参照。vantageのカメラソースは候補であり、avrとの一致は未検証です。OnePlusの製品プロパティは物理端末識別に使いません。

## 現在の成果

- 7.3本体にCCFの起動時クロック保持、IRQ/SCMの必要な公開APIを移植。
- clock core / 21 controllers / RPMh / command-db / IRQ / Alor pinctrl と SCM/PAS/TZMEM をビルド・リンク。
- avr_oplus_sync の7.3化とフェンスが残っている間のモジュール寿命管理を実装。元ROMのioctl配置を維持。
- CCFの登録時に不要なMMIOを読まない修正と、保持解除を繰り返しても利用者のクロックを落とさない修正を追加。
- 元の7.3 Imageでも再現したRCU TasksのSMP起動停止を調査。proxy donorが待機中でも実行キューに残る場合を、タスク・runqueueのロック下で判定する修正を追加。
- RAMのみのQEMUで、proxy executionを有効にした2コア・4コアの29テストが通過。実行中のタスクをRCUが早く解放しないことも確認。
- avr_oplus_sync は修正後の4コアQEMUで1,283チェック通過。ROMサービス・SELinuxとの実機検証は別途必要。
- ユーザーKGSL、元パッチ、調整アプリを ports/kgsl* に保持。共有メモリ供給・回収と温度制御APIを移植し、全Cソースからmsm_kgsl.oを生成。最終.koは8個の依存シンボル不足で未完成。
- 新しいshmem供給経路はSRCUで待機と終了時の排出を扱い、memcg・page cache・LRUの管理を維持。4コアQEMUの35テストでzramへの退避とデータ読み戻しも通過。

最新成果物は artifacts/android73-sysstats-stage6/（50モジュール、1,980インポートCRC一致）。前段階は artifacts/android73-smmu-memory-stage5/。以前のGPU支援段階は artifacts/android73-gpu-dependencies-stage4/。メモリ移植の段階は artifacts/android73-memory-stage3/。以前の基盤版は artifacts/android73-boot-core-stage2/。Image、36モジュール、設定、シンボル、SHA256SUMS を保存しています。端末対応boot.imgではありません。完了条件は VALIDATION.md を参照。

## ネイティブビルド

Ubuntu 26.04 arm64 rootfs、Clang 21.1.8、Rust 1.93.1、bindgen 0.72.1、pahole 1.31を使用。公式Kleaf指定ツールチェーンとは異なり、GKI認証や公式バイナリとの同一性を示しません。

```sh
python scripts/build_native.py --boot-core
```

coreパッチの適用済み状態を確認し、依存順でモジュールをビルドします。KGSLは未完成のためこの成功経路には含めません。パッチの新旧が混在していれば検査で停止します。

隔離テスト用の構成・外部テストも作る場合:

```sh
python scripts/build_native.py --qemu-tests
python scripts/run_qemu.py --test clock --cpus 4 --log reports/clock-test.log
python scripts/run_qemu.py --test oplus --cpus 4 --log reports/oplus-test.log
```

QEMUの終了コードだけでは合格にせず、成功マーカー・KTAP・警告・panic・RCU停止を検査します。QEMUには端末のディスクやネットワークを接続しません。上記の統合ビルド用スクリプトは各段階で実行した手順をまとめたもので、既存出力を使う統合再ビルドは成功しました。クリーンな入力からの一括再実行はまだ未検証です。

GPU支援モジュールも作る場合:

```sh
python scripts/build_native.py --gpu-deps --qemu-tests
python scripts/run_qemu.py --test modules --cpus 4 --log reports/gpu-modules.log
python scripts/run_qemu.py --test iommu --cpus 4 --log reports/io-pgtable.log
```

SMEM/SoCinfo、hwspinlock、LLCC、DCVS、3種のgovernor、IOMMU支援モジュールを移植しました。SCMには実際のGPU/DCVSファームウェアAPIを11個追加。15モジュールのロードと9モジュールのアンロード（常駐依存6個は維持）、FAST/LPAEのページテーブル3テストをQEMUで確認しています。ハードウェアのSMMU・GPU周波数制御・保護メモリは未検証です。

## 次の作業

KGSLの残る依存処理（SYNX、シャットダウン経路）と、保護メモリ・SMCI/PILの実機ファームウェア経路、UFS/ICE/PHY、電源・interconnect、表示・通信等を移植・検証します。GPUの追加Boost処理には熱上限変更時の再検査や並行処理の見直しも必要です。性能・電力の優劣は実機測定するまで判定しません。

現在の許可範囲は端末内ビルド、読み取りダンプ、イメージファイル作成です。boot/vendor_boot/dtbo等のパーティション書き込み、実機への7.3モジュール読み込み、再起動は実施していません。実機起動操作が必要になった時点で、具体的な成果物と復旧手順を準備して確認します。

## ARM SMMU・保護メモリの追加移植

MotorolaのARM SMMUコントローラ、IOMMU logger、secure-bufferの3モジュールが7.3でビルド・リンクしました。ページサイズはデバイスごとに管理し、接続APIとnesting親の割当APIを7.3に合わせています。nesting子ドメインや実機SMMU動作は未検証です。

SCMの複数領域の権限変更は、単一のファームウェア命令を維持し、記述子を7.3のTZMEM経由で渡します。元コードのロールバック件数、対象領域数、ゼロ除算を修正。実コードでファームウェア呼出しだけを置き換えた5件のKUnitがSMP4 QEMUで成功し、19支援モジュールのロードと12個のアンロード（常駐7個は維持）も成功しました。これは実機のメモリ権限変更を実行した証明ではありません。

## GPU統計連携

msm_sysstatsも実装を移植しました。GPUコールバックの解除をSRCUで同期し、swap・vmalloc統計は実カウンタから取得します。4件のKUnitと、実際のNetlinkによるメモリ・プロセス情報取得および不正要求の拒否をQEMUで確認しました。要求属性の扱いで検出したゲスト内クラッシュは修正し、失敗ログも保持しています。20個のロード、13個のアンロード、常駐7個の維持が成功しました。実ROMと実GPUの統計連携は未確認です。

## SYNXの移植中

SYNX・IPCLite・Qualcommハードウェアフェンスの全対象Cソースがコンパイルを通過しました。最終リンクには、remoteprocの通知・SOCCP電源制御・Gunyahの8依存が残っています。これらは成功済み50モジュールには含めていません。ALL/ANYの待機と重複通知のエラー動作を保つ互換処理は5件のKUnitに成功しました。ports/qti-fence/README.md と reports/qti-fence-dependencies.json に残作業を記録しています。
