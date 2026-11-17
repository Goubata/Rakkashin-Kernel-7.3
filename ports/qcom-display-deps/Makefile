# SPDX-License-Identifier: GPL-2.0-only
# qcom-soc 6.12 sources (canoe_perf.bzl): CONFIG_GH_IRQ_LEND=m, CONFIG_QCOM_FSA4480_I2C=m,
# CONFIG_QCOM_WCD_USBSS_I2C=m; imported by msm_drm (DP altmode audio switches, SDE VM irq lending)
obj-m += gh_irq_lend.o fsa4480_i2c.o wcd_usbss_i2c.o
fsa4480_i2c-y := fsa4480-i2c.o
wcd_usbss_i2c-y := wcd-usbss-regmap.o wcd-usbss-tables.o wcd-usbss-utils.o wcd939x-i2c.o
ccflags-y += -I$(src)/include -I$(src)/../gunyah/include -I$(src)/../gunyah-rm -I$(src)/../gunyah-rm/include -I$(src)/../secure-buffer/include -I$(src)/../qcom-iommu-util/include -I$(src)/../qcom-spmi/include
ccflags-y += -Wno-missing-prototypes -DCONFIG_GH_RM_DRV_MODULE=1 -DCONFIG_GH_IRQ_LEND_MODULE=1 \
	-DCONFIG_QCOM_FSA4480_I2C_MODULE=1 -DCONFIG_QCOM_WCD_USBSS_I2C_MODULE=1 -DCONFIG_GH_ARM64_DRV_MODULE=1 -DCONFIG_GH_DBL_MODULE=1 -DCONFIG_GH_MSGQ_MODULE=1 -DCONFIG_GUNYAH_MODULE=1 -DCONFIG_GUNYAH_QCOM_TRUSTED_VM_MODULE=1
CFLAGS_gh_irq_lend.o += -include $(src)/../gunyah-rm/avr-rm-deps.h
KBUILD_EXTRA_SYMBOLS += $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah/Module.symvers \
	$(src)/../qcom-charging/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../secure-buffer/Module.symvers
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers
CFLAGS_gh_irq_lend.o += -include $(src)/avr-gh-irq-lend-noavf.h
