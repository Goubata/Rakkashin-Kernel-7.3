# Motorola PMIC GPIO/MPP port for Android 7.3

Motorola SoC source: `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`.
Binding inputs are pinned in `reports/pmic-pinctrl-source-inputs.json`.
Native GPIO direction reference: `4dd306a07b2f456fd70ddaa28f4f65640374f202`.
The source auditor replays five files and compares the two direction functions
exactly with that native source. Vendor binding constants are forced before
native include guards to preserve the private driver/header interface.

The real drivers use PIN_CONFIG_LEVEL, propagate GPIO-set errors, bound physical
pin IDs, validate pin counts, defer missing parent regmap, inherit can_sleep and
use the target remove API. Native direction semantics disable output on input,
and keep input sensing on open-drain/source outputs. Sampling a line preserves
the requested output latch.

Seven isolated tests directly compile GPIO callback source with platform-driver
registration disabled and use RAM regmap/real gpiolib. Actual production modules
separately load/unload without a matching PMIC. Neither is physical probe or
SPMI/electrical/IRQ evidence. Test code is excluded from hardware snapshots.
MPP is an optional reference port: no MPP provider is selected in the target DT,
and its callback/probe behavior is not tested by the GPIO suite.

Open: physical subtype/valid pin holes/voltage/pull/strength, concurrent cached
state, partial-write recovery, parent/IRQ/suspend lifetime and real-device tests.
Original duplicate SID4 and whole-board DT/ROM acceptance remain unresolved.
