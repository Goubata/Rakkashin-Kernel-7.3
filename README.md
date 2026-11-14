# avr: Android 7.3 kernel port

Motorola avr（XT2611 / alor、ユーザー作成のOnePlusポートROM）用の移植作業です。端末内で7.3 Imageと付属モジュール、Qualcomm基盤・GPU支援・OnePlus連携およびRakkashin・Gunyah・GPU・UFSホスト・電源系を含む80モジュールをビルドしました。**実機の7.3起動・全機能動作は未確認で、常用版ではありません。**

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

最新成果物は artifacts/android73-regulators-stage17/（80モジュール、4,285インポートCRC一致）。Stage16は artifacts/android73-ufs-host-stage16/（77モジュール、4,168インポートCRC一致）。Stage15は artifacts/android73-ufs-core-stage15/（73モジュール、3,727インポートCRC一致）。Stage14は artifacts/android73-ice-stage14/（73モジュール、3,727インポート）。Stage13は artifacts/android73-storage-stage13/（73モジュール、3,700インポート）。Stage12は artifacts/android73-gpu-stage12/（71モジュール、3,624インポート）。Stage11は artifacts/android73-gpu-stage11/（71モジュール、3,623インポート）。Stage10は artifacts/android73-gpu-stage10/（71モジュール、3,622インポート）。前段階は artifacts/android73-gpu-stage9/（71モジュール、3,618インポート）。前段階は artifacts/android73-ipc-stage8/（56モジュール）。前段階は artifacts/android73-rakkashin-stage7/（52モジュール）。前段階は artifacts/android73-sysstats-stage6/。前段階は artifacts/android73-smmu-memory-stage5/。以前のGPU支援段階は artifacts/android73-gpu-dependencies-stage4/。メモリ移植の段階は artifacts/android73-memory-stage3/。以前の基盤版は artifacts/android73-boot-core-stage2/。Image、36モジュール、設定、シンボル、SHA256SUMS を保存しています。端末対応boot.imgではありません。完了条件は VALIDATION.md を参照。

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
