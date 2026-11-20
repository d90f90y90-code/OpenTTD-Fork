package org.openttdfork.game;

import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.system.Os;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/** Starts OpenTTD: unpacks the game data on first run, points the game to its private storage and hands over to SDL. */
public class OpenTTDActivity extends SDLActivity {
    private static final String ASSET_ROOT = "openttd";
    private static final String VERSION_FILE = "version.txt";

    private static final String DEFAULT_CONFIG =
        "[misc]\n" +
        "videodriver = sdl\n" +
        "[gui]\n" +
        "scroll_mode = 3\n";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File files = getFilesDir();
        File data = new File(files, "data");
        File config = new File(files, "config");

        try {
            Os.setenv("HOME", files.getAbsolutePath(), true);
            Os.setenv("XDG_DATA_HOME", data.getAbsolutePath(), true);
            Os.setenv("XDG_CONFIG_HOME", config.getAbsolutePath(), true);
            Os.setenv("XDG_CACHE_HOME", getCacheDir().getAbsolutePath(), true);
        } catch (Exception e) {
            e.printStackTrace();
        }

        try {
            extractGameData(new File(data, "openttd"));
            writeDefaultConfig(new File(new File(config, "openttd"), "openttd.cfg"));
        } catch (IOException e) {
            e.printStackTrace();
        }

        super.onCreate(savedInstanceState);
        applyFullscreen();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) applyFullscreen();
    }

    /** Use the whole display: draw under the camera cutout and hide the status and navigation bars. */
    private void applyFullscreen() {
        try {
            if (Build.VERSION.SDK_INT >= 28) {
                WindowManager.LayoutParams lp = getWindow().getAttributes();
                lp.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                getWindow().setAttributes(lp);
            }
            View decor = getWindow().getDecorView();
            if (Build.VERSION.SDK_INT >= 30) {
                getWindow().setDecorFitsSystemWindows(false);
                WindowInsetsController controller = decor.getWindowInsetsController();
                if (controller != null) {
                    controller.hide(WindowInsets.Type.systemBars());
                    controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                }
            } else {
                decor.setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void writeDefaultConfig(File cfg) throws IOException {
        if (cfg.exists()) return;
        File dir = cfg.getParentFile();
        if (dir != null) dir.mkdirs();
        try (OutputStream out = new FileOutputStream(cfg)) {
            out.write(DEFAULT_CONFIG.getBytes("UTF-8"));
        }
    }

    private void extractGameData(File target) throws IOException {
        AssetManager am = getAssets();
        String wanted = readAssetText(am, ASSET_ROOT + "/" + VERSION_FILE);
        File marker = new File(target, VERSION_FILE);
        if (marker.exists() && wanted.equals(readFileText(marker))) return;

        copyAssetDir(am, ASSET_ROOT, target);
    }

    private static void copyAssetDir(AssetManager am, String assetPath, File dest) throws IOException {
        String[] children = am.list(assetPath);
        if (children == null || children.length == 0) {
            copyAssetFile(am, assetPath, dest);
            return;
        }
        dest.mkdirs();
        for (String child : children) {
            copyAssetDir(am, assetPath + "/" + child, new File(dest, child));
        }
    }

    private static void copyAssetFile(AssetManager am, String assetPath, File dest) throws IOException {
        File parent = dest.getParentFile();
        if (parent != null) parent.mkdirs();
        try (InputStream in = am.open(assetPath); OutputStream out = new FileOutputStream(dest)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
        }
    }

    private static String readAssetText(AssetManager am, String path) {
        try (InputStream in = am.open(path)) {
            return readStream(in);
        } catch (IOException e) {
            return "unknown";
        }
    }

    private static String readFileText(File f) {
        try (InputStream in = new java.io.FileInputStream(f)) {
            return readStream(in);
        } catch (IOException e) {
            return "";
        }
    }

    private static String readStream(InputStream in) throws IOException {
        java.io.ByteArrayOutputStream bos = new java.io.ByteArrayOutputStream();
        byte[] buf = new byte[4096];
        int n;
        while ((n = in.read(buf)) > 0) bos.write(buf, 0, n);
        return bos.toString("UTF-8").trim();
    }
}
