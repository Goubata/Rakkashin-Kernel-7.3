/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_RPMH_VENDOR_API_H
#define AVR_RPMH_VENDOR_API_H
#include <soc/qcom/rpmh.h>
int rpmh_mode_solver_set(const struct device *dev, bool enable);

int rpmh_write_sleep_and_wake(const struct device *dev);

int rpmh_write_sleep_and_wake_no_child(const struct device *dev);

void rpmh_invalidate(const struct device *dev);

int rpmh_init_fast_path(const struct device *dev,
			struct tcs_cmd *cmd, int n);

int rpmh_update_fast_path(const struct device *dev,
			  struct tcs_cmd *cmd, int n, u32 update_mask);
int rpmh_drv_start(const struct device *dev);

int rpmh_drv_stop(const struct device *dev);

const struct device *rpmh_get_device(const char *name, u32 drv_id);

#endif
