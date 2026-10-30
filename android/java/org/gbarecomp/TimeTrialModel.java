package org.gbarecomp;

import java.util.Arrays;
import java.util.Locale;

/** Pure timing/record rules. All durations are guest PPU frames, never UI ticks. */
final class TimeTrialModel {
    static final int LEVELS = 60, SNAPSHOT_SIZE = 74, BEST_OFFSET = 14;
    static final int DISABLED = 0, WAITING = 1, RUNNING = 2, PAUSED = 3, FINISHED = 4, INVALIDATED = 5;
    static final long CYCLES_PER_FRAME = 280896, CLOCK_HZ = 16777216;
    // Exact millisecond conversion after reducing the hardware clock fraction.
    static final long MAX_FRAMES = Long.MAX_VALUE / 4389000L;

    static int levelIndex(long rawCourse) {
        return rawCourse >= 0 && rawCourse < 72 && rawCourse % 12 < 10
            ? (int)(rawCourse / 12 * 10 + rawCourse % 12) : -1;
    }

    static boolean duration(long frames) { return frames >= 0 && frames <= MAX_FRAMES; }

    static long milliseconds(long frames) {
        if (!duration(frames)) throw new IllegalArgumentException("Invalid time-trial duration");
        return frames * 4389000L / 262144L;
    }

    static String format(long frames) {
        long hundredths = milliseconds(frames) / 10;
        long seconds = hundredths / 100;
        if (seconds >= 3600) return String.format(Locale.US, "%d:%02d:%02d.%02d",
            seconds / 3600, seconds / 60 % 60, seconds % 60, hundredths % 100);
        return String.format(Locale.US, "%d:%02d.%02d", seconds / 60, seconds % 60, hundredths % 100);
    }

    static String delta(long current, long reference) {
        if (!duration(current) || !duration(reference)) throw new IllegalArgumentException("Invalid comparison");
        long difference = current - reference;
        return (difference < 0 ? "−" : "+") + format(Math.abs(difference));
    }

    static final class Snapshot {
        final boolean enabled;
        final int status, level, lastLevel, reason;
        final long frames, generation, completion, lastFrames, epoch, guestFrame;
        final long[] best;

        Snapshot(long[] input) {
            if (input == null || input.length != SNAPSHOT_SIZE) throw new IllegalArgumentException("Snapshot size");
            long[] values = input.clone();
            if (values[0] != 1 || values[1] < 0 || values[1] > 1 ||
                values[2] < DISABLED || values[2] > INVALIDATED ||
                values[9] < 0 || values[9] > 6 || values[12] != CYCLES_PER_FRAME || values[13] != CLOCK_HZ)
                throw new IllegalArgumentException("Snapshot schema");
            enabled = values[1] == 1;
            status = (int)values[2]; reason = (int)values[9];
            level = values[3] == 0 ? -1 : levelIndex(values[3] - 1);
            lastLevel = values[7] == 0 ? -1 : levelIndex(values[7] - 1);
            frames = values[4]; generation = values[5]; completion = values[6]; lastFrames = values[8];
            epoch = values[10]; guestFrame = values[11];
            if ((values[3] != 0 && level < 0) || (values[7] != 0 && lastLevel < 0) ||
                !duration(frames) || !duration(lastFrames) || generation < 0 || completion < 0 || epoch < 0 || guestFrame < 0 ||
                (!enabled && status != DISABLED) ||
                ((status == RUNNING || status == PAUSED || status == FINISHED) && level < 0) ||
                (completion > 0 && (lastLevel < 0 || lastFrames == 0)) ||
                (completion == 0 && (lastLevel >= 0 || lastFrames != 0)))
                throw new IllegalArgumentException("Snapshot values");
            best = Arrays.copyOfRange(values, BEST_OFFSET, SNAPSHOT_SIZE);
            for (long value : best) if (!duration(value)) throw new IllegalArgumentException("Snapshot record");
            if (lastLevel >= 0 && (best[lastLevel] == 0 || best[lastLevel] > lastFrames))
                throw new IllegalArgumentException("Snapshot completion record");
        }
    }

    static final class State {
        final long[] best, recordedAt, last;

        State(long[] best, long[] recordedAt, long[] last) {
            if (best == null || recordedAt == null || last == null || best.length != LEVELS ||
                    recordedAt.length != LEVELS || last.length != LEVELS)
                throw new IllegalArgumentException("Record count");
            this.best = best.clone(); this.recordedAt = recordedAt.clone(); this.last = last.clone();
            for (int i = 0; i < LEVELS; ++i) {
                if (!duration(best[i]) || !duration(last[i]) || recordedAt[i] < 0 ||
                        (best[i] == 0 && (recordedAt[i] != 0 || last[i] != 0)) ||
                        (best[i] > 0 && recordedAt[i] == 0) || (last[i] != 0 && last[i] < best[i]))
                    throw new IllegalArgumentException("Invalid time-trial record");
            }
        }

        int count() { int count = 0; for (long value : best) if (value != 0) ++count; return count; }
        boolean same(State other) {
            return Arrays.equals(best, other.best) && Arrays.equals(recordedAt, other.recordedAt) && Arrays.equals(last, other.last);
        }
    }

    static State empty() { return new State(new long[LEVELS], new long[LEVELS], new long[LEVELS]); }

    static State merge(State old, Snapshot snapshot, long now, boolean newCompletion) {
        if (now <= 0) throw new IllegalArgumentException("Invalid record date");
        long[] best = old.best.clone(), dates = old.recordedAt.clone(), last = old.last.clone();
        boolean changed = false;
        for (int i = 0; i < LEVELS; ++i) {
            if (snapshot.best[i] > 0 && (best[i] == 0 || snapshot.best[i] < best[i])) {
                best[i] = snapshot.best[i]; dates[i] = now; changed = true;
            }
        }
        if (newCompletion && snapshot.lastLevel >= 0 && last[snapshot.lastLevel] != snapshot.lastFrames) {
            last[snapshot.lastLevel] = snapshot.lastFrames; changed = true;
        }
        return changed ? new State(best, dates, last) : old;
    }
}
