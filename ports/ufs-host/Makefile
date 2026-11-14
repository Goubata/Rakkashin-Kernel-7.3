# SPDX-License-Identifier: GPL-2.0-only
obj-m += ufs-qcom.o ufshcd-crypto-qti.o
ufs-qcom-y := ufs-qcom-main.o vendor/ufs_hid_jedec.o
ccflags-y += -I$(srctree)/drivers/ufs/core -I$(src)/../qcom-clock-core/include -I$(src)/include -I$(src) -I$(srctree) -I$(src)/../ufs-phy-canoe/include
ccflags-y += -DCONFIG_SCSI_UFS_CRYPTO_QTI_MODULE=1 -DCONFIG_IPC_LOGGING=1 -DCONFIG_QCOM_MINIDUMP=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ice/Module.symvers $(src)/../ufs-phy-canoe/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-clock-core/Module.symvers
