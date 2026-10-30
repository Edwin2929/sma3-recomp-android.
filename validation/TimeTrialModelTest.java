package org.gbarecomp;

import java.util.Arrays;

public final class TimeTrialModelTest {
    private static int checks;
    private static void check(boolean ok) { ++checks; if (!ok) throw new AssertionError("check " + checks); }
    private static void rejects(Runnable action) {
        ++checks;
        try { action.run(); } catch (IllegalArgumentException expected) { return; }
        throw new AssertionError("Expected rejection " + checks);
    }
    static long[] snapshot(int level, long frames, long sequence, long best) {
        long[] data = new long[74];
        data[0] = data[1] = 1; data[2] = sequence == 0 ? 2 : 4;
        data[3] = level + 1; data[4] = frames; data[5] = 1; data[6] = sequence;
        if (sequence != 0) { data[7] = level + 1; data[8] = frames; }
        data[10] = 1; data[11] = 1000; data[12] = 280896; data[13] = 16777216;
        data[14 + TimeTrialModel.levelIndex(level)] = best;
        return data;
    }
    public static void main(String[] args) {
        check(TimeTrialModel.format(1000).equals("0:16.74"));
        check(TimeTrialModel.format(60).equals("0:01.00"));
        check(TimeTrialModel.format(1).equals("0:00.01"));
        check(TimeTrialModel.format(0).equals("0:00.00"));
        check(TimeTrialModel.delta(120, 60).equals("+0:01.00"));
        check(TimeTrialModel.delta(60, 120).equals("−0:01.00"));
        check(TimeTrialModel.format(216000).startsWith("1:00:"));
        rejects(() -> TimeTrialModel.milliseconds(-1));
        rejects(() -> TimeTrialModel.milliseconds(TimeTrialModel.MAX_FRAMES + 1));
        check(TimeTrialModel.milliseconds(TimeTrialModel.MAX_FRAMES) >= 0);
        for (int world = 0; world < 6; ++world) {
            for (int level = 0; level < 10; ++level)
                check(TimeTrialModel.levelIndex(world * 12 + level) == world * 10 + level);
            check(TimeTrialModel.levelIndex(world * 12 + 10) == -1);
            check(TimeTrialModel.levelIndex(world * 12 + 11) == -1);
        }
        check(TimeTrialModel.levelIndex(-1) == -1); check(TimeTrialModel.levelIndex(72) == -1);
        long[] raw = snapshot(0, 1000, 1, 1000);
        TimeTrialModel.Snapshot completed = new TimeTrialModel.Snapshot(raw);
        raw[14] = 7; check(completed.best[0] == 1000);
        TimeTrialModel.State state = TimeTrialModel.merge(TimeTrialModel.empty(), completed, 10000, true);
        check(state.count() == 1 && state.best[0] == 1000 && state.last[0] == 1000 && state.recordedAt[0] == 10000);
        check(TimeTrialModel.merge(state, completed, 20000, false) == state);
        state = TimeTrialModel.merge(state, new TimeTrialModel.Snapshot(snapshot(0, 1200, 2, 1000)), 20000, true);
        check(state.best[0] == 1000 && state.last[0] == 1200 && state.recordedAt[0] == 10000);
        state = TimeTrialModel.merge(state, new TimeTrialModel.Snapshot(snapshot(0, 900, 3, 900)), 30000, true);
        check(state.best[0] == 900 && state.last[0] == 900 && state.recordedAt[0] == 30000);
        // Session minima may cover a missed completion in a different level.
        long[] missed = snapshot(12, 1500, 5, 1500); missed[14] = 850;
        state = TimeTrialModel.merge(state, new TimeTrialModel.Snapshot(missed), 40000, true);
        check(state.best[0] == 850 && state.best[10] == 1500 && state.last[0] == 900 && state.last[10] == 1500);
        check(state.count() == 2);
        for (int field : new int[]{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14}) {
            long[] invalid = snapshot(0,1000,1,1000); invalid[field] = -1;
            rejects(() -> new TimeTrialModel.Snapshot(invalid));
        }
        rejects(() -> new TimeTrialModel.Snapshot(null));
        rejects(() -> new TimeTrialModel.Snapshot(new long[73]));
        long[] badClock = snapshot(0,1000,1,1000); badClock[13] = 60;
        rejects(() -> new TimeTrialModel.Snapshot(badClock));
        long[] badBest = snapshot(0,1000,1,1001);
        rejects(() -> new TimeTrialModel.Snapshot(badBest));
        long[] welcome = snapshot(0,1000,1,1000); welcome[3] = 12;
        rejects(() -> new TimeTrialModel.Snapshot(welcome));
        rejects(() -> new TimeTrialModel.State(new long[59], new long[60], new long[60]));
        long[] b = new long[60], dates = new long[60], last = new long[60]; b[0] = 60;
        rejects(() -> new TimeTrialModel.State(b, dates, last));
        dates[0] = 1; last[0] = 59;
        rejects(() -> new TimeTrialModel.State(b, dates, last));
        last[0] = 70;
        TimeTrialModel.State isolated = new TimeTrialModel.State(b, dates, last);
        Arrays.fill(b,0); check(isolated.best[0] == 60);
        System.out.println("PASS " + checks + " time-trial model checks");
    }
}
