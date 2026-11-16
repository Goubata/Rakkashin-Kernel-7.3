#!/bin/busybox sh
# Diagnostic: policy0 walt, policy2 schedutil, sustained mixed load (boot-time ordering case).
B=/bin/busybox
i=0; while [ ! -d /proc/sys/walt ] && [ $i -lt 100 ]; do $B usleep 100000; i=$((i+1)); done
echo walt > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor
echo walt > /sys/devices/system/cpu/cpufreq/policy2/scaling_governor
for k in 1 2 3 4 5 6; do $B sh -c 'while :; do :; done' & $B sh -c 'while :; do $B usleep 300; done' & done
$B taskset 4 $B sh -c 'while :; do :; done' &
$B sleep 20
$B dmesg | $B grep -q "WARNING:\|BUG:\|Oops" && { echo AVR_WALT_MIXED_FAIL; $B poweroff -f; }
echo AVR_WALT_MIXED_OK
