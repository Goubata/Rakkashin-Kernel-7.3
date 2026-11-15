# AVR TSENS v2 draft

Seven active saved AVR controllers use `qcom,tsens-v2`, with 49 declared sensors
and 49 matching thermal zones. Use the pinned Android common 7.3 TSENS driver as
an external module. The separate factory `msm-tsens` / `tsens26xx` driver does
not match this DT and is not included. No compatible strings or IRQ/resource
assignments were changed to force a match.

Local changes: external include path, sensor-count bounds, and the factory's
204000 mC generic-v2 threshold limit (native default 120000 mC). Actual DT trip
values stay unchanged. Factory reference bytes and source patch replay are
locked. No firmware calibration or new temperature limits are invented.

Build: `python scripts/build_tsens.py --tests`.
DT audit: `python scripts/audit_tsens_dt.py`.
Guest tests: `python scripts/run_qemu.py --test tsens --cpus 4 --timeout 120 --log reports/tsens-stage78-qemu.log`.

Eight private-RAM regmap cases exercise exact production source, all 4096 signed
temperature codes, all 16 hardware slots, valid/stable/unstable samples, read
errors, factory range and IRQ mask/threshold decoding. Production platform
driver registration/unregistration is checked separately in the guest. No real
TSENS probe, calibration, IRQ, critical protection, firmware or suspend proof is
claimed. Register-error handling in setters, complete probe/unbind lifetime and
factory persistent-temperature diagnostics remain pending. Not promoted into
the Stage57 main bundle. Do not treat the draft as safe physical thermal control.

Stage79 adds threshold-window register error propagation. Mask both boundaries
before updating, propagate the first failed operation and try to mask both on
failure. Eleven guest cases pass, including each of 29 transient register fault
positions, manual callback retry and permanent register failure. These tests use
a real tripless thermal-zone object but invoke the production callback directly.
Native thermal core caches the requested limits before calling set_trips and
will not automatically retry identical limits after failure. Framework-level
recovery is still required; no automatic recovery or atomic hardware rollback
is claimed. A permanent bus fault also prevents guaranteed masking.

Stage80 supplies a required updated Image with thermal-window-retry-7.3.patch.
The native core now caches only successful windows and invalidates its cache on
failure, allowing the next framework request to retry. Fourteen cases pass on
that Image, including direct calls to the running core's real window function
under a native zone lock. The old Image reproduces the skipped-retry failure.
This fixes the cache issue described above; it does not schedule an autonomous
retry or prove recovery when an interrupt-only zone receives no further request.
No native public type layout or exported CRC changes were introduced.
