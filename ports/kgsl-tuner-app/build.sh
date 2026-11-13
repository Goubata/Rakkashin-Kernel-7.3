#!/bin/bash
# Build and sign the APK without Gradle (same toolchain as other on-device projects)
set -euo pipefail
cd "$(dirname "$0")"
SDK=/usr/lib/android-sdk/platforms/android-34/android.jar
JAVAC=/usr/lib/jvm/java-17-openjdk-arm64/bin/javac
KS=release.keystore
rm -rf build && mkdir -p build/classes build/gen
aapt package -f -m -J build/gen -M AndroidManifest.xml -S res -I "$SDK" -F build/res.apk
"$JAVAC" -source 8 -target 8 -bootclasspath "$SDK" -classpath "$SDK" -Xlint:-options \
  -d build/classes $(find src build/gen -name '*.java')
dalvik-exchange --dex --min-sdk-version=26 --output=build/classes.dex build/classes
cp build/res.apk build/unsigned.apk
(cd build && aapt add -f unsigned.apk classes.dex >/dev/null)
zipalign -f -p 4 build/unsigned.apk build/aligned.apk
if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -storepass rkgputuner -keypass rkgputuner -alias rkgpu \
    -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=Goubata, O=Rakkashin, C=JP"
fi
apksigner sign --ks "$KS" --ks-pass pass:rkgputuner --key-pass pass:rkgputuner --ks-key-alias rkgpu \
  --out Rakkashin-GPU-Tuner.apk build/aligned.apk
apksigner verify Rakkashin-GPU-Tuner.apk && echo verified
