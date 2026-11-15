// SPDX-License-Identifier: GPL-2.0-only
/* Actual Q6/sysmon callbacks and native frameworks, no remote firmware. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/rpmsg.h>
#undef module_rpmsg_driver
#define module_rpmsg_driver(...)
#include "../../ports/qcom-remoteproc-services/qcom_q6v5.c"
#include "../../ports/qcom-remoteproc-services/qcom_sysmon.c"
struct fixture {
	struct qcom_q6v5 q6;
	struct qcom_sysmon mon;
	struct rpmsg_device rpdev;
	struct rpmsg_endpoint endpoint;
	struct device *dev;
	struct qcom_smem_state *state;
	u32 masks[2], values[2];
	unsigned calls;
	int error;
	bool clear_error;
};
static int update(void *priv, u32 mask, u32 value)
{
	struct fixture *f = priv;
	if (f->calls < 2) { f->masks[f->calls] = mask; f->values[f->calls] = value; }
	f->calls++;
	if (f->error) return f->error;
	if (!value && f->clear_error) return -EIO;
	if (value) complete(&f->q6.stop_done);
	return 0;
}
static const struct qcom_smem_state_ops state_ops = {.update_bits = update};
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	if (!f) return -ENOMEM;
	f->dev = root_device_register("avr_rproc_services_unit");
	if (IS_ERR(f->dev)) return PTR_ERR(f->dev);
	f->dev->of_node = of_find_node_by_path("/avr-qemu-services-parent");
	if (!f->dev->of_node) { root_device_unregister(f->dev); return -ENODEV; }
	f->q6.dev = f->mon.dev = f->dev;
	init_completion(&f->q6.start_done); init_completion(&f->q6.stop_done);
	init_completion(&f->mon.comp);
	f->mon.shutdown_irq = -ENODATA;
	mutex_init(&f->mon.lock);
	f->rpdev.ept = &f->endpoint;
	f->endpoint.priv = &f->mon;
	f->mon.ept = &f->endpoint;
	f->q6.stop_bit = 3;
	f->state = qcom_smem_state_register(f->dev->of_node, &state_ops, f);
	if (IS_ERR(f->state)) {
		of_node_put(f->dev->of_node); root_device_unregister(f->dev);
		return PTR_ERR(f->state);
	}
	f->q6.state = f->state;
	test->priv = f;
	return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	qcom_smem_state_unregister(f->state);
	of_node_put(f->dev->of_node); root_device_unregister(f->dev);
}
static void ready_and_wait(struct kunit *test)
{
	struct fixture *f = test->priv;
	KUNIT_EXPECT_EQ(test, qcom_q6v5_wait_for_start(&f->q6, 0), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, q6v5_ready_interrupt(0, &f->q6), IRQ_HANDLED);
	KUNIT_EXPECT_EQ(test, qcom_q6v5_wait_for_start(&f->q6, 1), 0);
}
static void stopped_watchdog_ack(struct kunit *test)
{
	struct fixture *f = test->priv;
	KUNIT_EXPECT_EQ(test, q6v5_wdog_interrupt(0, &f->q6), IRQ_HANDLED);
	KUNIT_EXPECT_TRUE(test, completion_done(&f->q6.stop_done));
	KUNIT_EXPECT_EQ(test, f->calls, 0U);
}
static void stopped_fatal_ignored(struct kunit *test)
{
	struct fixture *f = test->priv;
	KUNIT_EXPECT_EQ(test, q6v5_fatal_interrupt(0, &f->q6), IRQ_HANDLED);
	KUNIT_EXPECT_FALSE(test, completion_done(&f->q6.stop_done));
}
static void shutdown_ack_skips_state_request(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->q6.running = true; f->mon.shutdown_acked = true;
	KUNIT_EXPECT_EQ(test, qcom_q6v5_request_stop(&f->q6, &f->mon), 0);
	KUNIT_EXPECT_FALSE(test, f->q6.running);
	KUNIT_EXPECT_EQ(test, f->calls, 0U);
}
static void actual_state_stop_assert_and_clear(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->q6.running = true;
	KUNIT_EXPECT_EQ(test, qcom_q6v5_request_stop(&f->q6, NULL), 0);
	KUNIT_EXPECT_EQ(test, f->calls, 2U);
	KUNIT_EXPECT_EQ(test, f->masks[0], 8U); KUNIT_EXPECT_EQ(test, f->masks[1], 8U);
	KUNIT_EXPECT_EQ(test, f->values[0], 8U); KUNIT_EXPECT_EQ(test, f->values[1], 0U);
}
static void state_request_failure_propagates(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->q6.running = true; f->error = -EIO;
	KUNIT_EXPECT_EQ(test, qcom_q6v5_request_stop(&f->q6, NULL), -EIO);
	KUNIT_EXPECT_EQ(test, f->calls, 1U);
}
static void state_clear_failure_propagates(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->q6.running = true; f->clear_error = true;
	KUNIT_EXPECT_EQ(test, qcom_q6v5_request_stop(&f->q6, NULL), -EIO);
	KUNIT_EXPECT_EQ(test, f->calls, 2U);
}
static void panic_uses_state_api(struct kunit *test)
{
	struct fixture *f = test->priv;
	KUNIT_EXPECT_EQ(test, qcom_q6v5_panic(&f->q6), 200UL);
	KUNIT_EXPECT_EQ(test, f->calls, 1U); KUNIT_EXPECT_EQ(test, f->values[0], 8U);
}
static void sysmon_ack_and_short_response(struct kunit *test)
{
	struct fixture *f = test->priv;
	char ack[] = "ssr:ack";
	KUNIT_EXPECT_EQ(test, sysmon_callback(NULL, ack, sizeof(ack), NULL, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, sysmon_callback(&f->rpdev, ack, 3, &f->mon, 0), 0);
	KUNIT_EXPECT_FALSE(test, f->mon.ssr_ack);
	KUNIT_EXPECT_TRUE(test, completion_done(&f->mon.comp));
	reinit_completion(&f->mon.comp);
	KUNIT_EXPECT_EQ(test, sysmon_callback(&f->rpdev, ack, sizeof(ack), &f->mon, 0), 0);
	KUNIT_EXPECT_TRUE(test, f->mon.ssr_ack);
	KUNIT_EXPECT_TRUE(test, completion_done(&f->mon.comp));
	KUNIT_EXPECT_FALSE(test, qcom_sysmon_shutdown_irq_state(&f->mon));
}
static void detached_callback_rejects_stale_priv(struct kunit *test)
{
	struct fixture *f = test->priv;
	char ack[] = "ssr:ack";
	sysmon_remove(&f->rpdev);
	KUNIT_EXPECT_PTR_EQ(test, f->endpoint.priv, NULL);
	KUNIT_EXPECT_PTR_EQ(test, f->mon.ept, NULL);
	KUNIT_EXPECT_EQ(test, sysmon_callback(&f->rpdev, ack, sizeof(ack), &f->mon, 0), -EINVAL);
	KUNIT_EXPECT_FALSE(test, completion_done(&f->mon.comp));
	KUNIT_EXPECT_FALSE(test, sysmon_request_shutdown(&f->mon));
}
struct callback_worker {
	struct rpmsg_device *rpdev;
	struct completion entered;
	atomic_t accepted, rejected;
};
static int callback_thread(void *priv)
{
	struct callback_worker *w = priv;
	char ack[] = "ssr:ack";
	while (!kthread_should_stop()) {
		int ret = sysmon_callback(w->rpdev, ack, sizeof(ack), ERR_PTR(-EIO), 0);
		if (!ret) atomic_inc(&w->accepted);
		else atomic_inc(&w->rejected);
		complete(&w->entered);
		cond_resched();
	}
	return 0;
}
static const struct rproc_ops rproc_ops = {};
static void actual_sysmon_qmi_subdevice_lifecycle(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct rproc *rproc = rproc_alloc(f->dev, "avr-soccp-unit", &rproc_ops, NULL, 0);
	struct rpmsg_device rpdev = {};
	struct rpmsg_endpoint ept = {};
	struct callback_worker worker = {};
	struct task_struct *thread;
	struct qcom_sysmon *mon;
	char ack[] = "ssr:ack";
	int i;
	KUNIT_ASSERT_NOT_NULL(test, rproc);
	mon = qcom_add_sysmon_subdev(rproc, "soccp", 0);
	if (IS_ERR(mon)) { rproc_free(rproc); KUNIT_FAIL(test, "real QMI/sysmon initialization failed"); return; }
	KUNIT_EXPECT_FALSE(test, list_empty(&rproc->subdevs));
	KUNIT_EXPECT_FALSE(test, qcom_sysmon_shutdown_acked(mon));
	KUNIT_EXPECT_FALSE(test, qcom_sysmon_shutdown_irq_state(mon));
	rpdev.dev.parent = &rproc->dev; rpdev.ept = &ept;
	KUNIT_EXPECT_EQ(test, sysmon_probe(&rpdev), 0);
	KUNIT_EXPECT_EQ(test, sysmon_probe(&rpdev), -EBUSY);
	KUNIT_EXPECT_PTR_EQ(test, ept.priv, mon);
	worker.rpdev = &rpdev; init_completion(&worker.entered);
	atomic_set(&worker.accepted, 0); atomic_set(&worker.rejected, 0);
	thread = kthread_run(callback_thread, &worker, "avr_sysmon_ingress");
	if (IS_ERR(thread)) {
		qcom_remove_sysmon_subdev(mon); rproc_free(rproc);
		KUNIT_FAIL(test, "cannot start real callback worker"); return;
	}
	KUNIT_EXPECT_NE(test, wait_for_completion_timeout(&worker.entered, HZ), 0UL);
	qcom_remove_sysmon_subdev(mon);
	KUNIT_EXPECT_PTR_EQ(test, ept.priv, NULL);
	for (i = 0; i < 1000; i++)
		KUNIT_EXPECT_EQ(test, sysmon_callback(&rpdev, ack, sizeof(ack), ERR_PTR(-EIO), 0), -EINVAL);
	kthread_stop(thread);
	KUNIT_EXPECT_GT(test, atomic_read(&worker.accepted), 0);
	/* Consumer removal after provider removal must also be harmless. */
	sysmon_remove(&rpdev);
	KUNIT_EXPECT_TRUE(test, list_empty(&rproc->subdevs));
	rproc_free(rproc);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(ready_and_wait), KUNIT_CASE(stopped_watchdog_ack), KUNIT_CASE(stopped_fatal_ignored),
	KUNIT_CASE(shutdown_ack_skips_state_request), KUNIT_CASE(actual_state_stop_assert_and_clear),
	KUNIT_CASE(state_request_failure_propagates), KUNIT_CASE(state_clear_failure_propagates), KUNIT_CASE(panic_uses_state_api),
	KUNIT_CASE(sysmon_ack_and_short_response), KUNIT_CASE(detached_callback_rejects_stale_priv), KUNIT_CASE(actual_sysmon_qmi_subdevice_lifecycle), {}
};
static struct kunit_suite suite = {.name="avr-remoteproc-services",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
