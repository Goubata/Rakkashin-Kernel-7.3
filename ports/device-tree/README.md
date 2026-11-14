# AVR DT input reconstruction

実機から保存した vendor_boot v4 / DTBO v0 を基準に作業中です。7.3実機起動・binding検証・全ハード動作は未確認です。ここにある2本のラッパーだけで工場出荷時の全DT構成を再現できるわけではありません。

## 完全な静的ベースライン

`python scripts/audit_boot_tree.py` で保存画像の境界・FDTを検査・抽出します。`python scripts/roundtrip_avr_dts.py` は抽出済みの静的ベースとオーバーレイをDTSへ戻し、7.3のビルドツリーにあるDTCで再コンパイルします。

保存ダンプのベース3,777ノードとオーバーレイ1,943ノードは、再コンパイル後もバイト単位で元と一致しました。適用後5,102ノードの全プロパティも一致しています。出力は `artifacts/dt-dump-reconstruction/`、検証記録は `reports/dt-dump-reconstruction.json` です。これは元のハード定義を欠落なく編集可能にした結果であり、7.3のドライバ互換性を証明するものではありません。

実行中DTとの比較ではphandle値と参照先を区別します。番号が変わっただけのタッチ・音声参照は同じノードを指します。機種識別・DRAM・SCM・timer、起動時に追加されるノード等は別に記録しています。DRAM割当・ハイパーバイザの実行時情報・bootargsを固定して焼き込む処理はありません。比較の参照正規化は対象を限定しており、全bindingの検証には代用できません。

## 公開ソースとの対応

`python scripts/prepare_dt_sources.py` は固定コミットからAVRメインDTSのinclude graphを取り出し、Git blobとSHA256を検証します。`reports/dt-source-lock.json` に入力を固定します。Motorolaのcarrier-channel-ids定義も実ソースを使っています。

`python scripts/build_avr_dts.py --allow-partial` はこのメインDTSのみを7.3 DTCで試験ビルドします。現在81ファイルで、適用後3,813ノードです。元と比べ1,289ノード不足し、表示・音声・カメラ・GPU等のcomponent DTの合成が未完了です。`--allow-partial` を付けない実行は、不足をレポートへ記録して異常終了します。どちらの場合もイメージ化は許可されません。

AVRの公開DTSにはvantageとセンサー設計を共有する旨のコメントがありますが、別機種のピン・電源・周辺定義を無条件で転用しません。7.3のローカルbindingにalor/canoe/chora/SM8845専用ファイルは現状見つかっていません。共通bindingと実際に移植するドライバのOF match/property読み出しを突き合わせる必要があります。

## 残作業

component DTSの合成順序と製品選択の復元、各ドライバの7.3対応、DT schema検証、DTBOコンテナのサイズ・選択情報を保持した再構成、vendor_bootのramdisk構成・load順序・署名・AVB確認が残っています。生成物はファイルのみで、実機パーティションの書き込みや再起動は行っていません。

`.ko` 配置と依存の棚卸しは `python scripts/audit_module_deployment.py`。同名の7.3モジュールがある場合も互換性の候補として扱い、旧6.12モジュールの流用を成功扱いにはしません。SELinuxは実ROMのポリシーと必要な操作を確認して扱います。現時点でポリシー変更やpermissive化は行っていません。

## 表示・音声componentの追加実験

`python scripts/prepare_dt_sources.py --component display --component audio` で、両componentのベース拡張とAVRオーバーレイを含む159ファイルを固定して取得しました。音声bindingは一致コミットのaudio-kernelから取得します。`python scripts/build_dt_components.py` により全選択DTSが7.3 DTCでコンパイル・適用できました。

この実験の適用後ノード数は4,613、元との差は不足489・追加0です。OEMの正式な合成順序は未復元で、プロパティ差分も残っているため、完成DTBとして配布・イメージ化しません。順序・ハッシュ・差分は `reports/dt-component-build.json` に記録しています。カメラ・GPU・IPCLite・映像・通信等の追加定義が残っています。

`--components` はカメラも含む取り込みを試みますが、現時点の固定入力では `dt-bindings/msm-camera.h` を解決できず失敗します。この失敗は `reports/dt-component-camera-missing-binding.json` に保存しました。欠けた定数や空ヘッダで成功を装う処理はありません。component lockは最後の実行結果を記録するため、後続ビルドは成功したinclude graphのみに限定します。

## 現在のSELinux基準

