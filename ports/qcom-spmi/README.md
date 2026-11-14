# Canoe/V8 SPMI and PMIC port for 7.3

Source pin: `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`.
`audit_spmi_sources.py` verifies/replays six vendor source/header inputs and the
owned Makefile. Real V1–V8 operations and hardware tables are preserved. Native
SPMI/regmap core code is used; no replacement hardware provider is invented.

Managed actions enforce debugfs, controller/children, IRQ and OF reference order,
including failed probe. Arbiter publication is last and unpublication first;
mapping calls hold the same mutex, with balanced platform-device/node references.
The core's globally unique controller name is retained. Debug controller handles
ERR_PTR allocation failures. PMIC metadata lives through managed child teardown.

QTI owns explicit dentry references and drains custom data operations through the
full debugfs proxy before freeing private data. Manual managed unregister matches
the actual registered devres release wrapper, removes its record, then invokes
the unlocked release under its already-held mutex. This fixes the original no-op
without introducing recursive locking. Diagnostic data writes remain disabled,
consistent with the saved module's read symbol and absent write implementation.

QEMU tests are explicitly RAM regmap + recording native-SPMI framework tests,
not execution of physical arbiter MMIO, APID routing or PMIC transactions.
Actual QTI diagnostics and retained descriptor removal run against that RAM map.
Five KUnit cases and all four module unloads pass. Test doubles stay in tests/.

The strict DT checker still reports the original duplicate SID4. Candidate DT
only preserves saved-running child order (observed current sysfs selection),
without choosing a silicon identity or deleting a competing node. Node/property
bytes, reserve map and GPU/memory settings are preserved. No packaging approval.

Open: physical V8 APID/EE/secure access, transactions/IRQs, mapping consumer links,
concurrent cleanup, metadata getter lifetime, QTI cache/dump control concurrency,
duplicate-node resolution and all real-device acceptance. Native enumeration is
16 SID values; the selected DT uses max SID13, not proof of arbitrary eSPMI.
