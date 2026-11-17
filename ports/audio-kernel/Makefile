# SPDX-License-Identifier: GPL-2.0-only
# AVR 7.3 port of vendor/qcom/opensource/audio-kernel (canoe, product avr).
# Module/source list: Makefile.generated (scripts/gen_audio_makefile.py).
include $(src)/Makefile.generated
ccflags-y += $(AVR_AUDIO_DEFINES)
ccflags-y += -I$(src) -I$(src)/include -I$(src)/include/uapi/audio -I$(src)/asoc/codecs/tfa98xx/inc \
	-I$(src)/asoc/codecs/aw882xx/inc -I$(src)/config
ccflags-y += -I$(src)/../qcom-socinfo/include -I$(src)/../qcom-ipc-logging/include -I$(src)/../pinctrl-alor-vendor-core/include -I$(src)/../qcom-spmi/include -I$(src)/../qcom-charging/include -I$(src)/../qcom-display-deps/include -I$(src)/../qcom-minidump/include -I$(src)/../qcom-dma-heaps/include -I$(src)/../qcom-iommu-util/include -I$(src)/../secure-buffer/include -I$(src)/../qcom-glink-transport/include -I$(src)/../mm-drivers/include -I$(src)/../gunyah/include -I$(src)/../gunyah-rm/include 
ccflags-y += -Wno-missing-prototypes
ccflags-y += -I$(src)/avr-include -I$(src)/../mm-drivers/msm_ext_display/include
ccflags-y += -include $(src)/avr-canoe-vendor-config.h -include $(src)/avr-audio-compat.h
# vendor files define pr_fmt after force-included headers pulled printk.h in
ccflags-y += -Wno-macro-redefined
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-spmi/Module.symvers \
	$(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers \
	$(src)/../qcom-charging/Module.symvers $(src)/../mm-drivers/Module.symvers \
	$(src)/../qcom-socinfo/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers \
	$(src)/../qcom-pmic-glink/Module.symvers $(src)/../qcom-smem/Module.symvers \
	$(src)/../qcom-glink-transport/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers \
	$(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers \
	$(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers \
	$(src)/../qcom-usb/Module.symvers $(src)/../sched-walt/Module.symvers
