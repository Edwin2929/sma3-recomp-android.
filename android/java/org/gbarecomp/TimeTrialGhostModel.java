package org.gbarecomp;

import java.util.Arrays;

/** Coherent, bounded metadata; the native worker owns recorded route files. */
final class TimeTrialGhostModel {
    static final int SNAPSHOT_SIZE = 72, BEST_OFFSET = 12, MAX_SAMPLES = 72000;
    static final int NOT_CONFIGURED = 0, LOADING = 1, READY = 2, SAVING = 3, READ_ERROR = 4, WRITE_ERROR = 5;
    static final int NONE = 0, AWAITING_BESTS = 1, AWAITING_START = 2, MISSING = 3,
        LIMIT = 4, CAPTURE_UNAVAILABLE = 5, CANCELLED = 6, BUFFER_UNAVAILABLE = 7, INVALID_BESTS = 8;

    static final class Snapshot {
        final boolean enabled, bestsReady, recording, playing;
        final int storage, reason, level, loadedLevel;
        final long samples, referenceFrames, pending;
        final long[] best;

        Snapshot(long[] source) {
            if (source == null || source.length != SNAPSHOT_SIZE) throw new IllegalArgumentException("Ghost snapshot size");
            long[] value = source.clone();
            if (value[0] != 1) throw new IllegalArgumentException("Ghost schema");
            for (int i = 1; i <= 4; ++i)
                if (value[i] < 0 || value[i] > 1) throw new IllegalArgumentException("Ghost flag");
            if (value[5] < NOT_CONFIGURED || value[5] > WRITE_ERROR || value[6] < NONE || value[6] > INVALID_BESTS ||
                    value[8] < 0 || value[8] > MAX_SAMPLES || value[9] < 0 || value[9] > MAX_SAMPLES ||
                    value[10] < 0 || value[10] > 60) throw new IllegalArgumentException("Ghost range");
            level = course(value[7]); loadedLevel = course(value[11]);
            enabled = value[1] == 1; bestsReady = value[2] == 1; recording = value[3] == 1; playing = value[4] == 1;
            storage = (int)value[5]; reason = (int)value[6]; samples = value[8]; referenceFrames = value[9]; pending = value[10];
            best = Arrays.copyOfRange(value, BEST_OFFSET, SNAPSHOT_SIZE);
            for (long frames : best) if (frames < 0 || frames > MAX_SAMPLES) throw new IllegalArgumentException("Ghost duration");
        }

        private static int course(long rawPlusOne) {
            if (rawPlusOne == 0) return -1;
            int index = TimeTrialModel.levelIndex(rawPlusOne - 1);
            if (index < 0) throw new IllegalArgumentException("Ghost course");
            return index;
        }

        int count() { int count = 0; for (long frames : best) if (frames > 0) ++count; return count; }
    }
}
