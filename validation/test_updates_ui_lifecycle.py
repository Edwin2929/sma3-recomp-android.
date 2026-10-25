#!/usr/bin/env python3
"""Run the production UpdatesUi against controlled host lifecycle boundaries.

These are Java host tests, not Android instrumentation: Activity, dialogs and
the asynchronous manager are substitutes so callbacks can be delivered in a
chosen order. The production UpdatesUi and UpdatePolicy are compiled unchanged.
No APK is downloaded or installed. Requires JDK 17 (PATH, JAVA_HOME or --java-home).
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


STUBS = {
    "android/app/Activity.java": """
package android.app;
import android.content.Intent;
import android.content.ActivityNotFoundException;
import android.content.pm.PackageManager;
public class Activity {
    public final PackageManager packages = new PackageManager();
    public boolean foreground, finishing, destroyed, rejectInstaller;
    public int installerStarts, permissionStarts;
    public Intent installedIntent;
    public PackageManager getPackageManager() { return packages; }
    public String getPackageName() { return "org.sma3"; }
    public boolean isFinishing() { return finishing; }
    public boolean isDestroyed() { return destroyed; }
    public void startActivityForResult(Intent intent, int request) {
        if (!foreground) throw new AssertionError("permission UI while paused");
        ++permissionStarts;
    }
    public void startActivity(Intent intent) {
        if (!foreground) throw new AssertionError("installer while paused");
        if (rejectInstaller) throw new ActivityNotFoundException();
        ++installerStarts; installedIntent = intent;
    }
}
""",
    "android/app/AlertDialog.java": """
package android.app;
import java.util.LinkedHashMap;
import java.util.Map;
public class AlertDialog {
    public final Map<String, Runnable> actions = new LinkedHashMap<>();
    public String content = "";
    private boolean showing;
    public boolean isShowing() { return showing; }
    public void show() { showing = true; }
    public void dismiss() { showing = false; }
}
""",
    "android/content/pm/PackageManager.java": """
package android.content.pm;
public class PackageManager {
    public boolean permitted;
    public boolean canRequestPackageInstalls() { return permitted; }
}
""",
    "android/content/ActivityNotFoundException.java": """
package android.content;
public class ActivityNotFoundException extends RuntimeException {}
""",
    "android/content/Intent.java": """
package android.content;
import android.net.Uri;
public class Intent {
    public static final String ACTION_VIEW = "android.intent.action.VIEW";
    public static final int FLAG_GRANT_READ_URI_PERMISSION = 1;
    public final String action;
    public Uri data;
    public String type;
    public int flags;
    public ClipData clip;
    public Intent(String action) { this.action = action; }
    public Intent(String action, Uri data) { this.action = action; this.data = data; }
    public Intent setDataAndType(Uri data, String type) { this.data = data; this.type = type; return this; }
    public Intent addFlags(int flags) { this.flags |= flags; return this; }
    public void setClipData(ClipData clip) { this.clip = clip; }
}
""",
    "android/content/ClipData.java": """
package android.content;
import android.net.Uri;
public class ClipData {
    public final Uri uri;
    private ClipData(Uri uri) { this.uri = uri; }
    public static ClipData newRawUri(String label, Uri uri) { return new ClipData(uri); }
}
""",
    "android/net/Uri.java": """
package android.net;
public class Uri {
    public final String text;
    private Uri(String text) { this.text = text; }
    public static Uri parse(String text) { return new Uri(text); }
}
""",
    "android/provider/Settings.java": """
package android.provider;
public class Settings {
    public static final String ACTION_MANAGE_UNKNOWN_APP_SOURCES = "android.settings.MANAGE_UNKNOWN_APP_SOURCES";
}
""",
    "android/widget/TextView.java": """
package android.widget;
public class TextView {
    public String value;
    public void setText(String value) { this.value = value; }
}
""",
    "android/widget/Toast.java": """
package android.widget;
import android.app.Activity;
public class Toast {
    public static final int LENGTH_LONG = 1;
    public static Toast makeText(Activity activity, String text, int duration) { return new Toast(); }
    public void show() {}
}
""",
    "org/gbarecomp/SettingsPanel.java": """
