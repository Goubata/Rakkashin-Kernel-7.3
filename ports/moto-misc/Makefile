# SPDX-License-Identifier: GPL-2.0-only
# Motorola vendor modules from motorola-kernel-modules (MMI-W3WWL36.65-11-18)
# feature defines reverse-engineered from the phone .ko imports/strings
obj-m += utags.o mmi_sys_temp.o moto_binder.o moto_mmap_fault.o sx937x_sar.o awinic_sar.o
obj-m += cam_aw37004.o anc_fps_mmi.o stm_st54se_gpio.o goodix_fod_mmi.o
utags-y := misc/utag/utags.o
mmi_sys_temp-y := misc/mmi_sys_temp/mmi_sys_temp.o
moto_binder-y := moto_binder/moto_binder_main.o moto_binder/moto_binder_netfilter.o moto_binder/moto_binder_status.o moto_binder/moto_binder_sysfs.o
moto_mmap_fault-y := moto_mmap_fault/moto_mmap_fault_core.o
sx937x_sar-y := misc/sarsensor/semtech/sx937x/sx937x_sar_core.o
awinic_sar-y := misc/sarsensor/awinic/aw963xx/aw_sar.o misc/sarsensor/awinic/aw963xx/aw_sar_comm_interface.o misc/sarsensor/awinic/aw963xx/aw963xx.o
cam_aw37004-y := regulator/cam_aw37004/aw37004-regulator.o
anc_fps_mmi-y := input/misc/anc_fps_mmi/custom.o input/misc/anc_fps_mmi/jiiov_netlink.o input/misc/anc_fps_mmi/jiiov_platform.o
stm_st54se_gpio-y := ese/st54spi_gpio/st54spi_gpio.o
goodix_fod_mmi-y := input/misc/goodix_fod_mmi/gf_spi.o input/misc/goodix_fod_mmi/netlink.o input/misc/goodix_fod_mmi/platform.o
ccflags-y += -I$(srctree) -I$(src)/include -I$(src)/../moto-touch-deps/include -I$(src)/../qcom-panel-event/include -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../moto-touch-deps/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-smem/Module.symvers
CFLAGS_input/misc/goodix_fod_mmi/platform.o += -include $(src)/include/avr_gpio_compat.h
CFLAGS_input/misc/anc_fps_mmi/jiiov_platform.o += -include $(src)/include/avr_gpio_compat.h
