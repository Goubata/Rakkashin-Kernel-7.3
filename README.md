# avr: Android 7.3 kernel port

Motorola avr（XT2611 / alor、ユーザー作成のOnePlusポートROM）用の移植作業です。端末内で7.3 Imageと付属モジュール、Qualcomm基盤・GPU支援・OnePlus連携およびRakkashin・Gunyah・GPUを含む71モジュールをビルドしました。**実機の7.3起動・全機能動作は未確認で、常用版ではありません。**

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
- ユーザーKGSL、元パッチ、調整アプリを ports/kgsl* に保持。共有メモリ供給・回収と温度制御APIを移植し、全Cソースからmsm_kgsl.oを生成。現在は実依存を移植してmsm_kgsl.koまでリンク済み。実機GPU動作は未確認。
- 新しいshmem供給経路はSRCUで待機と終了時の排出を扱い、memcg・page cache・LRUの管理を維持。4コアQEMUの35テストでzramへの退避とデータ読み戻しも通過。

最新成果物は artifacts/android73-gpu-stage9/（71モジュール、3,618インポートCRC一致）。前段階は artifacts/android73-ipc-stage8/（56モジュール）。前段階は artifacts/android73-rakkashin-stage7/（52モジュール）。前段階は artifacts/android73-sysstats-stage6/。前段階は artifacts/android73-smmu-memory-stage5/。以前のGPU支援段階は artifacts/android73-gpu-dependencies-stage4/。メモリ移植の段階は artifacts/android73-memory-stage3/。以前の基盤版は artifacts/android73-boot-core-stage2/。Image、36モジュール、設定、シンボル、SHA256SUMS を保存しています。端末対応boot.imgではありません。完了条件は VALIDATION.md を参照。

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

## SYNX / Gunyah / GPUの最終リンク

実Gunyah輸送・VM管理、vendor RM、SCMメモリ割当通知とPAS再試行を移植し、SYNX・IPCLite・hw_fence・msm_kgslの全モジュールがリンクしました。CoreSightとQFPROMもセットへ追加。hard/soft依存の存在と循環がないことを確認しています。実機の描画・共有メモリ・ファームウェア通信は未確認です。

Motorolaの外部DDK用AVF設定がcoreの.configとは別に必要でした。canoe_perf.bzlと実機の逆アセンブル・QTVM通知インポートを照合し、関連モジュールへ反映しています。誤ったラベル範囲でリンク成功だけを動作互換扱いにはしません。詳細は reports/gunyah-avf-abi-audit.json。

GPUブーストの上限再適用とGMUエラー伝播も修正しました。最終Imageで10件のSSR/Gunyah、13件のSCM/PAS、8件のGPUポリシー、35件のclock/RCU/memory、5件のフェンス、計71 KUnitが成功。Gunyah環境でないゲストが拒否されることも確認しました。reports/gpu-stage9-validation.json に対象Imageと証拠ハッシュを記録しています。

```sh
python scripts/build_native.py --gpu-deps --qemu-tests --rakkashin
python scripts/build_ipc_deps.py --gpu-driver --tests
python scripts/audit_gunyah_abi.py
```

## Rakkashin独自機能の移植

5段階のプロファイル、UID別・top-app連携、起動ブースト、前景スレッドのheadroom、PELT倍率、Encore FAS、wakelock制御を7.3本体へ組み込みました。ADIOSは7.3用モジュールとしてリンク。ntsyncは7.3標準実装に旧Rakkashinの作成ioctlを追加し、古い呼び出しと新しい待機機能を両立しています。BBR/fq_codel、電力効率workqueue、suspend同期、LZ4 zramの設定も移しました。

第7段階のImageで、Rakkashin実動作122チェック、ntsync 13セルフテスト、wakelock 5 KUnit、既存clock/RCU/共有メモリ35 KUnitが成功。ADIOSはRAM上のディスクで4並列のデータ照合を3回行い、深度変更・切替・アンロードまで成功しました。これらはAndroid実機の性能や全機能の確認ではありません。

KernelSU-Nextは元ツリーに外部ディレクトリへのシンボリックリンクしかなく、まだ移植していません。GPU/ファームウェア依存、Android側制御サービス、SELinux、実機での電力・温度・フレーム制御も残っています。詳細は ports/rakkashin/README.md。再ビルドは `python scripts/build_native.py --gpu-deps --qemu-tests --rakkashin`。

次に実機ダンプを基に、vendor_boot・DLKM・DTB/DTBOの配置と依存を整合させます。DTはavrの実構成とMotorolaの一致ソースを優先し、7.3 bindingとの差を検証します。別SoCのDTSをそのまま置き換えたり、旧6.12モジュールを名前だけ変えて使ったりする工程はありません。イメージはファイル上で作成し、実機書き込みや再起動は行っていません。

## DTB / DTBOとモジュール配置の棚卸し

保存した実機の静的DTB/DTBOをDTSへ戻し、7.3側DTCで元とバイト単位で一致する再コンパイルに成功しました。適用後5,102ノードも維持しています。これは編集可能な基準の復元であり、7.3実機対応の完了ではありません。公開メインDTSのみの試験ビルドは1,289ノード不足するため、不完全として記録し、通常実行では失敗させます。詳細は [DT作業記録](ports/device-tree/README.md)。

保存済み544種類のモジュールと7.3用71個をELF情報で比較したところ、同名候補57種類、同名候補のない旧モジュール487種類でした。内蔵化・名前変更・不要化もあり得るので、この数は必要な移植個数をそのまま示しません。71個の宣言済みhard/softモジュール依存はこのセット内で揃っていますが、ハード初期化・ROM側のロード順・残るドライバ・SELinuxの検証は別途必要です。`reports/module-deployment-readiness.json` に配置候補と不足を保存しています。boot/vendor_boot/DTBO/DLKMの起動用セットはまだ完成していません。

表示・音声componentを追加した159ファイルの試験合成では4,613ノードまで復元し、不足は489ノードになりました。合成順序とプロパティ差分は未検証です。カメラ用bindingヘッダが未解決で、GPU・映像・通信なども残ります。稼働中ROMのSELinuxはEnforcing/policy 33で、実際のGPU/ntsync/oplus_sync/synxラベルとpolicyハッシュを保存しました。policy変更はしていません。

## SSR / Gunyahと実機モジュール解析

7.3 nativeのremoteproc共通実装と、MotorolaのGunyah ARM64・doorbell・message queueを追加でリンクしました。通知の順序・分離・登録解除と、descriptor寿命・capability未配布時の失敗・仮想IRQ管理の9 KUnitが4コアQEMUで成功しました。実機輸送は未確認です。`python scripts/build_ipc_deps.py --tests` と `python scripts/run_qemu.py --test platform --cpus 4 --log reports/platform.log` で再実行します。

保存済みPAS/hw_fenceの逆アセンブルから、実機モジュールはV1のrproc_set_state exportを必要としないことを確認しました。V2の実queue/IPCC/応答待ち処理を維持し、非対応V1は明示的に失敗させます。旧モジュールの流用・署名偽装・CRC書換えはしていません。

hw_fenceの旧8依存は実装へリンク済みです。現在の課題は実機検証と未移植ドライバ、GPU制御の期限・寿命管理、Gunyahの終了／失敗時処理、起動用イメージとROMポリシーの整合です。常用版ではありません。
