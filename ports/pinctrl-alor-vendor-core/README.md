# Alor vendor pinctrl core migration candidate

GPL-2.0-only source initially copied from MotorolaMobilityLLC/kernel-msm,
commit 037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1, drivers/pinctrl/qcom/.

The vendor SoC driver requires wake_reg/wake_bit and qup_regs fields absent
from the Android 7.3 pinctrl-msm header. This alternative carries its matching
vendor core, preserving those mechanisms for API migration. It must not be
loaded alongside another pinctrl-msm implementation exporting the same symbols.

Candidate only: no runtime validation. Compare compilation diagnostics with
../pinctrl-alor before choosing the final integration method.

## Compilation result

Both pinctrl-alor.o and pinctrl-msm.o compiled against Android 7.3 on the
avr arm64 host. Subsequently, pinctrl-msm.ko and pinctrl-alor.ko linked successfully after enabling the upstream SCM/PAS/TZMEM dependencies. Neither has been loaded on hardware.

Changes are in compat-7.3.patch: PIN_CONFIG_LEVEL naming, GPIO set return value,
valid-line query API, syscore registration API, and exported debug prototypes.
Vendor wakeup and QUP mechanisms are retained. The legacy global hibernation
state and registration/unregistration lifetime still require review before
runtime use. No feature was removed to silence a compile error.
