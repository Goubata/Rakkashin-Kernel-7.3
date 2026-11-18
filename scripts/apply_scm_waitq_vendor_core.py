#!/usr/bin/env python3
"""android16-6.12 (vendor qcom-scm) WAITQ semantics for the 7.3 qcom_scm.

Phone test 3 (2026-11-18): 7.3 hung for good in first-stage init inside
memory_dump_v2 probe, right after its first TZ calls, without any warning:
7.3 waits for a TZ waitqueue with wait_for_completion_state(TASK_IDLE) (no hung-task
report) and understands only part of the protocol the AVR TZ speaks. The vendor
driver the phone runs (qcom-soc drivers/firmware/qcom/qcom_scm{,-smc}.c):
  - a standard call may return QCOM_SCM_WAITQ_WAKE (3): wake the waiter of wq_ctx
    (a1) per flags (a3: WAKE_ONE / WAKE_ALL), then ACK with WAITQ_ACK(smc_call_ctx a2)
    and continue; 7.3 treats WAKE as a final result, the sleeper never wakes;
  - wq_ctx values are arbitrary ids (IDR, completion allocated on first use);
    7.3 indexes a fixed array 0..wq_cnt-1;
  - the waitq IRQ may report WAKE_ALL or WAKE_NONE (spurious double wakeup), 7.3
    rejects both.
This restores those semantics. In addition (diagnostic, not in 6.12), a waiter
logs every 10 s it stays asleep, naming the ctx, so a lost wakeup is visible.
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
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:70]!r}')
    p.write_text(s.replace(old, new))


H = D + 'qcom_scm.h'
edit(H, '#define QCOM_SCM_WAITQ_RESUME			0x02\n',
     '#define QCOM_SCM_WAITQ_ACK			0x01	/* ANDROID: vendor WAKE ack */\n#define QCOM_SCM_WAITQ_RESUME			0x02\n')
edit(H, '#define QCOM_SCM_WAITQ_SLEEP	2\n', '#define QCOM_SCM_WAITQ_SLEEP	2\n#define QCOM_SCM_WAITQ_WAKE	3	/* ANDROID: vendor protocol */\n')
edit(H, 'int qcom_scm_wait_for_wq_completion(struct device *dev, u32 wq_ctx);\n',
     'int qcom_scm_wait_for_wq_completion(struct device *dev, u32 wq_ctx);\n'
     'int qcom_scm_waitq_wake_flags(struct device *dev, u32 wq_ctx, u32 flags);\n')

S = D + 'qcom_scm-smc.c'
edit(S, '''int scm_get_wq_ctx(u32 *wq_ctx, u32 *flags, u32 *more_pending)
{''', '''/* ANDROID: vendor protocol: acknowledge a QCOM_SCM_WAITQ_WAKE and resume */
static void fill_wq_wake_ack_args(struct arm_smccc_args *ack, u32 smc_call_ctx)
{
	memset(ack->args, 0, sizeof(ack->args[0]) * ARRAY_SIZE(ack->args));

	ack->args[0] = ARM_SMCCC_CALL_VAL(ARM_SMCCC_STD_CALL,
					ARM_SMCCC_SMC_64, ARM_SMCCC_OWNER_SIP,
					SCM_SMC_FNID(QCOM_SCM_SVC_WAITQ, QCOM_SCM_WAITQ_ACK));

	ack->args[1] = QCOM_SCM_ARGS(1);

	ack->args[2] = smc_call_ctx;
}

int scm_get_wq_ctx(u32 *wq_ctx, u32 *flags, u32 *more_pending)
{''')
edit(S, '''	int ret;
	u32 wq_ctx, smc_call_ctx;
	struct arm_smccc_args resume;
	struct arm_smccc_args *smc = waitq;

	do {
		__scm_smc_do_quirk(smc, res);

		if (res->a0 == QCOM_SCM_WAITQ_SLEEP) {
			wq_ctx = res->a1;
			smc_call_ctx = res->a2;

			trace_scm_waitq_sleep(wq_ctx, smc_call_ctx);
			ret = qcom_scm_wait_for_wq_completion(dev, wq_ctx);
			if (ret)
				return ret;

			trace_scm_waitq_resume(smc_call_ctx);
			fill_wq_resume_args(&resume, smc_call_ctx);
			smc = &resume;
		}
	} while (res->a0 == QCOM_SCM_WAITQ_SLEEP);

	return 0;''', '''	int ret;
	u32 wq_ctx, smc_call_ctx;
	struct arm_smccc_args resume;
	struct arm_smccc_args *smc = waitq;

	do {
		__scm_smc_do_quirk(smc, res);

		if (res->a0 == QCOM_SCM_WAITQ_SLEEP) {
			wq_ctx = res->a1;
			smc_call_ctx = res->a2;

			trace_scm_waitq_sleep(wq_ctx, smc_call_ctx);
			ret = qcom_scm_wait_for_wq_completion(dev, wq_ctx);
			if (ret)
				return ret;

			trace_scm_waitq_resume(smc_call_ctx);
			fill_wq_resume_args(&resume, smc_call_ctx);
			smc = &resume;
		} else if (res->a0 == QCOM_SCM_WAITQ_WAKE) {
			/* ANDROID: 6.12 vendor protocol: wake wq_ctx, ack, continue */
			wq_ctx = res->a1;
			smc_call_ctx = res->a2;

			ret = qcom_scm_waitq_wake_flags(dev, wq_ctx, res->a3);
			if (ret)
				return ret;

			fill_wq_wake_ack_args(&resume, smc_call_ctx);
			smc = &resume;
		}
	} while (res->a0 == QCOM_SCM_WAITQ_SLEEP || res->a0 == QCOM_SCM_WAITQ_WAKE);

	return 0;''')

C = D + 'qcom_scm.c'
edit(C, '''	struct qcom_tzmem_pool *mempool;
	unsigned int wq_cnt;''', '''	struct qcom_tzmem_pool *mempool;
	unsigned int wq_cnt;
	/* ANDROID: vendor waitq ids are arbitrary: completions by id, made on first use */
	struct xarray waitq_xa;''')
edit(C, '#define QCOM_SMC_WAITQ_FLAG_WAKE_ONE	BIT(0)\n',
     '#define QCOM_SMC_WAITQ_FLAG_WAKE_ONE	BIT(0)\n#define QCOM_SMC_WAITQ_FLAG_WAKE_ALL	BIT(1)	/* ANDROID: vendor */\n#define QCOM_SMC_WAITQ_FLAG_WAKE_NONE	0x0\n')
edit(C, '''static struct completion *qcom_scm_get_completion(struct qcom_scm *scm, u32 wq_ctx)
{
	if (WARN_ON_ONCE(wq_ctx >= scm->wq_cnt))
		return ERR_PTR(-EINVAL);

	return &scm->waitq_comps[wq_ctx];
}

int qcom_scm_wait_for_wq_completion(struct device *dev, u32 wq_ctx)
{
	struct qcom_scm *scm = dev_get_drvdata(dev);
	struct completion *wq;

	wq = qcom_scm_get_completion(scm, wq_ctx);
	if (IS_ERR(wq))
		return PTR_ERR(wq);

	wait_for_completion_state(wq, TASK_IDLE);

	return 0;
}

static int qcom_scm_waitq_wakeup(struct qcom_scm *scm, unsigned int wq_ctx)
{
	struct completion *wq;

	wq = qcom_scm_get_completion(scm, wq_ctx);
	if (IS_ERR(wq))
		return PTR_ERR(wq);

	complete(wq);

	return 0;
}''', '''static struct completion *qcom_scm_get_completion(struct qcom_scm *scm, u32 wq_ctx)
{
	struct completion *wq, *old;

	if (wq_ctx < scm->wq_cnt)
		return &scm->waitq_comps[wq_ctx];

	/* ANDROID: 6.12 vendor: any id, completion allocated on first use */
	wq = xa_load(&scm->waitq_xa, wq_ctx);
	if (wq)
		return wq;
	wq = devm_kzalloc(scm->dev, sizeof(*wq), GFP_ATOMIC);
	if (!wq)
		return ERR_PTR(-ENOMEM);
	init_completion(wq);
	old = xa_cmpxchg(&scm->waitq_xa, wq_ctx, NULL, wq, GFP_ATOMIC);
	if (xa_is_err(old)) {
		devm_kfree(scm->dev, wq);
		return ERR_PTR(xa_err(old));
	}
	if (old) {
		devm_kfree(scm->dev, wq);
		return old;
	}
	return wq;
}

int qcom_scm_wait_for_wq_completion(struct device *dev, u32 wq_ctx)
{
	struct qcom_scm *scm = dev_get_drvdata(dev);
	struct completion *wq;
	unsigned int waited = 0;

	wq = qcom_scm_get_completion(scm, wq_ctx);
	if (IS_ERR(wq))
		return PTR_ERR(wq);

	/* ANDROID: uninterruptible like 6.12 vendor (TASK_IDLE hid hangs); report lost wakeups */
	while (!wait_for_completion_timeout(wq, 10 * HZ)) {
		waited += 10;
		dev_warn(scm->dev, "TZ waitq ctx %u: still asleep after %us\\n", wq_ctx, waited);
	}

	return 0;
}

static void qcom_scm_waitq_apply(struct completion *wq, u32 flags)
{
	if (flags == QCOM_SMC_WAITQ_FLAG_WAKE_ALL) {
		complete_all(wq);
		reinit_completion(wq);
	} else {
		complete(wq);
	}
}

int qcom_scm_waitq_wake_flags(struct device *dev, u32 wq_ctx, u32 flags)
{
	struct qcom_scm *scm = dev_get_drvdata(dev);
	struct completion *wq;

	if (flags == QCOM_SMC_WAITQ_FLAG_WAKE_NONE)
		return 0;
	if (flags != QCOM_SMC_WAITQ_FLAG_WAKE_ONE && flags != QCOM_SMC_WAITQ_FLAG_WAKE_ALL) {
		dev_err(scm->dev, "invalid waitq flags %u for wq_ctx %u\\n", flags, wq_ctx);
		return 0;
	}
	wq = qcom_scm_get_completion(scm, wq_ctx);
	if (IS_ERR(wq))
		return PTR_ERR(wq);
	qcom_scm_waitq_apply(wq, flags);
	return 0;
}
''')
edit(C, '''		if (flags != QCOM_SMC_WAITQ_FLAG_WAKE_ONE) {
			dev_err(scm->dev, "Invalid flags received for wq_ctx: %u\\n", flags);
			goto out;
		}

		ret = qcom_scm_waitq_wakeup(scm, wq_ctx);
		if (ret)
			goto out;''', '''		/* ANDROID: 6.12 vendor: two wakeups in close succession */
		if (flags == QCOM_SMC_WAITQ_FLAG_WAKE_NONE)
			goto out;

		if (flags != QCOM_SMC_WAITQ_FLAG_WAKE_ONE &&
		    flags != QCOM_SMC_WAITQ_FLAG_WAKE_ALL) {
			dev_err(scm->dev, "Invalid flags received for wq_ctx: %u\\n", flags);
			goto out;
		}

		ret = qcom_scm_waitq_wake_flags(scm->dev, wq_ctx, flags);
		if (ret)
			goto out;''')
edit(C, '''	for (i = 0; i < scm->wq_cnt; i++)
		init_completion(&scm->waitq_comps[i]);
''', '''	for (i = 0; i < scm->wq_cnt; i++)
		init_completion(&scm->waitq_comps[i]);
	xa_init(&scm->waitq_xa);
	dev_info(scm->dev, "waitq: %u contexts\\n", scm->wq_cnt);
''')
print('applied vendor SCM waitq semantics to', SRC)
