/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */
/* Motorola private hardware layout; 7.3 public slice descriptor stays authoritative. */
#ifndef AVR_LLCC_VENDOR_PRIVATE_H
#define AVR_LLCC_VENDOR_PRIVATE_H
#include <linux/soc/qcom/llcc-qcom.h>
#define LLCC_VERSION_2_0_0_0          0x02000000
#define LLCC_VERSION_2_1_0_0          0x02010000
#define LLCC_VERSION_3_1_0_0          0x03010000
#define LLCC_VERSION_4_1_0_0          0x04010000
#define LLCC_VERSION_5_0_0_0          0x05000000
#define LLCC_VERSION_6_0_0_0          0x06000000

#define AVR_LLCC_CPUSS 1
#define AVR_LLCC_VIDSC0 2
#define AVR_LLCC_VIDSC1 3
#define AVR_LLCC_ROTATOR 4
#define AVR_LLCC_VOICE 5
#define AVR_LLCC_AUDIO 6
#define AVR_LLCC_MDMHPGRW 7
#define AVR_LLCC_MDM 8
#define AVR_LLCC_MODHW 9
#define AVR_LLCC_CMPT 10
#define AVR_LLCC_GPUHTW 11
#define AVR_LLCC_GPU 12
#define AVR_LLCC_MMUHWT 13
#define AVR_LLCC_CMPTDMA 15
#define AVR_LLCC_DISP 16
#define AVR_LLCC_VIDFW 17
#define AVR_LLCC_CAMFW 18
#define AVR_LLCC_MDMHPFX 20
#define AVR_LLCC_MDMPNG 21
#define AVR_LLCC_AUDHW 22
#define AVR_LLCC_NPU 23
#define AVR_LLCC_WLHW 24
#define AVR_LLCC_PIMEM 25
#define AVR_LLCC_ECC 26
#define AVR_LLCC_DISP_VIG 27
#define AVR_LLCC_CVP 28
#define AVR_LLCC_MODPE 29
#define AVR_LLCC_APTCM 30
#define AVR_LLCC_WRCACHE 31
#define AVR_LLCC_CVPFW 32
#define AVR_LLCC_CPUSS1 33
#define AVR_LLCC_CAMEXP0 34
#define AVR_LLCC_CPUMTE 35
#define AVR_LLCC_CPUHWT 36
#define AVR_LLCC_MDMCLAD2 37
#define AVR_LLCC_CAMEXP1 38
#define AVR_LLCC_CMPTHCP 39
#define AVR_LLCC_LCPDARE 40
#define AVR_LLCC_MDM_CLD_2_1 41
#define AVR_LLCC_MDM_CLD_2_2 42
#define AVR_LLCC_MDM_CLD_2_3 43
#define AVR_LLCC_MDM_CLD_2_4 44
#define AVR_LLCC_AENPU 45
#define AVR_LLCC_ISLAND1 46
#define AVR_LLCC_ISLAND2 47
#define AVR_LLCC_ISLAND3 48
#define AVR_LLCC_ISLAND4 49
#define AVR_LLCC_CAMEXP2 50
#define AVR_LLCC_CAMEXP3 51
#define AVR_LLCC_CAMEXP4 52
#define AVR_LLCC_DISP_WB 53
#define AVR_LLCC_DISP_1 54
#define AVR_LLCC_SAIL 55
#define AVR_LLCC_GDSP 56
#define AVR_LLCC_VIDSC_LFT 57
#define AVR_LLCC_VIDSC_RGT 58
#define AVR_LLCC_GPU_MV 59
#define AVR_LLCC_EVA_LEFT 60
#define AVR_LLCC_EVA_RIGHT 61
#define AVR_LLCC_EVA_GAIN 62
#define AVR_LLCC_VIDDEPTH 63
#define AVR_LLCC_VIDVSP 64
#define AVR_LLCC_DISP_LFT 65
#define AVR_LLCC_DISP_RGT 66
#define AVR_LLCC_EVACSC_LFT 67
#define AVR_LLCC_EVACSC_RGT 68
#define AVR_LLCC_EVA3DR 69
#define AVR_LLCC_VIDDEC 70
#define AVR_LLCC_CAMOFE 71
#define AVR_LLCC_CAMRTIP 72
#define AVR_LLCC_CAMSRTIP 73
#define AVR_LLCC_CAMRTRF 74
#define AVR_LLCC_CAMSRTRF 75
#define AVR_LLCC_NW_DESC 76
#define AVR_LLCC_NW_DATA 77
#define AVR_LLCC_CUST_CORE 78
#define AVR_LLCC_APPS_SEC 79
#define AVR_LLCC_GPU_LITTLE 80
#define AVR_LLCC_OOBM_NS 81
#define AVR_LLCC_OOBM_S 82
#define AVR_LLCC_CPUSSMPAM 89
#define AVR_LLCC_CPU_MTE 35
#define AVR_LLCC_VIDEO_APV 83
#define AVR_LLCC_COMPUTE1 87
#define AVR_LLCC_CPUSS_OPP 88
#define AVR_LLCC_CPUSS_MPAM1 89
#define AVR_LLCC_PARTIALWRITES 90
#define AVR_LLCC_CAM_IPE_STROV 92
#define AVR_LLCC_CAM_OFE_STROV 93
#define AVR_LLCC_CPUSS_HEU 94
#define AVR_LLCC_MDM_PNG_FIXED 100

