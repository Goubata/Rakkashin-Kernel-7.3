# avr: Android 7.3 kernel port

Motorola avr（XT2611 / alor、ユーザー作成のOnePlusポートROM）用の移植作業です。端末内で7.3 Imageと付属モジュール、Qualcomm基盤・GPU支援・OnePlus連携およびRakkashin・Gunyah・GPU・UFSホスト・電源系・interconnect・PMIC・GPIOを含む91モジュールをビルドしました。**実機の7.3起動・全機能動作は未確認で、常用版ではありません。**

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

最新成果物は artifacts/android73-remoteproc-services-stage37/（108モジュール、5,480インポートCRC一致）。Stage35は artifacts/android73-state-ipc-stage35/（105モジュール、5,330インポートCRC一致）。Stage32は artifacts/android73-glink-transport-stage32/（103モジュール、5,216インポートCRC一致）。Stage30は artifacts/android73-rproc-gate-stage30/（100モジュール、5,054インポートCRC一致）。Stage29は artifacts/android73-pmic-glink-stage29/（100モジュール、5,054インポートCRC一致）。Stage25は artifacts/android73-pmic-ipc-stage25/（99モジュール、4,975インポートCRC一致）。Stage24は artifacts/android73-adc-lifetime-stage24/（94モジュール、CRCは reports/module-crc-stage24.json）。Stage23は artifacts/android73-thermal-alarm-stage23/（94モジュール、4,733インポートCRC一致）。Stage22は artifacts/android73-adc-thermal-stage22/（94モジュール、4,733インポートCRC一致）。Stage21は artifacts/android73-pmic-gpio-stage21/（91モジュール、4,649インポートCRC一致）。Stage20は artifacts/android73-regulator-lifetime-stage20/（89モジュール、4,567インポートCRC一致）。Stage19は artifacts/android73-spmi-stage19/（89モジュール、4,567インポートCRC一致）。Stage18は artifacts/android73-interconnect-stage18/（85モジュール、4,405インポートCRC一致）。Stage17は artifacts/android73-regulators-stage17/（80モジュール、4,285インポートCRC一致）。Stage16は artifacts/android73-ufs-host-stage16/（77モジュール、4,168インポートCRC一致）。Stage15は artifacts/android73-ufs-core-stage15/（73モジュール、3,727インポートCRC一致）。Stage14は artifacts/android73-ice-stage14/（73モジュール、3,727インポート）。Stage13は artifacts/android73-storage-stage13/（73モジュール、3,700インポート）。Stage12は artifacts/android73-gpu-stage12/（71モジュール、3,624インポート）。Stage11は artifacts/android73-gpu-stage11/（71モジュール、3,623インポート）。Stage10は artifacts/android73-gpu-stage10/（71モジュール、3,622インポート）。前段階は artifacts/android73-gpu-stage9/（71モジュール、3,618インポート）。前段階は artifacts/android73-ipc-stage8/（56モジュール）。前段階は artifacts/android73-rakkashin-stage7/（52モジュール）。前段階は artifacts/android73-sysstats-stage6/。前段階は artifacts/android73-smmu-memory-stage5/。以前のGPU支援段階は artifacts/android73-gpu-dependencies-stage4/。メモリ移植の段階は artifacts/android73-memory-stage3/。以前の基盤版は artifacts/android73-boot-core-stage2/。Image、36モジュール、設定、シンボル、SHA256SUMS を保存しています。端末対応boot.imgではありません。完了条件は VALIDATION.md を参照。

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

## 通知登録の寿命管理

Gunyah vendor RMのSCM/hyp・RM/QTVM通知に、登録失敗時の巻き戻しと終了時の解除・work排出を追加しました。OFノードの二重putと借用参照の扱いも修正。共通helperの8 KUnitがSMP4 QEMUで成功し、実行中コールバックが終わるまで解除が待つことも確認しました。Stage10の71モジュールは3,622インポートCRCが一致しています。実ハイパーバイザの終了、共有メモリ回収、サスペンドを検証した証明ではありません。reports/gpu-stage10-validation.json と ports/gunyah-rm/README.md を参照。

## PM通知と拡張DTの継続検証

共通Gunyah RMのPM通知は、後続初期化失敗・終了時に登録を解除するよう修正しました。実際のPM通知チェーンへの登録・解除を含む9件の共通helper試験がSMP4 QEMUで成功。実サスペンド、IRQや共有資源の全寿命は未確認です。`reports/gpu-stage11-validation.json` に対象と証拠を記録しています。

拡張component DTはGPU・フェンス・映像・IPCLite・BT・WLAN・AVRのNXP NFCを追加し、公開ソース189ファイルから5,063ノードを合成しました。不足39ノードのほか、GPU表・予約メモリ・未解釈の参照値等のプロパティ差が残ります。ノード数は動作率ではありません。カメラbindingは明示したVantage参照で、ドライバ互換性は未確認です。

