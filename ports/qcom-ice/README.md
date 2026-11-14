# Motorola ICE V2 port

Source is pinned to qcom-soc `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`.
`python scripts/audit_ice_sources.py` verifies original/staged hashes and replays
`port-7.3.patch`. `python scripts/build_storage_deps.py --ice --tests` builds it
against the configured core; `python scripts/run_qemu.py --test ice --cpus 4
--log reports/ice.log` runs isolated guest tests.

The real vendor ICE implementation retains register and HWKM programming,
version-dependent slot doubling and the 2.0 offset of 10. It uses new SCM V2
adapters: ES set 0x05 with six arguments, clear 0x06 with two, derive 0x07 with
four. The target TZMEM pool marshals and wipes buffers, fixing the vendor
wrapped-key error path that returned before releasing its shared allocation.
V2 calls remain standard/sleepable, preserve WAITQ handling and do not replay
EBUSY. Existing SCM callers keep their previous retry policy.

The target blk_crypto_key uses `bytes`. HWKM V1 module configuration is not
selected on Canoe. Invalid wrapped-key requests and unready HWKM fail before
MMIO. Slot arithmetic rejects negative/overflowing values. BIST failures return
errors rather than silently changing the requested HW-wrapped mode to raw.
Cold initialization/resume readiness and clock rollback were reviewed, but
actual register/clock/reset behavior is untested. The vendor eviction assumption
that uninitialized HWKM means a reset-cleared keytable remains a physical-test
requirement, as does the lifetime of the DT supplier/device link.

SHM Bridge is selected from the pinned canoe_perf.bzl. Its real TZMEM code is
linked into qcom-scm.ko to avoid a cycle between two separate modules. Build and
staging use Kbuild's modules.order, so old unselected qcom_tzmem.ko files cannot
enter artifacts. Native ICE must not also export the same functions.

Ten KUnit cases execute production descriptor builders, slot/readiness/retry
helpers and exported no-SCM/error guards. The real ICE module loads and unloads
on the guest, which has no matching hardware. No test programs a real key,
registers real SHM, touches phone storage or proves existing encrypted data can
be unlocked. UFS host/core and Android crypto profile integration remain open.
