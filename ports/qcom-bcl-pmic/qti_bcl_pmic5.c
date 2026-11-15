// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spmi.h>
#include <linux/slab.h>
#include <linux/nvmem-consumer.h>
#include <linux/ipc_logging.h>
#include "drivers/thermal/thermal_core.h"
#include <linux/unaligned.h>
#include "qti_bcl_common.h"
#include <linux/of_platform.h>

#define BCL_DRIVER_NAME       "bcl_pmic5"
#define MAX_BCL_NAME_LENGTH   40
#define BCL_MONITOR_EN        0x46
#define BCL_IRQ_STATUS        0x08
#define BCL_REVISION1         0x0
#define BCL_REVISION2         0x01
#define BCL_PARAM_1           0x0e
#define BCL_PARAM_2           0x0f

#define BCL_IBAT_HIGH         0x4B
#define BCL_IBAT_TOO_HIGH     0x4C
#define BCL_IBAT_TOO_HIGH_REV4 0x4D
#define BCL_IBAT_READ         0x86
#define BCL_IBAT_SCALING_UA   78127
#define BCL_IBAT_CCM_SCALING_UA   15625
#define BCL_IBAT_CCM_LANDO_SCALING_UA   152
#define BCL_ADC_IBAT_CCM_LANDO_SCALING_UA   39062
#define BCL_IBAT_SCALING_REV4_UA  93753

#define BCL_VBAT_READ         0x76
#define BCL_VBAT_CONV_REQ     0x72

#define BCL_GEN3_MAJOR_REV    4
#define BCL_PARAM_HAS_ADC      BIT(0)
#define BCL_PARAM_HAS_IBAT_ADC BIT(2)
#define ADC_CMN_BG_ADC_DVDD_SPARE1 0x3050

#define BCL_IRQ_L0       0x1
#define BCL_IRQ_L1       0x2
#define BCL_IRQ_L2       0x4

/*
 * 49827 = 64.879uV (one bit value) * 3 (voltage divider)
 *		* 256 (8 bit shift for MSB)
 */
#define BCL_VBAT_SCALING_UV   49827
#define BCL_VBAT_NO_READING   127
#define BCL_VBAT_BASE_MV      2000
#define BCL_VBAT_INC_MV       25
#define BCL_VBAT_MAX_MV       3600
#define BCL_VBAT_THRESH_BASE  0x8CA

#define BCL_IBAT_CCM_OFFSET   800
#define BCL_IBAT_CCM_LSB      100
#define BCL_IBAT_CCM_MAX_VAL  14

#define BCL_IBAT_CCM_LANDO_OFFSET   900
#define BCL_IBAT_CCM_LANDO_LSB      150
#define BCL_IBAT_CCM_LANDO_MAX_VAL  16

#define BCL_VBAT_4G_NO_READING 0x7fff
#define BCL_GEN4_MAJOR_REV    5
#define BCL_VBAT_SCALING_REV5_NV   194637  /* 64.879uV (one bit) * 3 VD */
#define BCL_IBAT_SCALING_REV5_NA   61037
#define BCL_IBAT_THRESH_SCALING_REV5_UA   156255L /* 610.37uA * 256 */
#define BCL_VBAT_TRIP_CNT     3

#define BCL_IBAT_COTTID_SCALING 366220L

#define BCL_TRIGGER_THRESHOLD 1
#define MAX_PERPH_COUNT       3
#define IPC_LOGPAGES          10

#define BPM_EN_OFFSET 0xA0
#define BPM_MAX_IBAT_OFFSET 0xA2
#define BPM_SYNC_VBAT_OFFSET 0xA4
#define BPM_MIN_VBAT_OFFSET 0xA6
#define BPM_SYNC_IBAT_OFFSET 0xA8
#define BCL_LVL0_CNT_OFFSET 0xAA
#define BCL_LVL1_CNT_OFFSET 0xAB
#define BCL_LVL2_CNT_OFFSET 0xAC
#define BPM_HOLD 0x81
#define BPM_CLR 0x80
#define EXTEND_BIT 15

#define BCL_IPC(dev, msg, args...)      do { \
			if ((dev) && (dev)->ipc_log) { \
				ipc_log_string((dev)->ipc_log, \
					"[%s]: %s: " msg, \
					current->comm, __func__, args); \
			} \
		} while (0)

enum bcl_monitor_type {
	BCL_MON_DEFAULT,
	BCL_MON_VBAT_ONLY,
	BCL_MON_IBAT_ONLY,
	BCL_MON_MAX,
};

enum {
	BCLBIG_COMP_VCMP_L0_THR,
	BCLBIG_COMP_VCMP_L1_THR,
	BCLBIG_COMP_VCMP_L2_THR,
	REG_MAX,
};

struct bcl_desc {
	bool vadc_type;
	u32 vbat_regs[REG_MAX];
	bool vbat_zone_enabled;
	u32 vcmp_thresh_base;
	u32 vcmp_thresh_max;
	u32 ibat_scaling_factor;
	u32 ibat_thresh_scaling_factor;
};

static char bcl_int_names[BCL_TYPE_MAX][25] = {
	"bcl-ibat-lvl0",
	"bcl-ibat-lvl1",
	"bcl-vbat-lvl0",
	"bcl-vbat-lvl1",
	"bcl-vbat-lvl2",
	"bcl-lvl0",
	"bcl-lvl1",
	"bcl-lvl2",
	"bcl-2s-ibat-lvl0",
	"bcl-2s-ibat-lvl1",
};

enum bcl_ibat_ext_range_type {
	BCL_IBAT_RANGE_LVL0,
	BCL_IBAT_RANGE_LVL1,
	BCL_IBAT_RANGE_LVL2,
	BCL_IBAT_RANGE_MAX,
};

static uint32_t bcl_ibat_ext_ranges[BCL_IBAT_RANGE_MAX] = {
	10,		/* default range factor */
	20,
	25
};

static BLOCKING_NOTIFIER_HEAD(bcl_pmic5_notifier);

static unsigned int boot_seq = 0;
static void mmi_get_boot_seq(void)
{
	struct device_node *n = of_find_node_by_path("/chosen");

	if (n == NULL)
		return;

	of_property_read_u32(n, "mmi,boot_seq", &boot_seq);
	of_node_put(n);
}

static int bcl_pmic5_notifier_register(struct notifier_block *n)
{
	return blocking_notifier_chain_register(&bcl_pmic5_notifier, n);
}

static void bcl_pmic5_notifier_unregister(struct notifier_block *n)
{
	blocking_notifier_chain_unregister(&bcl_pmic5_notifier, n);
}

static int bcl_read_multi_register(struct bcl_device *bcl_perph, int16_t reg_offset,
				void *data, size_t len)
{
	int ret = 0;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	ret = regmap_bulk_read(bcl_perph->regmap,
			       (bcl_perph->fg_bcl_addr + reg_offset),
			       data, len);
	if (ret < 0)
		pr_err("Error reading reg base:0x%04x len:%ld err:%d\n",
				bcl_perph->fg_bcl_addr + reg_offset, len, ret);
	else
		pr_debug("Read register:0x%04x len:%ld\n",
				bcl_perph->fg_bcl_addr + reg_offset,
				len);

	return ret;
}

static int bcl_read_register(struct bcl_device *bcl_perph, int16_t reg_offset,
				unsigned int *data)
{
	int ret = 0;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	ret = regmap_read(bcl_perph->regmap,
			       (bcl_perph->fg_bcl_addr + reg_offset),
			       data);
	if (ret < 0)
		pr_err("Error reading register 0x%04x err:%d\n",
				bcl_perph->fg_bcl_addr + reg_offset, ret);
	else
		pr_debug("Read register:0x%04x value:0x%02x\n",
				bcl_perph->fg_bcl_addr + reg_offset,
				*data);

	return ret;
}

