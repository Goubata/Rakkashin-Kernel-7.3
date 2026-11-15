# AVR hardware RNG draft

The saved/candidate node /soc/rng@10c3000 uses qcom,trng, matching the pinned
native Android common 7.3 driver. Its resource covers data/status registers;
there is no core clock reference, supported by native optional-clock handling.
Selected DT bytes/targets are preserved. No DT node or clock is invented.

Honor the hwrng nonblocking request flag, avoid all MMIO on zero-length reads,
return already copied bytes when a later FIFO wait fails, and clamp size_t
requests by INT_MAX before converting to the int-returning private helper.
Native entropy quality/registration and optional clock APIs are otherwise kept.

Reproduce build_rng.py --tests, audit_rng_dt.py and QEMU mode rng. Eleven guest
cases compile the complete production source with only driver registration and
MMIO access replaced in the test module. Use native polling/delays and a native
fixed-rate CCF clock. Cover buffer tails 1..65, unaligned outputs, guards, empty/
partial/delayed FIFO, timeout, zero word data, oversized request and clock balance.
The production module registers/unregisters separately without a matching guest
hardware node. No synthetic hwrng provider is registered, and no recorded data
is credited to kernel entropy. These tests do not prove actual entropy health,
quality, firmware enablement, real MMIO or physical probe/suspend/remove.

Artifact is a separate draft and is not promoted into the Stage57 main bundle.
