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

Next provider/transport review constraint: native rpmsg_core.c calls the driver's
remove callback BEFORE destroying the default endpoint. Clearing transport state
or merely flushing RX work inside remove cannot by itself exclude an already
running or newly arriving RX callback. The planned fix must first unpublish the
rpdev driver-data association and synchronize ingress readers, then drain work.
Link the rpmsg consumer to a bound provider under its device lock to order
provider unbind. Unpublish global registry entries and cancel init work before
children/notifier/queue/IPC destruction. Add actual-source detach/late-RX and
provider/consumer removal tests before including this module in an artifact.
