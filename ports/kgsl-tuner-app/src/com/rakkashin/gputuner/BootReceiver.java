package com.rakkashin.gputuner;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

public class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(final Context context, Intent intent) {
        if (!Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) return;
        if (!Gpu.prefs(context).getBoolean("on_boot", false)) return;
        final PendingResult result = goAsync();
        new Thread(new Runnable() {
            public void run() {
                try {
                    RootShell sh = RootShell.get();
                    if (Gpu.supported(sh)) Gpu.applySaved(context, sh);
                } catch (Exception e) {
                    Log.w("RkGpuTuner", "apply on boot failed", e);
                } finally {
                    result.finish();
                }
            }
        }).start();
    }
}
