/* SPDX-License-Identifier: GPL-2.0-only */
/* AVR 7.3: ports/qcom-llcc keys its slice tables by the vendor (firmware)
 * use-case ids; 7.3's public llcc-qcom.h has no display ids. Values from the
 * vendor header (LLCC_DISP 16, LLCC_DISP_WB 53, LLCC_DISP_1 54). */
#ifndef AVR_LLCC_DISPLAY_H
#define AVR_LLCC_DISPLAY_H
#include "../qcom-llcc/llcc-vendor-private.h"
#define LLCC_DISP	AVR_LLCC_DISP
#define LLCC_DISP_WB	AVR_LLCC_DISP_WB
#define LLCC_DISP_1	AVR_LLCC_DISP_1
#endif
