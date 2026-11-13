package com.rakkashin.gputuner;

import android.app.Activity;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;
import java.util.Locale;

public class MainActivity extends Activity {
    private static final boolean JA = Locale.getDefault().getLanguage().equals("ja");
    private static final int[] BUSY_STEPS = { 10, 20, 30, 50, 75, 100, 150, 200, 300, 500, 750, 1000 };

    private final Handler ui = new Handler(Looper.getMainLooper());
    private SharedPreferences prefs;
    private RootShell sh;
    private TextView status, monitor, busyLabel;
    private RadioGroup boostGroup;
    private SeekBar busyBar;
    private CheckBox onBoot;
    private Button retry;
    private LinearLayout controls;
    private volatile boolean monitoring;

    private static String t(String ja, String en) {
        return JA ? ja : en;
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }

    private TextView text(String s, float size, boolean bold) {
        TextView v = new TextView(this);
        v.setText(s);
        v.setTextSize(size);
        if (bold) v.setTypeface(Typeface.DEFAULT_BOLD);
        v.setPadding(0, dp(6), 0, dp(6));
        return v;
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = Gpu.prefs(this);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(16), dp(16), dp(16), dp(16));
        ScrollView scroll = new ScrollView(this);
        scroll.addView(root);
        scroll.setFitsSystemWindows(true);
        setContentView(scroll);

