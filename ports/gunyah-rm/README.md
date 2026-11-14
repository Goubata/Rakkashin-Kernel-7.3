# Gunyah resource-manager adapter: unfinished

Motorolaの `gh_rm_core.c` / `gh_rm_iface.c` を7.3へ移植中です。全対象Cソースがコンパイルされ、`gh_rm_drv.o` を生成できました。最終modpostは以下の実装不足で失敗します。

- `gunyah_rm_call`
- `gunyah_rm_notifier_register` / `gunyah_rm_notifier_unregister`
- `qcom_scm_assign_mem_notifier_register`

このモジュールが実装する3 exportはhw_fenceの残る3依存に対応します。ただし未リンクなので成功済み56モジュールには含めません。`reports/gunyah-rm-dependencies.json` に記録しています。

SoC元ソースは `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`、共通RM headerは `479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4`。OF firmware-node変換を7.3 APIへ変更し、AVF固有ヘッダはそのビルド時のみincludeします。`avr-rm-deps.h` は実ソースにある型・APIの宣言のみであり、実装を代用しません。

現代のメモリ割当通知経路を維持しています。`CONFIG_GUNYAH_LEGACY` を強制して検出・通知を回避する変更はありません。次の依存となるcommon側のrsc_mgr輸送ソースも固定コミットから取得済みです。VM管理・CMA・実hypercall・メモリ所有権を扱うので、輸送とSCMの実装および失敗時処理の検証が必要です。

通常の成功ビルドは `python scripts/build_ipc_deps.py --tests`。この未完成リンクの再現には `--rm-research` を追加します（現状、上記の未解決依存で失敗します）。
