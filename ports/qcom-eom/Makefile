# SPDX-License-Identifier: GPL-2.0-only
# qcom-soc drivers/eom (canoe_perf CONFIG_EOM_MSM=m; canoe PCIe EOM phy from
# CONFIG_ARCH_CANOE conditional_srcs) and drivers/eom/pcie/pci_phy_access.
obj-m += eom_driver.o pci_phy_access.o
eom_driver-y := phy_core.o eom_driver_core.o buffer_manager.o pcie_eom_canoe_phy.o
pci_phy_access-y := pcie/pci_phy_access.o
ccflags-y += -I$(src) -I$(src)/include -I$(src)/include/uapi -Wno-missing-prototypes \
	-DCONFIG_EOM_MSM_MODULE=1 -DCONFIG_ARCH_CANOE=1