package org.gbarecomp;
import android.app.Activity;
import android.app.AlertDialog;
import android.widget.TextView;
import java.util.function.Consumer;
final class SettingsPanel {
    static final int UPDATES = 6;
    private final AlertDialog dialog = new AlertDialog();
    SettingsPanel(Activity activity, String title, String subtitle, String button) {
        dialog.content = title + "\\n" + subtitle;
    }
    void toggle(String title, String subtitle, boolean value, Consumer<Boolean> change) {}
    void paragraph(String value) { dialog.content += "\\n" + value; }
    void section(String value) { paragraph(value); }
    TextView notice(String title, String value) { paragraph(title + "\\n" + value); return new TextView(); }
    void card(int icon, String title, String subtitle, Runnable action) { dialog.actions.put(title, action); }
    AlertDialog dialog(Runnable back) { return dialog; }
}
""",
    "org/gbarecomp/UpdateApkProvider.java": """
package org.gbarecomp;
import android.app.Activity;
import android.net.Uri;
final class UpdateApkProvider {
    static Uri uri(Activity activity) { return Uri.parse("content://org.sma3.updates/verified-update.apk"); }
}
""",
    "org/gbarecomp/UpdateManager.java": """
package org.gbarecomp;
import android.app.Activity;
import java.util.ArrayList;
import java.util.List;
final class UpdateManager {
    interface Listener { void changed(State state); }
    interface InstallReady { void ready(boolean verified, String failure); }
    static final class State {
        final UpdatePolicy.Asset asset;
        final String phase, error;
        final long received = 0, checkedAt = 0;
        State(UpdatePolicy.Asset asset, String phase, String error) {
            this.asset = asset; this.phase = phase; this.error = error;
        }
        boolean busy() { return phase.equals("checking") || phase.equals("downloading") || phase.equals("verifying"); }
    }
    static UpdateManager instance;
    static UpdateManager get(Activity activity) { return instance; }
    final List<Listener> listeners = new ArrayList<>();
    final UpdatePolicy.Asset asset;
    State current;
    InstallReady callback;
    int preparations;
    UpdateManager() {
        String name = "sma3-recomp-0.10.6-beta-arm64.apk";
        asset = UpdatePolicy.Asset.checked(123, 1024, name, UpdatePolicy.DOWNLOAD_PREFIX + name, "", "uploaded");
        current = new State(asset, "ready", "");
    }
    String installedName() { return "0.10.5-beta"; }
    State state() { return current; }
    boolean automatic() { return false; }
    void automatic(boolean value) {}
    boolean takeNotification(long id) { return false; }
    void maybeCheck() {}
    void checkNow() {}
    void download() {}
    void cancelDownload() {}
    UpdatePolicy.Asset readyAsset() { return asset; }
    void addListener(Listener listener) { listeners.add(listener); }
    void removeListener(Listener listener) { listeners.remove(listener); }
    void prepareInstall(InstallReady callback) { ++preparations; this.callback = callback; }
    void complete(boolean success, String failure) {
        InstallReady pending = callback; callback = null;
        if (pending == null) throw new AssertionError("No pending verification");
        pending.ready(success, failure);
    }
    void emit(String phase) {
        current = new State(asset, phase, "");
        for (Listener listener : new ArrayList<>(listeners)) listener.changed(current);
    }
}
""",
}

HARNESS = r"""
package org.gbarecomp;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;

