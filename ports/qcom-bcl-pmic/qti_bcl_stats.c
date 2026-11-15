// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "qti_bcl_common.h"

static struct dentry	*bcl_stats_parent;
static DEFINE_MUTEX(bcl_debugfs_lock);
static unsigned int bcl_stats_users;

void bcl_update_clear_stats(struct bcl_lvl_stats *bcl_stat)
{
	uint32_t iter = 0;
	unsigned long long last_duration;
	struct bcl_data_history *hist_data;

	mutex_lock(&bcl_stat->bcl_dev->stats_lock);
	if (!bcl_stat->trigger_state) {
		mutex_unlock(&bcl_stat->bcl_dev->stats_lock);
		return;
	}

	iter = (bcl_stat->counter % BCL_HISTORY_COUNT);
	hist_data = &bcl_stat->bcl_history[iter];
	hist_data->clear_ts = sched_clock();
	last_duration = DIV_ROUND_UP(
			hist_data->clear_ts - hist_data->trigger_ts,
			NSEC_PER_USEC);
	bcl_stat->total_duration += last_duration;
	if (last_duration > bcl_stat->max_duration)
		bcl_stat->max_duration = last_duration;

	bcl_stat->counter++;
	bcl_stat->trigger_state = false;
	mutex_unlock(&bcl_stat->bcl_dev->stats_lock);
}

void bcl_update_trigger_stats(struct bcl_lvl_stats *bcl_stat, int ibat, int vbat,
			unsigned long long trigger_ts)
{
	uint32_t iter = 0;

	mutex_lock(&bcl_stat->bcl_dev->stats_lock);
	iter = (bcl_stat->counter % BCL_HISTORY_COUNT);
	bcl_stat->bcl_history[iter].clear_ts = 0x0;
	bcl_stat->bcl_history[iter].trigger_ts = trigger_ts;
	bcl_stat->bcl_history[iter].ibat = ibat;
	bcl_stat->bcl_history[iter].vbat = vbat;
	bcl_stat->trigger_state = true;
	mutex_unlock(&bcl_stat->bcl_dev->stats_lock);
}

static int bcl_lvl_show(struct seq_file *s, void *data)
{
	struct bcl_lvl_stats *bcl_stat = s->private;
	unsigned long long last_duration = 0;
	int idx = 0, cur_counter = 0, loop_till = 0;

	mutex_lock(&bcl_stat->bcl_dev->stats_lock);
	seq_printf(s, "%-30s: %d\n",
				"SW Counter", bcl_stat->counter);
	seq_printf(s, "%-30s: %d\n",
				"Irq self cleared counter",
				bcl_stat->self_cleared_counter);
	seq_printf(s, "%-30s: %llu\n",
				"Max Mitigation at", bcl_stat->max_mitig_ts);
	seq_printf(s, "%-30s: %llu usec\n",
				"Max Mitigation latency",
				DIV_ROUND_UP(bcl_stat->max_mitig_latency,
				NSEC_PER_USEC));
	seq_printf(s, "%-30s: %llu usec\n",
				"BCL mitigation residency", bcl_stat->max_duration);
	seq_printf(s, "%-30s: %llu usec\n",
				"Total residency",	bcl_stat->total_duration);
	seq_printf(s, "Last %d iterations :\n", BCL_HISTORY_COUNT);
	seq_printf(s, "%s%10s%10s%15s%15s%16s\n", "idx", "ibat", "vbat",
				"trigger_ts", "clear_ts", "duration(usec)");

	cur_counter = (bcl_stat->counter % BCL_HISTORY_COUNT);
	idx = cur_counter - 1;
	loop_till = -2;
	/* print history data as stack. latest entry first */
	do {
		last_duration = 0;
		if (idx < 0) {
			idx = BCL_HISTORY_COUNT - 1;
			loop_till = cur_counter - 1;
			continue;
		}
		if (bcl_stat->bcl_history[idx].clear_ts)
			last_duration = DIV_ROUND_UP(
					bcl_stat->bcl_history[idx].clear_ts -
					bcl_stat->bcl_history[idx].trigger_ts,
					NSEC_PER_USEC);
		seq_printf(s, "[%d]%10d%10d%15llu%15llu%16llu\n", idx,
				bcl_stat->bcl_history[idx].ibat,
				bcl_stat->bcl_history[idx].vbat,
				bcl_stat->bcl_history[idx].trigger_ts,
				bcl_stat->bcl_history[idx].clear_ts,
				last_duration);
		--idx;
	} while (idx > loop_till);

	mutex_unlock(&bcl_stat->bcl_dev->stats_lock);
	return 0;
}
DEFINE_SHOW_ATTRIBUTE(bcl_lvl);