static int bcl_write_register(struct bcl_device *bcl_perph,
				int16_t reg_offset, uint8_t data)
{
	int  ret = 0;
	uint8_t *write_buf = &data;
	uint16_t base;

	if (!bcl_perph) {
		pr_err("BCL device not initialized\n");
		return -EINVAL;
	}
	base = bcl_perph->fg_bcl_addr;
	ret = regmap_write(bcl_perph->regmap, (base + reg_offset), *write_buf);
	if (ret < 0) {
		pr_err("Error reading register:0x%04x val:0x%02x err:%d\n",
				base + reg_offset, data, ret);
		return ret;
	}
	pr_debug("wrote 0x%02x to 0x%04x\n", data, base + reg_offset);

	return ret;
}

static void convert_vbat_thresh_val_to_adc(struct bcl_device *bcl_perph, int *val)
{
	/*
	 * Threshold register can be bit shifted from ADC MSB.
	 * So the scaling factor is half in those cases.
	 */
	if (bcl_perph->no_bit_shift)
		*val = ((s64)*val * 1000) / BCL_VBAT_SCALING_UV;
	else
		*val = ((s64)*val * 2000) / BCL_VBAT_SCALING_UV;
}

/* Common helper to convert nano unit to milli unit */
static void convert_adc_nu_to_mu_val(int *val, int scaling_factor)
{
	*val = div_s64((s64)*val * (s64)scaling_factor, 1000000);
}

static void convert_adc_to_vbat_val(int *val)
{
	*val = ((s64)*val * BCL_VBAT_SCALING_UV) / 1000;
}

static void convert_vbat_to_vcmp_val(int vbat, int *val)
{
	if (vbat > BCL_VBAT_MAX_MV)
		vbat = BCL_VBAT_MAX_MV;
	else if (vbat < BCL_VBAT_THRESH_BASE)
		vbat = BCL_VBAT_THRESH_BASE;

	*val = (vbat - BCL_VBAT_THRESH_BASE) / BCL_VBAT_INC_MV;
}

static void convert_ibat_to_adc_val(struct bcl_device *bcl_perph, int *val, int scaling_factor)
{
	/*
	 * Threshold register can be bit shifted from ADC MSB.
	 * So the scaling factor is half in those cases.
	 */
	if (bcl_perph->ibat_use_qg_adc)
		*val = (int)div_s64((s64)*val * 2000 * 2, scaling_factor);
	else if (bcl_perph->no_bit_shift)
		*val = (int)div_s64((s64)*val * 1000 * bcl_ibat_ext_ranges[BCL_IBAT_RANGE_LVL0],
				scaling_factor);
	else
		*val = (int)div_s64((s64)*val * 2000 * bcl_ibat_ext_ranges[BCL_IBAT_RANGE_LVL0],
				scaling_factor);

}

static void convert_adc_to_ibat_val(struct bcl_device *bcl_perph, int *val, int scaling_factor)
{
	/* Scaling factor will be half if ibat_use_qg_adc is true */
	if (bcl_perph->ibat_use_qg_adc)
		*val = (int)div_s64((s64)*val * scaling_factor, 2 * 1000);
	else
		*val = (int)div_s64((s64)*val * scaling_factor,
				1000 * bcl_ibat_ext_ranges[BCL_IBAT_RANGE_LVL0]);
}

static int8_t convert_ibat_to_ccm_val(int ibat,
	int ibat_ccm_max_val, int ibat_ccm_offset, int ibat_ccm_lsb)
{
	s64 val = div_s64((s64)ibat - ibat_ccm_offset, ibat_ccm_lsb);
	val = max_t(s64, val, 0);

	if (val > ibat_ccm_max_val) {
		pr_err(
		"CCM thresh:%d is invalid, use MAX supported threshold\n",
			ibat);
		val = ibat_ccm_max_val;
	}

	return val;
}

static int bcl_set_ibat(struct thermal_zone_device *tz, int low, int high)
{
	int ret = 0, ibat_ua, thresh_value;
	int8_t val = 0;
	int16_t addr;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)thermal_zone_device_priv(tz);

	if (READ_ONCE(bat_data->dev->stopping)) return high == INT_MAX ? 0 : -ENODEV;
	if (READ_ONCE(bat_data->dev->frozen)) return -EBUSY;

	mutex_lock(&bat_data->state_trans_lock);
	if (READ_ONCE(bat_data->dev->stopping) || READ_ONCE(bat_data->dev->frozen)) {
		ret = READ_ONCE(bat_data->dev->stopping) ? (high == INT_MAX ? 0 : -ENODEV) : -EBUSY;
		goto set_trip_exit;
	}
	thresh_value = high;
	if (bat_data->trip_thresh == thresh_value)
		goto set_trip_exit;

	if (bat_data->irq_num && bat_data->irq_enabled) {
		disable_irq_nosync(bat_data->irq_num);
		bat_data->irq_enabled = false;
	}
	if (thresh_value == INT_MAX) {
		bat_data->trip_thresh = thresh_value;
		goto set_trip_exit;
	}

	ibat_ua = thresh_value;
	if (bat_data->dev->ibat_ccm_enabled)
		convert_ibat_to_adc_val(bat_data->dev, &thresh_value,
				BCL_IBAT_CCM_SCALING_UA *
				bat_data->dev->ibat_ext_range_factor);
	else if (bat_data->dev->ibat_ccm_lando_enabled)
		convert_ibat_to_adc_val(bat_data->dev, &thresh_value,
				BCL_ADC_IBAT_CCM_LANDO_SCALING_UA *
				bat_data->dev->ibat_ext_range_factor);
	else if (bat_data->dev->dig_major >= BCL_GEN4_MAJOR_REV)
		convert_ibat_to_adc_val(bat_data->dev, &thresh_value,
				bat_data->dev->desc->ibat_thresh_scaling_factor *
				bat_data->dev->ibat_ext_range_factor);
	else if (bat_data->dev->dig_major >= BCL_GEN3_MAJOR_REV)
		convert_ibat_to_adc_val(bat_data->dev, &thresh_value,
				BCL_IBAT_SCALING_REV4_UA *
				bat_data->dev->ibat_ext_range_factor);
	else
		convert_ibat_to_adc_val(bat_data->dev, &thresh_value,
				BCL_IBAT_SCALING_UA *
				bat_data->dev->ibat_ext_range_factor);

	val = (int8_t)thresh_value;
	switch (bat_data->type) {
	case BCL_IBAT_LVL0:
	case BCL_2S_IBAT_LVL0:
		addr = BCL_IBAT_HIGH;
		pr_err("ibat high threshold:%d mA ADC:0x%02x\n",
				ibat_ua, val);
		break;
	case BCL_IBAT_LVL1:
	case BCL_2S_IBAT_LVL1:
		addr = BCL_IBAT_TOO_HIGH;
		if (bat_data->dev->dig_major >= BCL_GEN3_MAJOR_REV &&
			bat_data->dev->bcl_param_1 & BCL_PARAM_HAS_IBAT_ADC)
			addr = BCL_IBAT_TOO_HIGH_REV4;
		if (bat_data->dev->ibat_ccm_enabled)
			val = convert_ibat_to_ccm_val(ibat_ua,
				BCL_IBAT_CCM_MAX_VAL, BCL_IBAT_CCM_OFFSET, BCL_IBAT_CCM_LSB);
		if (bat_data->dev->ibat_ccm_lando_enabled)
			val = convert_ibat_to_ccm_val(ibat_ua,
				BCL_IBAT_CCM_LANDO_MAX_VAL, BCL_IBAT_CCM_LANDO_OFFSET,
				BCL_IBAT_CCM_LANDO_LSB);
		pr_err("ibat too high threshold:%d mA ADC:0x%02x\n",
				ibat_ua, val);
		break;
	default:
		ret = -EINVAL;
		goto set_trip_exit;
	}
	ret = bcl_write_register(bat_data->dev, addr, val);
	if (ret)
		goto set_trip_exit;
	bat_data->trip_thresh = ibat_ua;

	if (bat_data->irq_num && !bat_data->irq_enabled) {
		enable_irq(bat_data->irq_num);
		bat_data->irq_enabled = true;
	}

