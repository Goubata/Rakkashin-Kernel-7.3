# SPDX-License-Identifier: GPL-2.0-only
# Encore FAS (https://github.com/rem01project/encore_fas, commit f408a00),
# Android 7.3: uprobe API 3, hrtimer_setup, soft hrtimer modes.
obj-$(CONFIG_ENCORE_FAS) += encore_fas.o
encore_fas-y := encore_fas_main.o \
                encore_fas_ctx.o \
                encore_fas_evq.o \
                encore_fas_time.o

ccflags-y += -I$(src)/include -DFAS_VERSION=9 -DFAS_BRANCH=\"main-f408a00\"
ccflags-y += -DFAS_UPROBE_API=3 -DFAS_HAVE_HRTIMER_SOFT
