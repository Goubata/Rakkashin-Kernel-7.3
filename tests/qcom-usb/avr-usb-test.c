// SPDX-License-Identifier: GPL-2.0-only
/*
 * KUnit: dwc3-msm hooks the 7.3 dwc3/gadget core through kretprobes on these
 * symbols (dwc3-msm-ops.c dwc3_msm_probes[]). The phone binds dwc3-msm at boot,
 * so every target must exist and be probe-able in this kernel; QEMU has no
 * DWC3, so registration is checked directly here.
 */
#include <kunit/test.h>
#include <linux/kprobes.h>
#include <linux/module.h>

static const char *const targets[] = {
	"dwc3_gadget_run_stop", "dwc3_send_gadget_ep_cmd", "dwc3_gadget_reset_interrupt",
	"__dwc3_gadget_ep_enable", "dwc3_host_exit", "dwc3_gadget_pullup", "android_work",
	"usb_ep_set_maxpacket_limit", "dwc3_suspend_common",
	"trace_event_raw_event_dwc3_log_request", "trace_event_raw_event_dwc3_log_gadget_ep_cmd",
	"trace_event_raw_event_dwc3_log_trb", "trace_event_raw_event_dwc3_log_event",
	"trace_event_raw_event_dwc3_log_ep",
};

static int nop_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
	return 0;
}

static void kretprobes_register(struct kunit *test)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(targets); i++) {
		struct kretprobe rp = { .entry_handler = nop_entry, .maxactive = 8,
					.kp.symbol_name = targets[i] };
		int ret = register_kretprobe(&rp);

		KUNIT_EXPECT_EQ_MSG(test, ret, 0, "%s", targets[i]);
		if (!ret)
			unregister_kretprobe(&rp);
	}
}

static struct kunit_case cases[] = { KUNIT_CASE(kretprobes_register), {} };
static struct kunit_suite suite = { .name = "avr_dwc3_msm_probes", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
