#!/usr/bin/env python3
"""Name the SMC behind a lost TZ waitq wakeup (bring-up diagnostic).

Phone test 7: 7.3 reached Android userspace, but one TZ call stayed asleep on waitq
ctx 1 from ~0.7 s for good. Report the SMC function id, the waiting task and (once,
at 30 s) its stack, so the caller can be identified on the next boot.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
D = 'drivers/firmware/qcom/'


def edit(rel, old, new):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old, new))


edit(D + 'qcom_scm.h', 'int qcom_scm_wait_for_wq_completion(struct device *dev, u32 wq_ctx);\n',
     'int qcom_scm_wait_for_wq_completion(struct device *dev, u32 wq_ctx, u64 fn_id);\n')
edit(D + 'qcom_scm-smc.c', '''	u32 wq_ctx, smc_call_ctx;
	struct arm_smccc_args resume;
	struct arm_smccc_args *smc = waitq;
''', '''	u32 wq_ctx, smc_call_ctx;
	struct arm_smccc_args resume;
	struct arm_smccc_args *smc = waitq;
	u64 fn_id = waitq->args[0];	/* ANDROID: diagnostic */
''')
edit(D + 'qcom_scm-smc.c', '			ret = qcom_scm_wait_for_wq_completion(dev, wq_ctx);\n',
     '			ret = qcom_scm_wait_for_wq_completion(dev, wq_ctx, fn_id);\n')
edit(D + 'qcom_scm.c', '''int qcom_scm_wait_for_wq_completion(struct device *dev, u32 wq_ctx)
{''', '''int qcom_scm_wait_for_wq_completion(struct device *dev, u32 wq_ctx, u64 fn_id)
{''')
edit(D + 'qcom_scm.c', '''		dev_warn(scm->dev, "TZ waitq ctx %u: still asleep after %us\\n", wq_ctx, waited);
	}''', '''		dev_warn(scm->dev, "TZ waitq ctx %u: still asleep after %us (smc 0x%llx, %s/%d)\\n",
			 wq_ctx, waited, fn_id, current->comm, task_pid_nr(current));
		if (waited == 30)
			dump_stack();
	}''')
print('applied waitq diagnostics to', SRC)
