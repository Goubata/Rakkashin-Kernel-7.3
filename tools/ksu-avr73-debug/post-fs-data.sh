#!/system/bin/sh
# AVR 7.3 bring-up: only acts on the 7.3 test kernel
MARK=/data/local/tmp/avr73/.flowctrl-off
case "$(uname -r)" in
7.3.*) ;;
*)	# back on 6.12: restore the OPLUS logd flow control we disabled for the 7.3 boot
	[ -f $MARK ] && { setprop persist.logd.flowctrl.on 1; rm -f $MARK; }
	exit 0 ;;
esac
# OPLUS logd flow control never reports ready on 7.3 and drops every entry (phone test 16:
# logcat -S Total 5121, Now 0). Disable it for this boot and restart logd.
if [ "$(getprop persist.logd.flowctrl.on)" = "1" ]; then
	mkdir -p /data/local/tmp/avr73; touch $MARK
	setprop persist.logd.flowctrl.on 0
	setprop ctl.restart logd
	sleep 2
fi
MODDIR=${0%/*}
# The 7.3 modules are loaded from vendor_boot; vendor_dlkm stays 6.12, so vendor_modprobe.sh
# never reaches "setprop vendor.all.modules.ready 1" and init.qti.kernel.target.rc
# (on post-fs-data: wait_for_prop vendor.all.modules.ready 1) blocks init forever (phone
# tests 10-12: no zygote/adbd). Signal it here (test kit only; a 7.3 vendor_dlkm sets it itself).
setprop vendor.all.modules.ready 1
O=/data/local/tmp/avr73/boot-$(date +%Y%m%d-%H%M%S)
mkdir -p $O; ln -sfn $O /data/local/tmp/avr73/latest
echo "$(date) post-fs-data on $(uname -a)" > $O/events.txt
cat /proc/modules > $O/modules-postfs.txt
# logd health (phone test 14: logcat returned nothing on 7.3)
{ $MODDIR/scmtest; echo "--- logcat -S"; logcat -S 2>&1 | head -40; ls -laZ /dev/socket/logd* ; grep -E "logd" /proc/net/unix; log -p e -t AVR73 "logger probe $(date)"; sleep 1; logcat -d -t 5 2>&1; echo "logcat rc=$?"; } > $O/logd-check.txt 2>&1
# continuous logs: survive the reboot that netd1shot-fail etc. trigger (init syncs on reboot)
nohup logcat -b all -v threadtime -f $O/logcat-stream.txt > /dev/null 2>&1 &
nohup sh -c "cat /proc/kmsg > $O/kmsg-stream.txt" > /dev/null 2>&1 &
for m in synx-driver msm_kgsl msm_drm; do
	[ -f $MODDIR/ko/$m.ko ] || continue
	grep -q "^${m//-/_} " /proc/modules && { echo "$m already loaded" >> $O/insmod.txt; continue; }
	insmod $MODDIR/ko/$m.ko >> $O/insmod.txt 2>&1
	echo "insmod $m rc=$?" >> $O/insmod.txt
done
dmesg > $O/dmesg-postfs.txt
# traced netd run (phone tests 13-16: netd1shot self-test fails -> reboot,netd1shot-fail;
# logd keeps nothing on 7.3, so record failing syscalls instead)
nohup sh -c "
	i=0; while [ \"\$(getprop bpf.progs_loaded)\" != 1 ] && [ \$i -lt 120 ]; do sleep 0.2; i=\$((i+1)); done
	$MODDIR/sctrace $O/netd-trace.txt 4 /system/bin/netd
	sync" > /dev/null 2>&1 &
# background logger: does not depend on the late_start (service.sh) stage being reached
nohup sh -c '
O='"$O"'
i=0
while [ $i -lt 60 ]; do
	dmesg > $O/dmesg.txt
	logcat -d -b all > $O/logcat.txt 2>&1
	getprop > $O/getprop.txt
	cat /proc/modules > $O/modules.txt
	ps -A -o PID,PPID,STAT,NAME > $O/ps.txt 2>&1
	{ ls -lR /config/usb_gadget 2>&1 | head -200; for f in /sys/class/udc/*/state /sys/class/udc/*/current_speed; do echo "$f: $(cat $f 2>&1)"; done; } > $O/usb.txt
	echo "$(date) tick $i boot_completed=$(getprop sys.boot_completed) init.svc.adbd=$(getprop init.svc.adbd) sys.usb.state=$(getprop sys.usb.state)" >> $O/events.txt
	sync
	i=$((i+1)); sleep 10
done' > /dev/null 2>&1 &
