#!/usr/bin/env python3
"""android16-6.12 vendor qcom-scm "multi SMC" for QTEE object calls.

Phone test 4 (2026-11-18): with the vendor WAITQ semantics in place, a QTEE call
from another thread slept in TZ (waitq ctx 1) from ~0.7 s and was never woken, and
first-stage init blocked behind it in memory_dump_v2 (shmbridge registration):
7.3 holds the global qcom_scm_lock across the whole call, including the WAITQ
sleep, so no other SMC can enter TZ - but TZ parks a QTEE invoke until another
call it depends on has run (that call then returns WAITQ_WAKE). The vendor driver
(qcom-soc qcom_scm-smc.c __scm_smc_do) releases the global lock for whitelisted
calls (desc.multicall_allowed: smcinvoke invoke/legacy/callback response, doorbell
ack, qseecom) once TZ reports WAITQ queue info, bounding concurrency with a
semaphore of TZ's call-context count. Restored here for the 7.3 equivalents:
qcom_scm_invoke_smc{,_legacy}, qcom_scm_invoke_callback_response,
qcom_scm_qtee_invoke_smc, qcom_scm_qtee_callback_response and QSEECOM calls.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
D = 'drivers/firmware/qcom/'


def edit(rel, old, new, count=1):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != count:
        raise SystemExit(f'{rel}: anchor count {s.count(old)} != {count}: {old[:70]!r}')
    p.write_text(s.replace(old, new))


H = D + 'qcom_scm.h'
edit(H, '''	u64 args[MAX_QCOM_SCM_ARGS];
	u32 owner;
};''', '''	u64 args[MAX_QCOM_SCM_ARGS];
	u32 owner;
	bool multicall_allowed;	/* ANDROID: 6.12 vendor multi-SMC whitelist */
};''')
edit(H, 'int scm_get_wq_ctx(u32 *wq_ctx, u32 *flags, u32 *more_pending);\n',
     'int scm_get_wq_ctx(u32 *wq_ctx, u32 *flags, u32 *more_pending);\n'
     'void qcom_scm_smc_enable_multicall(unsigned int call_ctx_cnt);\n')

S = D + 'qcom_scm-smc.c'
edit(S, 'static DEFINE_MUTEX(qcom_scm_lock);\n', '''static DEFINE_MUTEX(qcom_scm_lock);
/* ANDROID: 6.12 vendor: TZ call contexts; whitelisted calls skip qcom_scm_lock */
static struct semaphore qcom_scm_sem = __SEMAPHORE_INITIALIZER(qcom_scm_sem, 1);
static bool qcom_scm_multicall;

void qcom_scm_smc_enable_multicall(unsigned int call_ctx_cnt)
{
	unsigned int i;

	if (call_ctx_cnt <= 1 || qcom_scm_multicall)
		return;
	for (i = 1; i < call_ctx_cnt; i++)
		up(&qcom_scm_sem);
	qcom_scm_multicall = true;
}
''')
edit(S, '''static int __scm_smc_do(struct device *dev, struct arm_smccc_args *smc,
			struct arm_smccc_res *res, bool atomic, bool retry_busy)
{''', '''static int __scm_smc_do(struct device *dev, struct arm_smccc_args *smc,
			struct arm_smccc_res *res, bool atomic, bool retry_busy,
			bool multicall)
{''')
edit(S, '''		mutex_lock(&qcom_scm_lock);

		ret = __scm_smc_do_quirk_handle_waitq(dev, smc, res);

		mutex_unlock(&qcom_scm_lock);''', '''		bool unlocked = multicall && READ_ONCE(qcom_scm_multicall);

		if (!unlocked)
			mutex_lock(&qcom_scm_lock);
		down(&qcom_scm_sem);

		ret = __scm_smc_do_quirk_handle_waitq(dev, smc, res);

		up(&qcom_scm_sem);
		if (!unlocked)
			mutex_unlock(&qcom_scm_lock);''')
edit(S, '	ret = __scm_smc_do(dev, &smc, &smc_res, atomic, retry_busy);\n',
     '	ret = __scm_smc_do(dev, &smc, &smc_res, atomic, retry_busy, desc->multicall_allowed);\n')
s = (SRC / S).read_text()
if '#include <linux/semaphore.h>' not in s:
    s = s.replace('#include <linux/mutex.h>\n', '#include <linux/mutex.h>\n#include <linux/semaphore.h>\n', 1)
    (SRC / S).write_text(s)

C = D + 'qcom_scm.c'
c = (SRC / C).read_text()
# whitelist: the SMCINVOKE descriptors (vendor invoke/legacy/callback and 7.3 qtee ones)
n = c.count('\t\t.svc = QCOM_SCM_SVC_SMCINVOKE,\n')
if '.multicall_allowed = true,	/* ANDROID: vendor whitelist */' not in c:
    c = c.replace('\t\t.svc = QCOM_SCM_SVC_SMCINVOKE,\n',
                  '\t\t.svc = QCOM_SCM_SVC_SMCINVOKE,\n\t\t.multicall_allowed = true,	/* ANDROID: vendor whitelist */\n')
    c = c.replace('''	lockdep_assert_held(&qcom_scm_qseecom_call_lock);

	status = qcom_scm_call(__scm->dev, desc, &scm_res);''', '''	lockdep_assert_held(&qcom_scm_qseecom_call_lock);

	{	/* ANDROID: 6.12 vendor whitelist (qcom_scm_qseecom_call) */
		struct qcom_scm_desc d = *desc;

		d.multicall_allowed = true;
		status = qcom_scm_call(__scm->dev, &d, &scm_res);
	}''', 1)
    old = '''	for (i = 0; i < scm->wq_cnt; i++)
		init_completion(&scm->waitq_comps[i]);
'''
    assert c.count(old) == 1
    (SRC / C).write_text(c)
print(f'multicall whitelist on {n} SMCINVOKE descriptors')
edit(C, '''	} else {
		ret = devm_request_threaded_irq(scm->dev, irq, NULL, qcom_scm_irq_handler,
						IRQF_ONESHOT, "qcom-scm", scm);
		if (ret < 0)
			return dev_err_probe(scm->dev, ret,
					     "Failed to request qcom-scm irq\\n");
	}''', '''	} else {
		ret = devm_request_threaded_irq(scm->dev, irq, NULL, qcom_scm_irq_handler,
						IRQF_ONESHOT, "qcom-scm", scm);
		if (ret < 0)
			return dev_err_probe(scm->dev, ret,
					     "Failed to request qcom-scm irq\\n");
		/* ANDROID: 6.12 vendor: waitq IRQ + TZ queue info -> multi-SMC */
		qcom_scm_smc_enable_multicall(scm->wq_cnt);
		dev_info(scm->dev, "multi-SMC for QTEE calls, %u TZ call contexts\\n", scm->wq_cnt);
	}''')
print('applied', SRC)