struct avr_llcc_edac_reg_data {
	char *name;
	u32 reg_cnt;
	u32 count_mask;
	u32 ways_mask;
	u8  count_shift;
	u8  ways_shift;
};

struct avr_llcc_edac_reg_offset {
	/* LLCC TRP registers */
	u32 trp_ecc_error_status0;
	u32 trp_ecc_error_status1;
	u32 trp_ecc_sb_err_syn0;
	u32 trp_ecc_db_err_syn0;
	u32 trp_ecc_error_cntr_clear;
	u32 trp_interrupt_0_status;
	u32 trp_interrupt_0_clear;
	u32 trp_interrupt_0_enable;

	/* LLCC Common registers */
	u32 cmn_status0;
	u32 cmn_interrupt_0_enable;
	u32 cmn_interrupt_2_enable;

	/* LLCC DRP registers */
	u32 drp_ecc_error_cfg;
	u32 drp_ecc_error_cntr_clear;
	u32 drp_interrupt_status;
	u32 drp_interrupt_clear;
	u32 drp_interrupt_enable;
	u32 drp_ecc_error_status0;
	u32 drp_ecc_error_status1;
	u32 drp_ecc_sb_err_syn0;
	u32 drp_ecc_db_err_syn0;
};

struct avr_llcc_drv_data {
	struct regmap **regmaps;
	struct regmap *bcast_regmap;
	struct regmap *bcast_and_regmap;
	const struct llcc_slice_config *cfg;
	const struct avr_llcc_edac_reg_offset *edac_reg_offset;
	struct mutex lock;
	u32 cfg_index;
	u32 cfg_size;
	u32 max_slices;
	u32 num_banks;
	unsigned long *bitmap;
	int ecc_irq;
	u32 version;
	bool cap_based_alloc_and_pwr_collapse;
	struct llcc_slice_desc *desc;
};

enum llcc_staling_mode {
	LLCC_STALING_MODE_CAPACITY, /* Default option on reset */
	LLCC_STALING_MODE_NOTIFY,
	LLCC_STALING_MODE_MAX
};

enum llcc_staling_notify_op {
	LLCC_NOTIFY_STALING_WRITEBACK,
	LLCC_NOTIFY_STALING_NO_WRITEBACK,
	LLCC_NOTIFY_STALING_OPS_MAX
};

struct llcc_staling_mode_params {
	enum llcc_staling_mode staling_mode;
	union {
		/* STALING_MODE_CAPACITY needs no params */
		struct staling_mode_notify_params {
			u8 staling_distance;
			enum llcc_staling_notify_op op;
		} notify_params;
	};
};

int llcc_configure_staling_mode(struct llcc_slice_desc *desc,
                             struct llcc_staling_mode_params *p);
int llcc_notif_staling_inc_counter(struct llcc_slice_desc *desc);
static_assert(AVR_LLCC_GPU == LLCC_GPU);
static_assert(AVR_LLCC_GPUHTW == LLCC_GPUHTW);
static_assert(AVR_LLCC_GPU_MV == LLCC_GPUMV);
#endif
