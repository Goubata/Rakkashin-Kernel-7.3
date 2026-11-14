# In-progress Motorola UFS host port

Sources are pinned to qcom-soc `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`.
`python scripts/audit_ufs_sources.py` verifies ten files and replays the reviewed
patch. `python scripts/build_storage_deps.py --ufs-host` attempts the real build.
Currently C compilation passes, but final modpost fails on four missing real
providers: ipc_log_context_create, ipc_log_string, msm_minidump_enabled and
msm_minidump_add_region. No fake providers or CRC replacements are used. These
unfinished modules are excluded from production snapshots.

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
ESI CPU array bounds/hotplug, hook/notifier/sysfs/minidump lifetime, initialization
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
is real core I/O on an emulator, not execution of the unfinished Qualcomm host,
Canoe PHY training, real SHM/ICE operations, encrypted unlock or phone acceptance.
