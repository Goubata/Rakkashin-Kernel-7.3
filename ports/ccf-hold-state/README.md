# Clock hold-state extension for Android 7.3

The Alor clock providers require Motorola's boot-clock hold/sync-state behavior,
including CLK_DONT_HOLD_STATE (bit 14). The original implementation was read
from MotorolaMobilityLLC/kernel-common, branch
android-16-release-w3wwl36.65-11-18, drivers/clk/clk.c.

hold-state-7.3.patch carries that implementation into the pinned Android 7.3
core, rather than treating the vendor flag as a no-op. The dev_has_sync_state
helper became private to driver core in 7.3; the staged clock implementation
uses equivalent local logic with READ_ONCE and public device/driver headers.

prepare.py regenerates the staged source from the source snapshots in the
adjacent avr-upstream-audit directory. It never edits the actively building
Android 7.3 tree. clk.c and compat.h are generated compilation-probe inputs.

The staged built-in clk.o compiled successfully on the avr Ubuntu arm64 host.
The corresponding Alor gcc-alor.o compiled successfully with its header.
These are object compilation checks only. The patch is not yet applied to the
baseline kernel, the vendor clock support library is not yet fully ported, and
boot-clock retention/release behavior has not been tested on hardware.
