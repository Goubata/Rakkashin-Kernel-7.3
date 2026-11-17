// SPDX-License-Identifier: GPL-2.0-only
/*
 * QEMU-only stand-in for the synx-driver symbols msm_drm imports. QEMU virt has
 * no IPCC/ipclite global shared memory, so the real synx-driver.ko correctly
 * refuses to load (synx_global_mem_init: -EINVAL). Same prototypes as
 * ports/qti-fence synx_compat.c (CRC-checked); every call reports "no synx".
 * Never shipped: the phone runs ports/qti-fence synx-driver.ko.
 */
#include <linux/err.h>
#include <linux/module.h>
#include <linux/types.h>
#include "synx_api.h"

struct synx_session *synx_initialize(struct synx_initialization_params *params)
{
	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL(synx_initialize);

int synx_uninitialize(struct synx_session *session)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_uninitialize);

int synx_create(struct synx_session *session, struct synx_create_params *params)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_create);

int synx_release(struct synx_session *session, u32 h_synx)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_release);

int synx_signal(struct synx_session *session, u32 h_synx, enum synx_signal_status status)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_signal);

int synx_recover(enum synx_client_id id)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_recover);

int synx_import(struct synx_session *session, struct synx_import_params *params)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_import);

int synx_enable_resources(enum synx_client_id id, enum synx_resource_type resource, bool enable)
{
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(synx_enable_resources);


int synx_release_n(struct synx_session *session, struct synx_release_n_params *pParams)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_release_n);

int synx_async_wait(struct synx_session *session, struct synx_callback_params *params)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_async_wait);

void *synx_get_fence(struct synx_session *session, u32 h_synx)
{
	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL(synx_get_fence);

int synx_get_status(struct synx_session *session, u32 h_synx)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_get_status);

int synx_merge(struct synx_session *session, struct synx_merge_params *params)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_merge);

int synx_merge_n(struct synx_session *session, struct synx_merge_n_params *params)
{
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(synx_merge_n);

int synx_wait(struct synx_session *session, u32 h_synx, u64 timeout_ms)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_wait);

int synx_cancel_async_wait(struct synx_session *session,
	struct synx_callback_params *params)
{
	return -ENODEV;
}
EXPORT_SYMBOL(synx_cancel_async_wait);

int synx_get(struct synx_session *session, struct synx_get_params *params)
{
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(synx_get);

int synx_signal_n(struct synx_session *session, struct synx_signal_n_params *params)
{
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(synx_signal_n);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AVR QEMU-only synx stand-in for msm_drm load tests");