ユーザーGPU設定は保存実機DT（静的画像と稼働中DTで一致）の21段階を復元しました。`--saved-gpu-table` の別出力ではGPU表の全プロパティ一致を確認し、全体5,069ノード、不足33ノードです。低電圧化やクロックの新しい値は生成していません。7.3でのGPU実動作・温度・安定性は未検証です。

Stage12では共通RMのscoped OFノード二重putとIRQ親参照の漏れを修正し、IPC/GPU全体を再ビルド・照合しました。新しい実機/QEMUハイパーバイザ試験を実施したという意味ではありません。`reports/gpu-stage12-validation.json` を参照。DTの不足33ノードと予約メモリの不一致は `reports/dt-critical-gaps.json` に切り分け、起動用イメージへの使用を保留しています。

## DSP/CVP・保存メモリのDT合成とUFS PHY

193ファイルの明示的なDSP/EVA参照プロファイルに、ダンプのGPU表・表示CMA pool・DMA heap・固定予約領域を適用し、静的基準と同じ5,102ノード（不足・追加0）になりました。DSP/CVPの31ノードは既知形式の参照先と引数を含めて一致。GPU表は引き続き全プロパティ完全一致です。表示poolは200 MiB、4 MiB alignmentの動的割当を保持し、bootloaderが決めるアドレスを固定していません。

同じ数値のphandleが異なるノードを指す場合を見逃さないよう比較も修正しました。範囲・参照・メモリの20検査が成功。全体にはまだ240ノードの未解釈の参照やプロパティ差が残り、OEM合成順序・binding・ドライバ実動作は未検証です。ノード数一致を完成扱いにはしません。`reports/dt-dsp-eva-reference-comparison.json` と `reports/dt-avr-dump-aligned-build.json` を参照。

UFS PHYはfirmware一致のMotorolaソース5ファイルを変更せず、`phy-qcom-ufs.ko` と `phy-qcom-ufs-qmp-v4-canoe.ko` を7.3でリンクしました。実機DTのcompatibleとモジュールaliasが対応し、SMP4 QEMUでロード・存在確認・逆順アンロードまで成功。実機のPHY training、UFS接続、読み書き、暗号化解除は未確認です。UFSホストとICE/wrapped-key対応も未完成で、起動用イメージではありません。`reports/storage-stage13-validation.json` を参照。

## ICE V2とSHM Bridge

Motorolaの既存ICEモジュールは、鍵登録0x05（6引数）・消去0x06（2引数）のV2 SCMを使用します。7.3標準のlegacy 0x04/0x03へ名前だけ対応させません。実ICEコードを移植し、7.3のTZMEMで共有・消去・解放します。wrappedキー入口のraw要求を成功扱いにする経路も修正し、HWKM未初期化・slot overflow・BIST失敗を検出します。実ファームウェアの鍵操作は未検証です。

一致ソースのcanoe設定に合わせSHM Bridgeを選択し、SCM/TZMEMを実体のある同じモジュールにまとめて依存循環を解消しました。旧standalone TZMEMの残存.koはKbuildのmodules.orderを基に除外します。ICE追加とTZMEM統合で合計は73個、3,727インポートCRC一致です。新ImageのSMP4 QEMUでICE 10、SCM/PAS 13、secure-buffer 5件が成功。これらは実キー登録・データ復号・実機SHM登録を証明しません。UFSホストは引き続き未完成です。`reports/ice-stage14-validation.json` を参照。

固定7.3基盤から11パッチ・54ファイルの再適用結果が現在のnativeコミットと完全一致することも確認しました。既にレビュー済みの重なったパッチを誤って再適用する問題を修正し、ソースの正確なコミット・クリーン状態・全パッチのハッシュを照合しています。

## Stage15: UFSコアの実I/O試験とホスト移植

7.3標準PCI UFSドライバをQEMUの実UFSエミュレータへ接続し、単一キューとMCQ両方で、異なるパターンを2回ずつ書き込み・読み戻しました。各構成で書き込み8 MiB・読み込み8 MiBが一致し、QEMUのprivate backing file先頭4 MiBも最終パターンと照合しました。新規の通常ファイルのみを仮想ディスクへ渡し、携帯のブロックデバイスは渡していません。各構成の3 KUnitでは、コントローラ状態・実IRQ記録・実キューモードも確認しています。

Motorolaホストは7.3のHID/device descriptor、電力交渉、scsi_cmnd通知、CCFフラグ、MSI/thermal/removal、暗号化profileへ接続し、Cコンパイルを通過しました。最終リンクは本物のIPC logging/minidumpの4 symbols不足で失敗しており、成果物へは含めません。空の成功関数には置き換えていません。`reports/ufs-stage15-validation.json` と `ports/ufs-host/README.md` を参照。

この結果はQualcomm PHY training、実機UFS/ICE、暗号化解除を証明しません。新Imageの保存先はStage15です。実機起動・常用・電力/性能比較は未実施で、全体目標は未完了です。

