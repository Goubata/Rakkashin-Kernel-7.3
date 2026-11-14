# Motorola regulator port for 7.3

Pinned source: `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`.
Three actual vendor drivers build: RPMh voting, regulator diagnostic control,
and the firmware's existing logical regulator. Logical supplies must never be
used as replacements for missing physical RPMh providers.

`audit_regulators_sources.py` replays all three files and the owned Makefile.
Headers/providers are the reviewed clock, native regulator, RPMh and IPC ports.
The matching saved ELF exports do not contain optional suspend-debug APIs.

OF references, managed child population, proxy registration failure, IPC
rollback/exit ordering and diagnostic-file lifetime are corrected. Only owned
debugfs files are removed; native regulator directories remain core owned.
Custom voltage access uses the full debugfs proxy, and file operations pin the
module. Lazy diagnostic consumer creation is serialized. A held consumer pins
the provider until device unbind; no forced unload is used.

`run_qemu.py --test regulators` builds an isolated QEMU DT from its own machine
and adds `tests/regulators/qemu-rail.dtso`. That synthetic logical rail is never
applied to AVR device DTs. Five actual framework/driver cases plus retained-fd
unbind/read/rebind/module-reference teardown pass. Physical RPMh voltage votes,
firmware acknowledgement/error recovery, suspend and concurrent teardown still
need acceptance. The Stage17 snapshot is partial, not flashable.
