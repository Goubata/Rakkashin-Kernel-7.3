#!/bin/sh
# make -k build of ports/msm-drm (shows all errors); run as root
cd "$(dirname "$0")/.." || exit 1
su -c "chroot /data/data/com.termux/files/home/.andronix/distros/ubuntu/rootfs /usr/bin/env -i PATH=/usr/sbin:/usr/bin:/sbin:/bin make -k -C /work/avr-src O=/work/avr-port/out/native-gki ARCH=arm64 LLVM=1 M=/work/avr-port/ports/${1:-msm-drm} -j8 modules" > reports/${1:-msm-drm}-stage120-build.log 2>&1
echo rc=$?
