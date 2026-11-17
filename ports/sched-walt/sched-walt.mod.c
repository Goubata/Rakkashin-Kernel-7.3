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

KSYMTAB_FUNC(set_moto_sched_enabled, "");
SYMBOL_FLAGS(set_moto_sched_enabled, 0x00);
KSYMTAB_FUNC(set_moto_sched_ops, "");
SYMBOL_FLAGS(set_moto_sched_ops, 0x01);
KSYMTAB_FUNC(set_task_boost, "");
SYMBOL_FLAGS(set_task_boost, 0x01);
KSYMTAB_FUNC(should_boost_bus_dcvs, "");
SYMBOL_FLAGS(should_boost_bus_dcvs, 0x01);
KSYMTAB_FUNC(walt_set_cpus_taken, "");
SYMBOL_FLAGS(walt_set_cpus_taken, 0x01);
KSYMTAB_FUNC(walt_unset_cpus_taken, "");
SYMBOL_FLAGS(walt_unset_cpus_taken, 0x01);
KSYMTAB_FUNC(walt_get_cpus_taken, "");
SYMBOL_FLAGS(walt_get_cpus_taken, 0x01);
KSYMTAB_FUNC(walt_set_enforce_high_irq_cpus, "");
SYMBOL_FLAGS(walt_set_enforce_high_irq_cpus, 0x01);
KSYMTAB_FUNC(walt_unset_enforce_high_irq_cpus, "");
SYMBOL_FLAGS(walt_unset_enforce_high_irq_cpus, 0x01);
KSYMTAB_FUNC(walt_get_enforce_high_irq_cpus, "");
SYMBOL_FLAGS(walt_get_enforce_high_irq_cpus, 0x01);
KSYMTAB_FUNC(walt_get_cpus_in_state1, "");
SYMBOL_FLAGS(walt_get_cpus_in_state1, 0x01);
KSYMTAB_FUNC(walt_get_halted_cpus, "");
SYMBOL_FLAGS(walt_get_halted_cpus, 0x01);
KSYMTAB_FUNC(sched_set_boost, "");
SYMBOL_FLAGS(sched_set_boost, 0x01);
KSYMTAB_FUNC(sched_get_nr_running_avg, "");
SYMBOL_FLAGS(sched_get_nr_running_avg, 0x01);
KSYMTAB_FUNC(sched_lpm_disallowed_time, "");
SYMBOL_FLAGS(sched_lpm_disallowed_time, 0x01);
KSYMTAB_FUNC(walt_pause_cpus, "");
SYMBOL_FLAGS(walt_pause_cpus, 0x01);
KSYMTAB_FUNC(walt_partial_pause_cpus, "");
SYMBOL_FLAGS(walt_partial_pause_cpus, 0x01);
KSYMTAB_FUNC(walt_resume_cpus, "");
SYMBOL_FLAGS(walt_resume_cpus, 0x01);
KSYMTAB_FUNC(walt_partial_resume_cpus, "");
SYMBOL_FLAGS(walt_partial_resume_cpus, 0x01);
KSYMTAB_FUNC(core_ctl_set_boost, "");
SYMBOL_FLAGS(core_ctl_set_boost, 0x01);
KSYMTAB_FUNC(core_ctl_notifier_register, "");
SYMBOL_FLAGS(core_ctl_notifier_register, 0x01);
KSYMTAB_FUNC(core_ctl_notifier_unregister, "");
SYMBOL_FLAGS(core_ctl_notifier_unregister, 0x01);
KSYMTAB_FUNC(cpufreq_walt_set_adaptive_freq, "");
SYMBOL_FLAGS(cpufreq_walt_set_adaptive_freq, 0x01);
KSYMTAB_FUNC(cpufreq_walt_get_adaptive_freq, "");
SYMBOL_FLAGS(cpufreq_walt_get_adaptive_freq, 0x01);
KSYMTAB_FUNC(cpufreq_walt_reset_adaptive_freq, "");
SYMBOL_FLAGS(cpufreq_walt_reset_adaptive_freq, 0x01);
KSYMTAB_FUNC(sched_walt_oscillate, "");
SYMBOL_FLAGS(sched_walt_oscillate, 0x01);