public final class UpdatesUiLifecycleTest {
    private static int checks, scenarios;
    private static void check(boolean condition, String message) {
        ++checks;
        if (!condition) throw new AssertionError(message);
    }
    private static final class Rig {
        final Activity activity = new Activity();
        final UpdateManager manager = new UpdateManager();
        final UpdatesUi ui;
        AlertDialog visible;
        int opens;
        Rig(boolean permission) {
            UpdateManager.instance = manager;
            activity.packages.permitted = permission;
            ui = new UpdatesUi(activity, (es,en,pt) -> en, (page,parent) -> {
                if (!activity.foreground) throw new AssertionError("dialog while paused");
                if (visible != null) visible.dismiss();
                visible = page; ++opens; page.show();
            }, this::leave);
        }
        void resume() { activity.foreground = true; ui.onResume(); }
        void pause() { ui.onPause(); activity.foreground = false; }
        void start() { resume(); ui.show(); }
        void tapInstall() {
            Runnable action = visible.actions.get("Install update");
            if (action == null) throw new AssertionError("Install action unavailable");
            action.run();
        }
        void permissionResult() { ui.onActivityResult(UpdatesUi.INSTALL_PERMISSION, 0, null); }
        void leave() { if (visible != null) visible.dismiss(); }
        void close() { ui.close(); activity.destroyed = true; }
    }

