# SPDX-License-Identifier: GPL-2.0-only
# vendor/qcom/opensource/wlan/platform for canoe (icnss2 target): the modules the phone loads.
# Per-module CONFIG_* from <module>/canoe_gki_defconfig (wlan_platform_modules.bzl; the
# _v01/plat_ipc modules use cnss_utils' defconfig). cnss2 (PCIe) is built for canoe but not loaded.

obj-m += icnss2.o
icnss2-y := icnss2/main.o icnss2/debug.o icnss2/power.o icnss2/genl.o icnss2/qmi.o
CFLAGS_icnss2/main.o += -DCONFIG_ICNSS2_MODULE=1 -DCONFIG_ICNSS2_QMI=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_WCNSS_MEM_PRE_ALLOC_MODULE=1 -DCONFIG_CNSS2_SMMU_DB_SUPPORT=1 -DCONFIG_CNSS2_SSR_DRIVER_DUMP=1 -I$(src)/icnss2 -I$(src)/cnss_utils
CFLAGS_icnss2/debug.o += -DCONFIG_ICNSS2_MODULE=1 -DCONFIG_ICNSS2_QMI=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_WCNSS_MEM_PRE_ALLOC_MODULE=1 -DCONFIG_CNSS2_SMMU_DB_SUPPORT=1 -DCONFIG_CNSS2_SSR_DRIVER_DUMP=1 -I$(src)/icnss2 -I$(src)/cnss_utils
CFLAGS_icnss2/power.o += -DCONFIG_ICNSS2_MODULE=1 -DCONFIG_ICNSS2_QMI=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_WCNSS_MEM_PRE_ALLOC_MODULE=1 -DCONFIG_CNSS2_SMMU_DB_SUPPORT=1 -DCONFIG_CNSS2_SSR_DRIVER_DUMP=1 -I$(src)/icnss2 -I$(src)/cnss_utils
CFLAGS_icnss2/genl.o += -DCONFIG_ICNSS2_MODULE=1 -DCONFIG_ICNSS2_QMI=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_WCNSS_MEM_PRE_ALLOC_MODULE=1 -DCONFIG_CNSS2_SMMU_DB_SUPPORT=1 -DCONFIG_CNSS2_SSR_DRIVER_DUMP=1 -I$(src)/icnss2 -I$(src)/cnss_utils
CFLAGS_icnss2/qmi.o += -DCONFIG_ICNSS2_MODULE=1 -DCONFIG_ICNSS2_QMI=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_WCNSS_MEM_PRE_ALLOC_MODULE=1 -DCONFIG_CNSS2_SMMU_DB_SUPPORT=1 -DCONFIG_CNSS2_SSR_DRIVER_DUMP=1 -I$(src)/icnss2 -I$(src)/cnss_utils
obj-m += cnss_nl.o
cnss_nl-y := cnss_genl/cnss_nl.o
CFLAGS_cnss_genl/cnss_nl.o += -DCONFIG_CNSS_GENL_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1
obj-m += cnss_prealloc.o
cnss_prealloc-y := cnss_prealloc/cnss_prealloc.o
CFLAGS_cnss_prealloc/cnss_prealloc.o += -DCONFIG_WCNSS_MEM_PRE_ALLOC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -I$(src)/cnss_utils
obj-m += cnss_utils.o
cnss_utils-y := cnss_utils/cnss_utils.o
CFLAGS_cnss_utils/cnss_utils.o += -DCONFIG_CNSS_UTILS_MODULE=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_PLAT_IPC_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_FEATURE_SMEM_MAILBOX=1
obj-m += wlan_firmware_service.o
wlan_firmware_service-y := cnss_utils/wlan_firmware_service_v01.o cnss_utils/device_management_service_v01.o cnss_utils/ip_multimedia_subsystem_private_service_v01.o
CFLAGS_cnss_utils/wlan_firmware_service_v01.o += -DCONFIG_CNSS_UTILS_MODULE=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_PLAT_IPC_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_FEATURE_SMEM_MAILBOX=1
CFLAGS_cnss_utils/device_management_service_v01.o += -DCONFIG_CNSS_UTILS_MODULE=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_PLAT_IPC_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_FEATURE_SMEM_MAILBOX=1
CFLAGS_cnss_utils/ip_multimedia_subsystem_private_service_v01.o += -DCONFIG_CNSS_UTILS_MODULE=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_PLAT_IPC_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_FEATURE_SMEM_MAILBOX=1
obj-m += cnss_plat_ipc_qmi_svc.o
cnss_plat_ipc_qmi_svc-y := cnss_utils/cnss_plat_ipc_qmi.o cnss_utils/cnss_plat_ipc_service_v01.o
CFLAGS_cnss_utils/cnss_plat_ipc_qmi.o += -DCONFIG_CNSS_UTILS_MODULE=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_PLAT_IPC_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_FEATURE_SMEM_MAILBOX=1
CFLAGS_cnss_utils/cnss_plat_ipc_service_v01.o += -DCONFIG_CNSS_UTILS_MODULE=1 -DCONFIG_CNSS_QMI_SVC_MODULE=1 -DCONFIG_CNSS_PLAT_IPC_QMI_SVC_MODULE=1 -DCONFIG_CNSS_OUT_OF_TREE=1 -DCONFIG_FEATURE_SMEM_MAILBOX=1
ccflags-y += -I$(src)/inc -I$(src)/../qcom-wlan-deps/include -I$(src)/../qcom-ipc-logging/include \
	-I$(src)/../qcom-socinfo/include -I$(src)/../qcom-rproc-common/include -I$(src)/../qcom-remoteproc-services/include \
	-I$(src)/../qcom-minidump/include -I$(src)/../qcom-debug-diag/include -Wno-missing-prototypes \
	-I$(src)/../secure-buffer/include -I$(src)/../securemsm/include -I$(src)/../pinctrl-alor-vendor-core/include -I$(src)/../qcom-dcvs/include -I$(src)/../qcom-iommu-util/include
ccflags-y += -include $(src)/avr-canoe-vendor-config.h -include $(src)/../qcom-rproc-common/include/linux/remoteproc/qcom_rproc_vendor.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers \
	$(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-rproc-common/Module.symvers\
	$(src)/../qcom-remoteproc-services/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers \
	$(src)/../qcom-smem/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers
