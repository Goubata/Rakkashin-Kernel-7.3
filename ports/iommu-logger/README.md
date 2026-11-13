# IOMMU logger

Source: Motorola kernel-msm 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1,
drivers/iommu/iommu-logger.c. Uses the shared vendor logger header from
../arm-smmu-vendor. Actual FAST/LPAE table layouts are retained.

Device and group references now remain held for the lifetime of a registration.
Unregister drops them. The module links and loads/unloads in isolated QEMU;
physical device registrations and crashdump inspection remain unverified.
