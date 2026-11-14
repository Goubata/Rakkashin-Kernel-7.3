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
