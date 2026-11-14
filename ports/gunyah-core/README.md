# Gunyah common transport / VM services

Motorola common `479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4` のhypercall、情報領域、RM輸送、VM管理、CMA、vCPU、irqfd、ioeventfd、QTVMを7.3へ移植しました。8モジュールがリンク済みです。`gunyah_qcom.c` は保存のみで、元の実機構成と同様にこのビルドでは選択していません。

`python scripts/audit_gunyah_core_sources.py` は16ファイルを固定Git blobと照合し、`port-7.3.patch` を元ソースへ再適用して一致を確認します。OF firmware-node、file mapping lookup、CMAのPFN走査、platform remove API、ファイル内関数のスコープを7.3へ合わせています。

Androidのrestricted hooksは7.3本体へ定義・exportし、モジュールを外しても残る形にしています。ARM64の既存direct-map復元処理も実関数をGPL exportしています。実装のない代替処理はありません。

SMP4 QEMUでhypercall/provider helperモジュールとplatform hooksをロード・アンロードし、Gunyah環境ではないゲストでは `gunyah.ko` がENODEVを返して登録されないことを確認しました。実RM、VM起動・共有メモリ・hibernationの成功は未確認です。PM notifierやIRQ・auxiliary deviceの初期化失敗時／終了時処理の監査も残ります。

基本再ビルド後、`python scripts/build_ipc_deps.py --gpu-driver --tests` でRM・フェンス・KGSLまで依存順に作ります。カーネル本体の3追加パッチは `scripts/build_native.py` が扱います。
