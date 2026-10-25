package org.gbarecomp;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.ZipFile;
import java.util.zip.ZipEntry;

/** One application-owned worker; callbacks never hold an Activity after it unregisters. */
final class UpdateManager {
    interface Listener { void changed(State state); }
    interface InstallReady { void ready(boolean verified, String failure); }
    static final class State {
        final UpdatePolicy.Asset asset;
        final String phase, error;
        final long received, checkedAt;
        State(UpdatePolicy.Asset asset, String phase, String error, long received, long checkedAt) {
            this.asset = asset; this.phase = phase; this.error = error;
            this.received = received; this.checkedAt = checkedAt;
        }
        boolean busy() { return phase.equals("checking") || phase.equals("downloading") || phase.equals("verifying"); }
    }
    private static volatile UpdateManager instance;
    static UpdateManager get(Context context) {
        UpdateManager result = instance;
        if (result == null) synchronized (UpdateManager.class) {
            if (instance == null) instance = new UpdateManager(context.getApplicationContext());
            result = instance;
        }
        return result;
    }

    private final Context context;
    private final SharedPreferences prefs;
    private final ExecutorService worker = Executors.newSingleThreadExecutor(task -> new Thread(() -> {
        android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);
        task.run();
    }, "sma3-updates"));
    private final Handler main = new Handler(Looper.getMainLooper());
    private final CopyOnWriteArrayList<Listener> listeners = new CopyOnWriteArrayList<>();
    private final UpdateHttp http = new UpdateHttp();
    private final PackageInfo installed;
    private final UpdatePolicy.Version installedVersion;
    private volatile State state;
    private volatile UpdateHttp.Cancel transfer;
    private boolean operation;

    private UpdateManager(Context context) {
        this.context = context;
        prefs = context.getSharedPreferences("sma3-updates", Context.MODE_PRIVATE);
        PackageInfo info = null;
        try { info = context.getPackageManager().getPackageInfo(context.getPackageName(), PackageManager.GET_SIGNING_CERTIFICATES); }
        catch (PackageManager.NameNotFoundException ignored) {}
        installed = info;
        installedVersion = info == null ? null : UpdatePolicy.Version.parse(info.versionName);
        UpdatePolicy.Asset cached = UpdateRelease.decode(prefs.getString("candidate", ""));
        if (cached != null && (installedVersion == null || cached.version.compareTo(installedVersion) <= 0)) cached = null;
        state = new State(cached, cached == null ? "idle" : "available", "", 0, prefs.getLong("last-check", 0));
        // Process death may leave a partial transfer. Only the exact private
        // filenames used by this updater are touched; game/mod/save data is separate.
        worker.execute(() -> {
            partialFile().delete();
            UpdatePolicy.Asset ready = UpdateRelease.decode(prefs.getString("ready-asset", ""));
            if (ready == null || installedVersion == null || ready.version.compareTo(installedVersion) <= 0) {
                readyFile().delete();
                prefs.edit().remove("ready-asset").remove("ready-hash").apply();
            }
        });
    }

    String installedName() { return installed == null ? "?" : installed.versionName; }
    State state() { return state; }
    boolean automatic() { return prefs.getBoolean("automatic", true); }
    void automatic(boolean enabled) { prefs.edit().putBoolean("automatic", enabled).apply(); if (enabled) maybeCheck(); }
    void addListener(Listener listener) { listeners.addIfAbsent(listener); }
    void removeListener(Listener listener) { listeners.remove(listener); }
    boolean takeNotification(long assetId) {
        if (prefs.getLong("notified-asset", 0) == assetId) return false;
        prefs.edit().putLong("notified-asset", assetId).apply(); return true;
    }

    void maybeCheck() {
        if (UpdatePolicy.shouldCheck(System.currentTimeMillis(), prefs.getLong("next-check", 0), automatic())) check(false);
    }
    void checkNow() { check(true); }

    private synchronized void check(boolean manual) {
        if (operation) return;
        long now = System.currentTimeMillis();
        long rateUntil = prefs.getLong("rate-until", 0);
        if (rateUntil > now && rateUntil - now <= UpdatePolicy.CHECK_INTERVAL_MS) {
            if (manual) publish(new State(state.asset, "error", "rate", 0, state.checkedAt));
            return;
        }
        if (installedVersion == null || installed == null) {
            if (manual) publish(new State(null, "error", "installed", 0, state.checkedAt));
            return;
        }
        operation = true;
        UpdatePolicy.Asset previous = state.asset;
        publish(new State(previous, "checking", "", 0, state.checkedAt));
        worker.execute(() -> {
            State terminal;
            try {
                boolean validCache = prefs.getBoolean("cache-valid", false);
                UpdateHttp.Metadata response = http.metadata(validCache ? prefs.getString("etag", "") : null, new UpdateHttp.Cancel());
                if (response.unchanged && !validCache) throw new UpdateHttp.Failure("metadata");
                UpdatePolicy.Asset selected = response.unchanged ? previous :
                    UpdateRelease.parse(new String(response.bytes, StandardCharsets.UTF_8), installedVersion);
                long checked = System.currentTimeMillis();
                SharedPreferences.Editor edit = prefs.edit().putString("candidate", UpdateRelease.encode(selected))
                    .putBoolean("cache-valid", true).putLong("last-check", checked)
                    .putLong("next-check", checked + UpdatePolicy.CHECK_INTERVAL_MS).remove("rate-until");
                if (!response.unchanged || !response.etag.isEmpty()) edit.putString("etag", response.etag);
                edit.apply();
                terminal = new State(selected, selected == null ? "current" : "available", "", 0, checked);
            } catch (Exception error) {
                String kind = failure(error);
                long retry = error instanceof UpdateHttp.Failure ? ((UpdateHttp.Failure) error).retryAt : 0;
                SharedPreferences.Editor edit = prefs.edit().putLong("next-check", Math.max(retry,
                    System.currentTimeMillis() + UpdatePolicy.FAILURE_BACKOFF_MS));
                if (retry > 0) edit.putLong("rate-until", retry);
                edit.apply();
                terminal = new State(previous, "error", kind, 0, state.checkedAt);
            }
            finishOperation(terminal);
        });
    }

    synchronized void download() {
        UpdatePolicy.Asset asset = state.asset;
        if (operation || asset == null) return;
        operation = true;
        UpdateHttp.Cancel cancellation = new UpdateHttp.Cancel();
        transfer = cancellation;
        publish(new State(asset, "downloading", "", 0, state.checkedAt));
        worker.execute(() -> {
            File partial = partialFile();
            State terminal;
            try {
                directory();
                partial.delete();
                long[] lastProgress = {0};
                String digest = http.download(asset, partial, cancellation, (done, total) -> {
                    long now = System.nanoTime();
                    if (now - lastProgress[0] > 250000000L || done == total) {
                        lastProgress[0] = now;
                        publish(new State(asset, "downloading", "", done, state.checkedAt));
                    }
                });
                cancellation.check();
                publish(new State(asset, "verifying", "", asset.size, state.checkedAt));
                verifyArchive(partial, asset);
                cancellation.check();
                try {
                    Files.move(partial.toPath(), readyFile().toPath(), StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
                } catch (IOException error) { throw new UpdateHttp.Failure("storage"); }
                if (!prefs.edit().putString("ready-asset", UpdateRelease.encode(asset)).putString("ready-hash", digest).commit()) {
                    readyFile().delete(); throw new UpdateHttp.Failure("storage");
                }
                terminal = new State(asset, "ready", "", asset.size, state.checkedAt);
            } catch (Exception error) {
                terminal = new State(asset, "error", failure(error), 0, state.checkedAt);
            } finally {
                partial.delete(); transfer = null;
            }
            finishOperation(terminal);
        });
    }

    void cancelDownload() { UpdateHttp.Cancel cancel = transfer; if (cancel != null) cancel.cancel(); }

    UpdatePolicy.Asset readyAsset() {
        UpdatePolicy.Asset ready = UpdateRelease.decode(prefs.getString("ready-asset", ""));
        return ready != null && installedVersion != null && ready.version.compareTo(installedVersion) > 0 && readyFile().isFile() ? ready : null;
    }

    synchronized void prepareInstall(InstallReady callback) {
        if (operation) { main.post(() -> callback.ready(false, "busy")); return; }
        operation = true;
        worker.execute(() -> {
            String problem = null;
            UpdatePolicy.Asset ready = UpdateRelease.decode(prefs.getString("ready-asset", ""));
            try {
                if (ready == null || !readyFile().isFile()) throw new UpdateHttp.Failure("missing_download");
                if (readyFile().length() != ready.size) throw new UpdateHttp.Failure("size");
                MessageDigest digest = UpdateHttp.sha256();
                try (FileInputStream input = new FileInputStream(readyFile())) {
                    byte[] block = new byte[65536]; int count;
                    while ((count = input.read(block)) != -1) digest.update(block, 0, count);
                }
                if (!UpdateHttp.hex(digest.digest()).equals(prefs.getString("ready-hash", "")))
                    throw new UpdateHttp.Failure("digest");
                verifyArchive(readyFile(), ready);
            } catch (Exception error) { problem = failure(error); }
            final String result = problem;
            if (problem != null) {
                readyFile().delete(); prefs.edit().remove("ready-asset").remove("ready-hash").apply();
            }
            finishOperation(problem == null ? state : new State(state.asset, "error", problem, 0, state.checkedAt));
            main.post(() -> callback.ready(result == null, result));
        });
    }

    private void verifyArchive(File apk, UpdatePolicy.Asset asset) throws IOException {
        PackageInfo archive = context.getPackageManager().getPackageArchiveInfo(apk.getAbsolutePath(), PackageManager.GET_SIGNING_CERTIFICATES);
        if (archive == null || archive.applicationInfo == null || installed == null) throw new UpdateHttp.Failure("package");
        boolean arm64;
        try (ZipFile zip = new ZipFile(apk)) {
            arm64 = arm64Library(zip, "lib/arm64-v8a/libmain.so") && arm64Library(zip, "lib/arm64-v8a/libSDL2.so");
        }
        String error = UpdatePolicy.identityError(context.getPackageName(), installed.getLongVersionCode(), signers(installed),
            archive.packageName, archive.getLongVersionCode(), signers(archive), archive.versionName,
            asset.version, archive.applicationInfo.minSdkVersion, Build.VERSION.SDK_INT, arm64);
        if (error != null) throw new UpdateHttp.Failure(error);
    }

    private static boolean arm64Library(ZipFile zip, String name) throws IOException {
        ZipEntry entry = zip.getEntry(name);
        if (entry == null || entry.isDirectory() || entry.getSize() < 20) return false;
        byte[] elf = new byte[20]; int at = 0;
        try (InputStream input = zip.getInputStream(entry)) {
            while (at < elf.length) {
                int read = input.read(elf, at, elf.length - at);
                if (read <= 0) return false;
                at += read;
            }
        }
        return UpdatePolicy.isArm64LibraryHeader(elf);
    }

    private static List<String> signers(PackageInfo info) {
        List<String> result = new ArrayList<>();
        if (info.signingInfo == null) return result;
        Signature[] signatures = info.signingInfo.getApkContentsSigners();
        if (signatures != null) for (Signature signature : signatures)
            result.add(UpdateHttp.hex(UpdateHttp.sha256().digest(signature.toByteArray())));
        return result;
    }

    private static String failure(Exception error) {
        if (error instanceof UpdateHttp.Failure) return ((UpdateHttp.Failure) error).kind;
        if (error instanceof org.json.JSONException) return "metadata";
        if (error instanceof SecurityException) return "permission";
        return "network";
    }
    private File directory() throws IOException {
        File folder = new File(context.getCacheDir(), "sma3-updates");
        if (!folder.isDirectory() && !folder.mkdirs()) throw new UpdateHttp.Failure("storage");
        return folder;
    }
    private File partialFile() { return new File(context.getCacheDir(), "sma3-updates/update.part"); }
    File readyFile() { return new File(context.getCacheDir(), "sma3-updates/verified-update.apk"); }
    private synchronized void finishOperation(State terminal) {
        // Cleanup finishes before another operation is allowed. Publish the
        // terminal state under the same lock, so a late old completion cannot
        // overwrite the state of a newly started check/download.
        operation = false;
        publish(terminal);
    }
    private void publish(State next) {
        state = next;
        main.post(() -> { for (Listener listener : listeners) listener.changed(next); });
    }
}
