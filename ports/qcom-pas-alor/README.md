# Alor PAS integration draft

Pinned native base `4dd306a07b2f456fd70ddaa28f4f65640374f202`; PAS, MDT loader
and PIL info sources are staged unchanged, with an owned external Makefile.
`scripts/audit_pas_alor_sources.py` verifies exact inputs and replay.

The baseline three modules link against real Q6V5, sysmon, common, SMEM and
native secure PAS providers. API headers select the actual providers explicitly.
This draft deliberately has no Alor compatible binding, since the cx/mx ARC
votes, rollback, ELF64 minidump metadata and early-attach cleanup are not yet
adapted/tested. It is excluded from the Stage37 108-module artifact.

Build result and CRC evidence: `reports/pas-alor-stage38-baseline-build.log`,
`reports/pas-alor-stage38-baseline-crc.json` and
`reports/pas-alor-stage38-baseline-validation.json`. Compilation/link/CRC does
not establish firmware image acceptance, secure ABI, physical rail behavior,
probe safety or daily-use functionality. No firmware was booted or partition
written. The actual board inputs remain in the Stage36/37 prerequisites reports.
