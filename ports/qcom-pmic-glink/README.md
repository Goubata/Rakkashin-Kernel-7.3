# Firmware PMIC GLINK experimental port

Pinned Motorola source with real native PDR/QMI/QRTR, RPMSG, SSR and IPC logging.
Per-client SRCU drains callbacks before release. A module-wide callback context
check rejects unregister from any GLINK callback (including cross-client waits).
Callers must stop concurrent TX and schedule unregister outside callbacks.
Public registration is restricted to consumer probe, links the consumer to its
bound provider and installs managed release; explicit unregister consumes that
same managed action before freeing the client.

Transport ingress uses module-lifetime SRCU. Remove unpublishes driver data,
waits for ingress, cancels init and drains RX. Supplier lookup retains devices;
locked binding/channel checks and managed links order transport/client removal
before provider teardown. Provider removes children while IDR/logging are live.

Thirteen actual-source software cases, six native QMI cases and 128 real local
QRTR packets pass. Real platform-driver bindings exercise dependency ordering
and managed release. The production module also registers/unregisters in QEMU.
The source fixture's callback-only cases call the internal registration body;
public registration/managed behavior is tested with actual bound platform
consumers. No physical firmware provider is emulated or claimed.

Stage29 includes this module experimentally (100 modules). Saved/candidate DT
channel names match both SOCCP channels and eight active child nodes. Native
7.3 has no qcom,alor-soccp-pas match: actual early-boot SOCCP/PAS handover, GLINK
SMEM transport, child charging/USB services, fault/stress tests and all physical
safety/boot acceptance remain incomplete. This is not a flashable boot image or
a daily-use kernel.
