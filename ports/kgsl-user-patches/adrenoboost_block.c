/*
 * Rakkashin: Adreno Boost for GMU based DCVS.
 *
 * The classic adrenoboost (msm-adreno-tz) biased the busy time fed to the
 * governor.  On gen8 the GMU firmware picks the frequency, so instead we watch
 * the DCVS power stats the GMU streams to us and, while the GPU is saturated,
 * temporarily raise the GMU minimum perf level a few steps above the current
 * level.  The floor never exceeds the max/thermal/pm_qos caps and is dropped
 * once load falls; pwr->min_pwrlevel (the user's floor) is left untouched, so
 * a GMU restart also clears it.  0 = off (stock), 1..3 = low/medium/high.
 */
static unsigned int adrenoboost;
module_param(adrenoboost, uint, 0644);
MODULE_PARM_DESC(adrenoboost, "Adreno Boost level 0-3 (0 = off)");

#define BOOST_WINDOW_US		16000
#define BOOST_HOLD_MS		64

static struct {
	struct kgsl_device *device;
	struct work_struct work;
	u64 total, busy;
	/* pwrlevel to request, or -1 to drop back to pwr->min_pwrlevel */
	int want;
	/* pwrlevel currently sent to the GMU, or -1 when not boosting */
	int applied;
	unsigned long until;
	u64 samples, windows, boosts;
	unsigned int last_busy;
} boost = { .want = -1, .applied = -1 };

/* busy % at which each level starts boosting */
static const u8 boost_up_pct[] = { 0, 90, 80, 70 };

static void adrenoboost_work(struct work_struct *work)
{
	struct kgsl_device *device = boost.device;
	struct kgsl_pwrctrl *pwr;
	int want;

	if (!device)
		return;

	pwr = &device->pwrctrl;
	kgsl_mutex_lock(&device->mutex);
	want = READ_ONCE(boost.want);
	if (device->state != KGSL_STATE_ACTIVE) {
		/* slumber exit resends pwr->min_pwrlevel to the GMU */
		boost.applied = -1;
	} else if (want != boost.applied) {
		u32 level = want < 0 ? pwr->min_pwrlevel : want;

		device->ftbl->gmu_based_dcvs_pwr_ops(device, level,
				GPU_PWRLEVEL_OP_MIN_PWRLEVEL);
		boost.applied = want;
		if (want >= 0)
			boost.boosts++;
	}
	kgsl_mutex_unlock(&device->mutex);
}

/* Called from the GMU trace (f2h) thread for every DCVS power stats packet */
void kgsl_pwrctrl_adrenoboost_sample(struct kgsl_device *device, u64 time, u64 busy)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	unsigned int level = min(READ_ONCE(adrenoboost), 3U);
	unsigned int pct;
	int want = -1, cap;

	if (boost.device != device || device->host_based_dcvs)
		return;

	boost.samples++;

	if (!level) {
		boost.total = boost.busy = 0;
		if (boost.want != -1) {
			WRITE_ONCE(boost.want, -1);
			schedule_work(&boost.work);
		}
		return;
	}

	boost.total += time;
	boost.busy += busy;
	if (boost.total < BOOST_WINDOW_US)
		return;

	pct = div64_u64(min(boost.busy, boost.total) * 100, boost.total);
	boost.total = boost.busy = 0;
	boost.windows++;
	boost.last_busy = pct;

	/* Highest frequency we may ask for: the lowest-frequency cap wins */
	cap = max3(pwr->max_pwrlevel, pwr->thermal_pwrlevel, pwr->pmqos_max_pwrlevel);

	if (pct >= boost_up_pct[level]) {
		want = max_t(int, (int)pwr->active_pwrlevel - (int)level, cap);
		/* only ever raise the floor while boosted */
		if (boost.want >= 0)
			want = min(want, boost.want);
		if (want >= (int)pwr->min_pwrlevel)
			want = -1;
		else
			boost.until = jiffies + msecs_to_jiffies(BOOST_HOLD_MS * level);
	} else if (boost.want >= 0 && time_before(jiffies, boost.until)) {
		/* keep the floor for the hold time, but never above a new cap */
		want = max(boost.want, cap);
		if (want >= (int)pwr->min_pwrlevel)
			want = -1;
	}

	/* also re-send after a GMU restart dropped an applied floor */
	if (want != boost.want || want != READ_ONCE(boost.applied)) {
		WRITE_ONCE(boost.want, want);
		schedule_work(&boost.work);
	}
}

static ssize_t adrenoboost_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", READ_ONCE(adrenoboost));
}

static ssize_t adrenoboost_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;
	int ret = kstrtou32(buf, 0, &val);

	if (ret)
		return ret;
	if (val > 3)
		return -EINVAL;

	WRITE_ONCE(adrenoboost, val);
	return count;
}

static ssize_t adrenoboost_stats_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
		"samples %llu windows %llu boosts %llu last_busy %u floor %d\n",
		boost.samples, boost.windows, boost.boosts, boost.last_busy,
		READ_ONCE(boost.applied));
}

