# AVR 7.3 kernel port — instructions for Claude

- Read `docs/HANDOFF-PC.md` first (current state, phone state, how to test, open items).
- Rules: reply in Japanese / never make sound / no partition writes, flashing or `fastboot boot` without the user's permission / never call something "done" before it works on the phone / keep moving autonomously.
- Commits: `git -c user.name=Codex -c user.email=codex@localhost commit` + Co-Authored-By.
- Sources: `../avr-upstream-audit/android-7.3` (core patches are committed there and kept as diffs in `patches/*.patch` and `reports/core-patch-checkpoint.json`).
