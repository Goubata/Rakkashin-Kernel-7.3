# SPDX-License-Identifier: GPL-2.0-only
obj-m += mmi_annotate.o mmi_info.o
mmi_annotate-y := mmi_annotate/mmi_annotate.o
mmi_info-y := mmi_info/mmi_info_main.o mmi_info/mmi_boot_info.o mmi_info/mmi_ram_info.o mmi_info/mmi_storage_info.o mmi_info/mmi_unit_info.o
ccflags-y += -I$(src)/include -I$(src)/../qcom-minidump/include -I$(src)/../qcom-smem/include
ccflags-y += -DCONFIG_QCOM_MINIDUMP_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-smem/Module.symvers
