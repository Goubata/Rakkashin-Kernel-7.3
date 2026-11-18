#!/system/bin/sh
# AVR 7.3 bring-up: only acts on the 7.3 test kernel
case "$(uname -r)" in 7.3.*) ;; *) exit 0 ;; esac
MODDIR=${0%/*}
O=/data/local/tmp/avr73/boot-$(date +%Y%m%d-%H%M%S)
mkdir -p $O; ln -sfn $O /data/local/tmp/avr73/latest
echo "$(date) post-fs-data on $(uname -a)" > $O/events.txt
cat /proc/modules > $O/modules-postfs.txt
for m in synx-driver msm_kgsl msm_drm; do
	[ -f $MODDIR/ko/$m.ko ] || continue
	grep -q "^${m//-/_} " /proc/modules && { echo "$m already loaded" >> $O/insmod.txt; continue; }
	insmod $MODDIR/ko/$m.ko >> $O/insmod.txt 2>&1
	echo "insmod $m rc=$?" >> $O/insmod.txt
done
dmesg > $O/dmesg-postfs.txt
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
