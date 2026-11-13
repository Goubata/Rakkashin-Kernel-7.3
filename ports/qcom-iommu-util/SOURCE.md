Motorola kernel-msm 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1, drivers/iommu/modules.bzl qcom_iommu_util.
FAST and LPAE engines are both included, matching the actual firmware configuration.
Vendor-only public headers are staged; target 7.3 IOMMU, DMA, IOVA and page-table
framework structures remain authoritative. The module now compiles and loads in an isolated guest. map_phys dispatch keeps
RAM and MMIO mapping semantics separate. Page-table allocations use the target
IOMMU allocator with its memory accounting. FAST/LPAE translation, partial unmap,
remap and a contiguous-page-group case passed software tests. Physical SMMU
registration, DMA coherency/TLB maintenance and secure mappings remain unverified.
Missing providers must be ported.
