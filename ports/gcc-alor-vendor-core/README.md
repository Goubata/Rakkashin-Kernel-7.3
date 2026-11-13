# Alor GCC migration candidate

Vendor source/headers from MotorolaMobilityLLC/kernel-msm commit
037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1, drivers/clk/qcom and
include/dt-bindings. Individual files retain original license notices.

This is a SoC object compilation probe using matching vendor clock headers.
The matching clock core implementations must also be ported and linked;
compiling gcc-alor.o alone does not establish runtime compatibility.

## First probe result

The SoC object stops on CLK_DONT_HOLD_STATE, absent from stock Android 7.3.
Motorola kernel-common's matching firmware branch defines bit 14 and implements
clock hold/sync-state behavior in drivers/clk/clk.c. Both the provider flag and
its common-core behavior must be ported together. No zero-valued compatibility
macro or feature removal has been used. Only headers actually used in this
probe are retained here.
