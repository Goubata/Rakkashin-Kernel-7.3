#!/system/bin/sh
case "$(uname -r)" in 7.3.*) ;; *) exit 0 ;; esac
O=$(readlink /data/local/tmp/avr73/latest)
[ -d "$O" ] || exit 0
i=0
while [ $i -lt 40 ]; do
	dmesg > $O/dmesg.txt
	logcat -d -b all > $O/logcat.txt 2>&1
	getprop > $O/getprop.txt
	cat /proc/modules > $O/modules.txt
	ps -A -o PID,PPID,STAT,NAME > $O/ps.txt 2>&1
	{ ls -lR /config/usb_gadget 2>&1 | head -200; echo; for f in /sys/class/udc/*/state /sys/class/udc/*/current_speed; do echo "$f: $(cat $f 2>&1)"; done; cat /config/usb_gadget/g1/UDC 2>&1; } > $O/usb.txt
	echo "$(date) tick $i boot_completed=$(getprop sys.boot_completed)" >> $O/events.txt
	sync
	i=$((i+1)); sleep 15
done
