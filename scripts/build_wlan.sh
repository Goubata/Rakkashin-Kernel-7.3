#!/bin/sh
# make -k build of ports/wlan-qcacld/qcacld-3.0 (run as root); log: reports/wlan-qcacld-build.log
cd "$(dirname "$0")/.." || exit 1
su -c "chroot /data/data/com.termux/files/home/.andronix/distros/ubuntu/rootfs /usr/bin/env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin make -k -C /work/avr-src O=/work/avr-port/out/native-gki ARCH=arm64 LLVM=1 M=/work/avr-port/ports/wlan-qcacld/qcacld-3.0 -j${JOBS:-8} modules" > reports/wlan-qcacld-build.log 2>&1
echo rc=$?