static int bpm_stats_show(struct seq_file *s, void *data)
{
	struct bcl_device *bcl_perph = s->private;
	struct bcl_bpm_stats snapshot;
	int ret;

	if (bcl_perph == NULL) {
		pr_err("NULL\n");
		return 0;
	}

	if (!bcl_perph->enable_bpm)
		return 0;

	ret = get_bpm_stats(bcl_perph, &snapshot);
	if (ret) return ret;

	seq_printf(s, "Max Ibat\t: %d \tSynchronus Vbat\t: %d\n",
		snapshot.max_ibat, snapshot.sync_vbat);
	seq_printf(s, "Min Vbat\t: %d \tSynchronus Ibat\t: %d\n",
		snapshot.min_vbat, snapshot.sync_ibat);
	seq_printf(s, "lvl0 count\t: %d\n", snapshot.lvl0_cnt);
	seq_printf(s, "lvl1 count\t: %d\n", snapshot.lvl1_cnt);
	seq_printf(s, "lvl2 count\t: %d\n", snapshot.lvl2_cnt);

	return 0;
}
DEFINE_SHOW_ATTRIBUTE(bpm_stats);

static int bpm_reset(void *data, u64 val)
{
	struct bcl_device *bcl_perph = data;
	unsigned int reset = 1;
	uint16_t base;
	int ret = 0;

	if (bcl_perph == NULL) {
		pr_err("bcl_perph is NULL\n");
		return 0;
	}

	mutex_lock(&bcl_perph->stats_lock);
	base = bcl_perph->fg_bcl_addr;
	ret = regmap_write(bcl_perph->regmap, (base + BPM_CLR_OFFSET), reset);
	if (ret < 0) {
		pr_err("Error reading register:0x%04x val:0x%02x err:%d\n",
			(base + BPM_CLR_OFFSET), reset, ret);
		mutex_unlock(&bcl_perph->stats_lock);
		return ret;
	}
	bcl_perph->last_bpm_reset_ts = sched_clock();
	mutex_unlock(&bcl_perph->stats_lock);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(bpm_reset_fops, NULL, bpm_reset, "%llu\n");

void bcl_stats_remove(struct bcl_device *bcl)
{
 mutex_lock(&bcl_debugfs_lock);
 if (bcl->stats_registered) {
  debugfs_remove_recursive(bcl->stats_dir); bcl->stats_dir = NULL; bcl->stats_registered = false;
  if (!--bcl_stats_users) { debugfs_remove_recursive(bcl_stats_parent); bcl_stats_parent = NULL; }
 }
 mutex_unlock(&bcl_debugfs_lock);
}
void bcl_stats_init(char *name, struct bcl_device *bcl, uint32_t stats_len)
{
 int idx; char path[BCL_STATS_NAME_LENGTH];
 mutex_lock(&bcl_debugfs_lock);
 if (!bcl_stats_parent) bcl_stats_parent = debugfs_create_dir("bcl_stats", NULL);
 if (IS_ERR(bcl_stats_parent)) { bcl_stats_parent = NULL; goto out; }
 bcl->stats_dir = debugfs_create_dir(name, bcl_stats_parent);
 if (IS_ERR_OR_NULL(bcl->stats_dir)) {
  bcl->stats_dir = NULL;
  if (!bcl_stats_users) { debugfs_remove_recursive(bcl_stats_parent); bcl_stats_parent = NULL; }
  goto out;
 }
 bcl_stats_users++; bcl->stats_registered = true;
 for (idx = 0; idx < min_t(uint32_t, stats_len, MAX_BCL_LVL_COUNT); idx++) {
  bcl->stats[idx].bcl_dev = bcl;
  snprintf(path, sizeof(path), "lvl%d_stats", idx);
  debugfs_create_file(path, 0444, bcl->stats_dir, &bcl->stats[idx], &bcl_lvl_fops);
 }
 if (bcl->enable_bpm) {
  debugfs_create_file("bpm_stats", 0444, bcl->stats_dir, bcl, &bpm_stats_fops);
  debugfs_create_file("reset", 0200, bcl->stats_dir, bcl, &bpm_reset_fops);
 }
out:
 mutex_unlock(&bcl_debugfs_lock);
}