set_trip_exit:
	mutex_unlock(&bat_data->state_trans_lock);

	return ret;
}

static int bcl_read_ibat_locked(struct thermal_zone_device *tz, int *adc_value)
{
	int ret = 0;
	unsigned int val = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)thermal_zone_device_priv(tz);
	struct bcl_device *bcl_perph = (struct bcl_device *)bat_data->dev;

	if (READ_ONCE(bat_data->dev->stopping)) return -ENODEV;
	if (bat_data->dev->dig_major < BCL_GEN4_MAJOR_REV)
		ret = bcl_read_register(bat_data->dev, BCL_IBAT_READ, &val);
	else
		ret = bcl_read_multi_register(bat_data->dev, BCL_IBAT_READ, &val, 2);

	if (ret)
		return ret;

	/* IBat ADC reading is in 2's compliment form */
	if (bat_data->dev->dig_major < BCL_GEN4_MAJOR_REV)
		*adc_value = sign_extend32(val, 7);
	else
		*adc_value = sign_extend32(val, 15);
	if (val == 0) {
		/*
		 * The sensor sometime can read a value 0 if there is
		 * consequtive reads
		 */
		*adc_value = bat_data->last_val;
	} else {
		if (bat_data->dev->ibat_ccm_enabled)
			convert_adc_to_ibat_val(bat_data->dev, adc_value,
				BCL_IBAT_CCM_SCALING_UA *
				bat_data->dev->ibat_ext_range_factor);
		else if (bat_data->dev->ibat_ccm_lando_enabled)
			convert_adc_to_ibat_val(bat_data->dev, adc_value,
				BCL_IBAT_CCM_LANDO_SCALING_UA *
				bat_data->dev->ibat_ext_range_factor);
		else if (bat_data->dev->dig_major >= BCL_GEN4_MAJOR_REV)
			convert_adc_nu_to_mu_val(adc_value,
				bat_data->dev->desc->ibat_thresh_scaling_factor);
		else if (bat_data->dev->dig_major >= BCL_GEN3_MAJOR_REV)
			convert_adc_to_ibat_val(bat_data->dev, adc_value,
				BCL_IBAT_SCALING_REV4_UA *
				bat_data->dev->ibat_ext_range_factor);
		else
			convert_adc_to_ibat_val(bat_data->dev, adc_value,
				BCL_IBAT_SCALING_UA *
					bat_data->dev->ibat_ext_range_factor);
		bat_data->last_val = *adc_value;
	}
	pr_debug("ibat:%d mA ADC:0x%02x\n", bat_data->last_val, val);
	BCL_IPC(bat_data->dev, "ibat:%d mA ADC:0x%02x\n",
		 bat_data->last_val, val);

	if (!bcl_perph->enable_bpm)
		return ret;

	ret = get_bpm_stats(bcl_perph, &bcl_perph->bpm_stats);

	return ret;
}

static int bcl_read_ibat(struct thermal_zone_device *tz, int *value)
{
 struct bcl_peripheral_data *p = thermal_zone_device_priv(tz);
 int ret, sample;
 mutex_lock(&p->state_trans_lock);
 ret = bcl_read_ibat_locked(tz, &sample);
 if (!ret) *value = sample;
 mutex_unlock(&p->state_trans_lock);
 return ret;
}

static int bcl_read_vbat_tz_locked(struct thermal_zone_device *tzd, int *adc_value)
{
	int ret = 0;
	unsigned int val = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)thermal_zone_device_priv(tzd);

	if (READ_ONCE(bat_data->dev->stopping)) return -ENODEV;
	if (bat_data->dev->dig_major < BCL_GEN4_MAJOR_REV)
		ret = bcl_read_register(bat_data->dev, BCL_VBAT_READ, &val);
	else
		ret = bcl_read_multi_register(bat_data->dev, BCL_VBAT_READ,
				&val, 2);

	if (ret)
		return ret;

	*adc_value = val;
	if ((bat_data->dev->dig_major < BCL_GEN4_MAJOR_REV &&
			*adc_value == BCL_VBAT_NO_READING) ||
		(bat_data->dev->dig_major >= BCL_GEN4_MAJOR_REV &&
			*adc_value == BCL_VBAT_4G_NO_READING)) {
		*adc_value = bat_data->last_val;
	} else {
		if (bat_data->dev->dig_major < BCL_GEN4_MAJOR_REV)
			convert_adc_to_vbat_val(adc_value);
		else
			convert_adc_nu_to_mu_val(adc_value,
				BCL_VBAT_SCALING_REV5_NV);
		bat_data->last_val = *adc_value;
	}

	pr_debug("vbat:%d mv ADC:0x%02x\n",
			bat_data->last_val, val);
	BCL_IPC(bat_data->dev, "vbat:%d mv ADC:0x%02x\n",
			bat_data->last_val, val);

	return ret;
}

static int bcl_read_vbat_tz(struct thermal_zone_device *tz, int *value)
{
 struct bcl_peripheral_data *p = thermal_zone_device_priv(tz);
 int ret, sample;
 mutex_lock(&p->state_trans_lock);
 ret = bcl_read_vbat_tz_locked(tz, &sample);
 if (!ret) *value = sample;
 mutex_unlock(&p->state_trans_lock);
 return ret;
}

static int bcl_set_adc_value(struct bcl_device *bcl_perph,
				int addr_idx, int temp, int *val)
{
	int ret = 0;
	int16_t addr;
	int thresh = temp;

	if (temp <= 0) {
		pr_err("Invalid input temp\n");
		return -EINVAL;
	} else if (temp < BCL_VBAT_THRESH_BASE) {
		pr_err("input temp is %d, lower than MIN\n", temp);
		return -EINVAL;
	} else if (temp > BCL_VBAT_MAX_MV) {
		pr_err("input temp is %d, higher than MAX\n", temp);
		return -EINVAL;
	}

	if (addr_idx < 0 || addr_idx >= REG_MAX) return -EINVAL;
	addr = bcl_perph->desc->vbat_regs[addr_idx];
	if ((addr_idx == BCLBIG_COMP_VCMP_L0_THR) &&
				bcl_perph->desc->vadc_type) {
		convert_vbat_thresh_val_to_adc(bcl_perph, &thresh);
		*val = thresh;
	} else {
		convert_vbat_to_vcmp_val(temp, val);
	}

	ret = bcl_write_register(bcl_perph, addr, *val);
	pr_err("threshold:%d mV ADC:0x%x\n", temp, *val);
	BCL_IPC(bcl_perph, "threshold:%d mV ADC:0x%x\n", temp, *val);
	return ret;
}

static int bcl_config_vph_cb(struct notifier_block *nb,
				unsigned long val, void *data)
{
	int ret = NOTIFY_OK;
	int *temp = (int *)data;
	int reg_data = 0;
	struct bcl_device *bcl_priv =
			container_of(nb, struct bcl_device, nb);

	if (READ_ONCE(bcl_priv->stopping) || !temp || val >= REG_MAX) return NOTIFY_DONE;
	ret = bcl_set_adc_value(bcl_priv, val, *temp, &reg_data);
	if (ret < 0)
		pr_err("Fail to set vph regs, err: %d\n", ret);

	pr_err("trip_id: %lu, vph:%d mV, ADC: 0x%x\n", val, *temp, reg_data);

	return notifier_from_errno(ret);
}

static int bcl_write_vbat_tz(struct thermal_zone_device *tzd,
					const struct thermal_trip *trip, int temp)
{
	int ret = 0, val = 0, trip_id = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)thermal_zone_device_priv(tzd);

	if (READ_ONCE(bat_data->dev->stopping)) return -ENODEV;
	trip_id = trip_to_trip_desc(trip) - tzd->trips;

	mutex_lock(&bat_data->state_trans_lock);
	if (READ_ONCE(bat_data->dev->stopping)) { ret = -ENODEV; goto exit; }
	ret = bcl_set_adc_value(bat_data->dev, trip_id, temp, &val);
	if (ret < 0) {
		pr_err("Fail to set vbat regs, err: %d\n", ret);
		goto exit;
	}

	if (bat_data->dev->desc->vbat_zone_enabled)
		ret = notifier_to_errno(blocking_notifier_call_chain(
			&bcl_pmic5_notifier, trip_id, &temp));

	pr_err("trip_id: %d, vbat:%d mV, ADC: 0x%x\n", trip_id, temp, val);

exit:
	mutex_unlock(&bat_data->state_trans_lock);
	return ret;
}

static struct thermal_zone_device_ops vbat_tzd_ops = {
	.get_temp = bcl_read_vbat_tz,
	.set_trip_temp = bcl_write_vbat_tz,
};

static int bcl_get_trend(struct thermal_zone_device *tz, const struct thermal_trip *trips,
			enum thermal_trend *trend)
{
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)thermal_zone_device_priv(tz);

	mutex_lock(&bat_data->state_trans_lock);
	if (!bat_data->last_val)
		*trend = THERMAL_TREND_DROPPING;
	else
		*trend = THERMAL_TREND_RAISING;
	mutex_unlock(&bat_data->state_trans_lock);

	return 0;
}

static int bcl_set_lbat(struct thermal_zone_device *tz, int low, int high)
{
	uint32_t bcl_lvl = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)thermal_zone_device_priv(tz);
	struct bcl_device *bcl_perph = bat_data->dev;

	if (READ_ONCE(bcl_perph->stopping)) return high == INT_MAX ? 0 : -ENODEV;
	if (READ_ONCE(bcl_perph->frozen)) return -EBUSY;
	mutex_lock(&bat_data->state_trans_lock);
	if (READ_ONCE(bcl_perph->stopping) || READ_ONCE(bcl_perph->frozen)) {
		mutex_unlock(&bat_data->state_trans_lock);
		return READ_ONCE(bcl_perph->stopping) ? (high == INT_MAX ? 0 : -ENODEV) : -EBUSY;
	}

	bcl_lvl = bat_data->type - BCL_LVL0;
	if (bcl_lvl >= MAX_BCL_LVL_COUNT) {
		pr_err("Invalid sensor type level:%d\n", bat_data->type);
		mutex_unlock(&bat_data->state_trans_lock);
		return -EINVAL;
	}

	if (high != BCL_TRIGGER_THRESHOLD &&
		bat_data->irq_num && bat_data->irq_enabled) {
		disable_irq_nosync(bat_data->irq_num);
		if (bat_data->irq_wake) { disable_irq_wake(bat_data->irq_num); bat_data->irq_wake = false; }
		bat_data->irq_enabled = false;

		pr_debug("lbat[%d]: disable irq:%d low: %d high: %d\n",
				bat_data->type,
				bat_data->irq_num,
				low, high);
	} else if (high == BCL_TRIGGER_THRESHOLD &&
		bat_data->irq_num && !bat_data->irq_enabled) {
		bcl_update_clear_stats(&bcl_perph->stats[bcl_lvl]);
		int ret = enable_irq_wake(bat_data->irq_num);
		if (ret) { mutex_unlock(&bat_data->state_trans_lock); return ret; }
		bat_data->irq_wake = true;
		enable_irq(bat_data->irq_num);
		bat_data->irq_enabled = true;
		pr_debug("lbat[%d]: enable irq:%d low: %d high: %d\n",
				bat_data->type,
				bat_data->irq_num,
				low, high);
		BCL_IPC(bcl_perph,
		"Irq %d cleared for bcl type %s.Rearm irq.low: %d, high: %d, irq_count: %d\n",
				bat_data->irq_num,
				bcl_int_names[bat_data->type],
				low, high, bcl_perph->stats[bcl_lvl].counter);
	}

	mutex_unlock(&bat_data->state_trans_lock);

	return 0;
}

static int bcl_read_lbat_locked(struct thermal_zone_device *tz, int *adc_value)
{
	int ret = 0;
	unsigned int val = 0;
	struct bcl_peripheral_data *bat_data =
		(struct bcl_peripheral_data *)thermal_zone_device_priv(tz);
	struct bcl_device *bcl_perph = bat_data->dev;

	if (READ_ONCE(bcl_perph->stopping)) return -ENODEV;
	ret = bcl_read_register(bcl_perph, BCL_IRQ_STATUS, &val);
	if (ret)
		goto bcl_read_exit;
	switch (bat_data->type) {
	case BCL_LVL0:
		*adc_value = val & BCL_IRQ_L0;
		break;
	case BCL_LVL1:
		*adc_value = val & BCL_IRQ_L1;
		break;
	case BCL_LVL2:
		*adc_value = val & BCL_IRQ_L2;
		break;
	default:
		pr_err("Invalid sensor type:%d\n", bat_data->type);
		ret = -ENODEV;
		goto bcl_read_exit;
	}
	bat_data->last_val = *adc_value;
	pr_debug("lbat:%d irq_status:%d lvl_val:%d\n", bat_data->type,
			val, bat_data->last_val);
	/* IRQ handler records auxiliary current/voltage once. Re-reading them
	 * here adds no level information and nests independent sensor locks. */
        pr_debug("LVLbat:%d val:%d\n", bat_data->type,
                        bat_data->last_val);
	BCL_IPC(bcl_perph, "LVLbat:%d irq_status:%d val:%d\n", bat_data->type,
			val, bat_data->last_val);

	if (!bcl_perph->enable_bpm)
		return ret;

	ret = get_bpm_stats(bcl_perph, &bcl_perph->bpm_stats);

bcl_read_exit:
	return ret;
}

static int bcl_read_lbat(struct thermal_zone_device *tz, int *value)
{
 struct bcl_peripheral_data *p = thermal_zone_device_priv(tz);
 int ret, sample;
 mutex_lock(&p->state_trans_lock);
 ret = bcl_read_lbat_locked(tz, &sample);
 if (!ret) *value = sample;
 mutex_unlock(&p->state_trans_lock);
 return ret;
}

