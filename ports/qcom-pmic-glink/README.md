# Firmware PMIC GLINK work in progress

Pinned Motorola source with real native PDR/QMI/QRTR, RPMSG, existing SSR and IPC
logging. Client RX/state callbacks use per-client SRCU and a callback mutex;
unregister removes publication then waits for callbacks. Self-unregister returns
-EDEADLK; clients must schedule removal elsewhere and stop concurrent TX first.

Six actual-source software-fixture cases pass: framing/input validation, dispatch
and state, duplicate owner rejection, self-unregister from RX/state, and external
unregister waiting for a blocked RX callback. Native QMI/QRTR tests run alongside.
No firmware endpoint or physical provider is used by these tests.

This driver remains excluded from hardware artifacts until global provider/rpmsg
references, unbind serialization and removal ordering (init work, notifiers, RX
queue, children, IPC and IDR) are resolved and tested. Live GLINK transport,
remote services and battery/charging clients are still missing.
