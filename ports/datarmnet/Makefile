# SPDX-License-Identifier: GPL-2.0-only
# vendor/qcom/opensource/datarmnet (core: rmnet_core, rmnet_ctl) and datarmnet-ext
# (aps, offload, perf, perf_tether, sch, shs, wlan) for canoe on android18-7.3.
# Sources from define_modules.bzl / ext/*/define_*.bzl; rmnet_ctl adds rmnet_ctl_client.c
# (CONFIG_ARCH_CANOE). Kconfig defaults: RMNET_LA_PLATFORM=y on canoe.

obj-m += rmnet_ctl.o rmnet_core.o
rmnet_ctl-y := core/rmnet_ctl_ipa.o core/rmnet_ctl_client.o
rmnet_core-y := core/rmnet_config.o core/rmnet_descriptor.o core/rmnet_genl.o core/rmnet_handlers.o core/rmnet_map_command.o core/rmnet_map_data.o core/rmnet_module.o core/rmnet_vnd.o core/dfc_qmap.o core/dfc_qmi.o core/qmi_rmnet.o core/wda_qmi.o core/rmnet_ll.o core/rmnet_ll_ipa.o core/rmnet_qmap.o core/rmnet_ll_qmap.o
obj-m += rmnet_aps.o
rmnet_aps-y := ext/aps/rmnet_aps_genl.o ext/aps/rmnet_aps_main.o
obj-m += rmnet_offload.o
rmnet_offload-y := ext/offload/rmnet_offload_engine.o ext/offload/rmnet_offload_knob.o ext/offload/rmnet_offload_main.o ext/offload/rmnet_offload_state.o ext/offload/rmnet_offload_stats.o ext/offload/rmnet_offload_tcp.o ext/offload/rmnet_offload_udp.o
obj-m += rmnet_perf.o
rmnet_perf-y := ext/perf/rmnet_perf_main.o ext/perf/rmnet_perf_tcp.o ext/perf/rmnet_perf_udp.o
obj-m += rmnet_perf_tether.o
rmnet_perf_tether-y := ext/perf_tether/rmnet_perf_tether_main.o
obj-m += rmnet_sch.o
rmnet_sch-y := ext/sch/rmnet_sch_main.o
obj-m += rmnet_shs.o
rmnet_shs-y := ext/shs/rmnet_shs_common.o ext/shs/rmnet_shs_config.o ext/shs/rmnet_shs_freq.o ext/shs/rmnet_shs_ll.o ext/shs/rmnet_shs_main.o ext/shs/rmnet_shs_modules.o ext/shs/rmnet_shs_wq.o ext/shs/rmnet_shs_wq_genl.o ext/shs/rmnet_shs_wq_mem.o
obj-m += rmnet_wlan.o
rmnet_wlan-y := ext/wlan/rmnet_wlan_connection.o ext/wlan/rmnet_wlan_fragment.o ext/wlan/rmnet_wlan_genl.o ext/wlan/rmnet_wlan_main.o ext/wlan/rmnet_wlan_stats.o
# phone rmnet_core/rmnet_ctl import ipam: built with the dataipa IPA3=m config
ccflags-y += -DCONFIG_IPA3_MODULE=1 -DCONFIG_SCHED_WALT_MODULE=1 -DCONFIG_SCHED_WALT_NR_CPUS=8 -DCONFIG_RMNET_LA_PLATFORM=1 -DCONFIG_RMNET_CORE_MODULE=1 -DCONFIG_RMNET_CTL_MODULE=1 -DRMNET_TRACE_INCLUDE_PATH=.
ccflags-y += -I$(src)/core -I$(src)/ext/aps/include -I$(src)/ext/perf/include -I$(src)/ext/shs/include -I$(src)/ext/wlan/include -I$(src)/ext/offload/include -I$(src)/ext/perf_tether/include -I$(src)/ext/sch/include -I$(src)/../dataipa/drivers/platform/msm/include \
	-I$(src)/../dataipa/drivers/platform/msm/include/linux -I$(src)/../dataipa/drivers/platform/msm/include/uapi \
	-I$(src)/../dataipa/avr-include -I$(src)/../dataipa/rmnet_mem -I$(src)/../qcom-ipc-logging/include \
	-I$(src)/../sched-walt/include -I$(src)/../sched-walt
ccflags-y += -Wno-missing-prototypes -Wno-misleading-indentation -Wno-macro-redefined
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../dataipa/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers \
	$(src)/../sched-walt/Module.symvers