int get_bpm_stats(struct bcl_device *bcl, struct bcl_bpm_stats *stats)
{
 struct bcl_bpm_stats snapshot = {};
 u8 raw[11]; int ret, release_ret;
 mutex_lock(&bcl->stats_lock);
 if (READ_ONCE(bcl->stopping)) { ret = -ENODEV; goto out; }
 ret = bcl_write_register(bcl, BPM_EN_OFFSET, BPM_HOLD);
 if (ret) goto out;
 ret = bcl_read_multi_register(bcl, BPM_MAX_IBAT_OFFSET, raw, sizeof(raw));
 release_ret = bcl_write_register(bcl, BPM_EN_OFFSET, BPM_CLR);
 if (!ret) ret = release_ret;
 if (ret) goto out;
 snapshot.max_ibat_adc = get_unaligned_le16(raw);
 snapshot.sync_vbat_adc = get_unaligned_le16(raw + 2);
 snapshot.min_vbat_adc = get_unaligned_le16(raw + 4);
 snapshot.sync_ibat_adc = get_unaligned_le16(raw + 6);
 snapshot.lvl0_cnt = raw[8]; snapshot.lvl1_cnt = raw[9]; snapshot.lvl2_cnt = raw[10];
 snapshot.max_ibat = snapshot.max_ibat_adc; snapshot.sync_vbat = snapshot.sync_vbat_adc;
 snapshot.min_vbat = snapshot.min_vbat_adc; snapshot.sync_ibat = snapshot.sync_ibat_adc;
 convert_adc_nu_to_mu_val(&snapshot.max_ibat, bcl->desc->ibat_scaling_factor);
 convert_adc_nu_to_mu_val(&snapshot.sync_vbat, BCL_VBAT_SCALING_REV5_NV);
 convert_adc_nu_to_mu_val(&snapshot.min_vbat, BCL_VBAT_SCALING_REV5_NV);
 convert_adc_nu_to_mu_val(&snapshot.sync_ibat, bcl->desc->ibat_scaling_factor);
 *stats = snapshot; bcl->last_bpm_read_ts = sched_clock();
out:
 mutex_unlock(&bcl->stats_lock); return ret;
}

static irqreturn_t bcl_handle_irq(int irq, void *data)
{
	struct bcl_peripheral_data *perph_data =
		(struct bcl_peripheral_data *)data;
	unsigned int irq_status = 0;
	int ibat = 0, vbat = 0, ret = 0;
	uint32_t bcl_lvl = 0;


	struct bcl_device *bcl_perph;
	unsigned long long start_ts = 0, end_ts = 0;

	bcl_perph = perph_data->dev;
	if (READ_ONCE(bcl_perph->stopping) || !perph_data->tz_dev) return IRQ_HANDLED;
	bcl_lvl = perph_data->type - BCL_LVL0;
	if (bcl_read_register(bcl_perph, BCL_IRQ_STATUS, &irq_status)) return IRQ_HANDLED;
	if (bcl_perph->param[BCL_IBAT_LVL0].tz_dev)
		bcl_read_ibat(bcl_perph->param[BCL_IBAT_LVL0].tz_dev, &ibat);
	else if (bcl_perph->param[BCL_2S_IBAT_LVL0].tz_dev)
		bcl_read_ibat(bcl_perph->param[BCL_2S_IBAT_LVL0].tz_dev, &ibat);
	if (bcl_perph->param[BCL_VBAT_LVL0].tz_dev)
		bcl_read_vbat_tz(bcl_perph->param[BCL_VBAT_LVL0].tz_dev, &vbat);

	if (bcl_lvl >= MAX_BCL_LVL_COUNT) {
		pr_err("Invalid sensor type level:%d\n", perph_data->type);
		return IRQ_HANDLED;
	}

	if (irq_status & perph_data->status_bit_idx) {
		start_ts = sched_clock();
		thermal_zone_device_update(perph_data->tz_dev,
				THERMAL_TRIP_VIOLATED);
		end_ts = sched_clock();

		switch (perph_data->type) {
		case BCL_LVL0:
			if (irq_status & BCL_IRQ_L0) atomic_long_inc(&bcl_perph->irq_events[0]);
			break;
		case BCL_LVL1:
			if (irq_status & BCL_IRQ_L1) atomic_long_inc(&bcl_perph->irq_events[1]);
			break;
		case BCL_LVL2:
			if (irq_status & BCL_IRQ_L2) atomic_long_inc(&bcl_perph->irq_events[2]);
			break;
		default:
			break;
		}

		pr_err(
		"Irq:%d triggered for bcl type:%s. status:%u, LVL0_cnt %lu, LVL1_cnt %lu, LVL2_cnt %lu, boot_seq %u, ibat=%d vbat=%d\n",
			irq, bcl_int_names[perph_data->type],
			irq_status, atomic_long_read(&bcl_perph->irq_events[0]), atomic_long_read(&bcl_perph->irq_events[1]), atomic_long_read(&bcl_perph->irq_events[2]), boot_seq, ibat, vbat);
		BCL_IPC(bcl_perph,
		"Irq:%d triggered for bcl type:%s. status:%u ibat=%d vbat=%d\n",
			irq, bcl_int_names[perph_data->type],
			irq_status, ibat, vbat);
		if (bcl_perph->enable_bpm) {
			BCL_IPC(bcl_perph,
			"BPM:lvl0:%d lvl1:%d lvl2:%d max_ibat:%d sync_vbat:%d min_vbat:%d sync_ibat:%d,ret:%d\n",
				bcl_perph->bpm_stats.lvl0_cnt, bcl_perph->bpm_stats.lvl1_cnt,
				bcl_perph->bpm_stats.lvl2_cnt,
				bcl_perph->bpm_stats.max_ibat, bcl_perph->bpm_stats.sync_vbat,
				bcl_perph->bpm_stats.min_vbat, bcl_perph->bpm_stats.sync_ibat, ret);
		}

		bcl_update_trigger_stats(&bcl_perph->stats[bcl_lvl], ibat, vbat, start_ts);
		mutex_lock(&bcl_perph->stats_lock);
		if (bcl_perph->stats[bcl_lvl].max_mitig_latency < (end_ts - start_ts)) {
			bcl_perph->stats[bcl_lvl].max_mitig_latency = end_ts - start_ts;
			bcl_perph->stats[bcl_lvl].max_mitig_ts = start_ts;
		}

		if (perph_data->irq_enabled)
			++bcl_perph->stats[bcl_lvl].self_cleared_counter;
		mutex_unlock(&bcl_perph->stats_lock);
	} else {
		mutex_lock(&bcl_perph->stats_lock);
		++bcl_perph->stats[bcl_lvl].self_cleared_counter;
		mutex_unlock(&bcl_perph->stats_lock);
		if (bcl_perph->enable_bpm)
			ret = get_bpm_stats(bcl_perph, &bcl_perph->bpm_stats);
	}

	return IRQ_HANDLED;
}

