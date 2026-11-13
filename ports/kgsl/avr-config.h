/* GPU-local Motorola canoe perf configuration; target core config remains authoritative. */
#define CONFIG_QCOM_ADRENO_DEFAULT_GOVERNOR "msm-adreno-tz"
#define CONFIG_QCOM_KGSL_MODULE 1
#define CONFIG_QCOM_KGSL_CONTEXT_DEBUG 1
#define CONFIG_QCOM_KGSL_HYBRID_ALLOCATION 1
#define CONFIG_QCOM_KGSL_IDLE_TIMEOUT 80
#define CONFIG_QCOM_KGSL_IOCOHERENCY_DEFAULT 1
#define CONFIG_QCOM_KGSL_PROCESS_RECLAIM 1
#define CONFIG_QCOM_KGSL_RT_MUTEX 1
#define CONFIG_QCOM_KGSL_SORT_POOL 1
#define CONFIG_QCOM_KGSL_SYNX 1
#define CONFIG_QCOM_KGSL_USE_SHMEM 1
#define CONFIG_QCOM_KGSL_CORESIGHT 1
#define CONFIG_QCOM_KGSL_POOL 1

/* Real vendor SoC identification module must be ported and linked. */
#define CONFIG_QCOM_SOCINFO_MODULE 1
#define CONFIG_QCOM_MEM_BUF_MODULE 1
#define CONFIG_MSM_SYSSTATS_MODULE 1
/* The matching vendor SMMU must be built, not its disabled fallback. */
#define CONFIG_ARM_SMMU_MODULE 1
/* Functional dependencies from Motorola canoe perf. Require real providers;
 * disabled header fallbacks would silently lose secure memory and cache/DCVS. */
#define CONFIG_QCOM_SECURE_BUFFER_MODULE 1
#define CONFIG_QCOM_LLCC_MODULE 1
#define CONFIG_QCOM_DCVS_MODULE 1
/* Match the firmware's external governor modules; they must be loaded first. */
#define CONFIG_DEVFREQ_GOV_QCOM_ADRENO_TZ_MODULE 1
#define CONFIG_DEVFREQ_GOV_QCOM_GPUBW_MON_MODULE 1
#define CONFIG_DEVFREQ_GOV_QCOM_ADRENO_RO_MODULE 1

/* Match the actual vendor io-pgtable engine configuration. */
#define CONFIG_IOMMU_IO_PGTABLE_FAST 1
#define CONFIG_IOMMU_IO_PGTABLE_CONTIG_HINT 1
