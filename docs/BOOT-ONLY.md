# 7.3 working with boot and vendor_boot changes only (no avr73-support needed)

Goal: make 7.3 fully usable with just two image writes, boot.img and vendor_boot.img. No KernelSU module, and no changes to /system, /vendor or /data.

## Where each part of avr73-support moved

| Runtime work in avr73-support | Boot-only replacement | Where |
|---|---|---|
| insmod synx/kgsl/msm_drm/camera/eva in post-fs-data | Autoload in the vendor_boot first stage (the deferral was a bring-up workaround for synx failures and DRM bridge bugs, both now fixed). Kernel parameters such as `msm_drm.dsi_display0=` on the cmdline are passed by libmodprobe | kit (`--defer` now only coresight funnel/replicator) |
| `setprop vendor.all.modules.ready 1` | Module loading returns `-EEXIST` for an already-loaded name before the version check. libmodprobe counts the 6.12 copies tried by vendor_modprobe.sh as loaded, so the script sets the property itself. The KSU built-in rc also sets it as a safety net | `kernel/module/main.c`, KSU rc |
| bind-mount the 7.3 goodix_fod_mmi/anc_fps_mmi | Load only the 7.3 anc_fps_mmi (jiiov, the sensor on this phone) in the first stage (`--extra`). Loading goodix_fod_mmi as well makes the jiiov HAL fail, and the script then rmmods both (2026-11-19). The fingerprint script's insmod of the 6.12 copies gets EEXIST and is ignored; the HAL binds to the already-loaded 7.3 driver | kit + the EEXIST change above |
| insmod adios/ntsync + chmod 0666 | First-stage load (`--extra`); `chmod 0666 /dev/ntsync` in the KSU built-in rc | kit, KSU rc |
| netbpfload.rc 27Q2 (bind mount) | Only when the netbpfload process reads `/system/etc/init/netbpfload.rc` at offset 0, `# 2026 2 37 0 0` is shown as `# 2027 2 38 0 0` (same length). Nothing on disk changes | `fs/read_write.c` |
| Turn off OPLUS logd flowctrl | **Not needed.** On the stable 7.3 (2026-11-19) it reaches flowctrl ready=1 with it on, and logs are kept. Dropped logs only happened during bring-up | none |
| Stop boot_monitor_service | `on property:init.svc.boot_monitor_service=running → stop` in the KSU built-in rc (same as now; can be removed if it doesn't misfire on the stable 7.3) | KSU rc |

Every change is in the 7.3 kernel itself, so booting 6.12 (dual vendor_boot) behaves as before.
Core changes: `scripts/apply_avr73_boot_only_core.py`.

## Build (on the phone, latest tree)
```
AVR_KERNEL_SRC=$PWD/kernel BUILD_JOBS=8 python3 scripts/build_production.py --kernel
AVR_KERNEL_SRC=$PWD/kernel BUILD_JOBS=8 python3 scripts/build_production.py --ports
python3 scripts/assemble_boot_kit.py --tree prod --set full \
    --defer coresight_funnel,coresight_replicator \
    --extra anc_fps_mmi,adios,ntsync --tag prod-bootonly
python3 scripts/check_boot_kit.py prod-bootonly --tree prod
python3 scripts/assemble_vendor_boot_dual.py prod-bootonly
```
The boot image adds `sched_proxy_exec=0` to the cmdline (same as the final build).