static int bcl_get_ibat_ext_range_factor(struct platform_device *pdev,
		uint32_t *ibat_range_factor)
{
	int ret = 0;
	const char *name;
	struct nvmem_cell *cell;
	size_t len;
	char *buf;
	uint32_t ext_range_index = 0;

	ret = of_property_read_string(pdev->dev.of_node, "nvmem-cell-names", &name);
	if (ret) {
		*ibat_range_factor = bcl_ibat_ext_ranges[BCL_IBAT_RANGE_LVL0];
		pr_debug("Default ibat range factor enabled %u\n", *ibat_range_factor);
		return 0;
	}

	cell = nvmem_cell_get(&pdev->dev, name);
	if (IS_ERR(cell)) {
		dev_err(&pdev->dev, "failed to get nvmem cell %s\n", name);
		return PTR_ERR(cell);
	}

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	if (IS_ERR(buf)) {
		dev_err(&pdev->dev, "failed to read nvmem cell %s\n", name);
		return PTR_ERR(buf);
	}

	if (len <= 0 || len > sizeof(uint32_t)) {
		dev_err(&pdev->dev, "nvmem cell length out of range %zu\n", len);
		kfree(buf);
		return -EINVAL;
	}
	memcpy(&ext_range_index, buf, min(len, sizeof(ext_range_index)));
	kfree(buf);

	if (ext_range_index >= BCL_IBAT_RANGE_MAX) {
		dev_err(&pdev->dev, "invalid BCL ibat scaling factor %d\n", ext_range_index);
		return -EINVAL;
	}

	*ibat_range_factor = bcl_ibat_ext_ranges[ext_range_index];
	pr_debug("ext_range_index %u, ibat range factor %u\n",
			ext_range_index, *ibat_range_factor);

	return 0;
}

static int bcl_get_devicetree_data(struct platform_device *pdev,
					struct bcl_device *bcl_perph)
{
	int ret = 0;
	const __be32 *prop = NULL;
	struct device_node *dev_node = pdev->dev.of_node;

	prop = of_get_address(dev_node, 0, NULL, NULL);
	if (prop) {
		bcl_perph->fg_bcl_addr = be32_to_cpu(*prop);
		pr_debug("fg_bcl@%04x\n", bcl_perph->fg_bcl_addr);
	} else {
		dev_err(&pdev->dev, "No fg_bcl registers found\n");
		return -ENODEV;
	}

	bcl_perph->ibat_use_qg_adc =  of_property_read_bool(dev_node,
				"qcom,ibat-use-qg-adc-5a");
	bcl_perph->no_bit_shift =  of_property_read_bool(dev_node,
				"qcom,pmic7-threshold");
	bcl_perph->ibat_ccm_enabled =  of_property_read_bool(dev_node,
						"qcom,ibat-ccm-hw-support");
	bcl_perph->ibat_ccm_lando_enabled = of_property_read_bool(dev_node,
						"qcom,ibat-ccm-lando-hw-support");
	ret = bcl_get_ibat_ext_range_factor(pdev,
					&bcl_perph->ibat_ext_range_factor);

	if (of_property_read_bool(dev_node, "qcom,bcl-mon-vbat-only"))
		bcl_perph->bcl_monitor_type = BCL_MON_VBAT_ONLY;
	else if (of_property_read_bool(dev_node, "qcom,bcl-mon-ibat-only"))
		bcl_perph->bcl_monitor_type = BCL_MON_IBAT_ONLY;
	else
		bcl_perph->bcl_monitor_type = BCL_MON_DEFAULT;

	return ret;
}

static int bcl_fetch_trip(struct platform_device *pdev, enum bcl_dev_type type,
 struct bcl_peripheral_data *data, irqreturn_t (*handle)(int, void *))
{
 int ret, irq = platform_get_irq_byname(pdev, bcl_int_names[type]);
 if (irq <= 0) return irq ?: -EINVAL;
 ret = devm_request_threaded_irq(&pdev->dev, irq, NULL, handle,
  IRQF_TRIGGER_RISING | IRQF_ONESHOT | IRQF_NO_AUTOEN, bcl_int_names[type], data);
 if (ret) return ret;
 data->irq_num = irq; data->irq_requested = true;
 return 0;
}

static int bcl_vbat_init(struct platform_device *pdev,
		enum bcl_dev_type type, struct bcl_device *bcl_perph)
{
	struct bcl_peripheral_data *vbat = &bcl_perph->param[type];
	unsigned int val = 0;
	int ret;

	vbat->dev = bcl_perph;
	vbat->irq_num = 0;
	vbat->irq_enabled = false;
	vbat->tz_dev = NULL;

	/* If revision 4 or above && bcl support adc, then only enable vbat */
	if (bcl_perph->dig_major >= BCL_GEN3_MAJOR_REV) {
		if (!(bcl_perph->bcl_param_1 & BCL_PARAM_HAS_ADC))
			return 0;
	} else {
		ret = bcl_read_register(bcl_perph, BCL_VBAT_CONV_REQ, &val);
		if (ret) return ret;
		if (!val) return 0;
	}

	vbat->ops = vbat_tzd_ops;
	vbat->tz_dev = devm_thermal_of_zone_register(&pdev->dev,
				type, vbat, &vbat->ops);
	if (IS_ERR(vbat->tz_dev)) {
		pr_debug("vbat[%s] register failed. err:%ld\n",
				bcl_int_names[type],
				PTR_ERR(vbat->tz_dev));
		ret = PTR_ERR(vbat->tz_dev);
		vbat->tz_dev = NULL;
		return ret == -ENODEV ? 0 : ret;
	}

	return 0;
}

static int bcl_probe_vbat(struct platform_device *pdev,
					struct bcl_device *bcl_perph)
{
	return bcl_vbat_init(pdev, BCL_VBAT_LVL0, bcl_perph);
}

static int bcl_ibat_init(struct platform_device *pdev,
			enum bcl_dev_type type, struct bcl_device *bcl_perph)
{
	struct bcl_peripheral_data *ibat = &bcl_perph->param[type];

	ibat->type = type;
	ibat->dev = bcl_perph;
	ibat->irq_num = 0;
	ibat->irq_enabled = false;
	ibat->ops.get_temp = bcl_read_ibat;
	ibat->ops.set_trips = bcl_set_ibat;
	ibat->tz_dev = devm_thermal_of_zone_register(&pdev->dev,
				type, ibat, &ibat->ops);
	if (IS_ERR(ibat->tz_dev)) {
		pr_debug("ibat:[%s] register failed. err:%ld\n",
				bcl_int_names[type],
				PTR_ERR(ibat->tz_dev));
		int ret = PTR_ERR(ibat->tz_dev);
		ibat->tz_dev = NULL;
		return ret == -ENODEV ? 0 : ret;
	}
	thermal_zone_device_update(ibat->tz_dev, THERMAL_DEVICE_UP);
	return 0;
}

static int bcl_get_ibat_config(struct platform_device *pdev,
		uint32_t *ibat_config)
{
	int ret = 0;
	const char *name;
	struct nvmem_cell *cell;
	size_t len;
	char *buf;

	ret = of_property_read_string(pdev->dev.of_node, "nvmem-cell-names", &name);
	if (ret) {
		if (of_find_property(pdev->dev.of_node, "nvmem-cell-names", NULL)) return ret;
		*ibat_config = 0;
		pr_debug("Default ibat config enabled %u\n", *ibat_config);
		return 0;
	}

	cell = nvmem_cell_get(&pdev->dev, name);
	if (IS_ERR(cell)) {
		dev_err(&pdev->dev, "failed to get nvmem cell %s\n", name);
		return PTR_ERR(cell);
	}

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	if (IS_ERR(buf)) {
		dev_err(&pdev->dev, "failed to read nvmem cell %s\n", name);
		return PTR_ERR(buf);
	}

	if (len <= 0 || len > sizeof(uint32_t)) {
		dev_err(&pdev->dev, "nvmem cell length out of range %zu\n", len);
		kfree(buf);
		return -EINVAL;
	}
	*ibat_config = 0;
	memcpy(ibat_config, buf, len);
	kfree(buf);

	return 0;
}
static int bcl_probe_ibat(struct platform_device *pdev,
					struct bcl_device *bcl_perph)
{
	uint32_t bcl_config = 0;