## Stage16: UFSホストのリンクと診断依存

Motorola固定ソースからSMEM minidumpとIPC loggingを移植し、実際の診断シンボル検索・DMA buffer iterator・CMA・folio・EEVDFインターフェースに接続しました。以前不足していた4 exportsを実コードで供給し、`ufs-qcom.ko` と `ufshcd-crypto-qti.ko` の最終リンクを通過しました。Stage16には77モジュールを保存し、4,168インポートCRCが一致します。

初期化前minidumpの重複名・登録取消・上限・SMEMへの引渡し競合を修正しました。SMEM表のELFヘッダ用1枠を予約し、IPCでは16枠の競合管理と登録解除を追加しました。解除を拒否された場合はダンプ対象メモリを解放せずエラーを記録します（その保持メモリの再回収は未実装）。UFSのtrace callbackはhost単位で登録し、失敗時の巻き戻しと解除完了待ちを実装しました。

新Imageの4コアQEMUで、実シンボル検索5項目、初期化前登録・IPCログの往復/再生成8項目が通過し、Qualcommホスト/暗号モジュールのロード・アンロードも確認しました。実UFSエミュレータのSQ/MCQ各3項目と各8 MiB書込み/8 MiB読戻しも通過しました。Qualcommの実機probe、SMEMバックエンド実稼働、ICE、暗号化解除、電源遷移の証明ではありません。

```sh
python scripts/build_storage_deps.py --ufs-host --diagnostics-tests
python scripts/run_qemu.py --test debug-symbol --cpus 4 --log reports/debug-symbol.log
python scripts/run_qemu.py --test storage-host --cpus 4 --log reports/storage-host.log
python scripts/snapshot_boot_core.py --name android73-ufs-host-stage16 --gpu-driver --rakkashin --ufs-host
```

全体の実機完了条件は未達です。パーティションは変更していません。`reports/ufs-stage16-validation.json` に範囲と未確認事項を記録します。

## Stage17: RPMhレギュレータと診断ファイルの寿命管理

固定MotorolaソースからRPMh、debug-regulator、元から存在する論理stub-regulatorを移植しました。実電源向けのRPMh投票処理は維持し、論理ドライバを物理レギュレータの代わりには使いません。保存DT・候補DT双方の72ノードでcompatibleが一致します（probe成功を意味しません）。保存済みdebug-regulatorの実ELF exportsから、任意のsuspend-debug拡張が含まれないことも確認しました。

OFノード参照の保持・解放、proxy登録失敗の伝播、管理された子デバイス生成、IPCログの失敗時巻き戻し/終了順を修正しました。診断ファイルは自分が生成した分だけ取り除き、アクセス終了後に内部データを解放します。電圧操作の独自file operationsにはdebugfsの保護を適用し、診断consumerの生成も排他しました。

QEMU専用の論理レール（実機DTへは入れない）で、参照数・電圧範囲・列挙・モード・負荷要求の5 KUnitが通過しました。開いた電圧ファイルからデバイス解除後に読み取ると停止する不具合を再現し、修正後は読取り拒否・再接続・module ref保持・解除後のアンロードまで通過しました。RPMhの実応答、実電圧、スリープ復帰は未検証です。

```sh
python scripts/build_storage_deps.py --regulators-tests
python scripts/run_qemu.py --test regulators --cpus 4 --log reports/regulators.log
python scripts/snapshot_boot_core.py --name android73-regulators-stage17 --gpu-driver --rakkashin --regulators
```

Stage17は80モジュール・4,285インポートCRC一致です。新しい本体変更はなくImageはStage16と同じです。常用版や実機起動可能なboot.imgではありません。詳細は `reports/regulators-stage17-validation.json`。

## Stage18: Alor interconnectとUSB/IPA参照の候補修正

BCM voter・RPMh interconnect・帯域診断・QoS・Alorの5モジュールを固定Motorolaソースからリンクしました。Motorola commonの5状態のbinding headerを先に読み込み、標準の3状態のheaderとの混在を防ぎます。クロック用倍率は64bitのローカル値へ適用し、frameworkの集計値を繰り返し変更しません。rate/commit、BCM初期化、リンク/診断登録のエラーを伝播し、管理されたclockの二重解放と子デバイスの終了順も修正しました。

4コアQEMUで実集計・5状態・性能タグ・初期値・リセット・倍率の非破壊性・commitエラー・native xlateの9項目が通過しました。依存3モジュールの終了/再登録とtrace制御も確認しました。元のAlor/BCM voterにはmodule_exitがないため常駐を維持し、アンロード可能とは主張しません。実NoC/RPMh/CRMやQoS MMIOは未検証です。

静的照合では231ノードのリンク先に欠落がなく、BCM最大所属数は2（容量3以内）でした。保存DTにはUSB usb-ipaがcnoc_cfgの空スロット523を指す不整合があり、保存済みqnoc-alor.koのELF relocationでも空であることを確認しました。同じ保存モジュールのcnoc_main slot523はqhs_ipaへ解決します。この根拠に基づき、IDを保ったままその参照先providerだけを変更する候補DTを別ファイルへ生成しました。実機のUSB/IPA修正成功を意味しません。

