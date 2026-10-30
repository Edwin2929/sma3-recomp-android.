package org.gbarecomp;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.InputDevice;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.EnumMap;
import java.util.Locale;

/** Allowlisted diagnostics only. Never reads files, logs, unique IDs or device names. */
final class BugReportCollector {
    private BugReportCollector() {}

    static BugReportModel.Preferences preferences(Context context) {
        final SharedPreferences prefs = context.getSharedPreferences("sma3-options", Context.MODE_PRIVATE);
        return new BugReportModel.Preferences() {
            public boolean contains(String key) { return prefs.contains(key); }
            public boolean flag(String key, boolean fallback) {
                try { return prefs.getBoolean(key, fallback); }
                catch (ClassCastException malformed) { return fallback; }
            }
            public int number(String key, int fallback) {
                try { return prefs.getInt(key, fallback); }
                catch (ClassCastException malformed) { return fallback; }
            }
        };
    }

    static BugReportModel.Snapshot capture(Activity activity, BugReportUi.RuntimeInfo runtime) {
        EnumMap<BugReportModel.Field, String> values = new EnumMap<>(BugReportModel.Field.class);
        values.putAll(BugReportModel.settings(preferences(activity)));
        String version = "unavailable";
        try {
            PackageInfo info = activity.getPackageManager().getPackageInfo(activity.getPackageName(), 0);
            if (info.versionName != null && !info.versionName.isEmpty())
                version = info.versionName + " (" + info.getLongVersionCode() + ")";
        } catch (PackageManager.NameNotFoundException | RuntimeException unavailable) { /* Still usable before a game starts. */ }
        values.put(BugReportModel.Field.APP, version);
        values.put(BugReportModel.Field.PACKAGE, activity.getPackageName());
        try { values.put(BugReportModel.Field.PAYLOAD, GbaGameConfig.load(activity).payloadVersion); }
        catch (RuntimeException unavailable) { values.put(BugReportModel.Field.PAYLOAD, "unavailable"); }
        values.put(BugReportModel.Field.ORIGIN, runtime.gameScreen ? "game" : "setup");
        values.put(BugReportModel.Field.DEVICE, Build.MANUFACTURER + " / " + Build.MODEL);
        values.put(BugReportModel.Field.ANDROID, Build.VERSION.RELEASE + " (API " + Build.VERSION.SDK_INT + ")");
        values.put(BugReportModel.Field.ARCH, String.join(", ", Build.SUPPORTED_ABIS));
        DisplayMetrics metrics = activity.getResources().getDisplayMetrics();
        values.put(BugReportModel.Field.WINDOW, metrics.widthPixels + " × " + metrics.heightPixels + " px");
        try {
            Display display = activity.getWindowManager().getDefaultDisplay();
            Display.Mode mode = display.getMode();
            values.put(BugReportModel.Field.DISPLAY, mode.getPhysicalWidth() + " × " + mode.getPhysicalHeight()
                + " px; " + decimal(mode.getRefreshRate()) + " Hz");
            values.put(BugReportModel.Field.REFRESH, decimal(display.getRefreshRate()) + " Hz");
        } catch (RuntimeException unavailable) {
            values.put(BugReportModel.Field.DISPLAY, "unavailable");
            values.put(BugReportModel.Field.REFRESH, "unavailable");
        }
        try {
            int count = 0;
            for (int id : InputDevice.getDeviceIds()) {
                InputDevice device = InputDevice.getDevice(id);
                if (device != null && !device.isVirtual() && (device.supportsSource(InputDevice.SOURCE_GAMEPAD)
                        || device.supportsSource(InputDevice.SOURCE_JOYSTICK))) ++count;
            }
            values.put(BugReportModel.Field.CONTROLLERS, Integer.toString(count));
        } catch (RuntimeException unavailable) { values.put(BugReportModel.Field.CONTROLLERS, "unavailable"); }
        values.put(BugReportModel.Field.INTERPOLATION, runtime.doubleRateActive == null
            ? "unavailable" : BugReportModel.enabled(runtime.doubleRateActive));
        PresentationStats stats = runtime.latest;
        values.put(BugReportModel.Field.OBSERVED_FPS, stats == null ? "unavailable" : stats.fps + " FPS");
        if (stats != null && stats.hasTimings()) {
            values.put(BugReportModel.Field.BASE_FPS, stats.baseFps + " FPS");
            values.put(BugReportModel.Field.EXTRA_FPS, stats.extraFps + " FPS");
            values.put(BugReportModel.Field.PREPARATION, stats.preparationMs + " ms");
            values.put(BugReportModel.Field.SWAP, stats.swapMs + " ms");
        }
        values.put(BugReportModel.Field.MODS, "unavailable");
        return new BugReportModel.Snapshot(values);
    }

    /** Worker-only call. The native catalog serializes reads/mutations with profile_mutex. */
    static BugReportModel.Snapshot profiles(BugReportModel.Snapshot source, boolean nativeReady) {
        if (!nativeReady) return source;
        try {
            String json = GbaNative.listMods();
            // Do not retain error strings, names, authors, IDs, paths or descriptions.
            if (json == null || json.length() > 1024 * 1024) return source;
            JSONObject response = new JSONObject(json);
            if (!response.optBoolean("ok", false)) return source;
            JSONArray mods = response.getJSONArray("mods");
            if (mods.length() > 1024) return source;
            int active = 0;
            for (int i = 0; i < mods.length(); ++i) {
                JSONObject mod = mods.getJSONObject(i);
                if (mod.getBoolean("enabled") && mod.optBoolean("compatible", true)) ++active;
            }
            return source.mods(mods.length(), active);
        } catch (UnsatisfiedLinkError | JSONException | RuntimeException unavailable) { return source; }
    }

    private static String decimal(float value) {
        return Float.isFinite(value) && value > 0 && value < 10000
            ? String.format(Locale.US, "%.2f", value) : "unavailable";
    }
}
