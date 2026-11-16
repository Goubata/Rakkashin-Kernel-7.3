#!/bin/busybox sh
# Real sched_walt halt path driven by the ported thermal_pause cooling devices.
B=/bin/busybox
fail() {
  echo "AVR_THERMAL_PAUSE_CHECK_FAIL $*"
  for p in $hogs; do echo "hog $p cpu=$($B awk '{print $39}' /proc/$p/stat) allowed=$($B grep Cpus_allowed_list /proc/$p/status)"; done
  for d in /sys/class/thermal/cooling_device*; do echo "$($B cat $d/type) cur=$($B cat $d/cur_state)"; done
  $B dmesg | $B grep -iE "pause|halt|walt|WARN|BUG|drain" | $B tail -30; $B poweroff -f; }
ok=0; pass() { ok=$((ok+1)); echo "pause-check ok: $*"; }
cdev() { for d in /sys/class/thermal/cooling_device*; do [ "$($B cat $d/type)" = "$1" ] && { echo $d; return; }; done; }
on_cpu() { for p in $hogs; do $B awk '{print $39}' /proc/$p/stat; done | $B grep -c "^$1\$"; }
wait_none() { i=0; while [ $i -lt 30 ]; do n=0; for c in "$@"; do n=$((n+$(on_cpu $c))); done; [ $n -eq 0 ] && return 0; $B usleep 200000; i=$((i+1)); done; return 1; }
wait_some() { i=0; while [ $i -lt 40 ]; do [ "$(on_cpu $1)" -gt 0 ] && return 0; $B usleep 200000; i=$((i+1)); done; return 1; }

[ "$($B od -An -tx1 /proc/device-tree/cpus/cpu@3/phandle | $B tr -d ' \n')" = "00008001" ] || fail "QEMU cpu@3 phandle drifted"
i=0; while [ ! -d /proc/sys/walt ] && [ $i -lt 100 ]; do $B usleep 100000; i=$((i+1)); done
[ -d /proc/sys/walt ] || fail "walt not initialised"
for p in 0 2; do echo walt > /sys/devices/system/cpu/cpufreq/policy$p/scaling_governor; done
i=0; while [ -z "$(cdev thermal-pause-8)" ] && [ $i -lt 50 ]; do $B usleep 100000; i=$((i+1)); done
for t in thermal-pause-4 thermal-pause-8 thermal-pause-C pause-cpu1; do
  d=$(cdev $t); [ -n "$d" ] || fail "cdev $t missing"
  [ "$($B cat $d/max_state)" = 1 ] || fail "$t max_state"
done
pass "4 cdevs (alias, default thermal-pause-C, pause-cpu1) max_state 1"

hogs=""; for k in 1 2 3 4 5 6; do $B sh -c 'while :; do :; done' & hogs="$hogs $!"; done
wait_some 3 || fail "hogs never reached cpu3 before pausing"
c8=$(cdev thermal-pause-8)
$B mkdir -p /sys/kernel/tracing; $B mount -t tracefs nodev /sys/kernel/tracing 2>/dev/null
T=/sys/kernel/tracing
for e in $T/events/schedwalt/halt_cpus $T/events/schedwalt/halt_cpus_start; do echo 1 > $e/enable 2>/dev/null; echo stacktrace > $e/trigger 2>/dev/null; done
echo 1 > $c8/cur_state; $B sleep 1
# DIAG: who moves tasks onto halted cpu3 after the drain?
echo 'stacktrace if dest_cpu==3' > $T/events/sched/sched_migrate_task/trigger 2>/dev/null
echo 1 > $T/events/sched/sched_migrate_task/enable 2>/dev/null
wait_none 3 || { echo 0 > $T/events/sched/sched_migrate_task/enable; $B grep -B2 -A22 "halt_cpus" $T/trace | $B head -120; $B grep -A25 "dest_cpu=3" $T/trace | $B head -30; fail "tasks still on paused cpu3"; }
echo 0 > $T/events/sched/sched_migrate_task/enable 2>/dev/null
[ "$($B cat $c8/cur_state)" = 1 ] || fail "cur_state not 1"
pass "pausing cpu3 drains it (6 hogs)"
echo 0 > $c8/cur_state; wait_some 3 || fail "cpu3 not used after resume"
pass "resume returns cpu3"

cg=$(cdev thermal-pause-C)
echo 1 > $cg/cur_state; wait_none 2 3 || fail "big cluster not drained by group pause"
echo 1 > $c8/cur_state; echo 0 > $cg/cur_state; $B sleep 1
wait_none 3 || fail "cpu3 lost pause when overlapping group resumed"
wait_some 2 || fail "cpu2 not resumed after group release"
echo 0 > $c8/cur_state; wait_some 3 || fail "cpu3 not resumed at end of overlap"
pass "group pause and overlapping single pause keep correct ownership"

for r in $($B seq 1 30); do echo $((r%2)) > $c8/cur_state; echo $(((r+1)%2)) > $(cdev thermal-pause-4)/cur_state; done
echo 0 > $c8/cur_state; echo 0 > $(cdev thermal-pause-4)/cur_state; $B sleep 2
wait_some 3 || fail "cpu3 not usable after toggles"; wait_some 2 || fail "cpu2 not usable after toggles"
pass "60 rapid toggles converge to running"

echo 60000 > /sys/module/avr_tp_sensor/parameters/temp 2>/dev/null
$B sleep 2; wait_none 3 || fail "thermal zone map did not pause cpu3"
echo 20000 > /sys/module/avr_tp_sensor/parameters/temp; $B sleep 2
wait_some 3 || fail "cpu3 not resumed after cooling"
pass "DT cooling map pauses/resumes cpu3 via step_wise"

echo 1 > $c8/cur_state; wait_none 3 || fail "pause before unload"
$B rmmod thermal_pause || fail "rmmod thermal_pause"
wait_some 3 || fail "cpu3 stayed paused after driver unload"
pass "unload while paused resumes cpu3"
kill $hogs
$B dmesg | $B grep -q "WARNING:\|BUG:\|Oops" && fail "kernel warning"
pass "no WARN/BUG"
echo "AVR_THERMAL_PAUSE_CHECKS checks=$ok"
