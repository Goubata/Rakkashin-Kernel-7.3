# Gunyah resource-manager adapter

Motorolaの `gh_rm_core.c` / `gh_rm_iface.c` から7.3用 `gh_rm_drv.ko` を生成しました。前段階で不足していた実common RM輸送とSCM assign-memory通知も移植され、最終リンクが通っています。hw_fenceが必要とする3 APIを提供します。実機でのprobe・通信成功は未確認です。

SoCソースは `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`、共通RM/QTVM headerは `479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4`。現代のメモリ割当通知経路を維持しています。`CONFIG_GUNYAH_LEGACY` による迂回はありません。

実機gh_rm_drvの `gunyah_qtvm_register_notifier` インポートとcanoe_perf.bzlを照合し、外部モジュール用 `CONFIG_QTVM_WITH_AVF=y` を反映しました。QTVM providerを有効にして実APIへリンクしています。ドアベルの範囲チェックは実機バイナリとも照合しました。`reports/gunyah-avf-abi-audit.json` を参照。通常のcore `.config` だけでは外部DDK設定は確認できません。

OF firmware-node変換は7.3 APIを使います。`avr-rm-deps.h` は不足したvendor宣言だけを補い、SCM通知本体は7.3側の実装です。

`python scripts/build_ipc_deps.py --rm` で輸送を含めて作成します。GPUまで作る場合は `--gpu-driver`。通常／失敗時の通知登録解除、IRQ・共有メモリ・VMの寿命、実hypercallとファームウェアとの相互運用は引き続き検証が必要です。リンク成功を常用動作の証明とは扱いません。


## Stage10: notifier lifetime

SCM/hypとRM/QTVMの通知を共通の登録・解除処理へまとめ、2つ目の登録失敗時は1つ目を巻き戻すようにしました。probe失敗・removeで全通知を解除し、関連workを排出してからIRQ親ノードを解放します。scoped cleanupと重なっていたnode putと、OF検索へ渡す借用参照も修正しました。割当通知を使うモードは必要な登録が両方成功してから有効になります。

8 KUnitで成功時、段階ごとの登録失敗、再試行、重複登録、任意の2つ目、繰り返し解除、実行中コールバックの排出を確認しました。テストは本番と同じhelperに実kernel notifier chainを接続し、登録エラーを境界で注入します。実VM・実hypervisorの終了を再現したものではありません。

`python scripts/audit_gunyah_core_sources.py --vendor-rm` で固定SoC/common由来の7ファイルと所有するglue 2ファイルを確認し、`port-7.3.patch` を元入力へ適用して一致を検証します。`python scripts/run_qemu.py --test rm-lifetime --cpus 4 --log reports/rm-lifetime.log` で隔離テストを再実行します。
