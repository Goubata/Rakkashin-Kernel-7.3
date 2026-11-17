/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AVR 7.3: android16-6.12 vendor additions of <linux/remoteproc/qcom_rproc.h>
 * (qcom-soc 037aa1c) provided by ports/qcom-rproc-common qcom_common.ko. The
 * 7.3 header has only the upstream subset; consumers force-include this.
 */
#ifndef AVR_QCOM_RPROC_VENDOR_H
#define AVR_QCOM_RPROC_VENDOR_H
#include <linux/types.h>
#include <linux/remoteproc/qcom_rproc.h>

struct notifier_block;
struct rproc;
struct rproc_subdev;
struct qcom_ssr_subsystem;

void *qcom_register_early_ssr_notifier(const char *name, struct notifier_block *nb);
int qcom_unregister_early_ssr_notifier(void *notify, struct notifier_block *nb);
void qcom_notify_early_ssr_clients(struct rproc_subdev *subdev);
struct qcom_ssr_subsystem *qcom_ssr_get_subsys(const char *name);
int qcom_notify_ssr_clients(struct qcom_ssr_subsystem *info, int state,
			    struct qcom_ssr_notify_data *data);
extern bool qcom_device_shutdown_in_progress;
extern void (*rproc_recovery_set_fn)(struct rproc *rproc);
#endif
