package com.rakkashin.gputuner;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.util.concurrent.TimeUnit;

/**
 * Runs each command in its own "su -c" process with a timeout.  A long-lived
 * interactive su shell hung with KernelSU (input never reached the root sh),
 * so nothing here depends on stdin.
 */
final class RootShell {
    private static RootShell instance;

    private RootShell() { }

    static synchronized RootShell get() throws Exception {
        if (instance != null) return instance;
        RootShell sh = new RootShell();
        String id = sh.run("id -u", 30).trim();
        if (!id.equals("0")) throw new Exception("root not granted");
        instance = sh;
        return instance;
    }

    String run(String cmd) throws Exception {
        return run(cmd, 10);
    }

    String run(String cmd, int timeoutSec) throws Exception {
        Process p = new ProcessBuilder("su", "-c", cmd).redirectErrorStream(true).start();
        p.getOutputStream().close();
        final InputStream in = p.getInputStream();
        final ByteArrayOutputStream buf = new ByteArrayOutputStream();
        Thread reader = new Thread(new Runnable() {
            public void run() {
                byte[] b = new byte[4096];
                int n;
                try {
                    while ((n = in.read(b)) > 0) buf.write(b, 0, n);
                } catch (Exception ignored) {
                }
            }
        });
        reader.start();
        if (!p.waitFor(timeoutSec, TimeUnit.SECONDS)) {
            p.destroy();
            throw new Exception("su timed out");
        }
        reader.join(1000);
        String out = buf.toString("UTF-8");
        if (out.endsWith("\n")) out = out.substring(0, out.length() - 1);
        return out;
    }
}
