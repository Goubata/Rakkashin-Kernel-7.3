#!/bin/sh
# Build ports whose avr-canoe-vendor-config.h is generated until the header is
# stable (it depends on the headers a successful build actually included). Run as root.
cd "$(dirname "$0")/.." || exit 1
rc=0
for p in "$@"; do
	for i in 1 2 3; do
		before=$(md5sum < ports/$p/avr-canoe-vendor-config.h 2>/dev/null)
		python3 -I scripts/gen_drm_vendor_config.py "$p" >/dev/null
		after=$(md5sum < ports/$p/avr-canoe-vendor-config.h)
		[ $i -gt 1 ] && [ "$before" = "$after" ] && break
		sh scripts/build_msm_drm.sh "$p" >/dev/null
	done
	if grep -qE "error:|ERROR:|\*\*\* " reports/$p-stage120-build.log; then echo "BUILD FAIL $p"; rc=1; else echo "ok $p (iter $i)"; fi
done
exit $rc
