# Firmware PMIC GLINK work in progress

Pinned Motorola source with basic 7.3 input/API fixes. It compiles against real
native PDR/QMI/QRTR, RPMSG, existing SSR and IPC logging. It is deliberately not
included in hardware snapshots or accepted as a charging driver.

Review must first resolve RX client lookup versus unregister, state callback
reentry, global provider/rpmsg references and teardown ordering (init work,
notifiers, RX queue, child devices, IPC context and IDR). Actual-code lifecycle
and framing tests, live transport, remote service and battery/charging clients
are still required. The current native IPC test scope is separate.