SYMBOL_CRC(set_moto_sched_enabled, 0x10239c19);
SYMBOL_CRC(set_moto_sched_ops, 0x52d57d70);
SYMBOL_CRC(set_task_boost, 0x2e1dff25);
SYMBOL_CRC(should_boost_bus_dcvs, 0x5e505530);
SYMBOL_CRC(walt_set_cpus_taken, 0x6fac3e23);
SYMBOL_CRC(walt_unset_cpus_taken, 0x51711841);
SYMBOL_CRC(walt_get_cpus_taken, 0xfd7e2bae);
SYMBOL_CRC(walt_set_enforce_high_irq_cpus, 0x6fac3e23);
SYMBOL_CRC(walt_unset_enforce_high_irq_cpus, 0x51711841);
SYMBOL_CRC(walt_get_enforce_high_irq_cpus, 0xfd7e2bae);
SYMBOL_CRC(walt_get_cpus_in_state1, 0xe9d9ceac);
SYMBOL_CRC(walt_get_halted_cpus, 0xfd7e2bae);
SYMBOL_CRC(sched_set_boost, 0x51330f0e);
SYMBOL_CRC(sched_get_nr_running_avg, 0x38a1770b);
SYMBOL_CRC(sched_lpm_disallowed_time, 0xc5ec8ad9);
SYMBOL_CRC(walt_pause_cpus, 0xed372be7);
SYMBOL_CRC(walt_partial_pause_cpus, 0xed372be7);
SYMBOL_CRC(walt_resume_cpus, 0xed372be7);
SYMBOL_CRC(walt_partial_resume_cpus, 0xed372be7);
SYMBOL_CRC(core_ctl_set_boost, 0x2a33519d);
SYMBOL_CRC(core_ctl_notifier_register, 0xdd98a2ad);
SYMBOL_CRC(core_ctl_notifier_unregister, 0xdd98a2ad);
SYMBOL_CRC(cpufreq_walt_set_adaptive_freq, 0x0783426e);
SYMBOL_CRC(cpufreq_walt_get_adaptive_freq, 0x15df9159);
SYMBOL_CRC(cpufreq_walt_reset_adaptive_freq, 0x47559d93);
SYMBOL_CRC(sched_walt_oscillate, 0xfa585d9f);

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xc799d30a, "init_task" },
	{ 0xf54e5881, "vendor_data_pad" },
	{ 0xf018baf9, "sched_clock" },
	{ 0x16b5b21d, "_printk_deferred" },
	{ 0xf296206e, "nr_cpu_ids" },
	{ 0xf4386284, "scnprintf" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0x0de4cab5, "__per_cpu_offset" },
	{ 0x6cef033f, "runqueues" },
	{ 0xb8c8345a, "alt_cb_patch_nops" },
	{ 0x582e858c, "__put_task_struct_rcu_cb" },
	{ 0x19d96b61, "call_rcu" },
	{ 0x2373037a, "refcount_warn_saturate" },
	{ 0x4e994f70, "__cpu_online_mask" },
	{ 0xbe1fa9a4, "__sw_hweight64" },
	{ 0xe37ba965, "ksoftirqd" },
	{ 0x5e505530, "rcu_is_watching" },
	{ 0x1127e91b, "__warn_printk" },
	{ 0x1176ef14, "tracepoint_srcu" },
	{ 0xc81fdd93, "__srcu_check_read_flavor" },
	{ 0xd272d446, "preempt_schedule_notrace" },
	{ 0xe82c42ee, "cpu_scale" },
	{ 0xd272d446, "__rcu_read_lock" },
	{ 0x959db01a, "rcu_lock_map" },
	{ 0x11fa8ee9, "lock_acquire" },
	{ 0x7851be11, "debug_lockdep_rcu_enabled" },
	{ 0x39ef7a8e, "lockdep_rcu_suspicious" },
	{ 0x7851be11, "rcu_read_lock_held" },
	{ 0x98d1efb5, "lock_release" },
	{ 0xd272d446, "__rcu_read_unlock" },
	{ 0xd0b3f3a9, "smp_call_function_single_async" },
	{ 0xe82c42ee, "hw_pressure" },
	{ 0x7851be11, "rcu_read_lock_sched_held" },
	{ 0xf296206e, "__num_possible_cpus" },
	{ 0xcd2684cf, "_raw_spin_lock_irqsave" },
	{ 0x6c62dbae, "_raw_spin_unlock_irqrestore" },
	{ 0xe82c42ee, "arch_freq_scale" },
	{ 0x1178ad06, "ksize" },
	{ 0x57962c48, "__tracepoint_android_vh_update_topology_flags_workfn" },
	{ 0x80e9f7d9, "tracepoint_probe_register" },
	{ 0x3239fbdb, "topology_update_done" },
	{ 0x571c73d5, "system_percpu_wq" },
	{ 0x088866de, "queue_work_on" },
	{ 0x16b5b21d, "_printk" },
	{ 0x4e994f70, "__cpu_active_mask" },
	{ 0x1eded390, "register_syscore" },
	{ 0x2d376275, "kmalloc_caches" },
	{ 0x8ee1928d, "mem_alloc_profiling_key" },
	{ 0x5f42c804, "__kmalloc_cache_noprof" },
	{ 0xc85a9c7f, "__raw_spin_lock_init" },
	{ 0x4e994f70, "__cpu_possible_mask" },
	{ 0x6e964b9d, "root_task_group" },
	{ 0xa4e7fec9, "css_next_child" },
	{ 0x57962c48, "__tracepoint_android_rvh_wake_up_new_task" },
	{ 0x80e9f7d9, "android_rvh_probe_register" },
	{ 0x57962c48, "__tracepoint_android_rvh_update_cpu_capacity" },
	{ 0x57962c48, "__tracepoint_android_rvh_sched_cpu_starting" },
	{ 0x57962c48, "__tracepoint_android_rvh_sched_cpu_dying" },
	{ 0x57962c48, "__tracepoint_android_rvh_set_task_cpu" },
	{ 0x57962c48, "__tracepoint_android_rvh_new_task_stats" },
	{ 0x57962c48, "__tracepoint_android_rvh_account_irq" },
	{ 0x57962c48, "__tracepoint_android_rvh_flush_task" },
	{ 0x57962c48, "__tracepoint_android_rvh_update_misfit_status" },
	{ 0x57962c48, "__tracepoint_android_rvh_enqueue_task" },
	{ 0x57962c48, "__tracepoint_android_rvh_dequeue_task" },
	{ 0x57962c48, "__tracepoint_android_rvh_try_to_wake_up" },
	{ 0x57962c48, "__tracepoint_android_rvh_tick_entry" },
	{ 0x57962c48, "__tracepoint_android_vh_scheduler_tick" },
	{ 0x57962c48, "__tracepoint_android_rvh_schedule" },
	{ 0x57962c48, "__tracepoint_android_rvh_cpu_cgroup_attach" },
	{ 0x57962c48, "__tracepoint_android_vh_cpu_cgroup_css_free" },
	{ 0x57962c48, "__tracepoint_android_rvh_cpu_cgroup_online" },
	{ 0x57962c48, "__tracepoint_android_rvh_sched_fork_init" },
	{ 0x57962c48, "__tracepoint_android_rvh_ttwu_cond" },
	{ 0x57962c48, "__tracepoint_android_rvh_sched_exec" },
	{ 0x57962c48, "__tracepoint_android_rvh_build_perf_domains" },
	{ 0x57962c48, "__tracepoint_cpu_frequency_limits" },
	{ 0x57962c48, "__tracepoint_android_rvh_do_sched_yield" },
	{ 0x57962c48, "__tracepoint_android_rvh_update_thermal_stats" },
	{ 0x57962c48, "__tracepoint_android_vh_dup_task_struct" },
	{ 0x57962c48, "__tracepoint_android_vh_freq_qos_add_request" },
	{ 0x57962c48, "__tracepoint_android_vh_freq_qos_update_request" },
	{ 0x57962c48, "__tracepoint_android_vh_freq_qos_remove_request" },
	{ 0x1a5ab47f, "wait_for_completion_interruptible" },
	{ 0x545a6fb7, "cpu_topology" },
	{ 0x6fabd45b, "__list_add_valid_or_report" },
	{ 0x31359e90, "kfree" },
	{ 0x802f8919, "__list_del_entry_valid_or_report" },
	{ 0xf92c2cb2, "cpufreq_cpu_get_raw" },
	{ 0x27865ec7, "__kmalloc_noprof" },
	{ 0x7aab6508, "of_get_cpu_node" },
	{ 0x8078ce17, "__of_parse_phandle_with_args" },
	{ 0xf92c2cb2, "cpufreq_cpu_get" },
	{ 0xb50f8a53, "cpufreq_cpu_put" },
	{ 0xb98e8186, "of_find_node_opts_by_path" },
	{ 0xdc20867a, "of_platform_populate" },
	{ 0xecd472fc, "stop_machine" },
	{ 0x55161516, "sched_feat_names" },
	{ 0x7b3da183, "match_string" },
	{ 0x6971ec84, "sched_feat_keys" },
	{ 0x8bb63d96, "static_key_disable" },
	{ 0xf296206e, "sysctl_sched_features" },
	{ 0x8998aa29, "topology_clear_scale_freq_source" },
	{ 0x10aae63b, "tasklist_lock" },
	{ 0x554acda6, "_raw_read_lock" },
	{ 0x318d87c2, "_raw_spin_lock_nested" },
	{ 0xb22e15f0, "_raw_spin_lock" },
	{ 0xb9062cec, "_raw_write_lock_irqsave" },
	{ 0x67903a9b, "_raw_write_unlock_irqrestore" },
	{ 0xb22e15f0, "_raw_spin_unlock" },
	{ 0x554acda6, "_raw_read_unlock" },
	{ 0xf5df0401, "strcmp" },
	{ 0xf8828461, "raw_spin_rq_lock_nested" },
	{ 0x60c331c2, "lock_is_held_type" },
	{ 0x9cf1d65a, "system_32bit_el0_cpumask" },
	{ 0x1400a461, "_find_next_bit" },
	{ 0x8ee1928d, "__scx_enabled" },
	{ 0x992a5140, "ext_sched_class" },
	{ 0x7ec472ba, "debug_locks" },
	{ 0xd272d446, "trace_hardirqs_off" },
	{ 0xc8d5fd9d, "lock_pin_lock" },
	{ 0x303ae86f, "balance_push_callback" },
	{ 0xd77a17e5, "lock_unpin_lock" },
	{ 0xd272d446, "trace_hardirqs_on" },
	{ 0x02102df4, "complete" },
	{ 0x90ee5f62, "cgroup_taskset_first" },
	{ 0x90ee5f62, "cgroup_taskset_next" },
	{ 0x110f4d3a, "memset" },
	{ 0x73f8ac84, "cgroup_mutex" },
	{ 0x075bcc9d, "css_set_lock" },
	{ 0x4e110715, "irq_work_queue" },
	{ 0xe6b91231, "__sw_hweight16" },
	{ 0x289bdf84, "return_address" },
	{ 0x422570b1, "irq_work_queue_on" },
	{ 0xb2896223, "update_rq_clock" },
	{ 0x554acda6, "_raw_write_lock" },
	{ 0x2ebc3f3d, "___task_rq_lock" },
	{ 0x554acda6, "_raw_write_unlock" },
	{ 0x7e3e04da, "uclamp_eff_value" },
	{ 0xee17885c, "get_cpu_device" },
	{ 0xd272d446, "rebuild_sched_domains" },
	{ 0xf2a4b08a, "jiffies" },
	{ 0x759a0049, "cpu_irqtime" },
	{ 0xd8f50094, "param_ops_bool" },
	{ 0x6a1b115a, "mutex_lock_nested" },
	{ 0x5f479e46, "mutex_unlock" },
	{ 0x012404d8, "proc_dointvec_minmax" },
	{ 0xe3fc5c0e, "deactivate_task" },
	{ 0xdf087e16, "activate_task" },
	{ 0x8ca7b5cf, "wake_up_process" },
	{ 0xa8c8a0ab, "kthread_create_on_node" },
	{ 0x33758dd3, "sched_setscheduler_nocheck" },
	{ 0x57962c48, "__tracepoint_android_rvh_get_nohz_timer_target" },
	{ 0x57962c48, "__tracepoint_android_rvh_set_cpus_allowed_by_task" },
	{ 0x57962c48, "__tracepoint_android_rvh_rto_next_cpu" },
	{ 0x57962c48, "__tracepoint_android_rvh_is_cpu_allowed" },
	{ 0x5e505530, "kthread_should_stop" },
	{ 0x378551e8, "stop_one_cpu" },
	{ 0x57962c48, "__tracepoint_sched_set_state_tp" },
	{ 0xd272d446, "schedule" },
	{ 0xb2e62cba, "__trace_set_current_state" },
	{ 0x8ee1928d, "housekeeping_overridden" },
	{ 0x73f8ac84, "sched_domains_mutex" },
	{ 0x7851be11, "rcu_read_lock_any_held" },
	{ 0xe22ad35e, "housekeeping_cpumask" },
	{ 0x0339d8d7, "housekeeping_test_cpu" },
	{ 0xe7f8d838, "cpumask_any_and_distribute" },
	{ 0x4e994f70, "__cpu_dying_mask" },
	{ 0x565aac8b, "android_pick_task" },
	{ 0xd77a17e5, "lock_repin_lock" },
	{ 0x50a24e76, "select_fallback_rq" },
	{ 0x803d4fca, "__migrate_task" },
	{ 0x222609d0, "__balance_callbacks" },
	{ 0x171b2953, "atomic_notifier_chain_register" },
	{ 0x171b2953, "atomic_notifier_chain_unregister" },
	{ 0xd4faaaa4, "atomic_notifier_call_chain" },
	{ 0xff9a6e5c, "ktime_get" },
	{ 0xd04e72f2, "kobject_init" },
	{ 0x69aba01d, "kobject_add" },
	{ 0xf5197fd4, "sscanf" },
	{ 0xb22e15f0, "_raw_spin_lock_irq" },
	{ 0xb22e15f0, "_raw_spin_unlock_irq" },
	{ 0x3996d9b9, "kstrtoull" },
	{ 0x4ddca08e, "trace_seq_printf" },
	{ 0xc2c8cbfe, "trace_seq_putc" },
	{ 0x3928b381, "trace_event_buffer_reserve" },
	{ 0xd92f9d65, "trace_event_buffer_commit" },
	{ 0x7f009d7f, "__trace_trigger_soft_disabled" },
	{ 0x65967be6, "perf_trace_buf_alloc" },
	{ 0x78bb3c0e, "perf_trace_run_bpf_submit" },
	{ 0x8a7493b2, "memcpy" },
	{ 0xb5bedffb, "strnlen" },
	{ 0x8f8e273c, "sized_strscpy" },
	{ 0x31b67631, "__fortify_panic" },
	{ 0x90d19584, "strlen" },
	{ 0x9c1eb3a7, "bpf_trace_run7" },
	{ 0x2f5219fc, "bpf_trace_run8" },
	{ 0x4a656250, "bpf_trace_run5" },
	{ 0x8dd4aa04, "bpf_trace_run9" },
	{ 0x6224aed6, "bpf_trace_run3" },
	{ 0x9415e527, "bpf_trace_run1" },
	{ 0x55540de0, "bpf_trace_run11" },
	{ 0x4407aaa6, "bpf_trace_run2" },
	{ 0xb5d1b808, "bpf_trace_run12" },
	{ 0x17b4523c, "bpf_trace_run4" },
	{ 0xaac95e47, "bpf_trace_run10" },
	{ 0x29882f2f, "bpf_trace_run6" },
	{ 0xfce6449b, "trace_raw_output_prep" },
	{ 0x1767fe83, "trace_event_printf" },
	{ 0xa5d86891, "trace_handle_return" },
	{ 0x051a9b0c, "trace_event_reg" },
	{ 0x5a1c08bd, "trace_event_raw_init" },
	{ 0xc531e61f, "alloc_workqueue_noprof" },
	{ 0x1eba76c4, "lockdep_init_map_type" },
	{ 0x5c070f2e, "delayed_work_timer_fn" },
	{ 0xad0d991d, "timer_init_key" },
	{ 0x30b658e2, "freq_qos_add_request" },
	{ 0x57fe468c, "input_register_handler" },
	{ 0xcf568e40, "cancel_delayed_work_sync" },
	{ 0x7162a662, "__msecs_to_jiffies" },
	{ 0x848344c8, "queue_delayed_work_on" },
	{ 0xd272d446, "cpus_read_lock" },
	{ 0xe3d2e9ca, "freq_qos_update_request" },
	{ 0xd272d446, "cpus_read_unlock" },
	{ 0x39af717e, "input_register_handle" },
	{ 0x39af717e, "input_open_device" },
	{ 0x3354674c, "input_unregister_handle" },
	{ 0x3354674c, "input_close_device" },
	{ 0x012404d8, "proc_dointvec" },
	{ 0x012404d8, "proc_douintvec_minmax" },
	{ 0x087f5f1c, "register_sysctl_sz" },
	{ 0x012404d8, "proc_dostring" },
	{ 0xfaec86e2, "find_vpid" },
	{ 0x89659df5, "get_pid_task" },
	{ 0x2ebc3f3d, "_task_rq_lock" },
	{ 0x012404d8, "proc_doulongvec_minmax" },
	{ 0xb1efaff0, "sysctl_vals" },
	{ 0xd76dee5b, "cpufreq_register_governor" },
	{ 0xb50f8a53, "cpufreq_enable_fast_switch" },
	{ 0x2f00c83b, "__kthread_init_worker" },
	{ 0x9994c9e6, "kthread_worker_fn" },
	{ 0xfe55cc06, "kthread_bind_mask" },
	{ 0xb1285c7c, "mutex_init_lockdep" },
	{ 0x849cb39d, "gov_attr_set_init" },
	{ 0xac432b2c, "get_governor_parent_kobj" },
	{ 0xb7297e56, "kobject_init_and_add" },
	{ 0xafdc4c8d, "kobject_put" },
	{ 0x7294dabb, "kthread_stop" },
	{ 0x001875d1, "kthread_flush_worker" },
	{ 0xe41e1da6, "mutex_destroy" },
	{ 0xb50f8a53, "cpufreq_disable_fast_switch" },
	{ 0xc43b7291, "gov_attr_set_put" },
	{ 0xd272d446, "synchronize_rcu" },
	{ 0xc77fcc4d, "irq_work_sync" },
	{ 0xc88ae55f, "kthread_cancel_work_sync" },
	{ 0x8f804433, "cpufreq_driver_resolve_freq" },
	{ 0x8f804433, "cpufreq_driver_fast_switch" },
	{ 0xb13b71f3, "__cpufreq_driver_target" },
	{ 0x7a77627c, "kthread_queue_work" },
	{ 0xafb18b07, "kstrtouint" },
	{ 0xb9f815fe, "kstrtobool" },
	{ 0x8a4521ee, "kstrtoint" },
	{ 0xeac1e857, "strsep" },
	{ 0x8ee1928d, "sched_uclamp_used" },
	{ 0x20477f76, "governor_sysfs_ops" },
	{ 0x7e514d40, "strnstr" },
	{ 0x57962c48, "__tracepoint_android_rvh_show_max_freq" },
	{ 0x57962c48, "__tracepoint_android_rvh_cpu_capacity_show" },
	{ 0x57962c48, "__tracepoint_android_rvh_before_do_sched_yield" },
	{ 0xacd41f5b, "get_task_mm" },
	{ 0xe400321a, "down_read" },
	{ 0xa826c956, "mas_find" },
	{ 0x2330e616, "d_path" },
	{ 0x214ee1da, "skip_spaces" },
	{ 0xe400321a, "up_read" },
	{ 0xa718217c, "mmput" },
	{ 0xfdbae8c6, "usleep_range_state" },
	{ 0xf0fb447e, "set_task_cpu" },
	{ 0xe3fc5c0e, "wakeup_preempt" },
	{ 0xda0a175f, "stop_one_cpu_nowait" },
	{ 0x958914ad, "wake_up_if_idle" },
	{ 0x79412bc5, "double_rq_lock" },
	{ 0x571c73d5, "system_highpri_wq" },
	{ 0xb2896223, "resched_curr" },
	{ 0x57962c48, "__tracepoint_android_rvh_sched_nohz_balancer_kick" },
	{ 0x57962c48, "__tracepoint_android_rvh_can_migrate_task" },
	{ 0x57962c48, "__tracepoint_android_rvh_find_busiest_queue" },
	{ 0x57962c48, "__tracepoint_android_rvh_sched_newidle_balance" },
	{ 0x57962c48, "__tracepoint_android_rvh_find_new_ilb" },
	{ 0xf296206e, "lockdep_recursion" },
	{ 0x7ec472ba, "hardirqs_enabled" },
	{ 0xa2ea45d7, "lock_set_class" },
	{ 0x4ae4dd0b, "pick_highest_pushable_task" },
	{ 0xee32bd19, "migrate_swap" },
	{ 0x57962c48, "__tracepoint_sched_switch" },
	{ 0x57962c48, "__tracepoint_android_rvh_select_task_rq_rt" },
	{ 0x57962c48, "__tracepoint_android_rvh_find_lowest_rq" },
	{ 0xf4b631ce, "cpu_busy_with_softirqs" },
	{ 0x9966ac96, "cpupri_find_fitness" },
	{ 0x4fd35ce4, "em_cpu_get" },
	{ 0x57962c48, "__tracepoint_android_rvh_select_task_rq_fair" },
	{ 0x57962c48, "__tracepoint_android_vh_binder_set_priority" },
	{ 0x57962c48, "__tracepoint_android_vh_binder_restore_priority" },
	{ 0x57962c48, "__tracepoint_android_rvh_check_preempt_wakeup_fair" },
	{ 0x57962c48, "__tracepoint_android_rvh_replace_next_task_fair" },
	{ 0x57962c48, "__tracepoint_sched_overutilized_tp" },
	{ 0x67b6332e, "__cpuhp_setup_state" },
	{ 0x80e9f7d9, "tracepoint_probe_unregister" },
	{ 0xcc39254f, "__cpuhp_remove_state" },
	{ 0x49dd95d9, "trace_print_flags_seq" },
	{ 0x3accfef3, "kstrdup" },
	{ 0x57962c48, "__tracepoint_rwmmio_read" },
	{ 0x57962c48, "__tracepoint_rwmmio_post_read" },
	{ 0xf5e9ca2e, "log_read_mmio" },
	{ 0xec62c96b, "log_post_read_mmio" },
	{ 0x4c64f0c5, "__platform_driver_register" },
	{ 0x66375a18, "of_cpu_device_node_get" },
	{ 0xa782aeef, "platform_get_resource" },
	{ 0x848b0139, "devm_ioremap" },
	{ 0xa8b3a968, "of_device_get_match_data" },
	{ 0xa29d9ac3, "_dev_err" },
	{ 0xa407f4df, "iomem_resource" },
	{ 0x3f8a8dbd, "__devm_request_region" },
	{ 0xb9062cec, "_raw_read_lock_irqsave" },
	{ 0x67903a9b, "_raw_read_unlock_irqrestore" },
	{ 0x6fa71d2f, "__rwlock_init" },
	{ 0xecfe08b6, "cpufreq_quick_get" },
	{ 0x57962c48, "__tracepoint_android_rvh_cpufreq_transition" },
	{ 0x02d27c40, "debugfs_create_dir" },
	{ 0xe1dea6eb, "debugfs_create_u32" },
	{ 0x459b5e7e, "debugfs_create_file_full" },
	{ 0xf4386284, "snprintf" },
	{ 0xf7ec7b9a, "simple_read_from_buffer" },
	{ 0xab67cf67, "simple_open" },
	{ 0x7c48cc86, "sort" },
	{ 0x57962c48, "__tracepoint_android_vh_mutex_add_waiter_pos" },
	{ 0xbaa0f5fe, "freq_qos_remove_request" },
	{ 0x67628f51, "msleep" },
	{ 0xd8f50094, "param_ops_int" },
	{ 0x0b230c63, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0xc799d30a,
	0xf54e5881,
	0xf018baf9,
	0x16b5b21d,
	0xf296206e,
	0xf4386284,
	0xd272d446,
	0x0de4cab5,
	0x6cef033f,
	0xb8c8345a,
	0x582e858c,
	0x19d96b61,
	0x2373037a,
	0x4e994f70,
	0xbe1fa9a4,
	0xe37ba965,
	0x5e505530,
	0x1127e91b,
	0x1176ef14,
	0xc81fdd93,
	0xd272d446,
	0xe82c42ee,
	0xd272d446,
	0x959db01a,
	0x11fa8ee9,
	0x7851be11,
	0x39ef7a8e,
	0x7851be11,
	0x98d1efb5,
	0xd272d446,
	0xd0b3f3a9,
	0xe82c42ee,
	0x7851be11,
	0xf296206e,
	0xcd2684cf,
	0x6c62dbae,
	0xe82c42ee,
	0x1178ad06,
	0x57962c48,
	0x80e9f7d9,
	0x3239fbdb,
	0x571c73d5,
	0x088866de,
	0x16b5b21d,
	0x4e994f70,
	0x1eded390,
	0x2d376275,
	0x8ee1928d,
	0x5f42c804,
	0xc85a9c7f,
	0x4e994f70,
	0x6e964b9d,
	0xa4e7fec9,
	0x57962c48,
	0x80e9f7d9,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x1a5ab47f,
	0x545a6fb7,
	0x6fabd45b,
	0x31359e90,
	0x802f8919,
	0xf92c2cb2,
	0x27865ec7,
	0x7aab6508,
	0x8078ce17,
	0xf92c2cb2,
	0xb50f8a53,
	0xb98e8186,
	0xdc20867a,
	0xecd472fc,
	0x55161516,
	0x7b3da183,
	0x6971ec84,
	0x8bb63d96,
	0xf296206e,
	0x8998aa29,
	0x10aae63b,
	0x554acda6,
	0x318d87c2,
	0xb22e15f0,
	0xb9062cec,
	0x67903a9b,
	0xb22e15f0,
	0x554acda6,
	0xf5df0401,
	0xf8828461,
	0x60c331c2,
	0x9cf1d65a,
	0x1400a461,
	0x8ee1928d,
	0x992a5140,
	0x7ec472ba,
	0xd272d446,
	0xc8d5fd9d,
	0x303ae86f,
	0xd77a17e5,
	0xd272d446,
	0x02102df4,
	0x90ee5f62,
	0x90ee5f62,
	0x110f4d3a,
	0x73f8ac84,
	0x075bcc9d,
	0x4e110715,
	0xe6b91231,
	0x289bdf84,
	0x422570b1,
	0xb2896223,
	0x554acda6,
	0x2ebc3f3d,
	0x554acda6,
	0x7e3e04da,
	0xee17885c,
	0xd272d446,
	0xf2a4b08a,
	0x759a0049,
	0xd8f50094,
	0x6a1b115a,
	0x5f479e46,
	0x012404d8,
	0xe3fc5c0e,
	0xdf087e16,
	0x8ca7b5cf,
	0xa8c8a0ab,
	0x33758dd3,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x5e505530,
	0x378551e8,
	0x57962c48,
	0xd272d446,
	0xb2e62cba,
	0x8ee1928d,
	0x73f8ac84,
	0x7851be11,
	0xe22ad35e,
	0x0339d8d7,
	0xe7f8d838,
	0x4e994f70,
	0x565aac8b,
	0xd77a17e5,
	0x50a24e76,
	0x803d4fca,
	0x222609d0,
	0x171b2953,
	0x171b2953,
	0xd4faaaa4,
	0xff9a6e5c,
	0xd04e72f2,
	0x69aba01d,
	0xf5197fd4,
	0xb22e15f0,
	0xb22e15f0,
	0x3996d9b9,
	0x4ddca08e,
	0xc2c8cbfe,
	0x3928b381,
	0xd92f9d65,
	0x7f009d7f,
	0x65967be6,
	0x78bb3c0e,
	0x8a7493b2,
	0xb5bedffb,
	0x8f8e273c,
	0x31b67631,
	0x90d19584,
	0x9c1eb3a7,
	0x2f5219fc,
	0x4a656250,
	0x8dd4aa04,
	0x6224aed6,
	0x9415e527,
	0x55540de0,
	0x4407aaa6,
	0xb5d1b808,
	0x17b4523c,
	0xaac95e47,
	0x29882f2f,
	0xfce6449b,
	0x1767fe83,
	0xa5d86891,
	0x051a9b0c,
	0x5a1c08bd,
	0xc531e61f,
	0x1eba76c4,
	0x5c070f2e,
	0xad0d991d,
	0x30b658e2,
	0x57fe468c,
	0xcf568e40,
	0x7162a662,
	0x848344c8,
	0xd272d446,
	0xe3d2e9ca,
	0xd272d446,
	0x39af717e,
	0x39af717e,
	0x3354674c,
	0x3354674c,
	0x012404d8,
	0x012404d8,
	0x087f5f1c,
	0x012404d8,
	0xfaec86e2,
	0x89659df5,
	0x2ebc3f3d,
	0x012404d8,
	0xb1efaff0,
	0xd76dee5b,
	0xb50f8a53,
	0x2f00c83b,
	0x9994c9e6,
	0xfe55cc06,
	0xb1285c7c,
	0x849cb39d,
	0xac432b2c,
	0xb7297e56,
	0xafdc4c8d,
	0x7294dabb,
	0x001875d1,
	0xe41e1da6,
	0xb50f8a53,
	0xc43b7291,
	0xd272d446,
	0xc77fcc4d,
	0xc88ae55f,
	0x8f804433,
	0x8f804433,
	0xb13b71f3,
	0x7a77627c,
	0xafb18b07,
	0xb9f815fe,
	0x8a4521ee,
	0xeac1e857,
	0x8ee1928d,
	0x20477f76,
	0x7e514d40,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0xacd41f5b,
	0xe400321a,
	0xa826c956,
	0x2330e616,
	0x214ee1da,
	0xe400321a,
	0xa718217c,
	0xfdbae8c6,
	0xf0fb447e,
	0xe3fc5c0e,
	0xda0a175f,
	0x958914ad,
	0x79412bc5,
	0x571c73d5,
	0xb2896223,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0xf296206e,
	0x7ec472ba,
	0xa2ea45d7,
	0x4ae4dd0b,
	0xee32bd19,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0xf4b631ce,
	0x9966ac96,
	0x4fd35ce4,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x57962c48,
	0x67b6332e,
	0x80e9f7d9,
	0xcc39254f,
	0x49dd95d9,
	0x3accfef3,
	0x57962c48,
	0x57962c48,
	0xf5e9ca2e,
	0xec62c96b,
	0x4c64f0c5,
	0x66375a18,
	0xa782aeef,
	0x848b0139,
	0xa8b3a968,
	0xa29d9ac3,
	0xa407f4df,
	0x3f8a8dbd,
	0xb9062cec,
	0x67903a9b,
	0x6fa71d2f,
	0xecfe08b6,
	0x57962c48,
	0x02d27c40,
	0xe1dea6eb,
	0x459b5e7e,
	0xf4386284,
	0xf7ec7b9a,
	0xab67cf67,
	0x7c48cc86,
	0x57962c48,
	0xbaa0f5fe,
	0x67628f51,
	0xd8f50094,
	0x0b230c63,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"init_task\0"
	"vendor_data_pad\0"
	"sched_clock\0"
	"_printk_deferred\0"
	"nr_cpu_ids\0"
	"scnprintf\0"
	"__stack_chk_fail\0"
	"__per_cpu_offset\0"
	"runqueues\0"
	"alt_cb_patch_nops\0"
	"__put_task_struct_rcu_cb\0"
	"call_rcu\0"
	"refcount_warn_saturate\0"
	"__cpu_online_mask\0"
	"__sw_hweight64\0"
	"ksoftirqd\0"
	"rcu_is_watching\0"
	"__warn_printk\0"
	"tracepoint_srcu\0"
	"__srcu_check_read_flavor\0"
	"preempt_schedule_notrace\0"
	"cpu_scale\0"
	"__rcu_read_lock\0"
	"rcu_lock_map\0"
	"lock_acquire\0"
	"debug_lockdep_rcu_enabled\0"
	"lockdep_rcu_suspicious\0"
	"rcu_read_lock_held\0"
	"lock_release\0"
	"__rcu_read_unlock\0"
	"smp_call_function_single_async\0"
	"hw_pressure\0"
	"rcu_read_lock_sched_held\0"
	"__num_possible_cpus\0"
	"_raw_spin_lock_irqsave\0"
	"_raw_spin_unlock_irqrestore\0"
	"arch_freq_scale\0"
	"ksize\0"
	"__tracepoint_android_vh_update_topology_flags_workfn\0"
	"tracepoint_probe_register\0"
	"topology_update_done\0"
	"system_percpu_wq\0"
	"queue_work_on\0"
	"_printk\0"
	"__cpu_active_mask\0"
	"register_syscore\0"
	"kmalloc_caches\0"
	"mem_alloc_profiling_key\0"
	"__kmalloc_cache_noprof\0"
	"__raw_spin_lock_init\0"
	"__cpu_possible_mask\0"
	"root_task_group\0"
	"css_next_child\0"
	"__tracepoint_android_rvh_wake_up_new_task\0"
	"android_rvh_probe_register\0"
	"__tracepoint_android_rvh_update_cpu_capacity\0"
	"__tracepoint_android_rvh_sched_cpu_starting\0"
	"__tracepoint_android_rvh_sched_cpu_dying\0"
	"__tracepoint_android_rvh_set_task_cpu\0"
	"__tracepoint_android_rvh_new_task_stats\0"
	"__tracepoint_android_rvh_account_irq\0"
	"__tracepoint_android_rvh_flush_task\0"
	"__tracepoint_android_rvh_update_misfit_status\0"
	"__tracepoint_android_rvh_enqueue_task\0"
	"__tracepoint_android_rvh_dequeue_task\0"
	"__tracepoint_android_rvh_try_to_wake_up\0"
	"__tracepoint_android_rvh_tick_entry\0"
	"__tracepoint_android_vh_scheduler_tick\0"
	"__tracepoint_android_rvh_schedule\0"
	"__tracepoint_android_rvh_cpu_cgroup_attach\0"
	"__tracepoint_android_vh_cpu_cgroup_css_free\0"
	"__tracepoint_android_rvh_cpu_cgroup_online\0"
	"__tracepoint_android_rvh_sched_fork_init\0"
	"__tracepoint_android_rvh_ttwu_cond\0"
	"__tracepoint_android_rvh_sched_exec\0"
	"__tracepoint_android_rvh_build_perf_domains\0"
	"__tracepoint_cpu_frequency_limits\0"
	"__tracepoint_android_rvh_do_sched_yield\0"
	"__tracepoint_android_rvh_update_thermal_stats\0"
	"__tracepoint_android_vh_dup_task_struct\0"
	"__tracepoint_android_vh_freq_qos_add_request\0"
	"__tracepoint_android_vh_freq_qos_update_request\0"
	"__tracepoint_android_vh_freq_qos_remove_request\0"
	"wait_for_completion_interruptible\0"
	"cpu_topology\0"
	"__list_add_valid_or_report\0"
	"kfree\0"
	"__list_del_entry_valid_or_report\0"
	"cpufreq_cpu_get_raw\0"
	"__kmalloc_noprof\0"
	"of_get_cpu_node\0"
	"__of_parse_phandle_with_args\0"
	"cpufreq_cpu_get\0"
	"cpufreq_cpu_put\0"
	"of_find_node_opts_by_path\0"
	"of_platform_populate\0"
	"stop_machine\0"
	"sched_feat_names\0"
	"match_string\0"
	"sched_feat_keys\0"
	"static_key_disable\0"
	"sysctl_sched_features\0"
	"topology_clear_scale_freq_source\0"
	"tasklist_lock\0"
	"_raw_read_lock\0"
	"_raw_spin_lock_nested\0"
	"_raw_spin_lock\0"
	"_raw_write_lock_irqsave\0"
	"_raw_write_unlock_irqrestore\0"
	"_raw_spin_unlock\0"
	"_raw_read_unlock\0"
	"strcmp\0"
	"raw_spin_rq_lock_nested\0"
	"lock_is_held_type\0"
	"system_32bit_el0_cpumask\0"
	"_find_next_bit\0"
	"__scx_enabled\0"
	"ext_sched_class\0"
	"debug_locks\0"
	"trace_hardirqs_off\0"
	"lock_pin_lock\0"
	"balance_push_callback\0"
	"lock_unpin_lock\0"
	"trace_hardirqs_on\0"
	"complete\0"
	"cgroup_taskset_first\0"
	"cgroup_taskset_next\0"
	"memset\0"
	"cgroup_mutex\0"
	"css_set_lock\0"
	"irq_work_queue\0"
	"__sw_hweight16\0"
	"return_address\0"
	"irq_work_queue_on\0"
	"update_rq_clock\0"
	"_raw_write_lock\0"
	"___task_rq_lock\0"
	"_raw_write_unlock\0"
	"uclamp_eff_value\0"
	"get_cpu_device\0"
	"rebuild_sched_domains\0"
	"jiffies\0"
	"cpu_irqtime\0"
	"param_ops_bool\0"
	"mutex_lock_nested\0"
	"mutex_unlock\0"
	"proc_dointvec_minmax\0"
	"deactivate_task\0"
	"activate_task\0"
	"wake_up_process\0"
	"kthread_create_on_node\0"
	"sched_setscheduler_nocheck\0"
	"__tracepoint_android_rvh_get_nohz_timer_target\0"
	"__tracepoint_android_rvh_set_cpus_allowed_by_task\0"
	"__tracepoint_android_rvh_rto_next_cpu\0"
	"__tracepoint_android_rvh_is_cpu_allowed\0"
	"kthread_should_stop\0"
	"stop_one_cpu\0"
	"__tracepoint_sched_set_state_tp\0"
	"schedule\0"
	"__trace_set_current_state\0"
	"housekeeping_overridden\0"
	"sched_domains_mutex\0"
	"rcu_read_lock_any_held\0"
	"housekeeping_cpumask\0"
	"housekeeping_test_cpu\0"
	"cpumask_any_and_distribute\0"
	"__cpu_dying_mask\0"
	"android_pick_task\0"
	"lock_repin_lock\0"
	"select_fallback_rq\0"
	"__migrate_task\0"
	"__balance_callbacks\0"
	"atomic_notifier_chain_register\0"
	"atomic_notifier_chain_unregister\0"
	"atomic_notifier_call_chain\0"
	"ktime_get\0"
	"kobject_init\0"
	"kobject_add\0"
	"sscanf\0"
	"_raw_spin_lock_irq\0"
	"_raw_spin_unlock_irq\0"
	"kstrtoull\0"
	"trace_seq_printf\0"
	"trace_seq_putc\0"
	"trace_event_buffer_reserve\0"
	"trace_event_buffer_commit\0"
	"__trace_trigger_soft_disabled\0"
	"perf_trace_buf_alloc\0"
	"perf_trace_run_bpf_submit\0"
	"memcpy\0"
	"strnlen\0"
	"sized_strscpy\0"
	"__fortify_panic\0"
	"strlen\0"
	"bpf_trace_run7\0"
	"bpf_trace_run8\0"
	"bpf_trace_run5\0"
	"bpf_trace_run9\0"
	"bpf_trace_run3\0"
	"bpf_trace_run1\0"
	"bpf_trace_run11\0"
	"bpf_trace_run2\0"
	"bpf_trace_run12\0"
	"bpf_trace_run4\0"
	"bpf_trace_run10\0"
	"bpf_trace_run6\0"
	"trace_raw_output_prep\0"
	"trace_event_printf\0"
	"trace_handle_return\0"
	"trace_event_reg\0"
	"trace_event_raw_init\0"
	"alloc_workqueue_noprof\0"
	"lockdep_init_map_type\0"
	"delayed_work_timer_fn\0"
	"timer_init_key\0"
	"freq_qos_add_request\0"
	"input_register_handler\0"
	"cancel_delayed_work_sync\0"
	"__msecs_to_jiffies\0"
	"queue_delayed_work_on\0"
	"cpus_read_lock\0"
	"freq_qos_update_request\0"
	"cpus_read_unlock\0"
	"input_register_handle\0"
	"input_open_device\0"
	"input_unregister_handle\0"
	"input_close_device\0"
	"proc_dointvec\0"
	"proc_douintvec_minmax\0"
	"register_sysctl_sz\0"
	"proc_dostring\0"
	"find_vpid\0"
	"get_pid_task\0"
	"_task_rq_lock\0"
	"proc_doulongvec_minmax\0"
	"sysctl_vals\0"
	"cpufreq_register_governor\0"
	"cpufreq_enable_fast_switch\0"
	"__kthread_init_worker\0"
	"kthread_worker_fn\0"
	"kthread_bind_mask\0"
	"mutex_init_lockdep\0"
	"gov_attr_set_init\0"
	"get_governor_parent_kobj\0"
	"kobject_init_and_add\0"
	"kobject_put\0"
	"kthread_stop\0"
	"kthread_flush_worker\0"
	"mutex_destroy\0"
	"cpufreq_disable_fast_switch\0"
	"gov_attr_set_put\0"
	"synchronize_rcu\0"
	"irq_work_sync\0"
	"kthread_cancel_work_sync\0"
	"cpufreq_driver_resolve_freq\0"
	"cpufreq_driver_fast_switch\0"
	"__cpufreq_driver_target\0"
	"kthread_queue_work\0"
	"kstrtouint\0"
	"kstrtobool\0"
	"kstrtoint\0"
	"strsep\0"
	"sched_uclamp_used\0"
	"governor_sysfs_ops\0"
	"strnstr\0"
	"__tracepoint_android_rvh_show_max_freq\0"
	"__tracepoint_android_rvh_cpu_capacity_show\0"
	"__tracepoint_android_rvh_before_do_sched_yield\0"
	"get_task_mm\0"
	"down_read\0"
	"mas_find\0"
	"d_path\0"
	"skip_spaces\0"
	"up_read\0"
	"mmput\0"
	"usleep_range_state\0"
	"set_task_cpu\0"
	"wakeup_preempt\0"
	"stop_one_cpu_nowait\0"
	"wake_up_if_idle\0"
	"double_rq_lock\0"
	"system_highpri_wq\0"
	"resched_curr\0"
	"__tracepoint_android_rvh_sched_nohz_balancer_kick\0"
	"__tracepoint_android_rvh_can_migrate_task\0"
	"__tracepoint_android_rvh_find_busiest_queue\0"
	"__tracepoint_android_rvh_sched_newidle_balance\0"
	"__tracepoint_android_rvh_find_new_ilb\0"
	"lockdep_recursion\0"
	"hardirqs_enabled\0"
	"lock_set_class\0"
	"pick_highest_pushable_task\0"
	"migrate_swap\0"
	"__tracepoint_sched_switch\0"
	"__tracepoint_android_rvh_select_task_rq_rt\0"
	"__tracepoint_android_rvh_find_lowest_rq\0"
	"cpu_busy_with_softirqs\0"
	"cpupri_find_fitness\0"
	"em_cpu_get\0"
	"__tracepoint_android_rvh_select_task_rq_fair\0"
	"__tracepoint_android_vh_binder_set_priority\0"
	"__tracepoint_android_vh_binder_restore_priority\0"
	"__tracepoint_android_rvh_check_preempt_wakeup_fair\0"
	"__tracepoint_android_rvh_replace_next_task_fair\0"
	"__tracepoint_sched_overutilized_tp\0"
	"__cpuhp_setup_state\0"
	"tracepoint_probe_unregister\0"
	"__cpuhp_remove_state\0"
	"trace_print_flags_seq\0"
	"kstrdup\0"
	"__tracepoint_rwmmio_read\0"
	"__tracepoint_rwmmio_post_read\0"
	"log_read_mmio\0"
	"log_post_read_mmio\0"
	"__platform_driver_register\0"
	"of_cpu_device_node_get\0"
	"platform_get_resource\0"
	"devm_ioremap\0"
	"of_device_get_match_data\0"
	"_dev_err\0"
	"iomem_resource\0"
	"__devm_request_region\0"
	"_raw_read_lock_irqsave\0"
	"_raw_read_unlock_irqrestore\0"
	"__rwlock_init\0"
	"cpufreq_quick_get\0"
	"__tracepoint_android_rvh_cpufreq_transition\0"
	"debugfs_create_dir\0"
	"debugfs_create_u32\0"
	"debugfs_create_file_full\0"
	"snprintf\0"
	"simple_read_from_buffer\0"
	"simple_open\0"
	"sort\0"
	"__tracepoint_android_vh_mutex_add_waiter_pos\0"
	"freq_qos_remove_request\0"
	"msleep\0"
	"param_ops_int\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");

