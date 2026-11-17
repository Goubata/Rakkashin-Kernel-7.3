#!/bin/sh
# Rebuild every port and test module against the current out/native-gki (run as root).
# Includes nested tests/qcom-tee/fake-scm (regenerated from the 7.3 qcom_scm sources).
cd "$(dirname "$0")/.." || exit 1
fail=0
for p in ports/*/; do
	[ "$p" = ports/pinctrl-alor/ ] && continue	# built by build_native with the vendor core
	[ -f "$p/Makefile" ] || [ -f "$p/Kbuild" ] || continue
	python3 scripts/stage_tool.py build "${p%/}" >/dev/null 2>&1 || { echo "BUILD FAIL $p"; fail=1; }
done
python3 tests/qcom-tee/prepare_fake_scm.py >/dev/null || { echo "FAKE-SCM PREP FAIL"; fail=1; }
python3 scripts/stage_tool.py build tests/qcom-tee/fake-scm >/dev/null 2>&1 || { echo "BUILD FAIL fake-scm"; fail=1; }
for t in tests/*/; do
	[ -f "$t/Makefile" ] || continue
	python3 scripts/stage_tool.py build "${t%/}" >/dev/null 2>&1 || { echo "TBUILD FAIL $t"; fail=1; }
done
exit $fail