        status = text(t("root 権限を確認中…", "Checking root…"), 14, false);
        root.addView(status);
        retry = new Button(this);
        retry.setText(t("再試行", "Retry"));
        retry.setVisibility(View.GONE);
        retry.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                retry.setVisibility(View.GONE);
                status.setText(t("root 権限を確認中…", "Checking root…"));
                startInit();
            }
        });
        root.addView(retry);

        controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setVisibility(View.GONE);
        root.addView(controls);

        // Adreno Boost
        controls.addView(text("Adreno Boost", 18, true));
        controls.addView(text(t("GPU が高負荷のときだけ周波数の下限を一時的に引き上げ、立ち上がりを速くします。"
                + "上限・熱制限は超えません。強くするほど電池を使います。",
                "Temporarily raises the GPU frequency floor only while the GPU is saturated, so it ramps up "
                + "faster. Never exceeds the max/thermal caps. Higher levels use more power."), 13, false));
        boostGroup = new RadioGroup(this);
        String[] names = {
            t("0: オフ（純正どおり）", "0: Off (stock)"),
            t("1: 弱（負荷 90% 以上で発動）", "1: Low (busy >= 90%)"),
            t("2: 中（負荷 80% 以上で発動）", "2: Medium (busy >= 80%)"),
            t("3: 強（負荷 70% 以上で発動）", "3: High (busy >= 70%)"),
        };
        for (int i = 0; i < names.length; i++) {
            RadioButton b = new RadioButton(this);
            b.setText(names[i]);
            b.setId(100 + i);
            boostGroup.addView(b);
        }
        boostGroup.check(100 + prefs.getInt("boost", 0));
        boostGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            public void onCheckedChanged(RadioGroup g, int id) {
                prefs.edit().putInt("boost", id - 100).apply();
                applyAsync();
            }
        });
        controls.addView(boostGroup);

        // gpu_busy window
        controls.addView(text(t("GPU 負荷の更新間隔", "GPU load update interval"), 18, true));
        controls.addView(text(t("gpu_busy を何 ms ごとに更新するか。短いほど rk_gpud などの制御が負荷変化に速く反応します。"
                + "純正は 1000 ms。",
                "How often gpu_busy is refreshed. Shorter lets governors such as rk_gpud react faster. "
                + "Stock is 1000 ms."), 13, false));
        busyLabel = text("", 15, false);
        controls.addView(busyLabel);
        busyBar = new SeekBar(this);
        busyBar.setMax(BUSY_STEPS.length - 1);
        busyBar.setProgress(stepOf(prefs.getInt("busy_ms", Gpu.DEFAULT_BUSY_MS)));
        updateBusyLabel();
        busyBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar s, int p, boolean user) {
                updateBusyLabel();
            }
            public void onStartTrackingTouch(SeekBar s) { }
            public void onStopTrackingTouch(SeekBar s) {
                prefs.edit().putInt("busy_ms", BUSY_STEPS[s.getProgress()]).apply();
                applyAsync();
            }
        });
        controls.addView(busyBar);

        onBoot = new CheckBox(this);
        onBoot.setText(t("起動時にこの設定を自動で適用する", "Apply these settings at boot"));
        onBoot.setChecked(prefs.getBoolean("on_boot", false));
        onBoot.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton b, boolean on) {
                prefs.edit().putBoolean("on_boot", on).apply();
            }
        });
        controls.addView(onBoot);

        Button reset = new Button(this);
        reset.setText(t("おすすめ設定に戻す（Boost オフ・100 ms）", "Reset to defaults (Boost off, 100 ms)"));
        reset.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                prefs.edit().putInt("boost", 0).putInt("busy_ms", Gpu.DEFAULT_BUSY_MS).apply();
                boostGroup.check(100);
                busyBar.setProgress(stepOf(Gpu.DEFAULT_BUSY_MS));
                applyAsync();
            }
        });
        controls.addView(reset);

        controls.addView(text(t("モニター", "Monitor"), 18, true));
        monitor = text("", 14, false);
        monitor.setTypeface(Typeface.MONOSPACE);
        controls.addView(monitor);

        startInit();
    }

    private void startInit() {
        new Thread(new Runnable() {
            public void run() {
                init();
            }
        }).start();
    }

    private void showRetry() {
        ui.post(new Runnable() {
            public void run() {
                retry.setVisibility(View.VISIBLE);
            }
        });
    }

    private int stepOf(int ms) {
        int best = 0;
        for (int i = 0; i < BUSY_STEPS.length; i++)
            if (Math.abs(BUSY_STEPS[i] - ms) < Math.abs(BUSY_STEPS[best] - ms)) best = i;
        return best;
    }

    private void updateBusyLabel() {
        int ms = BUSY_STEPS[busyBar.getProgress()];
        busyLabel.setText(ms + " ms" + (ms == Gpu.STOCK_BUSY_MS ? t("（純正）", " (stock)")
                : ms == Gpu.DEFAULT_BUSY_MS ? t("（おすすめ）", " (default)") : ""));
    }

    private void setStatus(final String s) {
        ui.post(new Runnable() {
            public void run() {
                status.setText(s);
            }
        });
    }

    private void init() {
        try {
            sh = RootShell.get();
        } catch (Exception e) {
            setStatus(t("root 権限が必要です。KernelSU / Magisk でこのアプリに root を許可してください。",
                    "Root is required. Grant root to this app in KernelSU / Magisk."));
            showRetry();
            return;
        }
        try {
            if (!Gpu.supported(sh)) {
                setStatus(t("改造版 GPU ドライバー（kgsl-avr）が読み込まれていません。"
                        + "TWRP で kgsl-avr のインストール ZIP を焼いてから再起動してください。",
                        "The modified GPU driver (kgsl-avr) is not loaded. Flash the kgsl-avr installer "
                        + "ZIP in TWRP and reboot."));
                showRetry();
                return;
            }
            final String model = sh.run("cat " + Gpu.KGSL + "/gpu_model").trim();
            final int boost = Integer.parseInt(sh.run("cat " + Gpu.BOOST).trim());
            final int busy = Integer.parseInt(sh.run("cat " + Gpu.BUSY_MS).trim());
            ui.post(new Runnable() {
                public void run() {
                    status.setText(t("改造版ドライバー動作中", "Modified driver active") + " — " + model);
                    controls.setVisibility(View.VISIBLE);
                    // show what the driver is really using right now
                    boostGroup.check(100 + Math.max(0, Math.min(3, boost)));
                    busyBar.setProgress(stepOf(busy));
                    updateBusyLabel();
                }
            });
            startMonitor();
        } catch (Exception e) {
            setStatus(t("エラー: ", "Error: ") + e.getMessage());
        }
    }

    private void applyAsync() {
        new Thread(new Runnable() {
            public void run() {
                try {
                    if (sh != null) Gpu.applySaved(MainActivity.this, sh);
                } catch (Exception e) {
                    setStatus(t("適用に失敗: ", "Apply failed: ") + e.getMessage());
                }
            }
        }).start();
    }

    private void startMonitor() {
        if (monitoring || sh == null) return;
        monitoring = true;
        new Thread(new Runnable() {
            public void run() {
                while (monitoring) {
                    try {
                        final String s = sh.run(
                            "K=" + Gpu.KGSL + "; "
                            + "echo \"" + t("周波数", "clock") + ":   $(cat $K/clock_mhz) MHz\"; "
                            + "echo \"" + t("上限", "max") + ":     $(cat $K/max_clock_mhz) MHz\"; "
                            + "echo \"" + t("GPU 負荷", "GPU busy") + ": $(cat $K/gpu_busy_percentage)\"; "
                            + "echo \"" + t("温度", "temp") + ":     $(( $(cat $K/temp) / 1000 )) °C\"; "
                            + "cat $K/adrenoboost_stats | tr ' ' '\\n' | paste - - | sed 's/\\t/: /'");
                        ui.post(new Runnable() {
                            public void run() {
                                monitor.setText(s);
                            }
                        });
                        Thread.sleep(1000);
                    } catch (Exception e) {
                        monitoring = false;
                    }
                }
            }
        }).start();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (controls.getVisibility() == View.VISIBLE) startMonitor();
    }

    @Override
    protected void onPause() {
        super.onPause();
        monitoring = false;
    }
}
