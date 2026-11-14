# Gunyah resource-manager adapter

Motorolaの `gh_rm_core.c` / `gh_rm_iface.c` から7.3用 `gh_rm_drv.ko` を生成しました。前段階で不足していた実common RM輸送とSCM assign-memory通知も移植され、最終リンクが通っています。hw_fenceが必要とする3 APIを提供します。実機でのprobe・通信成功は未確認です。

SoCソースは `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`、共通RM/QTVM headerは `479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4`。現代のメモリ割当通知経路を維持しています。`CONFIG_GUNYAH_LEGACY` による迂回はありません。

実機gh_rm_drvの `gunyah_qtvm_register_notifier` インポートとcanoe_perf.bzlを照合し、外部モジュール用 `CONFIG_QTVM_WITH_AVF=y` を反映しました。QTVM providerを有効にして実APIへリンクしています。ドアベルの範囲チェックは実機バイナリとも照合しました。`reports/gunyah-avf-abi-audit.json` を参照。通常のcore `.config` だけでは外部DDK設定は確認できません。

OF firmware-node変換は7.3 APIを使います。`avr-rm-deps.h` は不足したvendor宣言だけを補い、SCM通知本体は7.3側の実装です。

`python scripts/build_ipc_deps.py --rm` で輸送を含めて作成します。GPUまで作る場合は `--gpu-driver`。通常／失敗時の通知登録解除、IRQ・共有メモリ・VMの寿命、実hypercallとファームウェアとの相互運用は引き続き検証が必要です。リンク成功を常用動作の証明とは扱いません。
