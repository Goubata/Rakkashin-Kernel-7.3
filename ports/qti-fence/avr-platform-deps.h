/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_PLATFORM_DEPS_H
#define AVR_PLATFORM_DEPS_H
#include <linux/remoteproc.h>
/* Actual vendor SOCCP power-vote API, implemented by qcom_q6v5_pas.c.
 * No substitute state assignment or success stub is provided here. */
int rproc_set_state(struct rproc *rproc, bool state);
#endif
