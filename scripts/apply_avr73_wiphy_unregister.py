#!/usr/bin/env python3
"""7.3 reboot hang: wiphy_unregister() waits forever for rdev->opencount == 0.

Phone 2026-11-19 (reboot from the power menu never completes, force reset needed):
vendor_init writes the icnss sysfs shutdown node -> rproc_shutdown(WPSS) -> icnss
removes qca_cld3 -> hdd_wlan_exit() -> wiphy_unregister() sleeps in D state with all
WLAN netdevs already gone (only phy0 left), so vendor_init never returns and init's
shutdown never reaches the kernel. Something opened the wiphy without a netdev
(P2P device / NAN / PD, which nl80211 starts with opencount++) and was not stopped.

wiphy_unregister now waits in 10 s steps. On each timeout it logs every wdev left and
stops the running netdev-less ones through cfg80211's own stop paths (the same calls
_cfg80211_unregister_wdev() makes). If the count is still non-zero with no wdev left
at all (a pure accounting leak: nothing can be open), it reports and continues instead
of hanging the reboot.
"""
from pathlib import Path
import sys

K = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1] / 'kernel')
p = K / 'net/wireless/core.c'
s = p.read_text()
old = '''void wiphy_unregister(struct wiphy *wiphy)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);

	wait_event(rdev->dev_wait, ({
		int __count;
		wiphy_lock(&rdev->wiphy);
		__count = rdev->opencount;
		wiphy_unlock(&rdev->wiphy);
		__count == 0; }));
'''
new = '''/* ANDROID: avr 7.3: never hang reboot on a leaked opencount (see wiphy_unregister) */
static bool avr_wiphy_unregister_reap(struct cfg80211_registered_device *rdev, int round)
{
	struct wireless_dev *wdev;
	bool any = false;
	int count;

	rtnl_lock();
	wiphy_lock(&rdev->wiphy);
	count = rdev->opencount;
	list_for_each_entry(wdev, &rdev->wiphy.wdev_list, list) {
		any = true;
		pr_warn("cfg80211: %s: unregister waits (opencount %d): wdev iftype %d netdev %s running %d\\n",
			wiphy_name(&rdev->wiphy), count, wdev->iftype,
			wdev->netdev ? wdev->netdev->name : "-", wdev->is_running);
		if (wdev->netdev || !wdev_running(wdev))
			continue;
		switch (wdev->iftype) {
		case NL80211_IFTYPE_P2P_DEVICE:
			cfg80211_stop_p2p_device(rdev, wdev);
			break;
		case NL80211_IFTYPE_NAN:
			cfg80211_stop_nan(rdev, wdev);
			break;
		case NL80211_IFTYPE_PD:
			cfg80211_stop_pd(rdev, wdev);
			break;
		default:
			break;
		}
	}
	if (!any && rdev->opencount > 0 && round >= 2) {
		pr_err("cfg80211: %s: opencount %d leaked with no wdev left; continuing unregister\\n",
		       wiphy_name(&rdev->wiphy), rdev->opencount);
		rdev->opencount = 0;
	}
	count = rdev->opencount;
	wiphy_unlock(&rdev->wiphy);
	rtnl_unlock();
	return count == 0;
}

void wiphy_unregister(struct wiphy *wiphy)
{
	struct cfg80211_registered_device *rdev = wiphy_to_rdev(wiphy);
	int round = 0;

	while (!wait_event_timeout(rdev->dev_wait, ({
		int __count;
		wiphy_lock(&rdev->wiphy);
		__count = rdev->opencount;
		wiphy_unlock(&rdev->wiphy);
		__count == 0; }), 10 * HZ)) {
		if (avr_wiphy_unregister_reap(rdev, round++))
			break;
	}
'''
if new not in s:
    if s.count(old) != 1:
        raise SystemExit('net/wireless/core.c: wiphy_unregister anchor not found')
    p.write_text(s.replace(old, new))
print('applied wiphy_unregister reboot-hang fix to', K)
