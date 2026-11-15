# Alor PAS integration draft

Native base `4dd306a07b2f456fd70ddaa28f4f65640374f202`. Source/reviewed patch
replay: `scripts/audit_pas_alor_sources.py`. PIL info remains a pinned original. PAS has an Alor descriptor and owned
counted ARC rail helper; MDT has reviewed input/metadata/arithmetic checks.
This draft is excluded from the Stage37 108-module artifact.

Alor values match pinned vendor `037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1`:
PAS51, DTB PAS65, crash SMEM656, minidump24, soccp.mbn/soccp_dtb.mbn, early boot,
auto boot and ELF64 dump metadata. `qcom,alor-soccp-pas` now selects that
actual descriptor, with no unrelated SoC power-domain alias. This makes a future
load on a matching board attempt early attach/start; do not confuse the draft
with a reviewed device boot artifact.

Regulator acquisition requires the saved cx/mx order and exact two-cell requests.
It uses real devm regulator APIs and preserves ARC level64/load0 as raw votes,
not physical 64 microvolts. It bypasses native optional cx/px and hardcoded cx
load for this target. Enable tracks successful voltage/load/enable operations;
release reverses owned operations, stops on a cleanup failure and retains flags
for retry. A dirty state refuses another enable. The original failure and a
rollback failure remain separately visible. A mutex serializes state transitions.
Start unwind, handover and removal call the ARC release helper.

Build: `python scripts/build_storage_deps.py --pas-rails-tests`.
Guest: `python scripts/run_qemu.py --test pas-rails --cpus 4 --log reports/pas-rails-qemu.log`.
Eight cases compile the exact rail helper with test-only recording regulator
calls. They cover six forward/six release failures, retry/idempotence, simultaneous
primary/rollback failures, positive load return values, invalid state, duplicate
enables and two concurrent workers. These are not real regulator framework or
hardware tests. The real PAS/MDT/PIL modules separately register/unregister in
a hardware-free guest. No matching Alor device is probed there.

`audit_alor_pas_dt.py` checks eleven pinned descriptor values and selected saved/
candidate rail, firmware and memory-region inputs. Actual memory role/firmware
loader acceptance and secure ABI still require validation. Remaining blockers:
clock/IRQ/handover lifetime and DTB/main authentication unwind; real regulator
provider/failure handling; resource-table/ELF64 loading and early attach; real
remote services, suspend and whole-board boot/ROM/policy acceptance. Cleanup
failure recovery is software-tested only; a physical hardware failure cannot be
assumed recoverable. No firmware boot, partition write or daily-use claim.

Evidence: `reports/pas-alor-stage39-validation.json`.


Stage41–42 runs the real exported MDT library against the exact mounted-file
captures (also matched against both FAT slots). `tests/mdt-loader` compares each
entire 6MiB/256KiB destination with expected PT_LOAD bytes, zeroed BSS and
untouched gaps, plus 32-byte guards. Metadata header/hash bytes match the original
firmware. No authenticate/reset/PAS boot API is invoked.

A padded-backing reproducer shows the old metadata reader copying beyond the
advertised firmware length. Header/table/file bounds now reject it before copy.
The ELF32 parser explicitly rejects incompatible class/byte order, null inputs
and missing output length; address/file additions are widened before arithmetic.
The final seven loader cases and eight rail regressions pass on SMP4. This does
not validate secure-world authentication, physical loading, split images or all
malformed firmware. Evidence: `reports/soccp-loader-stage42-validation.json`.


Stage43 adds `avr_pas_proxy.h` for counted PDS/clocks/optional supplies/ARC
ownership, serializes handover against partial acquire/unwind, and defers early
handover until successful main authentication return. Incomplete release keeps
its owned bit for retry; duplicate handover/stop cannot double-disable. Eight
recorded-callback cases and 15 regressions pass. This is not yet secure main/DTB
ownership or a complete driver lifecycle/physical test.