	int ret = bcl_get_ibat_config(pdev, &bcl_config);
	if (ret) return ret;
	if (bcl_config > 1) return -EINVAL;

	if (bcl_config == 1) {
		ret = bcl_ibat_init(pdev, BCL_2S_IBAT_LVL0, bcl_perph);
		if (ret) return ret;
		return bcl_ibat_init(pdev, BCL_2S_IBAT_LVL1, bcl_perph);
	} else {
		ret = bcl_ibat_init(pdev, BCL_IBAT_LVL0, bcl_perph);
		if (ret) return ret;
		return bcl_ibat_init(pdev, BCL_IBAT_LVL1, bcl_perph);
	}
}

static int bcl_lvl_init(struct platform_device *pdev,
	enum bcl_dev_type type, int sts_bit_idx, struct bcl_device *bcl_perph)
{
	struct bcl_peripheral_data *lbat = &bcl_perph->param[type];
	int ret;

	lbat->type = type;
	lbat->dev = bcl_perph;
	lbat->status_bit_idx = sts_bit_idx;
	ret = bcl_fetch_trip(pdev, type, lbat, bcl_handle_irq);
	if (ret) return ret;

	lbat->ops.get_temp = bcl_read_lbat;
	lbat->ops.set_trips = bcl_set_lbat;
	lbat->ops.get_trend = bcl_get_trend;

	lbat->tz_dev = devm_thermal_of_zone_register(&pdev->dev,
				type, lbat, &lbat->ops);
	if (IS_ERR(lbat->tz_dev)) {
		pr_debug("lbat:[%s] register failed. err:%ld\n",
				bcl_int_names[type],
				PTR_ERR(lbat->tz_dev));
		ret = PTR_ERR(lbat->tz_dev);
		lbat->tz_dev = NULL;
		return ret == -ENODEV ? 0 : ret;
	}
	thermal_zone_device_update(lbat->tz_dev, THERMAL_DEVICE_UP);
	return 0;
}

static int bcl_probe_lvls(struct platform_device *pdev,
					struct bcl_device *bcl_perph)
{
	int ret = bcl_lvl_init(pdev, BCL_LVL0, BCL_IRQ_L0, bcl_perph);
	if (ret) return ret;
	ret = bcl_lvl_init(pdev, BCL_LVL1, BCL_IRQ_L1, bcl_perph);
	if (ret) return ret;
	return bcl_lvl_init(pdev, BCL_LVL2, BCL_IRQ_L2, bcl_perph);
}

static int bcl_version_init_and_check(struct bcl_device *bcl_perph)
{
	int ret = 0;
	unsigned int val = 0;

	ret = bcl_read_register(bcl_perph, BCL_REVISION2, &val);
	if (ret < 0)
		return ret;

	bcl_perph->dig_major = val;
	ret = bcl_read_register(bcl_perph, BCL_REVISION1, &val);
	if (ret) return ret;
	bcl_perph->dig_minor = val;

	if (bcl_perph->dig_major >= BCL_GEN3_MAJOR_REV) {
		ret = bcl_read_register(bcl_perph, BCL_PARAM_1, &val);
		if (ret < 0)
			return ret;
		bcl_perph->bcl_param_1 = val;

		val = 0;
		ret = bcl_read_register(bcl_perph, BCL_PARAM_2, &val);
		if (ret) return ret;
		bcl_perph->bcl_type = val;
	} else {
		bcl_perph->bcl_param_1 = 0;
		bcl_perph->bcl_type = 0;
	}

	if ((bcl_perph->bcl_monitor_type == BCL_MON_VBAT_ONLY) ||
			(bcl_perph->bcl_monitor_type == BCL_MON_IBAT_ONLY)) {
		ret = regmap_read(bcl_perph->regmap, ADC_CMN_BG_ADC_DVDD_SPARE1, &val);
		if (ret < 0) {
			pr_err("Error read reg ADC_CMN_BG_ADC_DVDD_SPARE1, err:%d\n", ret);
			return ret;
		}

		if ((val != 0x71) && (val != 0xF1)) {
			if (bcl_perph->bcl_monitor_type == BCL_MON_VBAT_ONLY) {
				bcl_perph->bcl_monitor_type = BCL_MON_DEFAULT;
				return 0;
			} else if (bcl_perph->bcl_monitor_type == BCL_MON_IBAT_ONLY) {
				pr_debug("BCL monitor Ibat only is not required, value:%d\n",
					val);
				return -ENODEV;
			}
		}
	}

	return 0;
}

static int bcl_configure_bcl_peripheral(struct bcl_device *bcl)
{
 return bcl_write_register(bcl, BCL_MONITOR_EN, BIT(7));
}
static void bcl_quiesce(void *data)
{
 struct bcl_device *bcl = data; int i;
 WRITE_ONCE(bcl->stopping, true);
 if (bcl->notifier_registered) { bcl_pmic5_notifier_unregister(&bcl->nb); bcl->notifier_registered = false; }
 bcl_stats_remove(bcl);
 for (i = 0; i < BCL_TYPE_MAX; i++) {
  struct bcl_peripheral_data *p = &bcl->param[i];
  mutex_lock(&p->state_trans_lock);
  if (p->irq_wake) { disable_irq_wake(p->irq_num); p->irq_wake = false; }
  if (p->irq_requested && p->irq_enabled) disable_irq_nosync(p->irq_num);
  p->irq_enabled = false;
  mutex_unlock(&p->state_trans_lock);
  if (p->irq_requested) { devm_free_irq(bcl->dev, p->irq_num, p); p->irq_requested = false; }
 }
 /* Wait for native polling callbacks before releasing their log context. */
 for (i = 0; i < BCL_TYPE_MAX; i++)
  if (bcl->param[i].tz_dev) thermal_zone_device_disable(bcl->param[i].tz_dev);
 if (bcl->ipc_log) { ipc_log_context_destroy(bcl->ipc_log); bcl->ipc_log = NULL; }
}
static void bcl_remove(struct platform_device *pdev)
{
 struct bcl_device *bcl = platform_get_drvdata(pdev);
 if (bcl) bcl_quiesce(bcl);
 platform_set_drvdata(pdev, NULL);
}
static int bcl_probe(struct platform_device *pdev)
{
 struct bcl_device *bcl; char *name; int ret, i;
 bcl = devm_kzalloc(&pdev->dev, sizeof(*bcl), GFP_KERNEL);
 if (!bcl) return -ENOMEM;
 bcl->dev = &pdev->dev; mutex_init(&bcl->stats_lock);
 for (i = 0; i < MAX_BCL_LVL_COUNT; i++) bcl->stats[i].bcl_dev = bcl;
 for (i = 0; i < BCL_TYPE_MAX; i++) {
  mutex_init(&bcl->param[i].state_trans_lock);
  bcl->param[i].type = i; bcl->param[i].dev = bcl; bcl->param[i].trip_thresh = INT_MAX;
 }
 bcl->desc = of_device_get_match_data(&pdev->dev);
 if (!bcl->desc) return -EINVAL;
 bcl->regmap = dev_get_regmap(pdev->dev.parent, NULL);
 if (!bcl->regmap) return -EPROBE_DEFER;
 ret = bcl_get_devicetree_data(pdev, bcl); if (ret) return ret;
 ret = bcl_version_init_and_check(bcl); if (ret) return ret;
 if (bcl->bcl_monitor_type != BCL_MON_IBAT_ONLY) { ret = bcl_probe_vbat(pdev, bcl); if (ret) goto fail; }
 if (bcl->bcl_monitor_type != BCL_MON_VBAT_ONLY) { ret = bcl_probe_ibat(pdev, bcl); if (ret) goto fail; }
 ret = bcl_probe_lvls(pdev, bcl); if (ret) goto fail;
 ret = bcl_configure_bcl_peripheral(bcl); if (ret) goto fail;
 if (!bcl->desc->vbat_zone_enabled) {
  bcl->nb.notifier_call = bcl_config_vph_cb;
  ret = bcl_pmic5_notifier_register(&bcl->nb); if (ret) goto fail;
  bcl->notifier_registered = true;
 }
 ret = bcl_write_register(bcl, BPM_EN_OFFSET, BIT(7)); if (ret) goto fail;
 bcl->enable_bpm = 1;
 name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "bcl_%s", dev_name(&pdev->dev));
 if (!name) { ret = -ENOMEM; goto fail; }
 bcl->ipc_log = ipc_log_context_create(IPC_LOGPAGES, name, 0);
 bcl_stats_init(name, bcl, MAX_BCL_LVL_COUNT);
 ret = devm_add_action_or_reset(&pdev->dev, bcl_quiesce, bcl); if (ret) return ret;
 platform_set_drvdata(pdev, bcl); mmi_get_boot_seq(); return 0;
