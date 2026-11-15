// SPDX-License-Identifier: GPL-2.0-only
#include "test-api.h"
#define qcom_smem_get avr_info_smem_get
#define qcom_smem_alloc avr_info_smem_alloc
#define proc_create avr_info_proc_create
#include "../../ports/moto-info/mmi_info/mmi_unit_info.c"
