# OxygenOS software fences on Android 7.3

Source: Goubata/avr-oxygenos-dsu-port a7142e82df011d607010b439b9c1cfd7828c076c,
project/tools/avrcompat/odm-v42/avr_oplus_sync.c (GPL-2.0-only).
The docs mention kmod/oplus_sync, but that path is absent in this checkout.

Preserved /dev/oplus_sync, misc node permissions and original ioctl layout.
In particular the existing ROM module puts fence_fd before pad in GET_DEADLINE;
this differs from upstream sw_sync. Do not silently switch these fields.

7.3 changes use the fence lock helpers, extern_lock and updated sequence comparison.
A timeline pins the module until all its fences are released; module exit waits
for an RCU grace period so that fence callbacks cannot execute unloaded text.

Build: scripts/build_native.py --boot-core, or the normal external module make
against out/native-gki. Do not load into the running phone's 6.12 kernel.

An isolated RAM-only QEMU SMP2 guest passed 1283 userspace assertions, including
fence signal/poll/status, independent opens, duplicate sequence values, bad ioctl
arguments, timeline cancellation, 32-bit sequence wrap, and refusing module unload
while a fence remains. This does not verify the actual ROM services or SELinux.
No reverse engineering was needed for this module because source was provided.