fail:
 bcl_quiesce(bcl); return ret;
}
static int bcl_freeze(struct device *dev)
{
 struct bcl_device *bcl = dev_get_drvdata(dev); int i;
 if (READ_ONCE(bcl->stopping)) return -ENODEV;
 if (bcl->frozen) return 0;
 WRITE_ONCE(bcl->frozen, true);
 for (i = 0; i < BCL_TYPE_MAX; i++) {
  struct bcl_peripheral_data *p = &bcl->param[i];
  mutex_lock(&p->state_trans_lock); p->resume_irq = p->irq_enabled;
  if (p->irq_wake) { disable_irq_wake(p->irq_num); p->irq_wake = false; }
  if (p->irq_requested && p->irq_enabled) disable_irq_nosync(p->irq_num);
  p->irq_enabled = false; mutex_unlock(&p->state_trans_lock);
  if (p->irq_requested) synchronize_irq(p->irq_num);
 }
 return 0;
}
static int bcl_restore(struct device *dev)
{
 struct bcl_device *bcl = dev_get_drvdata(dev); int i, ret;
 if (READ_ONCE(bcl->stopping)) return -ENODEV;
 if (!bcl->frozen) return 0;
 ret = bcl_configure_bcl_peripheral(bcl); if (ret) return ret;
 for (i = 0; i < BCL_TYPE_MAX; i++) {
  struct bcl_peripheral_data *p = &bcl->param[i];
  if (!p->resume_irq || !p->irq_requested) continue;
  ret = enable_irq_wake(p->irq_num); if (ret) goto unwind;
  p->irq_wake = true;
 }
 for (i = 0; i < BCL_TYPE_MAX; i++) {
  struct bcl_peripheral_data *p = &bcl->param[i];
  mutex_lock(&p->state_trans_lock);
  if (p->resume_irq && p->irq_requested) { enable_irq(p->irq_num); p->irq_enabled = true; }
  p->resume_irq = false; mutex_unlock(&p->state_trans_lock);
 }
 WRITE_ONCE(bcl->frozen, false);
 for (i = 0; i < BCL_TYPE_MAX; i++)
  if (bcl->param[i].tz_dev) thermal_zone_device_update(bcl->param[i].tz_dev, THERMAL_DEVICE_UP);
 return 0;
unwind:
 while (--i >= 0) { struct bcl_peripheral_data *p = &bcl->param[i];
  if (p->irq_wake) { disable_irq_wake(p->irq_num); p->irq_wake = false; }
 }
 return ret;
}
static const struct dev_pm_ops bcl_pm_ops = {
	.freeze = bcl_freeze,
	.restore = bcl_restore,
};

static const struct bcl_desc pmih010x_data = {
	.vadc_type = true,
	.vbat_regs = {
		[BCLBIG_COMP_VCMP_L0_THR]		= 0x48,
		[BCLBIG_COMP_VCMP_L1_THR]		= 0x49,
		[BCLBIG_COMP_VCMP_L2_THR]		= 0x4A,
	},
	.vbat_zone_enabled = true,
	.vcmp_thresh_base = 2250,
	.vcmp_thresh_max = 3600,
	.ibat_scaling_factor = BCL_IBAT_SCALING_REV5_NA,
	.ibat_thresh_scaling_factor = BCL_IBAT_THRESH_SCALING_REV5_UA,
};

static const struct bcl_desc pm8550_data = {
	.vadc_type = false,
	.vbat_regs = {
		[BCLBIG_COMP_VCMP_L0_THR]		= 0x47,
		[BCLBIG_COMP_VCMP_L1_THR]		= 0x49,
		[BCLBIG_COMP_VCMP_L2_THR]		= 0x4A,
	},
	.vbat_zone_enabled = false,
	.vcmp_thresh_base = 2250,
	.vcmp_thresh_max = 3600,
	.ibat_scaling_factor = BCL_IBAT_SCALING_REV5_NA,
	.ibat_thresh_scaling_factor = BCL_IBAT_THRESH_SCALING_REV5_UA,
};

static const struct bcl_desc pmh0101_data = {
	.vadc_type = false,
	.vbat_regs = {
		[BCLBIG_COMP_VCMP_L0_THR]		= 0x47,
		[BCLBIG_COMP_VCMP_L1_THR]		= 0x49,
		[BCLBIG_COMP_VCMP_L2_THR]		= 0x4A,
	},
	.vbat_zone_enabled = false,
	.vcmp_thresh_base = 1500,
	.vcmp_thresh_max = 4000,
	.ibat_scaling_factor = BCL_IBAT_SCALING_REV5_NA,
	.ibat_thresh_scaling_factor = BCL_IBAT_THRESH_SCALING_REV5_UA,
};

static const struct bcl_desc pmiv010x_data = {
	.vadc_type = true,
	.vbat_regs = {
		[BCLBIG_COMP_VCMP_L0_THR]		= 0x48,
		[BCLBIG_COMP_VCMP_L1_THR]		= 0x49,
		[BCLBIG_COMP_VCMP_L2_THR]		= 0x4A,
	},
	.vbat_zone_enabled = true,
	.vcmp_thresh_base = 2250,
	.vcmp_thresh_max = 3600,
	.ibat_scaling_factor = BCL_IBAT_COTTID_SCALING,
	.ibat_thresh_scaling_factor = BCL_IBAT_SCALING_REV4_UA,
};

static const struct of_device_id bcl_match[] = {
	{ .compatible = "qcom,bcl-v5", .data = &pmih010x_data},
	{ .compatible = "qcom,pmh0101-bcl-v5", .data = &pmh0101_data},
	{ .compatible = "qcom,pmiv010x-bcl-v5", .data = &pmiv010x_data},
	{ .compatible = "qcom,pm8550-bcl-v5", .data = &pm8550_data},
	{ }
};
MODULE_DEVICE_TABLE(of, bcl_match);

static struct platform_driver bcl_driver = {
	.probe  = bcl_probe,
	.remove = bcl_remove,
	.driver = {
		.name           = BCL_DRIVER_NAME,
		.pm = &bcl_pm_ops,
		.of_match_table = bcl_match,
	},
};

module_platform_driver(bcl_driver);
MODULE_DESCRIPTION("Qualcomm PMIC BCL voltage/current and level monitor draft");
MODULE_LICENSE("GPL");
