/* SPDX-License-Identifier: GPL-2.0-only */
/* QEMU-only TrustZone stand-in for the real qcom-scm sources (see prepare_fake_scm.py). */
#ifndef AVR_FAKE_TZ_H
#define AVR_FAKE_TZ_H
#include <linux/arm-smccc.h>
void avr_fake_smc_quirk(unsigned long a0, unsigned long a1, unsigned long a2,
			unsigned long a3, unsigned long a4, unsigned long a5,
			unsigned long a6, unsigned long a7, struct arm_smccc_res *res,
			struct arm_smccc_quirk *quirk);
void avr_fake_smc(unsigned long a0, unsigned long a1, unsigned long a2,
		  unsigned long a3, unsigned long a4, unsigned long a5,
		  unsigned long a6, unsigned long a7, struct arm_smccc_res *res);

/* test introspection (tests/qcom-tee KUnit) */
struct avr_fake_tz_stats {
	unsigned int bridges_live, bridge_creates, bridge_deletes;
	u64 last_bridge_flags, last_bridge_vmids, last_bridge_pfn;
	unsigned int invokes, callbacks, releases_from_ns, services_live, ns_objects_held;
	unsigned int unknown_calls;
};
void avr_fake_tz_get_stats(struct avr_fake_tz_stats *s);
#endif
