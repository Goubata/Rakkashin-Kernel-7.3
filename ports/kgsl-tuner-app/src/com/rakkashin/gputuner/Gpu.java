package com.rakkashin.gputuner;

import android.content.Context;
import android.content.SharedPreferences;

/** sysfs knobs of the Rakkashin kgsl driver (kgsl-avr) and the saved settings. */
final class Gpu {
    static final String KGSL = "/sys/class/kgsl/kgsl-3d0";
    static final String BOOST = KGSL + "/adrenoboost";
    static final String BOOST_STATS = KGSL + "/adrenoboost_stats";
    static final String BUSY_MS = "/sys/module/msm_kgsl/parameters/busy_update_ms";

    static final int STOCK_BUSY_MS = 1000;
    static final int DEFAULT_BUSY_MS = 100;

    static SharedPreferences prefs(Context c) {
        return c.getSharedPreferences("gpu", Context.MODE_PRIVATE);
    }

    static boolean supported(RootShell sh) throws Exception {
        return sh.run("[ -e " + BOOST + " ] && [ -e " + BUSY_MS + " ] && echo yes").trim().equals("yes");
    }

    static void apply(RootShell sh, int boost, int busyMs) throws Exception {
        sh.run("echo " + boost + " > " + BOOST + "; echo " + busyMs + " > " + BUSY_MS);
    }

    static void applySaved(Context c, RootShell sh) throws Exception {
        SharedPreferences p = prefs(c);
        apply(sh, p.getInt("boost", 0), p.getInt("busy_ms", DEFAULT_BUSY_MS));
    }
}
