# Rakkashin KGSL 7.3 port in progress

Input: kgsl-avr-1.0-source.zip, SHA256 0b65d98f38556912f5fd486535f455b580287c178197fd07cc22b0448c656e1f.
Official graphics reference: 61a6d039282d94bd2ea2f908cef580e0d1006686, W3WWL36.65-11-18.
Vendor-only headers: SoC 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1;
Gunyah core headers and PERF_MODE tag: Motorola common 479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4.

The ZIP's busy_update_ms, adrenoboost and adrenoboost_stats ABI and tuning app
remain present. The original ZIP and patches remain intact.

## Build status

All GPU C files compiled and msm_kgsl.o linked (kgsl-attempt23.log). This is a
relocatable object, not a usable msm_kgsl.ko. Modpost fails with 11 actual missing
provider symbols: reports/kgsl-link-dependencies.json. No unresolved-symbol
suppression or stock CRC reconstruction is used.

Only GPU-local configuration and actual functional dependency choices are used,
not the ZIP's whole 6.12 vendor_autoconf.h. Secure-buffer, LLCC and DCVS are
explicitly enabled to prevent disabled header fallbacks from quietly omitting
protected memory and cache/frequency control. Their real providers must be ported.

## Migrated interfaces

- Timer callbacks, private functions, DMA coherence flags, unmapped-area API,
  scatterlist page iteration, const bin_attribute callbacks and Coresight path API.
- Fence locks, 64-bit initialization, sequence comparison, array constructor and
  local diagnostics replacing removed value_str callbacks. Fence lifecycle and
  concurrent access still need deeper review and hardware stress tests.
- shmem_file_setup uses vma_flags_t. The hybrid allocation/reclaim path stays on.
- A new sleepable shmem folio provider uses SRCU. Unregister drains callbacks;
  registration failures propagate and KGSL tears it down before freeing pools.
  Nonblocking GFP requests use normal shmem, and the driver preserves NOIO/NOFS.
  Driver-owned inode data is zeroed by core inode initialization. Returned folios
  still pass through core memcg charging, page-cache and LRU accounting.
- Reclaim walks folios with references, isolates batches and uses the 7.3
  reclaimer. The old RCU-only xarray walk did not meet folio_isolate_lru's reference
  contract. CPU mappings use exported zap_special_vma_range; all KGSL mapping
  ops set PFNMAP or MIXEDMAP. Reclaimed large-folio reads use the actual file index.
- Cooling registration uses ID 0; the actual avr GPU node has #cooling-cells=2,
  so the 7.3 legacy thermal mapping format applies. Thermal handling stays active.
- GPU register initialization and ordinary SMMU aperture calls use matching
  existing 7.3 SCM APIs. LPAC/tuning and nine legacy DCVS methods now have actual vendor SCM implementations
  in the firmware module. Shutdown retry/secure backend integration remains unresolved.

## Evidence and remaining work

RAM-only QEMU SMP4 passed 35 clock/RCU/memory cases. The six memory cases cover
provider supply/accounting, fallback, unrelated inode isolation, unregister,
in-flight sleeping callback drain, and actual shmem swap-out to guest zram plus
data read-back. This tests the core memory path, not KGSL GPU DMA on avr.

Ported support providers: SoC info, SMEM, hwspinlock, LLCC/DCVS, three external
devfreq governors, and qcom_iommu_util (FAST and LPAE). Actual external governors
are selected using firmware configuration and MODULE_SOFTDEP, preserving their
registration-before-GPU order. Eight missing symbols now belong to shutdown retry and SYNX.
The actual vendor ARM SMMU, logger and secure-buffer drivers now link on 7.3;
no physical controller probe or secure assignment has been verified.

The three software page-table cases passed in SMP4 QEMU: FAST mapping/partial
unmap/remap, LPAE equivalent, and partial unmap of a 16-page contiguous group.
These check actual provider translation tables with emulated TLB callbacks;
they do not verify device DMA, cache coherency or physical TLB maintenance.
Fifteen support modules loaded in QEMU; nine exit-capable drivers unloaded and
six permanent firmware/RPMh dependencies remained as designed.

Still required: mem-buf and the complete secure firmware backend, physical
SMMU validation and SYNX, plus broader driver lifecycle/bounds review. The vendor SMMU
must use the same avr_adreno_smmu_priv extension as KGSL; it cannot be cast to the
upstream drm/msm private layout. The secure firmware backend (including the
vendor SMCI/PIL path) requires review against the device firmware.

The original Boost code still needs thermal-cap revalidation, successful-GMU-call
tracking, delayed expiry and concurrency review. Reusing an old boost floor can
currently outrun a newly lowered cap. No physical boot, GPU reset/suspend test,
protected-content check or efficiency measurement has passed yet.

Stage5 adds ARM SMMU, IOMMU logger and secure-buffer: 49 built modules with
1,912 matching import CRCs. The SMP4 module smoke test loaded 19, unloaded 12
and retained seven permanent providers. Five secure-buffer KUnit cases execute
the production batching and rollback code with a mocked SCM transport: invalid
input, first-call failure, multi-destination rollback, bounded-prefix ownership
flags and full batches. No actual page ownership is transferred by these tests.

Stage6 adds actual msm_sysstats with SRCU callback draining and live memory
counters: 50 modules, all 1,980 import CRCs match. Four KUnit cases and real
userspace Netlink memory/task/invalid-request checks pass in a diskless SMP4
guest. The initial Netlink crash from absent family policy is fixed and its
failure log retained. Twenty module loads, thirteen unloads, seven permanent
providers retained. Physical GPU/ROM statistics remain unverified.
