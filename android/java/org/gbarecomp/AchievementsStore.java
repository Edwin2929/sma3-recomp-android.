package org.gbarecomp;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.AtomicFile;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/** One process-owned, serial store: lifecycle recreation cannot race disk writes. */
final class AchievementsStore {
    interface NativeSnapshot { long[] read(); }
    interface Listener { void updated(Result result); }
    interface Persistence {
        AchievementsModel.State read() throws IOException;
        void write(AchievementsModel.State state) throws IOException;
    }
    enum Problem { NONE, LOADING, READ, WRITE, NATIVE }
    static final class Result {
        final AchievementsModel.State state;
        final Problem problem;
        final int newUnlocks;
        Result(AchievementsModel.State state, Problem problem, int newUnlocks) {
            this.state = state; this.problem = problem; this.newUnlocks = newUnlocks;
        }
    }

    private static AchievementsStore instance;
    private static File instanceDirectory;
    static synchronized AchievementsStore get(Context context) {
        File directory = context.getApplicationContext().getFilesDir();
        if (instance == null || !directory.equals(instanceDirectory)) {
            instanceDirectory = directory;
            instance = new AchievementsStore(new Disk(new File(directory, "achievements/progress-v1.json")),
                Executors.newSingleThreadExecutor(task -> new Thread(task, "SMA3 achievements")),
                new Handler(Looper.getMainLooper()));
        }
        return instance;
    }

    private final Persistence disk;
    private final Executor worker;
    private final Handler main;
    private AchievementsModel.State current = AchievementsModel.empty();
    private AchievementsModel.State saved = current;
    private boolean loaded;
    private volatile Result latest = new Result(current, Problem.LOADING, 0);

    AchievementsStore(Persistence disk, Executor worker, Handler main) {
        this.disk = disk; this.worker = worker; this.main = main;
    }
    Result latest() { return latest; }

    void poll(NativeSnapshot source, Listener listener) {
        worker.execute(() -> {
            if (!load(listener)) return;
            Problem problem = Problem.NONE;
            try { current = AchievementsModel.merge(current, source.read(), System.currentTimeMillis()); }
            catch (UnsatisfiedLinkError | RuntimeException error) {
                problem = Problem.NATIVE;
            }
            finish(problem, listener);
        });
    }

    void notifications(boolean enabled, Listener listener) {
        worker.execute(() -> {
            if (!load(listener)) return;
            current = current.notifications(enabled);
            finish(Problem.NONE, listener);
        });
    }

    private boolean load(Listener listener) {
        if (loaded) return true;
        try {
            current = disk.read(); saved = current; loaded = true;
            return true;
        } catch (IOException | RuntimeException error) {
            // Preserve an unreadable/future-format file. Never overwrite it with
            // an empty catalog, and retry on the next sampling request.
            publish(new Result(current, Problem.READ, 0), listener);
            return false;
        }
    }

    private void finish(Problem problem, Listener listener) {
        int unlocked = 0;
        if (!current.same(saved)) {
            try {
                disk.write(current);
                unlocked = AchievementsModel.newUnlocks(saved, current);
                saved = current;
            } catch (IOException | RuntimeException error) {
                // Current evidence and first-unlock dates remain queued in
                // memory. A later poll retries, even if its native snapshot is
                // older or unavailable. Announcements require a durable write.
                problem = Problem.WRITE;
            }
        }
        publish(new Result(current, problem, unlocked), listener);
    }

    private void publish(Result result, Listener listener) {
        latest = new Result(result.state, result.problem, 0);
        if (listener != null) main.post(() -> listener.updated(result));
    }

    static final class Disk implements Persistence {
        private static final int MAX_BYTES = 64 * 1024;
        private final AtomicFile file;
        Disk(File path) { file = new AtomicFile(path); }

        public AchievementsModel.State read() throws IOException {
            File base = file.getBaseFile();
            if (!base.exists() && !new File(base.getPath() + ".bak").exists()) return AchievementsModel.empty();
            try (FileInputStream input = file.openRead()) {
                ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                byte[] buffer = new byte[4096];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    if (bytes.size() + count > MAX_BYTES) throw new IOException("Achievement file too large");
                    bytes.write(buffer, 0, count);
                }
                return decode(new String(bytes.toByteArray(), StandardCharsets.UTF_8));
            }
        }

        public void write(AchievementsModel.State state) throws IOException {
            File directory = file.getBaseFile().getParentFile();
            if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Achievement directory unavailable");
            byte[] bytes = encode(state).getBytes(StandardCharsets.UTF_8);
            FileOutputStream output = null;
            try {
                output = file.startWrite();
                output.write(bytes);
                // Explicit sync propagates failure instead of relying only on
                // AtomicFile's best-effort finishWrite logging.
                output.getFD().sync();
                file.finishWrite(output);
                // finishWrite has a void API and may log a failed rename. Read
                // the committed record before reporting a durable unlock.
                if (!state.same(read())) throw new IOException("Achievement commit verification failed");
            } catch (IOException | RuntimeException error) {
                if (output != null) file.failWrite(output);
                throw new IOException("Achievement write failed", error);
            }
        }

        static String encode(AchievementsModel.State state) throws IOException {
            try {
                JSONObject root = new JSONObject();
                root.put("schema", AchievementsModel.SCHEMA);
                root.put("notifications", state.notifications);
                JSONObject evidence = new JSONObject(), dates = new JSONObject();
                for (int i = 0; i < AchievementsModel.METRICS; ++i)
                    evidence.put(AchievementsModel.METRIC_KEYS[i], state.metrics[i]);
                for (int i = 0; i < AchievementsModel.DEFINITIONS.length; ++i)
                    dates.put(AchievementsModel.DEFINITIONS[i].id, state.unlockedAt[i]);
                root.put("evidence", evidence); root.put("unlocked_at", dates);
                return root.toString() + "\n";
            } catch (JSONException error) { throw new IOException("Achievement encoding failed", error); }
        }

        private static long integer(JSONObject object, String key) throws JSONException, IOException {
            Object value = object.get(key);
            if (!(value instanceof Integer) && !(value instanceof Long))
                throw new IOException("Achievement value is not an integer");
            return ((Number)value).longValue();
        }

        static AchievementsModel.State decode(String json) throws IOException {
            try {
                JSONObject root = new JSONObject(json);
                if (integer(root, "schema") != AchievementsModel.SCHEMA)
                    throw new IOException("Unsupported achievement file");
                Object notifications = root.get("notifications");
                if (!(notifications instanceof Boolean)) throw new IOException("Invalid notification setting");
                JSONObject evidence = root.getJSONObject("evidence"), dates = root.getJSONObject("unlocked_at");
                long[] metrics = new long[AchievementsModel.METRICS];
                long[] unlockedAt = new long[AchievementsModel.DEFINITIONS.length];
                for (int i = 0; i < metrics.length; ++i) metrics[i] = integer(evidence, AchievementsModel.METRIC_KEYS[i]);
                for (int i = 0; i < unlockedAt.length; ++i) unlockedAt[i] = integer(dates, AchievementsModel.DEFINITIONS[i].id);
                return new AchievementsModel.State(metrics, unlockedAt, (Boolean)notifications);
            } catch (JSONException | IllegalArgumentException error) {
                throw new IOException("Invalid achievement file", error);
            }
        }
    }
}
