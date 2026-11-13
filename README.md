# avr: Android 7.3 kernel port

この実機で7.3本体・付属モジュールと、初期のQualcomm依存モジュール／Alor pinctrlモジュールをビルドできました。端末全体の移植は未完了で、起動・常用動作は未確認です。

## 固定した入力

- Android common `android18-7.3`: `4dd306a07b2f456fd70ddaa28f4f65640374f202`
- Motorola SoC `android-16-release-w3wwl36.65-11-18`: `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`
- Rakkashin参照: `7e0d0a6daa4959e047f650513a0a243d3e2565aa`
- 公式manifest: `75faf58baa4a84e46094af70ab882590fad58fa7`

`manifests/android-7.3-pinned.xml` は公式manifestの60プロジェクトを個別コミットに固定したものです。元の構成を保持しているため、取得には大きな空き容量と通信量が必要です。`reports/aosp-source-lock.json` に対応するSHAを記録しています。再生成スクリプトはその時点の依存ブランチ先端を解決するため、実行すると固定値が更新されます。

## 最初のビルド

Linux x86_64ホストにgit、Python 3、Android repoを用意して実行します。Clang・Rust等は公式manifestのprebuiltを使用します。

```sh
bash scripts/build_baseline.sh /absolute/path/to/new-build-directory
```

これは未改変の7.3 GKIのビルドを検証する段階です。生成物にはavr用ドライバーを含まず、端末へ書き込む段階ではありません。スクリプトはまだビルドホストで実行していません。

## 移植の順序

1. 未改変Android 7.3のビルドを通す。
2. avr純正ファームウェアのmodules.load、modules.dep、全ko、DTB/DTBO、設定とソースの対応を固定する。
3. Qualcomm SoCモジュールを7.3向けにビルドし、hook/API/構造体変更を解決する。
4. 起動必須のクロック、電源、interconnect、SMMU、UFS、暗号化経路を通す。
5. 表示、タッチ、GPU、通信、音声、カメラ等を順に検証する。
6. 充電・温度・スリープ復帰・待機電力・長時間負荷を検証する。
7. Rakkashin独自機能を依存関係ごとに移植する。

`reports/port-inventory.json` の125個はQualcomm canoeの候補リストで、実機avrの使用モジュール一覧ではありません。同名ソースパスの有無だけで機能の有無や互換性は判断しません。

6.18の移植画像は参考情報として扱い、未公開の移植パッチや機能動作は前提にしません。

## この実機でのネイティブビルド（2026-11-13）

ユーザーの許可を受け、既存のUbuntu 26.04 arm64 rootfsにClang 21.1.8、make 4.4.1、Rust 1.93.1／rust-src、bindgen 0.72.1、pahole 1.31、libdw-dev等を導入しました。既存の未完了dpkg処理と不足依存関係も修復しています。

```sh
python scripts/build_native.py
```

この経路はLinux arm64ネイティブのmakeビルドです。公式Kleaf指定のClang r596125／Rust 1.96.0とは異なるため、公式GKI認証や同一バイナリの再現を意味しません。Rustを有効にしたgki_defconfigとrustavailableは成功しました。ビルド結果はreportsのログを参照してください。

chrootのproc、dev、work/avr-src、work/avr-portにはbind mountを使用しています。Androidのboot/vendor_boot/dtboへの書き込みは行っていません。

実機はDevice Tree上avr/alorで、ユーザー作成のOnePlusポートROMを使用中です。ro.product等の製品名は実機識別に使いません。

`device-baseline/` に現在の設定、稼働DTB、ロード済みモジュール一覧、vendor 323モジュール、system 104モジュールと起動イメージの読み取りダンプを保存しています。候補リストとは別に、これを実機基準とします。boot_aはheader v4、ramdiskなしでした。

## 完了した最初のビルド

7.3.0-4kのImage（46,488,064 bytes）と付属モジュール128個をこの実機上でビルドし、makeの終了コード0を確認しました。arm64 Imageのmagicも検証しています。成果物とSHA-256は `artifacts/android73-baseline/` に保存しています。

これは未改変Android 7.3の土台です。avr用移植コードはまだImageに組み込んでいません。実機起動・常用動作は未確認で、端末対応boot.imgではありません。

Alor pinctrlの2オブジェクト、状態保持処理を移植したCCFのオブジェクト、Alor GCCのオブジェクトがコンパイルできました。pinctrlのモジュールリンクで判明したqcom_scm_io_readl/writel不足は、SCM/PAS/TZMEMを設定に追加して再ビルドすることで解消しました。pinctrl-msm.koとpinctrl-alor.koのリンクも終了コード0で完了しています。未解決シンボルを無視する設定は使用していません。


## 初期のQualcomm依存関係とpinctrlのビルド

```sh
python scripts/build_native.py --platform --pinctrl
```

`configs/avr-platform.fragment` をGKI設定へマージし、SCM/PAS/TZMEMをビルドしてからpinctrlの2モジュールをリンクします。現在の成果物は `artifacts/android73-firmware-stage1/`。5モジュールすべてのvermagicが7.3.0-4kであることを確認しています。CCF移植パッチとAlor GCCはまだこのImageに統合していません。現時点で実行中のビルドはありません。
