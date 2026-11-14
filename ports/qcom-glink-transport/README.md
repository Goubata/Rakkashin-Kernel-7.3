# Native GLINK SMEM experimental transport

Six files pinned to Android common 7.3 base 4dd306a07b2f456fd70ddaa28f4f65640374f202
build real qcom_glink, qcom_glink_smem and rpmsg_ctrl modules. The private RPMSG
interface is from the same native tree. Explicit external-module flags enable
real declarations; core GKI GLINK remains disabled to avoid duplicate providers.
Remoteproc common imports all three real GLINK register/unregister/SSR APIs.

Tests compile actual native and SMEM sources together on guarded RAM. They cover
wrapping, padding, reserve preservation, invalid accesses, size overflow,
VERSION/ACK wire bytes, real RPMSG control binding and a blocked-response removal.
Doorbell recording is test-only. Pre-fix suites reproduced invalid index handling
and a response worker preventing remove. Abort/wake now precedes RX cancellation.
Eleven transport cases and 31 related regression cases pass on SMP4.

Saved/candidate edge values match after resolving renumbered phandles to provider
paths; five active edges, PID19 within the staged SMEM host range, descriptor/FIFO
IDs 478/479/480 and 15 common command IDs are covered. This does not prove a real
partition, coherency, IRQ/mailbox delivery or firmware compatibility. Vendor
zero-copy (command10/feature bit3) remains unimplemented and is not advertised.
The negotiation test verifies it is excluded even when a peer offers it.

Physical SoCCP/PAS, IPCC and SMP2P producers, complete channel/intent/data/SSR
stress, vendor extensions, charging/USB services and whole-phone acceptance
remain pending. Stage32 is experimental (103 modules), not a flashable boot image.
