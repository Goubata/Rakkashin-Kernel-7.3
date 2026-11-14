# Canoe UFS PHY on 7.3

These are five unmodified source files from Motorola qcom-soc commit
`037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`, matched to the saved AVR firmware
release. The driver matches the saved `/soc/ufsphy_mem@1d80000` compatible
`qcom,ufs-phy-qmp-v4-canoe`. The native 7.3 QMP UFS match table does not contain
that compatible. Calibration tables, lane setup, regulator/reset handling and
board tuning are retained as supplied, without inventing register values.

After preparing the native kernel build and Ubuntu bind mounts:

```sh
python scripts/build_storage_deps.py
python scripts/run_qemu.py --test storage-phy --cpus 4 --log reports/storage-phy.log
```

The build command verifies every source against its pinned Git blob before
compiling the two modules. The isolated guest inserts both modules, checks
`/sys/module`, removes them in reverse order and checks their absence. It has
no matching UFS hardware, so this does not exercise probe, PHY calibration,
training, suspend or storage access. The phone kernel never loads these files.

Stage13 contains 73 modules with 3,700 import CRCs consistent and hard/soft
module dependencies present. UFS host adaptation, ICE/hardware-wrapped keys,
interconnect/regulator/IOMMU integration, encrypted unlock and physical
acceptance remain required. Native `CONFIG_SCSI_UFS_QCOM` is currently disabled;
the two PHY modules alone cannot provide a working Android storage stack.