    private static void directInstall() {
        ++scenarios;
        Rig r = new Rig(true); r.start(); r.tapInstall();
        check(r.manager.preparations == 1 && r.activity.installerStarts == 0, "tap verifies before installer");
        r.manager.complete(true, null);
        check(r.activity.installerStarts == 1, "verified foreground request opens installer once");
        Intent intent = r.activity.installedIntent;
        check(intent.flags == Intent.FLAG_GRANT_READ_URI_PERMISSION, "installer receives read grant only");
        check("application/vnd.android.package-archive".equals(intent.type), "APK MIME type");
        check(intent.clip != null && intent.clip.uri == intent.data, "same content URI in data and ClipData");
        check(r.visible.actions.containsKey("Install update"), "usable Updates page underneath installer");
        r.pause(); r.resume();
        check(r.activity.installerStarts == 1, "return from cancelled installer does not relaunch it");
    }
    private static void verificationNavigation() {
        ++scenarios;
        Rig r = new Rig(true); r.start(); r.tapInstall(); r.leave();
        int opens = r.opens;
        r.manager.complete(true, null);
        check(r.activity.installerStarts == 0 && r.opens == opens, "leaving verification suppresses callback UI");
        r.manager.emit("available");
        check(r.opens == opens, "later metadata never reopens abandoned page");
        r.ui.show();
        check(r.activity.installerStarts == 0, "reopening Updates requires another Install tap");
    }
    private static void verificationPauseAndDestroy() {
        ++scenarios;
        Rig r = new Rig(true); r.start(); r.tapInstall(); r.pause();
        int opens = r.opens;
        r.manager.complete(true, null);
        check(r.activity.installerStarts == 0 && r.opens == opens, "verification completion cannot open UI in background");
        r.resume();
        check(r.activity.installerStarts == 1, "owned verification page may continue after resume");
        r = new Rig(true); r.start(); r.tapInstall(); r.pause(); r.close();
        opens = r.opens; r.manager.complete(true, null);
        check(r.activity.installerStarts == 0 && r.opens == opens, "destroyed Activity ignores verification result");
        check(r.manager.listeners.isEmpty(), "destroy removes manager observer");
    }
    private static void permissionBeforeResume() {
        ++scenarios;
        Rig r = new Rig(false); r.start(); r.tapInstall();
        check(r.activity.permissionStarts == 1 && r.manager.preparations == 0, "explicit tap opens permission settings");
        r.pause(); r.activity.packages.permitted = true;
        int opens = r.opens; r.permissionResult();
        check(r.opens == opens && r.manager.preparations == 0, "permission result waits for foreground before dialog or verification");
        r.resume();
        check(r.manager.preparations == 1 && r.activity.installerStarts == 0, "owned permission result begins verification on resume");
        r.manager.complete(true, null);
        check(r.activity.installerStarts == 1, "owned successful return continues one explicit install request");
    }
    private static void deniedPermission() {
        ++scenarios;
        Rig r = new Rig(false); r.start(); r.tapInstall(); r.pause();
        int opens = r.opens; r.permissionResult();
        check(r.opens == opens, "denied permission result opens no paused dialog");
        r.resume();
        check(r.manager.preparations == 0 && r.activity.installerStarts == 0, "denied permission never verifies or installs");
        check(r.visible.content.contains("Allow installs"), "permission error remains visible after onResume");
        opens = r.opens; r.manager.emit("available");
        check(r.opens == opens, "metadata cannot replace permission notice");
        check(r.activity.permissionStarts == 1, "no automatic permission request loop");
    }
    private static void recreatedPermissionReturn() {
        ++scenarios;
        Rig r = new Rig(true);
        r.permissionResult();
        check(r.opens == 0 && r.manager.preparations == 0, "new controller has no restored install authority");
        r.resume();
        check(r.visible != null && r.visible.content.contains("Tap Install update"), "recreated controller offers explicit continuation");
        check(r.manager.preparations == 0 && r.activity.installerStarts == 0, "recreation never auto-installs");
        r.tapInstall(); r.manager.complete(true, null);
        check(r.activity.installerStarts == 1, "fresh tap after recreation can install");
    }
    private static void abandonedPermissionReturn() {
        ++scenarios;
        Rig r = new Rig(false); r.start(); r.tapInstall(); r.pause(); r.leave();
        r.activity.packages.permitted = true;
        int opens = r.opens; r.permissionResult(); r.resume();
        check(r.opens == opens && r.manager.preparations == 0, "abandoned permission origin cannot reopen UI");
        r = new Rig(false); r.start(); r.tapInstall(); r.pause();
        r.activity.packages.permitted = true; r.permissionResult(); r.leave();
        opens = r.opens; r.resume();
        check(r.opens == opens && r.manager.preparations == 0, "ownership rechecked between result and resume");
    }
    private static void errorsRemainReviewable() {
        ++scenarios;
        Rig r = new Rig(true); r.start(); r.tapInstall();
        r.manager.complete(false, "digest");
        check(r.activity.installerStarts == 0 && r.visible.content.contains("incomplete"), "failed digest prevents installer and displays error");
        int opens = r.opens; r.pause(); r.resume(); r.manager.emit("ready");
        check(r.opens == opens, "verification notice survives resume and metadata update");
        r = new Rig(true); r.start(); r.activity.rejectInstaller = true; r.tapInstall();
        r.manager.complete(true, null);
        check(r.activity.installerStarts == 0 && r.visible.content.contains("could not be opened"), "missing installer handled without crash");
    }
    public static void main(String[] args) {
        directInstall(); verificationNavigation(); verificationPauseAndDestroy();
        permissionBeforeResume(); deniedPermission(); recreatedPermissionReturn();
        abandonedPermissionReturn(); errorsRemainReviewable();
        System.out.println("PASS production UpdatesUi lifecycle scenarios=" + scenarios + " checks=" + checks);
    }
}
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--java-home", default=os.environ.get("JAVA_HOME"))
    args = parser.parse_args()
    javac = str(Path(args.java_home) / "bin" / "javac") if args.java_home else shutil.which("javac")
    java = str(Path(args.java_home) / "bin" / "java") if args.java_home else shutil.which("java")
    if not javac or not java:
        parser.error("JDK 17 required: set JAVA_HOME or pass --java-home")
    repo = Path(__file__).resolve().parents[1]
    production = repo / "android" / "java" / "org" / "gbarecomp"
    with tempfile.TemporaryDirectory(prefix="sma3-updates-ui-") as temporary:
        root = Path(temporary)
        sources = []
        for name, contents in {**STUBS, "org/gbarecomp/UpdatesUiLifecycleTest.java": HARNESS}.items():
            path = root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents, encoding="utf-8")
            sources.append(str(path))
        output = root / "classes"
        output.mkdir()
        subprocess.run([javac, "-encoding", "UTF-8", "-source", "17", "-target", "17", "-d", str(output),
                        str(production / "UpdatesUi.java"), str(production / "UpdatePolicy.java"), *sources], check=True)
        subprocess.run([java, "-cp", str(output), "org.gbarecomp.UpdatesUiLifecycleTest"], check=True)


if __name__ == "__main__":
    main()
