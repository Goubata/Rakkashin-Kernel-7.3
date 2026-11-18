#!/system/bin/sh
# AVR 7.3 bring-up: only acts on the 7.3 test kernel
MARK=/data/local/tmp/avr73/.flowctrl-off
case "$(uname -r)" in
7.3.*) ;;
*)	# back on 6.12: restore the OPLUS logd flow control we disabled for the 7.3 boot
	[ -f $MARK ] && { setprop persist.logd.flowctrl.on 1; rm -f $MARK; }
	exit 0 ;;
esac
MODDIR=${0%/*}
O=/data/local/tmp/avr73/boot-$(date +%Y%m%d-%H%M%S)
mkdir -p $O; chmod 777 $O; ln -sfn $O /data/local/tmp/avr73/latest
echo "$(date) post-fs-data on $(uname -a) uptime $(cut -d' ' -f1 /proc/uptime)" > $O/events.txt
# Phone test 23: the Connectivity mainline netd.o/netd@funcs.o pick program variants by
# [kernel range] x [API range]. For kernel >= 7.3 the ingress_stats / get+setsockopt variants
# need API 27Q2 (Android 18), and the 26Q2 ones stop at 7.3, so on this Android 17 (26Q2)
# build netbpfload silently loads none of them and netd aborts. netbpfload reads its API
# level from the first line of netbpfload.rc; for the test boot claim 27Q2 (bind mount,
# nothing written to /system) so the 7.3-native variants load.
RC=/system/etc/init/netbpfload.rc
if head -1 $RC | grep -q "^# 2026 2 37 0 0"; then
	sed '1s/^# 2026 2 37 0 0.*/# 2027 2 38 0 0 # avr73 test: claim 27q2 for the 7.3 netd bpf variants/' $RC > $MODDIR/netbpfload-27q2.rc
	chcon u:object_r:system_file:s0 $MODDIR/netbpfload-27q2.rc
	chmod 644 $MODDIR/netbpfload-27q2.rc
	mount -o bind $MODDIR/netbpfload-27q2.rc $RC && echo "netbpfload.rc -> 27q2: $(head -1 $RC)" >> $O/events.txt
fi
# Watchers first: this script starts ~8 s into boot but the module loads below take ~9 s, and
# netbpfload starts at ~17.3 s (phone test 21), so attaching after them races with it.
# netbpfload: trace-only (failing syscalls such as bpf() errnos + liblog payloads, which are
# lost while logd keeps nothing on 7.3). Phone test 21: six netd_shared programs
# (ingress_stats, get/setsockopt, accecn*) are missing on 7.3 and netd aborts on them.
R1_TRACEONLY=1 nohup $MODDIR/rescue1shot $O/netbpfload-trace.txt 120 netbpfload > /dev/null 2>&1 &
# netd1shot rescue (phone tests 13-18: the netd self-test fails and init reboots with
# "netd1shot-fail" before adbd starts). Attach to the real netd1shot, log failing syscalls and
# turn its failure into exit 0 so the 7.3 test boot stays up for live debugging over adb.
nohup $MODDIR/rescue1shot $O/netd1shot-trace.txt 120 > /dev/null 2>&1 &
# OPLUS logd flow control never reports ready on 7.3 and drops every entry (phone test 16:
# logcat -S Total 5121, Now 0). Disable it for this boot and restart logd.
if [ "$(getprop persist.logd.flowctrl.on)" = "1" ]; then
	touch $MARK
	setprop persist.logd.flowctrl.on 0
	setprop ctl.restart logd
	sleep 2
fi
# The 7.3 modules are loaded from vendor_boot; vendor_dlkm stays 6.12, so vendor_modprobe.sh
# never reaches "setprop vendor.all.modules.ready 1" and init.qti.kernel.target.rc
# (on post-fs-data: wait_for_prop vendor.all.modules.ready 1) blocks init forever (phone
# tests 10-12: no zygote/adbd). Signal it here (test kit only; a 7.3 vendor_dlkm sets it itself).
setprop vendor.all.modules.ready 1
# adbd normally starts only once system_server's UsbDeviceManager applies adb_enabled
# (persist.sys.usb.config=none on this ROM). The 7.3 test boot never gets that far, so start
# the adb gadget function ourselves (phone test 19: init.svc.adbd=stopped, sys.usb.state=none).
setprop sys.usb.config adb
cat /proc/modules > $O/modules-postfs.txt
# logd health (phone test 14: logcat returned nothing on 7.3)
{ $MODDIR/scmtest; echo "--- logcat -S"; logcat -S 2>&1 | head -40; ls -laZ /dev/socket/logd* ; grep -E "logd" /proc/net/unix; log -p e -t AVR73 "logger probe $(date)"; sleep 1; logcat -d -t 5 2>&1; echo "logcat rc=$?"; } > $O/logd-check.txt 2>&1
# continuous logs: survive the reboot that netd1shot-fail etc. trigger (init syncs on reboot)
nohup logcat -b all -v threadtime -f $O/logcat-stream.txt > /dev/null 2>&1 &
nohup sh -c "cat /proc/kmsg > $O/kmsg-stream.txt" > /dev/null 2>&1 &
for m in synx-driver msm_kgsl msm_drm; do
	[ -f $MODDIR/ko/$m.ko ] || continue
	grep -q "^${m//-/_} " /proc/modules && { echo "$m already loaded" >> $O/insmod.txt; continue; }
	# insmod (unlike modprobe) does not apply "<module>.<param>=" from the kernel command
	# line. Phone test 24: without msm_drm.dsi_display0=<panel> msm_drm fell back to
	# mipi_mot_video_dummy_qhd, so the real panel was never driven (splash stays on = OLED risk).
	P=$(tr ' ' '\n' < /proc/cmdline | grep "^${m//-/_}\." | sed "s/^${m//-/_}\.//" | tr '\n' ' ')
	echo "insmod $m params: $P" >> $O/insmod.txt
	insmod $MODDIR/ko/$m.ko $P >> $O/insmod.txt 2>&1
	echo "insmod $m rc=$?" >> $O/insmod.txt
done
dmesg > $O/dmesg-postfs.txt
# background logger: does not depend on the late_start (service.sh) stage being reached
nohup sh -c '
O='"$O"'
MODDIR='"$MODDIR"'
i=0
while [ $i -lt 60 ]; do
	dmesg > $O/dmesg.txt
	logcat -d -b all > $O/logcat.txt 2>&1
	getprop > $O/getprop.txt
	cat /proc/modules > $O/modules.txt
	ps -A -o PID,PPID,STAT,NAME > $O/ps.txt 2>&1
	{ ls -lR /config/usb_gadget 2>&1 | head -200; for f in /sys/class/udc/*/state /sys/class/udc/*/current_speed; do echo "$f: $(cat $f 2>&1)"; done; } > $O/usb.txt
	# init "on boot" copies persist.sys.usb.config (none) over our early setprop: keep re-asserting
	[ "$(getprop sys.usb.config)" = adb ] || setprop sys.usb.config adb
	[ $i -eq 2 ] && ls -laRZ /sys/fs/bpf > $O/bpffs.txt 2>&1
	# 7.3 su from adb shell fails (EBADF), so keep every log readable by the shell user
	chmod 666 $O/* 2>/dev/null
	echo "$(date) tick $i boot_completed=$(getprop sys.boot_completed) init.svc.adbd=$(getprop init.svc.adbd) sys.usb.state=$(getprop sys.usb.state)" >> $O/events.txt
	sync
	# OLED burn-in guard: a 7.3 test boot whose display is not driven keeps the static boot
	# splash on the panel. Reboot back to 6.12 after ~4 min unless a live session asked to keep
	# it (touch /data/local/tmp/avr73/keep). Logs are already synced above.
	if [ $i -ge 24 ] && [ ! -f /data/local/tmp/avr73/keep ]; then
		echo "$(date) burn-in guard: rebooting at tick $i" >> $O/events.txt; sync
		reboot
	fi
	i=$((i+1)); sleep 10
done' > /dev/null 2>&1 &
