#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x57962c48, "__tracepoint_android_rvh_check_preempt_wakeup_fair" },
	{ 0x80e9f7d9, "android_rvh_probe_register" },
	{ 0x57962c48, "__tracepoint_android_rvh_replace_next_task_fair" },
	{ 0x57962c48, "__tracepoint_android_rvh_find_new_ilb" },
	{ 0x57962c48, "__tracepoint_android_vh_binder_set_priority" },
	{ 0x80e9f7d9, "tracepoint_probe_register" },
	{ 0x57962c48, "__tracepoint_android_vh_binder_restore_priority" },
	{ 0x16b5b21d, "_printk" },
	{ 0xa8c8a0ab, "kthread_create_on_node" },
	{ 0x17e939d2, "kthread_bind" },
	{ 0xb8c8345a, "alt_cb_patch_nops" },
	{ 0x27a9e179, "wake_up_process" },
	{ 0x67628f51, "msleep" },
	{ 0x7294dabb, "kthread_stop" },
	{ 0x582e858c, "__put_task_struct_rcu_cb" },
	{ 0x19d96b61, "call_rcu" },
	{ 0xd0a90dbc, "kunit_unary_assert_format" },
	{ 0x9d8d2bcb, "__kunit_do_failed_assertion" },
	{ 0xabf0dae0, "__kunit_abort" },
	{ 0x2373037a, "refcount_warn_saturate" },
	{ 0xd0a90dbc, "kunit_binary_assert_format" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0xf2a4b08a, "jiffies" },
	{ 0x7162a662, "__msecs_to_jiffies" },
	{ 0x5e505530, "kthread_should_stop" },
	{ 0x0bac81a1, "schedule_timeout_interruptible" },
	{ 0xfdbae8c6, "usleep_range_state" },
	{ 0xd8f50094, "param_ops_int" },
	{ 0x0b230c63, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0x57962c48,
	0x80e9f7d9,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x80e9f7d9,
	0x57962c48,
	0x16b5b21d,
	0xa8c8a0ab,
	0x17e939d2,
	0xb8c8345a,
	0x27a9e179,
	0x67628f51,
	0x7294dabb,
	0x582e858c,
	0x19d96b61,
	0xd0a90dbc,
	0x9d8d2bcb,
	0xabf0dae0,
	0x2373037a,
	0xd0a90dbc,
	0xd272d446,
	0xf2a4b08a,
	0x7162a662,
	0x5e505530,
	0x0bac81a1,
	0xfdbae8c6,
	0xd8f50094,
	0x0b230c63,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"__tracepoint_android_rvh_check_preempt_wakeup_fair\0"
	"android_rvh_probe_register\0"
	"__tracepoint_android_rvh_replace_next_task_fair\0"
	"__tracepoint_android_rvh_find_new_ilb\0"
	"__tracepoint_android_vh_binder_set_priority\0"
	"tracepoint_probe_register\0"
	"__tracepoint_android_vh_binder_restore_priority\0"
	"_printk\0"
	"kthread_create_on_node\0"
	"kthread_bind\0"
	"alt_cb_patch_nops\0"
	"wake_up_process\0"
	"msleep\0"
	"kthread_stop\0"
	"__put_task_struct_rcu_cb\0"
	"call_rcu\0"
	"kunit_unary_assert_format\0"
	"__kunit_do_failed_assertion\0"
	"__kunit_abort\0"
	"refcount_warn_saturate\0"
	"kunit_binary_assert_format\0"
	"__stack_chk_fail\0"
	"jiffies\0"
	"__msecs_to_jiffies\0"
	"kthread_should_stop\0"
	"schedule_timeout_interruptible\0"
	"usleep_range_state\0"
	"param_ops_int\0"
	"module_layout\0"
;

MODULE_INFO(depends, "kunit");