候補DTの14 providers / 426 interconnect参照は全て表内に解決します。元の保存DTの不整合は監査結果に残しています。変更はUSBの1プロパティだけで、保存GPU21段階と安定メモリ設定は完全維持されています。全DT binding・他ハードウェア・画像パッケージ・実機常用確認は未完了です。

```sh
python scripts/build_storage_deps.py --interconnect-tests
python scripts/run_qemu.py --test interconnect --cpus 4 --log reports/interconnect.log
python scripts/build_interconnect_dt.py
python scripts/audit_interconnect_dt.py
python scripts/snapshot_boot_core.py --name android73-interconnect-stage18 --gpu-driver --rakkashin --interconnect
```

候補DTは `artifacts/dt-interconnect-stage18/avr-interconnect-candidate.dtb`。
Stage18の本体ImageはStage16と同一です。`reports/interconnect-stage18-validation.json` に範囲を記録しています。実機7.3の起動と全機能動作は未確認で、boot.imgや常用版はまだありません。

## Stage19: Canoe/V8 SPMI、PMIC、診断の接続

固定MotorolaソースからSPMI arbiter、V8対応診断コントローラ、PMIC MFD、QTI regmap診断の4モジュールを移植しました。7.3のfwnode IRQ domainへ対応し、debugfs→子デバイス/コントローラ→IRQ→OF参照の終了順を管理します。初期化済みのarbiterだけを公開し、mapping APIと解除を排他し、device/node参照を釣り合わせました。SPMI名にはcoreのグローバルな番号を使い、ハードウェア内bus番号の重複で衝突しないようにしました。

PMIC子デバイスが終了するまでrevision metadataを保持します。QTIの独自data操作はdebugfsの保護を使い、自分のファイルだけを除去して参照を解放します。元の手動解除APIが誤ったdevres識別子で実際には解除できない問題も、RAM上の試験で再現して修正しました。保存済みモジュールに合わせて診断dataの書込みは無効のままです。

4コアQEMUで5項目（RAM regmap往復/範囲、mapping引数、native SPMI引渡し、長さ/エラー伝播）が通過しました。実QTI診断からRAM値を読み、解除後の保持fd読取りを拒否し、core所有ファイルを残したまま全4モジュールを解放する試験も通過しました。記録用のSPMI test doubleとRAM backendは試験モジュールだけに存在し、成果物へは含めません。実V8トランザクション・割り込み・PMIC probeの証明ではありません。

DT検査で、同じbus/SID4を持つPMICノード2つの順序が、静的な再構成DTと保存running DTで逆になっていると分かりました。実機6.12のsysfsが選択しているノードも読み取り確認し、17個のSPMI子サブツリーを保存running順へ並べ替えた候補DTを作りました。予約マップ・ヘッダ・文字列・全ノード/プロパティ値は変えず、GPU21段階と保存メモリも維持しています。bootloaderの動的ノード/値はコピーしていません。

**重複SID定義自体は残っており、厳密なDT検証は未通過です。** 候補は現在のrunning DTと同じ先行選択を維持しますが、どちらかを推測で無効化していません。候補の23 PMICノード・最大SID13・28 IRQ参照は静的記述の検査であり、実機で23個がprobeしたという意味ではありません。

```sh
python scripts/build_storage_deps.py --spmi-tests
python scripts/run_qemu.py --test spmi --cpus 4 --log reports/spmi.log
python scripts/preserve_spmi_dt_order.py
python scripts/audit_spmi_dt.py  # 現存する重複SIDのためexit 1、未解決事項として記録
python scripts/snapshot_boot_core.py --name android73-spmi-stage19 --gpu-driver --rakkashin --spmi
```

候補DT: `artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb`。
本体ImageはStage16と同じです。常用・実機起動・充電/温度/IRQ・ROM全体の確認は未完了で、パーティションへの書込みはしていません。`reports/spmi-stage19-validation.json` に検証範囲を記録します。

## Stage20: レギュレータの手動解除を実際に検証

実コードのproxy consumerとregulator debugにも、登録時と異なるdevres識別子を使う手動解除の不具合がありました。QEMU論理レールで修正前に「解除後もenable要求が残る」「debugファイルが残る」の2件を再現し、正しい登録キーでdevresを取り外して既存のロック下で解放するように修正しました。再帰ロックは導入しません。

電源要求の登録→解除を2回繰り返し、診断の解除→再登録も確認しました。既存5項目と追加2項目の全7項目、保持fd・unbind/rebind・参照保護・unloadが通過しています。モジュール数は89のままで、実機電源や故障時の復旧を証明する試験ではありません。`reports/regulator-lifetime-stage20-validation.json`。

## Stage21: PMIC GPIO/MPPの接続

