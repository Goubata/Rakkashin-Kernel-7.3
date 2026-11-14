# Alor interconnect port for Android 7.3

Motorola SoC source: `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`.
Private five-bucket binding header: Motorola common
`479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4`.

Real Alor topology, BCM/CRM/RPMh voting, QoS and diagnostic implementations build
as five modules. No fake hardware provider substitutes for missing code.
`audit_interconnect_sources.py` replays 13 source/Makefile inputs, including the
unmodified common header. The owned Makefile forces that header before the
native three-bucket include guard for every member of this private module ABI.

Scaling leaves framework aggregates untouched and uses 64-bit intermediates.
Rate/commit/BCM/link/debug errors propagate. BCM address publication follows full
aux-data/capacity validation. Managed clocks are not double-released, children
leave before their parent provider, and debug provider walks hold their list
lock. Dentry lookup references and trace unregister are balanced.

Nine actual aggregation/xlate cases and module/trace control tests pass in SMP4
QEMU, with no matching NoC device. Original Alor and BCM drivers lack module_exit
and remain resident. Other libraries' unload/reload is tested before Alor loads.

Open: actual MMIO/RPMh/CRM acknowledgements, QoS error propagation and vote
rollback, static BCM probe/removal state, voter lifetime races, concurrent request
list and trace control access, and full physical workload/power acceptance.

`audit_saved_interconnect.py` separately inspects the saved 6.12 ELF, without
executing it. `build_interconnect_dt.py` creates a one-property USB/IPA candidate
repair; `audit_interconnect_dt.py` validates candidate indices and retains saved
DT errors in its output. The candidate is never flashed or packaged as ready.
