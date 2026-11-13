Motorola kernel-msm 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1,
drivers/iommu/arm/arm-smmu/modules.bzl ARM_SMMU + ARM_SMMU_QCOM sources.
Private controller structures are compiled together; public core IOMMU/DMA
structures remain from 7.3. The avr_adreno_smmu_priv extension is shared with
KGSL explicitly, not overlaid on the upstream drm/msm layout. FAST and LPAE
remain selected; bypass is disabled by default. Hardware operation unverified.
