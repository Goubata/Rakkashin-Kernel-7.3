# Qualcomm remoteproc common helpers

Android common 7.3 commit `4dd306a07b2f456fd70ddaa28f4f65640374f202` の `drivers/remoteproc/qcom_common.c/.h` を無改変で外部モジュールとしてビルドしています。private remoteproc headerは同じ7.3ツリーから参照します。SMEMは既に移植した実装へリンクします。

`qcom_register_ssr_notifier` / `qcom_unregister_ssr_notifier` と、実際のremoteproc subdeviceによる通知を提供します。Vendorとnativeの4イベントの値・通知データ配置を確認しました。通常起動・クラッシュ・別サブシステムへの分離・登録解除・sleep可能なコールバックの5 KUnitがSMP4 QEMUで成功しました。実DSP/モデムの起動やSSR成功の証明ではありません。

Vendorのearly SSR、タイムアウト監視、Alorのremoteproc固有設定、GLINK/RPMSG輸送は別の移植対象です。このモジュール単体で全remoteproc機能が動作するとは扱いません。

`python scripts/build_ipc_deps.py --tests` で既存の7.3ビルドに対して作成します。実機6.12カーネルにはロードしていません。

Stage30 project policy: selected GLINK/SMD subdevice startup requires a real
edge. If the native disabled-feature API returns NULL, startup returns
EOPNOTSUPP. This avoids treating unavailable IPC as a completed functional port.
Two actual-source tests reproduce the prior zero returns and verify rejection;
real transport, firmware boot and physical remoteproc operation remain pending.
