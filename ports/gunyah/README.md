# Gunyah base modules for AVR

Motorola SoC `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1` の実装から `gh_arm_drv.ko`, `gh_dbl.ko`, `gh_msgq.ko` を7.3向けにリンクしました。不足する共通Gunyah定義はMotorola common `479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4` 由来です。ソースの由来と差分は `reports/ipc-source-provenance.json` に固定しています。

Hypercall、capability、IRQ、キュー処理は実ソースを維持しています。変更はファイル内限定のmessage-queue allocatorをstaticにするスコープ修正です。新しいalways-success backendやCRC書換えはありません。

SMP4のディスクを接続しないQEMUで3モジュールのロード・アンロードに成功。登録の排他、送受信方向で共有するdescriptorの寿命、capabilityが未配布の場合のNONBLOCK失敗、仮想IRQ確保・解放の4 KUnitに成功しました。テストは実際の関数を呼びますが、hypercallを発行する手前の経路だけです。実機ハイパーバイザとの通信成功は未確認です。

リソースマネージャーは隣の `gunyah-rm/` で移植中です。これら3モジュールだけではGunyah輸送やGPUの依存は完成しません。