固定Motorola GPIO/MPPソースを7.3のPIN_CONFIG_LEVEL、戻り値を持つGPIO set、remove callbackへ接続しました。ピン数と物理ピン番号/引数数を検査し、親regmapが無い間はprobeを遅延し、can_sleepは親regmapから取得します。GPIOの方向切替は固定7.3 native実装と完全一致させ、CMOS出力は入力bufferを無効化し、open-drain/sourceでは実ラインを読み取れるようにします。入力の読取りでは要求中の出力ラッチ値を書き換えません。

実GPIO callbackソースを登録処理無効の試験TUへ直接取り込み、RAM regmapとgpiolib上で7項目を確認しました。ピン境界、set/get、読書きのエラー伝播、CMOS方向切替、open-drainのRAM入力サンプリング/要求値保持、無効な電源入力番号の拒否が通過しました。本番モジュール2つは別途ロード/アンロードしています。実PMICのprobeや電圧・波形・IRQの検証ではありません。試験用コード/RAM backendは本番成果物へ含めません。

静的な範囲検査では、候補DTの9 GPIO provider、3 phandle参照、7 pin名が表内でした。選択DTにMPP providerは0で、MPPは追加の移植済み参考ドライバです。MPP callback/probe試験は未実施です。全DTの重複SID4、実GPIOの有効穴、pad subtype、部分書込み後の復旧、並行アクセス、実IRQは未解決/未検証です。

```sh
python scripts/build_storage_deps.py --regulators-tests --pmic-gpio-tests
python scripts/run_qemu.py --test regulators --cpus 4 --log reports/regulators.log
python scripts/run_qemu.py --test pmic-gpio --cpus 4 --log reports/pmic-gpio.log
python scripts/audit_pmic_pinctrl_dt.py
python scripts/snapshot_boot_core.py --name android73-pmic-gpio-stage21 --gpu-driver --rakkashin --pmic-pinctrl
```

本体ImageとStage19候補DTの値は変更していません。Stage21は91モジュール・4,649インポートCRC一致ですが、実機7.3起動・常用版の完成は未達です。`reports/pmic-pinctrl-stage21-validation.json`。

## Stage22: ADCとPMIC温度監視の接続

実ソースのADC共通換算・GEN3/GEN4 ADC・温度警報の3モジュールを追加。
換算テーブルは固定したMotorolaソースの値を保持。ADC割り込み前のwork初期化、
失敗時の解除順序、クライアント登録のNULL参照、7.3のworkqueue指定を修正した。
独立したSMP4ゲストで12項目の実コードテストと実モジュールのロード・解除が通過。
PM7の100kΩは固定テーブル上25.6°Cで、電池用の25.0°C換算とは別に検証した。
保存DT・候補DTとも35チャンネル（GEN3:20、GEN4:15）、温度警報12ノード、
修飾されたIIO参照6件を確認。これは実機の測定・過熱保護の証明ではない。

```sh
python scripts/build_storage_deps.py --adc-thermal-tests
python scripts/run_qemu.py --test adc-thermal --cpus 4 --log reports/adc-thermal-qemu-stage22-final.log
python scripts/audit_adc_thermal_dt.py
python scripts/snapshot_boot_core.py --name android73-adc-thermal-stage22 --gpu-driver --rakkashin --adc-thermal
```

94モジュール・4,733 CRC一致。参照の寿命、温度警報のtrip/復帰処理、
実機PMIC・温度保護・起動・常用検証は未完。`reports/adc-thermal-stage22-validation.json`。

## Stage23: 過熱警報の復帰・書き込み失敗を検証

実ドライバのRAM regmapとnative thermal zoneを使う8項目を追加し、計20項目が通過。
修正前は、書き込み失敗後のしきい値キャッシュ、DACとLITEの繰り返し設定、
警報有効化失敗後のinitializedフラグの4項目で失敗を再現した。
trip番号を毎回ゼロから列挙し、キャッシュは書き込み成功後に更新する。
復帰時は初期化を先に済ませ、IRQはDTのトリガ設定を引き継いで重複登録を防ぐ。
未知のGEN2リビジョンとレジスタ範囲外は拒否する。
94モジュールの実機過熱保護、サスペンド、常用確認は引き続き未達。
`reports/thermal-alarm-stage23-validation.json`。

## Stage24: ADC利用側と通知の寿命

提供側のdevice lock下でprobe時にmanaged device linkを作り、解除時は利用側が先に
停止する。busy/unboundの提供側はdeferする。呼出側の要求構造体は保持せずコピーし、
通知の選択はmutex内、コールバック呼出しはmutex外で行う。
外部からのdisableは実行中コールバックの終了を待ち、自身のコールバック内からの
disableは自己待ちせず残りの通知とcontextをクリアする。
実driver coreの解除順序、所有コピー、自身のdisable、別スレッドのdisable待ちを
含む計24項目がSMP4で通過。実機の利用側ドライバが契約を守るかは今後確認する。
inactive clientレコードの提供側解除までの保持、ハードウェア部分更新の回復も未完。
`reports/adc-lifetime-stage24-validation.json`。

