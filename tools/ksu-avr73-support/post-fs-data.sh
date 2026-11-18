#!/system/bin/sh
# avr 7.3 support (production): the boot-time pieces the 7.3 kernel still needs
# on this ROM. Only acts on a 7.3 kernel; on 6.12 it only undoes its logd change.
# Debug-only parts of tools/ksu-avr73-debug (tracers, kmsg/log dumps, root command
# channel, burn-in reboot guard) are not included.
MARK=/data/adb/avr73-support.flowctrl-off
case "$(uname -r)" in
7.3.*) ;;
*)	[ -f $MARK ] && { setprop persist.logd.flowctrl.on 1; rm -f $MARK; }
	exit 0 ;;
esac
MODDIR=${0%/*}
LOG=$MODDIR/boot.log
echo "$(date) post-fs-data on $(uname -r)" > $LOG

# OPLUS boot_monitor_service counts boots it thinks are unstable and its repair
# ladder ends in a reboot to recovery (phone test 30). Keep it stopped on 7.3.
setprop ctl.stop boot_monitor_service

# Connectivity mainline netd BPF programs are picked by [kernel range] x [API
# range]; for kernel >= 7.3 only the 27Q2 variants exist, so on this Android 17
# (26Q2) build netbpfload loads none and netd aborts (phone test 23). Claim 27Q2
# for netbpfload (bind mount, /system is not modified).
RC=/system/etc/init/netbpfload.rc
if head -1 $RC | grep -q "^# 2026 2 37 0 0"; then
	sed '1s/^# 2026 2 37 0 0.*/# 2027 2 38 0 0 # avr73: claim 27q2 for the 7.3 netd bpf variants/' $RC > $MODDIR/netbpfload-27q2.rc
	chcon u:object_r:system_file:s0 $MODDIR/netbpfload-27q2.rc
	chmod 644 $MODDIR/netbpfload-27q2.rc
	mount -o bind $MODDIR/netbpfload-27q2.rc $RC && echo "netbpfload.rc -> 27q2" >> $LOG
fi

# init.oem.fingerprint2.sh insmods the 6.12 goodix_fod_mmi/anc_fps_mmi from
# /vendor/lib/modules; use the 7.3 builds (phone test 45).
for m in goodix_fod_mmi anc_fps_mmi; do
	[ -f $MODDIR/ko/$m.ko ] && [ -f /vendor/lib/modules/$m.ko ] || continue
	chcon u:object_r:vendor_file:s0 $MODDIR/ko/$m.ko
	mount -o bind $MODDIR/ko/$m.ko /vendor/lib/modules/$m.ko && echo "fps: $m -> 7.3" >> $LOG
done

# OPLUS logd flow control never reports ready on 7.3 and drops every entry.
if [ "$(getprop persist.logd.flowctrl.on)" = "1" ]; then
	touch $MARK
	setprop persist.logd.flowctrl.on 0
	setprop ctl.restart logd
fi

# vendor_dlkm stays 6.12, so vendor_modprobe.sh never sets this and
# init.qti.kernel.target.rc waits for it forever (phone tests 10-12).
setprop vendor.all.modules.ready 1

# Modules deferred in the vendor_boot kit: GPU/display must be up before
# composer/surfaceflinger start, camera after synx (phone tests 24-40).
for m in synx-driver msm_kgsl msm_drm camera msm-eva; do
	[ -f $MODDIR/ko/$m.ko ] || continue
	grep -q "^${m//-/_} " /proc/modules && continue
	# insmod does not apply "<module>.<param>=" from the kernel command line
	# (msm_drm.dsi_display0=<panel>), so pass them here.
	P=$(tr ' ' '\n' < /proc/cmdline | grep "^${m//-/_}\." | sed "s/^${m//-/_}\.//" | tr '\n' ' ')
	insmod $MODDIR/ko/$m.ko $P >> $LOG 2>&1
	echo "insmod $m rc=$?" >> $LOG
done

# Rakkashin features that are modules on 7.3 (built in on the 6.12 Rakkashin
# kernel): ADIOS I/O scheduler (the Rakkashin control module switches the UFS
# queues to it at late_start) and ntsync (/dev/ntsync).
for m in adios ntsync; do
	[ -f $MODDIR/ko/$m.ko ] || continue
	grep -q "^$m " /proc/modules && continue
	insmod $MODDIR/ko/$m.ko >> $LOG 2>&1
	echo "insmod $m rc=$?" >> $LOG
done
[ -c /dev/ntsync ] && chmod 0666 /dev/ntsync

# keep boot_monitor_service stopped until boot completes
(
	for i in $(seq 1 120); do
		[ "$(getprop init.svc.boot_monitor_service)" = running ] && setprop ctl.stop boot_monitor_service
		[ "$(getprop sys.boot_completed)" = 1 ] && break
		sleep 1
	done
	setprop ctl.stop boot_monitor_service
) > /dev/null 2>&1 &