`python scripts/capture_selinux_baseline.py` により、稼働中6.12 ROMがEnforcing・policy version 33であること、実際のpolicyハッシュ、GPU/ntsync/oplus_sync/synxのデバイスラベルを読み取りました。`reports/selinux-port-baseline.json` に保存しています。OnePlus連携ノードは `/dev/oplus_sync` です。モジュールの名前とデバイスの名前を混同しません。

この観測は7.3でのアクセス許可を証明しません。サービスのdomain、必要なioctl/xperm、ueventdによる所有者・ラベル、実機で発生するAVCの確認が必要です。現行policyは変更していません。

## 拡張componentとユーザーGPU設定の復元

`python scripts/prepare_dt_sources.py --components --camera-reference --extended-platform` は公開固定入力にGPU・フェンス・HFI・IPCLite・映像・BT・WLAN・AVR NXP NFCを追加します。カメラの不足ヘッダはVantageの固定参照を明示して使い、AVRカメラドライバとの互換性は未確認です。IPCLiteの不足ヘッダは復元データで、実装上のslot番号と実機ダンプのhardware signal番号を別々に検証しています。42ノードのmailbox/interrupt/provider/client整合を確認し、元vendorヘッダを回収したとは主張しません。

`python scripts/build_dt_components.py --camera-reference --extended-platform` は189ソースファイルから5,063ノードを試験合成。不足39ノードに加え、プロパティ差があります。特に公開ソースの予約領域は保存実機と異なり、未解決です。OEM合成順序・7.3 binding・実機ドライバを検証するまでpackagingは禁止です。

ユーザーの低電圧化・クロック・段数変更は公開標準値へ置き換えません。`python scripts/reconstruct_gpu_dt.py --write` は保存した静的DTと稼働中DTのGPU表が一致することを確認し、21段階の周波数・電圧corner・ACD・bus設定と初期段を `recovered/avr-saved-gpu-pwrlevels.dtso` に復元します。新しいチューニング値は生成しません。移植ドライバの32段階上限内、index連続、周波数順序、bus範囲を確認しますが、これは7.3での電圧/firmware動作の検証ではありません。

`python scripts/build_dt_components.py --camera-reference --extended-platform --saved-gpu-table` は別の研究出力にこの表を最後に適用します。適用後の22ノード（表親と21段）の全プロパティがダンプと完全一致することを確認し、一致しなければ失敗します。出力は `artifacts/dt-avr-platform-gpu-saved-build/`、照合は `reports/gpu-dt-reconstruction.json` です。全体5,069ノード、不足33ノードで、完成DTBや常用動作の証明ではありません。

## DSP/EVA参照と保存メモリを含むプロファイル

一致リリースにDSP/EVA DT refはありませんでした。別MotorolaリリースからAlor向けのDSP (`a6c46c4e0203f4679ac7249a1e5d30a7fedfad3c`) とEVA (`90082d47f3259fe1888019f8a6e78dffa47f24ad`) を参照入力として固定しています。DSPのAlor 2ファイルは `3a921fd2ace6c373cc4a115852bf29f2beab4452` にも同じGit blobで存在します。firmware一致とは区別します。

```sh
python scripts/prepare_dt_sources.py --components --camera-reference --extended-platform --dsp-eva-reference
python scripts/reconstruct_memory_dt.py --write
python scripts/build_dt_components.py --camera-reference --extended-platform --dsp-eva-reference --saved-gpu-table --saved-memory-layout
```

193入力ファイルを使うこの研究出力は `artifacts/dt-avr-dump-aligned-build/`。GPU表の完全一致を維持したまま5,102ノード（不足0、追加0）になりました。DSP/CVPの31ノードについて、scalar/string、既知形式の参照先と引数は保存静的DTと一致します。比較は同じphandle数値が別ノードを指す場合も検出し、未知形式を適当にphandleとみなしません。全体では240ノードに未解釈の参照やプロパティ差が残っています。これは全体のsemantic一致やfirmware/driver互換性の証明ではありません。

保存メモリoverlayは静的・稼働中の両ダンプで一致する3ノードに限定します。表示用shared-dma-poolは200 MiB / 4 MiB alignmentで、動的割当のままです。heapのmemory-regionはoverlay内の参照で結び、ダンプの数値phandleを流用しません。固定予約領域はダンプの0x80e00000 / 4 MiBを復元し、保存済みの他の固定領域との非重複を確認しています。実機CMA allocation、DMA heap driver、suspendやfirmware動作は未確認です。

`reports/dt-dsp-eva-reference-comparison.json` と `reports/memory-dt-reconstruction.json` に検証範囲とハッシュを記録しています。ノード一致のみではpackagingを許可しません。DTBOコンテナ・vendor_boot・DLKM・ROM側の整合と実機検証は引き続き必要です。
