# In-progress Motorola UFS host port

Sources are pinned to qcom-soc `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`.
`python scripts/audit_ufs_sources.py` verifies ten files and replays the reviewed
patch. `python scripts/build_storage_deps.py --ufs-host` attempts the real build.
Both real host modules now link with the ported SMEM minidump and IPC logging
providers and are included in the Stage16 partial snapshot. The four previously
missing exports are implemented by actual pinned source, not success stubs.
QEMU confirms module loading/unloading without a matching Qualcomm device;
physical UFS probe, encrypted unlock and daily use remain unverified.

The variant keeps its actual MCQ resource metadata in private host storage.
It uses target scsi_cmnd/request CPU/tag information, negotiate_pwr_mode,
config_scsi_dev, MSI locking, thermal provider ID, platform removal and core
DME operations. HID support reads the native device-descriptor-derived bit.
The old post-VCC-off delay maps to the target's actual vcc_off_delay_us field,
retaining the 5ms requirement for matching devices. The target UFS core now
records real last-IRQ metadata, based on pinned common vendor behavior.

The real QTI wrapped-key crypto profile uses the target bytes field and
CUSTOM_CRYPTO_PROFILE contract, propagates initialization errors and retains
raw programming in the profile module for non-HWKM requests. The old nullable
program_key variant-op slot no longer exists in 7.3. Firmware/ICE integration
and encrypted-data compatibility still require physical validation.

Source review remains open for mapped-resource bounds and insertion cleanup,
ESI CPU array bounds/hotplug, notifier/sysfs/minidump lifetime, initialization
rollback, clock/PHY/power transitions, error recovery and actual I/O.

Separately, the native PCI UFS core is tested using QEMU UFS and private regular
backing files (no phone block device). Build/run:

```sh
python scripts/build_storage_deps.py --ufs-core-tests
python scripts/run_qemu.py --test ufs --cpus 4 --log reports/ufs-sq.log
python scripts/run_qemu.py --test ufs --ufs-mcq --cpus 4 --log reports/ufs-mcq.log
```

Each mode writes and directly reads two distinct 4MiB patterns, flushes, then
checks controller state/IRQ bookkeeping/queue mode with three KUnit cases. The
runner independently checks the final 4MiB pattern in the backing file. This
is real core I/O on an emulator, not Qualcomm host hardware execution,
Canoe PHY training, real SHM/ICE operations, encrypted unlock or phone acceptance.

Trace callbacks are now scoped to their host, checked on registration, unwound
on partial failure and synchronized on teardown. Broader removal cleanup and
IRQ/CPU hotplug ownership still require review. Diagnostic source locks/replay
are checked by `scripts/audit_diagnostics_sources.py`.