## Stage25: 実QRTR/QMI/PDRと入力長の検証

固定したandroid18-7.3基点は維持し、外部PDR利用を選択できるKconfig変更と
QMI入力長の修正を追加。15パッチ・60ファイルが基点から完全に再現する。
QRTR、QRTR-SMD、QMI、PDRメッセージ、PDR helperのnative 5モジュールを追加した。
修正前に途中で切れたTLVの成功扱いを再現し、frame/TLV/要素/配列長/文字列長の
読取り範囲を修正。正常wireの既知値と往復、短いframe/TLV、handle終了を6項目で検証。
実AF_QIPCRTRローカルソケットで128回の双方向送受信を検証した。
新ImageでADC24項目とclock/RCU35項目も通過。99モジュール・4,975 CRC一致。

```sh
python scripts/build_native.py --boot-core --gpu-deps --rakkashin --qemu-tests --pmic-glink
python scripts/build_storage_deps.py --adc-thermal-tests --pmic-ipc-tests
python scripts/run_qemu.py --test pmic-ipc --cpus 4 --log reports/pmic-ipc-stage25-bounds-qemu.log
python scripts/snapshot_boot_core.py --name android73-pmic-ipc-stage25 --gpu-driver --rakkashin --pmic-ipc
```

Motorola qti_pmic_glinkの草稿はリンクしたが、RX/client解除・provider寿命・
後始末に未修正の競合があるため、この成果物には含めない。
リモートサービス、実充電、電池表示、物理7.3起動と常用判定は未完。
`reports/pmic-ipc-stage25-validation.json`。

## Stage26: GLINK受信・クライアント解除

実MotorolaソースのクライアントにSRCUとcallback mutexを追加。IDR検索中に保護を
取得し、公開を外した後にRX/state callbackの終了を待って解放する。
callback呼出しはIDR mutexの外で行い、自己解除は-EDEADLKとして別workerへの
解除依頼を必要とする。TXとunregisterは呼出側で直列化する契約を明示した。
実ソースをdriver初期化なしでコンパイルするsoftware fixtureで6項目が通過。
QMI6項目・QRTR128packetと合わせて警告なし。実provider/rpmsgのprobeや解除は
このfixtureに含まない。実vendorモジュールのインポートCRCも一致するが、
提供側の寿命と後始末はまだ未完のため、成果物はStage25の99モジュールのまま。

```sh
python scripts/build_storage_deps.py --pmic-glink-tests
python scripts/run_qemu.py --test pmic-glink --cpus 4 --log reports/pmic-glink-stage26-final-qemu.log
```

`reports/pmic-glink-stage26-validation.json`。実充電、物理7.3起動、常用版完成は未達。

## Stage27–29: GLINKの受信・提供側・クライアント寿命

RPMSG coreはdriver.removeの後でdefault endpointを破棄するため、受信入口にSRCUを
置き、関連付けの公開停止→入口待ち→init/RX work待ちの順に解除する。
提供側はdevice参照を保持してbinding/channelを検証し、managed linkで利用側を先に
解除。clientはprobe中だけ登録し、devresの自動解除と明示解除を同じ処理へ接続した。
別clientをcallbackから同期解除する場合も-EDEADLKにして循環待ちを防ぐ。

実ソース13項目・native QMI6項目・実QRTR送受信128回と、実モジュールの
登録/解除が警告なしで通過。100モジュール・5,054 CRC一致の実験用成果物に追加。
保存DTと候補DTはSOCCPの2チャネル・有効な子サービス8ノードを持つ。
`qcom,alor-soccp-pas`はnative側に対応表がなく、実SoCCP/PAS early-boot handoverと
SMEM通信、Motorola/QTI充電・USB子ドライバの接続が引き続き必要。

```sh
python scripts/build_storage_deps.py --pmic-glink-tests
python scripts/run_qemu.py --test pmic-glink --cpus 4 --log reports/pmic-glink-stage29-final-qemu.log
python scripts/audit_pmic_glink_dt.py
python scripts/snapshot_boot_core.py --name android73-pmic-glink-stage29 --gpu-driver --rakkashin --pmic-glink
```

実機の7.3起動、実充電、常用の完成は未達。`reports/pmic-glink-stage29-validation.json`。

## Stage30: 選択された通信サブデバイスには実際のedgeを要求

native helperではGLINK/SMDが無効な場合のNULLを開始成功として扱える。
全機能を接続する本プロジェクトでは、有効な通信サブデバイスのedgeがなければ
-EOPNOTSUPPを返す方針とした。修正前の2項目失敗、修正後の2項目通過を確認。
実GLINK/QMIの19項目・QRTR128回・実GLINKモジュール登録/解除も再通過した。
これは通信の実装完了を示すものではなく、欠落した通信を成功に見せないための処理。
100モジュール・5,054 CRC一致。`reports/rproc-gate-stage30-validation.json`。

