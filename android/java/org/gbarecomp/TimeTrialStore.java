package org.gbarecomp;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.AtomicFile;

import org.json.JSONArray;
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

/** Process-owned record storage; neither game frames nor the UI perform disk writes. */
final class TimeTrialStore {
    interface NativeSnapshot { long[] read(); }
    interface Listener { void updated(Result result); }
    interface Persistence {
        TimeTrialModel.State read() throws IOException;
        void write(TimeTrialModel.State value) throws IOException;
    }
    enum Problem { NONE, LOADING, READ, WRITE, NATIVE }
    static final class Result {
        final TimeTrialModel.State state;
        final TimeTrialModel.Snapshot snapshot;
        final Problem problem;
        final long comparedCompletion, previousBest;
        Result(TimeTrialModel.State state, TimeTrialModel.Snapshot snapshot, Problem problem,
               long comparedCompletion, long previousBest) {
            this.state = state; this.snapshot = snapshot; this.problem = problem;
            this.comparedCompletion = comparedCompletion; this.previousBest = previousBest;
        }
    }

    private static TimeTrialStore instance;
    private static File instanceDirectory;
    static synchronized TimeTrialStore get(Context context) {
        File directory = context.getApplicationContext().getFilesDir();
        if (instance == null || !directory.equals(instanceDirectory)) {
            instanceDirectory = directory;
            instance = new TimeTrialStore(new Disk(new File(directory, "time-trials/records-v1.json")),
                Executors.newSingleThreadExecutor(task -> new Thread(task, "SMA3 time trials")),
                new Handler(Looper.getMainLooper()));
        }
        return instance;
    }

    private final Persistence disk;
    private final Executor worker;
    private final Handler main;
    private TimeTrialModel.State current = TimeTrialModel.empty(), saved = current;
    private TimeTrialModel.Snapshot snapshot;
    private boolean loaded;
    private long seenCompletion, previousBest;
    private volatile Result latest = new Result(current, null, Problem.LOADING, 0, 0);

    TimeTrialStore(Persistence disk, Executor worker, Handler main) {
        this.disk = disk; this.worker = worker; this.main = main;
    }
    Result latest() { return latest; }

    void poll(NativeSnapshot source, Listener listener) {
        worker.execute(() -> {
            if (!loaded) {
                try { current = disk.read(); saved = current; loaded = true; }
                catch (IOException | RuntimeException failure) {
                    publish(Problem.READ, listener); return;
                }
            }
            Problem problem = Problem.NONE;
            try {
                TimeTrialModel.Snapshot next = new TimeTrialModel.Snapshot(source.read());
                boolean completed = next.completion > seenCompletion;
                if (completed) {
                    // If several finishes arrived between polls, their order
                    // is unavailable. Compare the last against the current
                    // best instead of inventing a "new record" notification.
                    previousBest = next.completion == seenCompletion + 1 ? current.best[next.lastLevel] : -1;
                    seenCompletion = next.completion;
                }
                current = TimeTrialModel.merge(current, next, Math.max(1, System.currentTimeMillis()), completed);
                snapshot = next;
            } catch (UnsatisfiedLinkError | RuntimeException failure) { problem = Problem.NATIVE; }
            if (!current.same(saved)) {
                try { disk.write(current); saved = current; }
                catch (IOException | RuntimeException failure) { problem = Problem.WRITE; }
            }
            publish(problem, listener);
        });
    }

    private void publish(Problem problem, Listener listener) {
        Result result = new Result(current, snapshot, problem, seenCompletion, previousBest);
        latest = result;
        if (listener != null) main.post(() -> listener.updated(result));
    }

    static final class Disk implements Persistence {
        private static final int MAX_BYTES = 32 * 1024;
        private final AtomicFile file;
        Disk(File path) { file = new AtomicFile(path); }

        public TimeTrialModel.State read() throws IOException {
            File base = file.getBaseFile();
            if (!base.exists() && !new File(base.getPath() + ".bak").exists()) return TimeTrialModel.empty();
            try (FileInputStream input = file.openRead()) {
                ByteArrayOutputStream bytes = new ByteArrayOutputStream();
                byte[] buffer = new byte[2048];
                int count;
                while ((count = input.read(buffer)) != -1) {
                    if (bytes.size() + count > MAX_BYTES) throw new IOException("Time-trial record too large");
                    bytes.write(buffer, 0, count);
                }
                return decode(new String(bytes.toByteArray(), StandardCharsets.UTF_8));
            }
        }

        public void write(TimeTrialModel.State value) throws IOException {
            File directory = file.getBaseFile().getParentFile();
            if (!directory.isDirectory() && !directory.mkdirs()) throw new IOException("Time-trial directory unavailable");
            byte[] encoded = encode(value).getBytes(StandardCharsets.UTF_8);
            FileOutputStream output = null;
            try {
                output = file.startWrite(); output.write(encoded); output.getFD().sync(); file.finishWrite(output);
                if (!value.same(read())) throw new IOException("Time-trial commit verification failed");
            } catch (IOException | RuntimeException failure) {
                if (output != null) file.failWrite(output);
                throw new IOException("Time-trial write failed", failure);
            }
        }

        static String encode(TimeTrialModel.State value) throws IOException {
            try {
                JSONObject root = new JSONObject();
                root.put("schema", 1); root.put("cycles_per_frame", TimeTrialModel.CYCLES_PER_FRAME);
                root.put("clock_hz", TimeTrialModel.CLOCK_HZ);
                root.put("best_frames", array(value.best)); root.put("recorded_at", array(value.recordedAt));
                root.put("last_frames", array(value.last));
                return root.toString() + "\n";
            } catch (JSONException failure) { throw new IOException("Time-trial encoding failed", failure); }
        }

        private static JSONArray array(long[] values) {
            JSONArray result = new JSONArray(); for (long value : values) result.put(value); return result;
        }

        private static long integer(Object value) throws IOException {
            if (!(value instanceof Integer) && !(value instanceof Long)) throw new IOException("Time-trial integer required");
            return ((Number)value).longValue();
        }

        private static long[] values(JSONArray input) throws IOException, JSONException {
            if (input.length() != TimeTrialModel.LEVELS) throw new IOException("Time-trial level count");
            long[] values = new long[TimeTrialModel.LEVELS];
            for (int i = 0; i < values.length; ++i) values[i] = integer(input.get(i));
            return values;
        }

        static TimeTrialModel.State decode(String encoded) throws IOException {
            try {
                JSONObject root = new JSONObject(encoded);
                if (root.length() != 6 || integer(root.get("schema")) != 1 ||
                        integer(root.get("cycles_per_frame")) != TimeTrialModel.CYCLES_PER_FRAME ||
                        integer(root.get("clock_hz")) != TimeTrialModel.CLOCK_HZ)
                    throw new IOException("Unsupported time-trial record");
                return new TimeTrialModel.State(values(root.getJSONArray("best_frames")),
                    values(root.getJSONArray("recorded_at")), values(root.getJSONArray("last_frames")));
            } catch (JSONException | IllegalArgumentException failure) {
                throw new IOException("Invalid time-trial record", failure);
            }
        }
    }
}
