#!/system/bin/sh
# AVR 7.3 bring-up: only acts on the 7.3 test kernel
case "$(uname -r)" in 7.3.*) ;; *) exit 0 ;; esac
MODDIR=${0%/*}
O=/data/local/tmp/avr73/boot-$(date +%Y%m%d-%H%M%S)
mkdir -p $O; ln -sfn $O /data/local/tmp/avr73/latest
echo "$(date) post-fs-data on $(uname -a)" > $O/events.txt
cat /proc/modules > $O/modules-postfs.txt
# deferred modules (kit --defer): load in dependency order, record each result
for m in synx-driver msm_kgsl msm_drm; do
	[ -f $MODDIR/ko/$m.ko ] || continue
	insmod $MODDIR/ko/$m.ko >> $O/insmod.txt 2>&1
	echo "insmod $m rc=$?" >> $O/insmod.txt
done
dmesg > $O/dmesg-postfs.txt