```sh
python scripts/build_storage_deps.py --rproc-gate-tests --pmic-glink-tests
python scripts/run_qemu.py --test rproc-gate --cpus 4 --log reports/rproc-gate-stage30-after.log
python scripts/snapshot_boot_core.py --name android73-rproc-gate-stage30 --gpu-driver --rakkashin --pmic-glink
```

`reports/soccp-next-source-review.json`に次のSoCCP移植条件を記録した。
7.3には他SoC向けのearly_boot/attach実装があり、PAS ID・firmware名は一部一致する。
AVRのalor対応表、cx/mx電源・割り込み・SMEM/handover条件の照合と実装はまだ必要。
他SoCのcompatibleを名目上追加する変更は行っていない。物理起動・常用は未達。

## Stage31–32: 実GLINK SMEM輸送をremoteproc補助層へ接続

固定nativeソースからGLINK core/SSR・SMEM輸送・RPMSG controlの3モジュールを追加。
remoteproc commonが実register/unregister/SSRの3関数へリンクすることをELFで確認。
リング位置・サイズ・読書き範囲を検査し、READ_NOTIFの予約領域とメモリバリアを保持。
送信領域待ちのRX応答workerでremoveが止まるケースを再現し、TXのabort/wakeを
RXキャンセルより先に行う修正を入れた。実ソースのRAM試験ではVERSION/ACKも確認。

通信層11項目、remoteprocエラー2項目、PMIC/QMI19項目、SSR/Gunyah10項目が通過。
103モジュール・5,216 CRC一致。保存DTと候補DTのGLINK edge 5件は、phandle番号を
参照先へ解決して一致。SMEM item478/479/480と共通wire command15件の番号も一致。
Vendor zero-copyはまだ未実装で、未実装機能を相手へ広告しないことを試験している。
実SoCCP、IPCC mailbox/IRQ、SMP2P producer、全データ/SSR/zero-copyと充電等は未完。

```sh
python scripts/build_storage_deps.py --glink-transport-tests --pmic-glink-tests --rproc-gate-tests
python scripts/run_qemu.py --test glink-transport --cpus 4 --log reports/glink-transport-stage32-final-qemu.log
python scripts/audit_glink_transport_dt.py
python scripts/snapshot_boot_core.py --name android73-glink-transport-stage32 --gpu-driver --rakkashin --pmic-glink
```

`reports/glink-transport-stage32-validation.json`。実機起動・常用版完成・効率の実測は未達。


## Stage33–35: IPCC・SMP2P・SMEM状態APIの接続

実IPCC IRQ/mailboxとSMP2P producerを追加。固定チャンネル記憶域・ID範囲検査、
IRQ→mailbox→domainのdevres解除順序、共有SMEMサイズ/配列/名前の境界を修正した。
保存DTのWPSS長名は元のstrscpy(16)短縮を維持し、SSRの履歴初期化もvendorの
V1/V2差を保持。保存/候補DTのIPCC 2基・SMP2P 6系統・SoCCP状態4参照を照合した。
C共有項目layoutの一致は、全プロトコル・ファームウェア互換性の証明ではない。

SMEM状態APIの解除競合・orphan再取得・bit32受入れを実API/DT/並行タスクで再現
（修正前3失敗）。ネイティブコアにSRCUによる実行中callback待機とlookup解除を追加し、
修正後3成功。unregisterは状態callback外のsleep可能文脈で行う契約。
16パッチ・61ファイルを基点から完全再現し、Image #22を再ビルドした。

新Imageでstate3、IPCC7、SMP2P8、GLINK11、PMIC/QMI19、SSR/Gunyah10、regulator7の
計65件が成功。QRTRローカル128packetと実ドライバ登録/解除も確認。
DT関連ホスト24件も成功。本番ドライバの実機probe・IRQ配送・firmware動作の確認は未実施。

最新の部分成果物と検証は `artifacts/android73-state-ipc-stage35/` と
`reports/state-ipc-stage35-validation.json`。Alor SoCCPの電源/PAS/early attach、
実サービス、全DT、画像packaging・SELinux・実機常用/効率測定は残る。書き込みはしていない。

```sh
python scripts/build_storage_deps.py --ipcc-tests --smp2p-tests --smem-state-tests
python scripts/run_qemu.py --test smem-state --log reports/smem-state-qemu.log
python scripts/audit_state_ipc_dt.py
python scripts/audit_core_patch_replay.py
python scripts/snapshot_boot_core.py --name android73-state-ipc-stage35 --gpu-driver --rakkashin --pmic-glink
```


Stage36の次段要件は `scripts/audit_soccp_provider_requirements.py` と
`reports/soccp-provider-requirements-stage36.json` に固定。cx/mxはARCリソース
`cx.lvl`/`mx.lvl`のraw level64・load0であり、物理64uVへの読み替えはしない。
Alor descriptorはcheck_status=falseのため、別SoC用TCSR/spareプロパティを
勝手に追加する根拠はない。Q6V5 commonと実sysmonがまだ未リンクで、
Alor minidump24/ELF64や電源rollbackを含むPAS実装は次段の未完作業。


