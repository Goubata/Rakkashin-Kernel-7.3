# SPDX-License-Identifier: GPL-2.0-only
obj-m += gcc-alor.o
ccflags-y += -I$(src)/include
# Header for the staged hold-state implementation; apply its full core patch
# before integrating this provider into a kernel image.
ccflags-y += -include $(src)/../ccf-hold-state/compat.h
