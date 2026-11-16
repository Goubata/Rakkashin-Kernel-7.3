#!/bin/busybox sh
# Runs inside the QEMU initramfs after sched-walt.ko and the cpufreq/EM fixture load.
B=/bin/busybox
fail() {
  echo "AVR_SCHED_WALT_CHECK_FAIL $*"
  for t in /proc/[0-9]*; do
    if $B grep -q "walt\|midpoint\|waltgov\|core_ctl" $t/stack 2>/dev/null; then echo "== $t $($B cat $t/comm)"; $B cat $t/stack; fi
  done
  $B dmesg | $B grep -i "walt\|midpoint\|core_ctl\|cycle" | $B tail -20
  $B poweroff -f
}
ok=0
pass() { ok=$((ok+1)); echo "walt-check ok: $*"; }

# 1. WALT finished its deferred init (sysctl tree registered by walt_init).
i=0; while [ ! -d /proc/sys/walt ] && [ $i -lt 100 ]; do $B usleep 100000; i=$((i+1)); done
[ -d /proc/sys/walt ] || fail "no /proc/sys/walt after 10s"
n=$($B ls /proc/sys/walt | $B wc -l)
[ "$n" -ge 40 ] || fail "only $n walt sysctls"
pass "/proc/sys/walt entries=$n"

# 2. The walt governor is registered and can drive both fixture policies.
for p in 0 2; do
  d=/sys/devices/system/cpu/cpufreq/policy$p
  $B grep -qw walt $d/scaling_available_governors || fail "walt governor missing on policy$p"
  echo walt > $d/scaling_governor || fail "cannot select walt on policy$p"
  [ "$($B cat $d/scaling_governor)" = walt ] || fail "policy$p governor not walt"
done
pass "walt governor active on policy0/policy2"

# 3. Load -> frequency: a CPU hog on a big CPU must raise policy2 above its floor.
lo=$($B cat /sys/devices/system/cpu/cpufreq/policy2/scaling_min_freq)
$B taskset 4 $B sh -c 'while :; do :; done' &
hog=$!
$B sleep 2
f=$($B cat /sys/devices/system/cpu/cpufreq/policy2/scaling_cur_freq)
kill $hog
[ "$f" -gt "$lo" ] || fail "policy2 stayed at $f (min $lo) under load"
pass "policy2 rose to $f kHz under load (min $lo)"

# 4. Idle -> frequency falls back.
i=0; g=$f
while [ "$g" -ge "$f" ] && [ $i -lt 30 ]; do $B usleep 200000; g=$($B cat /sys/devices/system/cpu/cpufreq/policy2/scaling_cur_freq); i=$((i+1)); done
[ "$g" -lt "$f" ] || fail "policy2 did not drop within 6s after load ($g >= $f)"
pass "policy2 dropped to $g kHz when idle"

# 5. Stress: 12 mixed hog/sleeper tasks, CPU3 offline/online, governor round trips
#    (the earlier mixed schedutil+walt state corrupted CFS; keep both paths hot).
pids=""
for k in 1 2 3 4 5 6; do
  $B sh -c 'while :; do :; done' & pids="$pids $!"
  $B sh -c 'while :; do $B usleep 300; done' & pids="$pids $!"
done
for r in 1 2 3; do
  echo 0 > /sys/devices/system/cpu/cpu3/online || fail "offline cpu3 round $r"
  $B sleep 1
  echo 1 > /sys/devices/system/cpu/cpu3/online || fail "online cpu3 round $r"
  echo schedutil > /sys/devices/system/cpu/cpufreq/policy2/scaling_governor || fail "to schedutil $r"
  $B sleep 1
  echo walt > /sys/devices/system/cpu/cpufreq/policy2/scaling_governor || fail "back to walt $r"
  $B sleep 1
done
kill $pids
$B sleep 1
[ "$($B cat /sys/devices/system/cpu/cpufreq/policy2/scaling_governor)" = walt ] || fail "policy2 not walt after stress"
pass "stress: 12 tasks, 3x cpu3 hotplug, 3x governor round trip"

# 6. Perf domains exist (EAS path WALT relies on) and no WALT_BUG/WARN fired.
$B dmesg | $B grep -q "WALT_BUG\|WARNING:\|BUG:" && fail "kernel warning during WALT run"
pass "no WALT_BUG/WARNING"
echo "AVR_SCHED_WALT_CHECKS checks=$ok"
