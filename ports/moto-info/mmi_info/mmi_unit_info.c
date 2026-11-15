/*
 * Copyright (C) 2013 Motorola Mobility LLC
 * Copyright (C) 2018 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/seq_file.h>
#include <asm/setup.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <asm/system_misc.h>
#include "mmi_info.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
#include <linux/soc/qcom/smem.h>
/* Match to
 * vendor/qcom/nonhlos/boot_images/QcomPkg/SDMPkg/Include/smem_type.h
 */
#define SMEM_ID_VENDOR0 134
#else
#include <soc/qcom/smem.h>
#endif

#define SMEM_KERNEL_RESERVE SMEM_ID_VENDOR0

static struct proc_dir_entry *unitinfo_procfs_file;
static char serialno[SERIALNO_MAX_LEN];
static char carrier[CARRIER_MAX_LEN];
static char baseband[BASEBAND_MAX_LEN];
static char androidboot_device[ANDROIDBOOT_DEVICE_MAX_LEN];
static unsigned int androidboot_radio;
static char androidboot_radio_str[RADIO_MAX_LEN];

static int mmi_bootarg_setup(void)
{
	char *value;
	int ret;

	memset(serialno, 0, sizeof(serialno));
	memset(carrier, 0, sizeof(carrier));
	memset(baseband, 0, sizeof(baseband));
	memset(androidboot_device, 0, sizeof(androidboot_device));
	memset(androidboot_radio_str, 0, sizeof(androidboot_radio_str));
	androidboot_radio = 0;
	ret = mmi_get_bootarg("androidboot.radio=", &value);
	if (!ret) {
		if (kstrtouint(value, 16, &androidboot_radio))
			androidboot_radio = 0;
		strscpy(androidboot_radio_str, value, sizeof(androidboot_radio_str));
	} else if (ret != -ENOENT) {
		return ret;
	}
#define COPY_BOOTARG(key, destination) do { \
	ret = mmi_copy_bootarg(key, destination, sizeof(destination)); \
	if (ret) return ret; \
} while (0)
	COPY_BOOTARG("androidboot.device=", androidboot_device);
	COPY_BOOTARG("androidboot.baseband=", baseband);
	COPY_BOOTARG("androidboot.carrier=", carrier);
	COPY_BOOTARG("androidboot.serialno=", serialno);
#undef COPY_BOOTARG
	return 0;
}

static int unitinfo_seq_show(struct seq_file *f, void *ptr)
{
	/*seq_printf(f, "Hardware\t: %s\n", arch_read_hardware_id());*/
	seq_printf(f, "Revision\t: %04x\n", mmi_chosen_data.system_rev);
	seq_printf(f, "Serial\t\t: %08x%08x\n",
		mmi_chosen_data.system_serial_high,
		mmi_chosen_data.system_serial_low);

	seq_printf(f, "Device\t\t: %s\n", androidboot_device);
	/* Zero is not a valid "Radio" value.      */
	/* Lack of "Radio" entry in cpuinfo means: */
	/*	look for radio in "Revision"       */
	if (strnlen(androidboot_radio_str, RADIO_MAX_LEN))
		seq_printf(f, "Radio\t\t: %s\n", androidboot_radio_str);

	seq_printf(f, "MSM Hardware\t: %s\n", mmi_chosen_data.msm_hw);

	return 0;
}

static int mmi_unit_smem_setup(void)
{
	int ret = 0;
	struct mmi_unit_info *mui_copy;
	void *mui;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
	size_t mui_size;
#endif

	#define SMEM_KERNEL_RESERVE_SIZE 1024
	mui_copy = kzalloc(SMEM_KERNEL_RESERVE_SIZE, GFP_KERNEL);
	if (!mui_copy) {
		pr_err("%s: failed to allocate space for mmi_unit_info\n",
			__func__);
		ret = -ENOMEM;
		goto err;
	}

	mui_copy->version = MMI_UNIT_INFO_VER;
	mui_copy->system_rev = mmi_chosen_data.system_rev;
	mui_copy->system_serial_low = mmi_chosen_data.system_serial_low;
	mui_copy->system_serial_high = mmi_chosen_data.system_serial_high;
	strscpy(mui_copy->machine, "", MACHINE_MAX_LEN);
	strscpy(mui_copy->barcode, serialno, BARCODE_MAX_LEN);
	strscpy(mui_copy->baseband, mmi_chosen_data.baseband, BASEBAND_MAX_LEN);
	strscpy(mui_copy->carrier, carrier, CARRIER_MAX_LEN);
	strscpy(mui_copy->device, androidboot_device, DEVICE_MAX_LEN);
	mui_copy->radio = androidboot_radio;
	strscpy(mui_copy->radio_str, androidboot_radio_str, RADIO_MAX_LEN);
	mui_copy->powerup_reason = mmi_chosen_data.powerup_reason;

	pr_info("mmi_unit_info (SMEM) for modem: version = 0x%02x,"
		" device = '%s', radio = 0x%x, radio_str = '%s',"
		" system_rev = 0x%04x, system_serial = 0x%08x%08x,"
		" machine = '%s', barcode = '%s', baseband = '%s',"
		" carrier = '%s', pu_reason = 0x%08x\n",
		mui_copy->version,
		mui_copy->device,
		mui_copy->radio,
		mui_copy->radio_str,
		mui_copy->system_rev,
		mui_copy->system_serial_high, mui_copy->system_serial_low,
		mui_copy->machine, mui_copy->barcode,
		mui_copy->baseband, mui_copy->carrier,
		mui_copy->powerup_reason);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
    ret = qcom_smem_alloc(QCOM_SMEM_HOST_ANY, SMEM_KERNEL_RESERVE,
                          SMEM_KERNEL_RESERVE_SIZE);
    if (ret && ret != -EEXIST)
        goto err_free;
    mui = qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_KERNEL_RESERVE, &mui_size);
    if (IS_ERR(mui)) {
        ret = PTR_ERR(mui);
        goto err_free;
    }
    if (!mui || mui_size != SMEM_KERNEL_RESERVE_SIZE) {
        ret = -EINVAL;
        goto err_free;
    }
#else
    mui = (struct mmi_unit_info *)smem_alloc(SMEM_KERNEL_RESERVE,
                                            SMEM_KERNEL_RESERVE_SIZE, 0, SMEM_ANY_HOST_FLAG);
    if (!mui) {
        ret = -ENOMEM;
        goto err_free;
    }
#endif
    memcpy(mui, mui_copy, SMEM_KERNEL_RESERVE_SIZE);

err_free:
	kfree(mui_copy);
err:
	return ret;
}

static int unitinfo_open(struct inode *inode, struct file *file)
{
	return single_open(file, unitinfo_seq_show, inode->i_private);
}

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static const struct proc_ops unitinfo_operations = {
	.proc_open		= unitinfo_open,
	.proc_read		= seq_read,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};
#else
static const struct file_operations unitinfo_operations = {
	.open		= unitinfo_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

int mmi_unit_info_init(void)
{
	int ret = mmi_bootarg_setup();

	if (ret)
		return ret;
	ret = mmi_unit_smem_setup();
	if (ret)
		return ret;
	unitinfo_procfs_file = proc_create("unitinfo", 0444, NULL, &unitinfo_operations);
	return unitinfo_procfs_file ? 0 : -ENOMEM;
}

void mmi_unit_info_exit(void)
{
	proc_remove(unitinfo_procfs_file);
	unitinfo_procfs_file = NULL;
}
