# Rakkashin KGSL 7.3 port in progress

Input: kgsl-avr-1.0-source.zip, SHA256 0b65d98f38556912f5fd486535f455b580287c178197fd07cc22b0448c656e1f.
Official reference: Motorola graphics-kernel 61a6d039282d94bd2ea2f908cef580e0d1006686, matching W3WWL36.65-11-18.
Source comparison: reports/kgsl-source-comparison.json. Preserve busy_update_ms, adrenoboost and adrenoboost_stats ABI for the supplied app.

Use only GPU-local configuration in avr-config.h, compile against actual 7.3 headers and link against real 7.3 dependency Module.symvers. The supplied old full vendor_autoconf.h and stock CRC reconstruction scripts are reference inputs, not used for this port. No installer is run.

Status: staged, API migration and dependency builds pending; not yet a usable 7.3 GPU driver. Thermal cap, concurrency and suspend behavior still need review and hardware measurements.

## API migration status

The staged source retains the original custom busy_update_ms / Adreno Boost
implementation and its UAPI. Older timer and DMA coherence accessors, unmapped
area API, fence locking, sequence initialization and removed fence debug methods
have been adapted. These changes do not yet produce a linked module.

Blocking areas from kgsl-attempt12.log:
- Restore the hybrid shmem allocation path using a real 7.3 hook/context design;
  shmem_inode_info.android_vendor_data1 and android_rvh_shmem_get_folio were removed.
  Do not disable shmem/reclaim to obtain a superficial successful build.
- Convert shmem_file_setup to the target vma_flags_t API.
- Register the correct cooling-device ID with the new thermal API.
- Port the actual vendor SMMU / io-pgtable, ICC performance tags, mem-buf,
  Gunyah, sysstats and SCM GPU tuning providers, then link with their real symbols.
- Shared vendor SMMU extension now has its own avr_adreno_smmu_priv name; the
  future vendor SMMU provider must use precisely the same definition. It is
  incompatible with the upstream drm/msm private structure and must not be cast
  to a generic upstream provider.

GPU policy review still required: in the supplied high-load branch, retaining
an older boost floor after computing the thermal cap can choose a faster floor
than a newly lowered cap. The worker must revalidate limits when applying a
request and handle failed GMU requests; delayed expiry and concurrent state
access also require review. These are open correctness issues, not measured
performance improvements. The input archive and original patch remain intact.

Header provenance: vendor-only additions come from SoC 037aa1c6c; Gunyah core
headers and the PERF_MODE tag originate in Motorola common 479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4.