## Stage37: Q6V5 common・実sysmon/QMI・AOSS QMP

ネイティブ基点の実Q6V5、sysmon、QMPを3モジュールとして追加。実QMP/sysmon/QMI
関数へのELF importとCRC一致を確認し、以前の要件表で不足していた共通関数は
すべて実exportが存在する状態になった。これはAlor PAS完成・起動成功を意味しない。

Q6 IRQ drain、QMP参照解放、SMEM文字列境界、停止要求の失敗伝播を修正。
sysmonはIRQ callbackデータの解放順、任意IRQ未指定、RPC接続/解除の競合を修正。
古いpriv引数を使わずSRCU区間でendpointの現在の関連付けを読み、解除時に消して待つ。
4CPUゲスト11件成功。実QMI/subdeviceの登録/解除、RPMSG祖先検索/二重接続拒否、
並行ingress中のprovider解除と解除後1,000回の拒否、後からconsumer解除を含む。
実3モジュールの登録/解除も別に確認したが、物理QMPやfirmwareは動かしていない。

最新部分成果物は `artifacts/android73-remoteproc-services-stage37/`。
108モジュール、5,480 import CRC一致、全hard/soft依存が存在しグラフは非循環。
Imageと保存DT/GPU 21段の入力は変更していない。実機probe、障害注入、全通信、
Alor電源/PAS・ELF64 dump、起動画像と常用・効率評価は未完。
`reports/remoteproc-services-stage37-validation.json` を参照。


Stage38ではnative PAS・MDT loader・PIL infoの未改変基点3モジュールを実依存へ
リンクし、CRC一致まで確認した。`ports/qcom-pas-alor/` はAlor専用電源/ELF64/
終了経路の適応前なので、Alor compatibleはまだ登録せずStage37の108個にも含めない。
`reports/pas-alor-stage38-baseline-validation.json` に範囲と次段を記録した。

## Stage39: Alor専用PASのARC電源制御

ドラフトPASにAlor固有descriptorとcompatibleを実装した。PAS51/DTB65、
crash SMEM656、minidump24、firmware名、early boot/auto bootとELF64を保存元へ合わせ、
他SoCのpower-domain定義へ置き換えずcx/mx ARC vote64/load0を取得する。
成功した電圧/負荷/enable操作を数えて逆順に解除し、解除失敗は状態を保持して再試行。
同時アクセス・二重投入も制御する。実regulator APIを使う本番処理であり、
試験用の記録/失敗backendは別のKUnit TUだけに限定している。

4CPUの8件成功。投入6段階・解除6段階、元失敗とrollback失敗の併発、正のset_load戻り値、
繰り返し・再試行・2並行workerを含む。実PAS/MDT/PILの登録・解除と132 import CRCも成功。
11個のdescriptor値と保存/候補DTの選択項目が一致。実regulator framework/ARC/SoCCP、
clock/IRQ/handover・DTB認証失敗経路・firmware/resource-table/ELF64 loaderは未検証。

`reports/pas-alor-stage39-validation.json`。新しいAlorドライバはまだ成果物に入れず、
配布部分は引き続きStage37の108個。Image・保存DT・GPU設定の変更や実機への書き込みはない。

## Stage40: 実機SoCCP firmwareの読み取り照合

実機ではSoCCPがattachedで、firmwareは `/vendor/soccp_firmware/image/` にある。
専用soccp_a/bはFAT12 volumeで、両slot内のsoccp.mbn/soccp_dtb.mbnとマウント済み
2ファイルのSHA256が一致した。実イメージはいずれもELF32/EM_RISCVであり、
vendor descriptorのuses_elf64（dump metadata）とfirmware入力classは別である。
firmwareをELF64へ変換する根拠はない。

選択したloadable segmentは保存DTの6MiB/256KiB予約領域へ収まり、全segmentがinline。
native ELF32ヘッダ形状・末尾hash segmentなど選択した前提条件も一致した。
これは実loader実行・署名認証・物理restartの成功ではない。
ROMのueventdはこの専用directoryを検索し、initはforce_sysfs_fallback=1を設定する。
実機も1、7.3設定のUSER_HELPERも有効。移植版のmount/init/要求時刻は別途検証が必要。

read-only scriptと証拠は `scripts/inspect_soccp_*_readonly.py`、
`scripts/audit_soccp_firmware_inputs.py`、`reports/soccp-firmware-stage40-validation.json`。
volumeのmount・変更・抽出・firmware起動は行わず、filesystem/block読み取りと
メタデータ報告だけを行った。次は実ファイルを使うnative loaderのguarded-RAM試験、
PASのsecure/error/resource ownershipと実機起動条件の検証。
