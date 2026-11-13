# Vendor ARM SMMU on 7.3

Source: Motorola kernel-msm W3WWL36.65-11-18,
037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1,
drivers/iommu/arm/arm-smmu (five translation units from modules.bzl).

The port retains Qualcomm hardware behavior and uses actual target IOMMU types.
Adaptations: 3-argument attach callbacks, firmware-spec ownership by IOMMU core,
per-device page-size mask, remove callback, local trace headers, private function
prototypes and explicit shared avr_adreno_smmu_priv GPU extension. The old
pre-attach enable_nesting operation is represented by NEST_PARENT allocation;
S2/nesting hardware is required rather than silently falling back to S1.
No nested-child allocator or additional paging flags are advertised.

Dependencies: qcom_iommu_util, SoCinfo and iommu_logger plus native firmware.
The .ko links and loads/unloads on isolated QEMU without a Qualcomm SMMU.
Controller probe, stream routing, TLB/cache maintenance, protected contexts,
GPU fault recovery and suspend/resume are unverified on avr.
