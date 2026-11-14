package org.openttdfork.game;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Typeface;
import android.os.Bundle;
import android.system.Os;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.PrintWriter;
import java.io.RandomAccessFile;
import java.io.StringWriter;

/**
 * Launcher activity. If the previous run crashed it shows what happened, otherwise it starts the game.
 */
public class DiagnosticsActivity extends Activity {
    private File crashFile;
    private File stderrFile;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        File files = getFilesDir();
        crashFile = new File(files, "crash.txt");
        stderrFile = new File(files, "openttd-stderr.txt");

        try {
            Os.setenv("HOME", files.getAbsolutePath(), true);
            Os.setenv("XDG_DATA_HOME", new File(files, "data").getAbsolutePath(), true);
            Os.setenv("XDG_CONFIG_HOME", new File(files, "config").getAbsolutePath(), true);
            Os.setenv("XDG_CACHE_HOME", getCacheDir().getAbsolutePath(), true);
        } catch (Exception e) {
            e.printStackTrace();
        }

        final File javaCrashFile = crashFile;
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                try {
                    StringWriter sw = new StringWriter();
                    e.printStackTrace(new PrintWriter(sw));
                    FileOutputStream out = new FileOutputStream(javaCrashFile);
                    out.write(("Java crash in thread " + t.getName() + ":\n" + sw).getBytes("UTF-8"));
                    out.close();
                } catch (Exception ignored) {
                }
                android.os.Process.killProcess(android.os.Process.myPid());
            }
        });

        if (crashFile.exists()) {
            showReport();
        } else {
            startGame();
        }
    }

    private void startGame() {
        startActivity(new Intent(this, OpenTTDActivity.class));
        finish();
    }

    private String readTail(File f, int maxBytes) {
        if (!f.exists()) return "(no file)";
        try (RandomAccessFile raf = new RandomAccessFile(f, "r")) {
            long len = raf.length();
            int n = (int) Math.min(len, maxBytes);
            raf.seek(len - n);
            byte[] buf = new byte[n];
            raf.readFully(buf);
            return new String(buf, "UTF-8");
        } catch (Exception e) {
            return "(cannot read: " + e + ")";
        }
    }

    private void showReport() {
        final String report = "=== crash.txt ===\n" + readTail(crashFile, 6000)
            + "\n\n=== openttd-stderr.txt (end) ===\n" + readTail(stderrFile, 5000)
            + "\n\n=== device ===\n" + android.os.Build.MANUFACTURER + " " + android.os.Build.MODEL
            + ", Android " + android.os.Build.VERSION.RELEASE + ", ABI " + android.os.Build.SUPPORTED_ABIS[0];

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(24, 24, 24, 24);

        TextView title = new TextView(this);
        title.setText("OpenTTD Fork crashed last time");
        title.setTextSize(20);
        root.addView(title);

        LinearLayout buttons = new LinearLayout(this);
        buttons.setOrientation(LinearLayout.HORIZONTAL);

        Button copy = new Button(this);
        copy.setText("Copy");
        copy.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                cm.setPrimaryClip(ClipData.newPlainText("OpenTTD crash report", report));
            }
        });
        buttons.addView(copy);

        Button retry = new Button(this);
        retry.setText("Try again");
        retry.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                crashFile.delete();
                startGame();
            }
        });
        buttons.addView(retry);

        Button reset = new Button(this);
        reset.setText("Reset data");
        reset.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                crashFile.delete();
                deleteRecursive(new File(getFilesDir(), "data"));
                deleteRecursive(new File(getFilesDir(), "config"));
                startGame();
            }
        });
        buttons.addView(reset);
        root.addView(buttons);

        ScrollView scroll = new ScrollView(this);
        TextView body = new TextView(this);
        body.setTypeface(Typeface.MONOSPACE);
        body.setTextSize(11);
        body.setTextIsSelectable(true);
        body.setText(report);
        scroll.addView(body);
        root.addView(scroll);

        setContentView(root);
    }

    private static void deleteRecursive(File f) {
        File[] children = f.listFiles();
        if (children != null) for (File c : children) deleteRecursive(c);
        f.delete();
    }
}
