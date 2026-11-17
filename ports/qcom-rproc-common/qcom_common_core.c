// SPDX-License-Identifier: GPL-2.0-only
/*
 * AVR: composite-module wrapper so qcom_common.ko can carry the vendor
 * qcom_tracepoints.c object (kbuild cannot name an object after its module),
 * plus the android16-6.12 vendor rproc_qcom_common API the phone modules import
 * that the 7.3-based qcom_common.c lacks (qcom-soc 037aa1c qcom_common.c):
 *   qcom_ssr_get_subsys, qcom_{register,unregister}_early_ssr_notifier,
 *   qcom_notify_early_ssr_clients (icnss2, pci-msm-drv, qcom_q6v5),
 *   qcom_notify_ssr_clients, qcom_device_shutdown_in_progress (+ the
 *   /sys/kernel/qcom_rproc/shutdown_in_progress store; pdr_interface,
 *   qcom_sysmon), rproc_recovery_set_fn (qcom_q6v5_pas hook pointer).
 * The early notifier heads live in a name-keyed side table instead of a new
 * member of struct qcom_ssr_subsystem, so qcom_common.c stays unmodified; the
 * notifier semantics (srcu chain, QCOM_SSR_BEFORE_SHUTDOWN, NULL data) match.
 */
#define qcom_ssr_get_subsys avr_native_ssr_get_subsys
#include "qcom_common.c"
#undef qcom_ssr_get_subsys

#include <linux/kobject.h>
#include <linux/remoteproc/qcom_rproc_vendor.h>
#include <linux/sysfs.h>

struct qcom_ssr_subsystem *qcom_ssr_get_subsys(const char *name)
{
	return avr_native_ssr_get_subsys(name);
}
EXPORT_SYMBOL_GPL(qcom_ssr_get_subsys);

struct avr_early_ssr {
	const char *name;
	struct srcu_notifier_head head;
	struct list_head list;
};

static LIST_HEAD(avr_early_ssr_list);
static DEFINE_MUTEX(avr_early_ssr_lock);

static struct avr_early_ssr *avr_early_ssr_get(const char *name)
{
	struct avr_early_ssr *e;

	mutex_lock(&avr_early_ssr_lock);
	list_for_each_entry(e, &avr_early_ssr_list, list)
		if (!strcmp(e->name, name))
			goto out;
	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e) {
		e = ERR_PTR(-ENOMEM);
		goto out;
	}
	e->name = kstrdup_const(name, GFP_KERNEL);
	if (!e->name) {
		kfree(e);
		e = ERR_PTR(-ENOMEM);
		goto out;
	}
	srcu_init_notifier_head(&e->head);
	list_add_tail(&e->list, &avr_early_ssr_list);
out:
	mutex_unlock(&avr_early_ssr_lock);
	return e;
}

void *qcom_register_early_ssr_notifier(const char *name, struct notifier_block *nb)
{
	struct qcom_ssr_subsystem *info = qcom_ssr_get_subsys(name);
	struct avr_early_ssr *e;

	if (IS_ERR(info))
		return info;
	e = avr_early_ssr_get(name);
	if (IS_ERR(e))
		return e;
	srcu_notifier_chain_register(&e->head, nb);
	return &e->head;
}
EXPORT_SYMBOL(qcom_register_early_ssr_notifier);

int qcom_unregister_early_ssr_notifier(void *notify, struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(notify, nb);
}
EXPORT_SYMBOL(qcom_unregister_early_ssr_notifier);

void qcom_notify_early_ssr_clients(struct rproc_subdev *subdev)
{
	struct qcom_rproc_ssr *ssr = to_ssr_subdev(subdev);
	struct avr_early_ssr *e = avr_early_ssr_get(ssr->info->name);

	if (!IS_ERR(e))
		srcu_notifier_call_chain(&e->head, QCOM_SSR_BEFORE_SHUTDOWN, NULL);
}
EXPORT_SYMBOL(qcom_notify_early_ssr_clients);

int qcom_notify_ssr_clients(struct qcom_ssr_subsystem *info, int state,
			    struct qcom_ssr_notify_data *data)
{
	if (!info || state < 0)
		return -EINVAL;
	return srcu_notifier_call_chain(&info->notifier_list, state, data);
}
EXPORT_SYMBOL_GPL(qcom_notify_ssr_clients);

bool qcom_device_shutdown_in_progress;
EXPORT_SYMBOL(qcom_device_shutdown_in_progress);

void (*rproc_recovery_set_fn)(struct rproc *rproc) = NULL;
EXPORT_SYMBOL_GPL(rproc_recovery_set_fn);

static ssize_t qcom_rproc_shutdown_request_store(struct kobject *kobj, struct kobj_attribute *attr,
						 const char *buf, size_t count)
{
	bool val;
	int ret = kstrtobool(buf, &val);

	if (ret)
		return ret;
	qcom_device_shutdown_in_progress = val;
	pr_info("qcom rproc: Device shutdown requested: %s\n", val ? "true" : "false");
	return count;
}
static struct kobj_attribute shutdown_requested_attr = __ATTR(shutdown_in_progress, 0220, NULL,
							  qcom_rproc_shutdown_request_store);
static struct kobject *avr_qcom_rproc_kobj;

static int __init avr_qcom_common_vendor_init(void)
{
	int ret;

	avr_qcom_rproc_kobj = kobject_create_and_add("qcom_rproc", kernel_kobj);
	if (!avr_qcom_rproc_kobj)
		return -ENOMEM;
	ret = sysfs_create_file(avr_qcom_rproc_kobj, &shutdown_requested_attr.attr);
	if (ret)
		kobject_put(avr_qcom_rproc_kobj);
	return ret;
}
module_init(avr_qcom_common_vendor_init);

static void __exit avr_qcom_common_vendor_exit(void)
{
	struct avr_early_ssr *e, *tmp;

	kobject_put(avr_qcom_rproc_kobj);
	list_for_each_entry_safe(e, tmp, &avr_early_ssr_list, list) {
		list_del(&e->list);
		srcu_cleanup_notifier_head(&e->head);
		kfree_const(e->name);
		kfree(e);
	}
}
module_exit(avr_qcom_common_vendor_exit);
