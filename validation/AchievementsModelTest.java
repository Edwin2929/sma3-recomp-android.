package org.gbarecomp;

import java.util.Arrays;
import java.util.HashSet;

/** Standalone tests of monotonic evidence, stable thresholds and invalid inputs. */
public final class AchievementsModelTest {
    private static int checks;
    private static void check(boolean value, String name) {
        ++checks; if (!value) throw new AssertionError(name);
    }
    private static void invalid(long[] snapshot) {
        try { AchievementsModel.merge(AchievementsModel.empty(), snapshot, 123); }
        catch (IllegalArgumentException expected) { ++checks; return; }
        throw new AssertionError("Malformed snapshot was accepted");
    }
    private static long[] snapshot(int metric, long value) {
        long[] result = new long[8]; result[0] = 1; result[metric + 1] = value; return result;
    }
    public static void main(String[] arguments) {
        HashSet<String> ids = new HashSet<>();
        for (AchievementsModel.Definition definition : AchievementsModel.DEFINITIONS) {
            check(ids.add(definition.id), "Stable unique IDs");
            check(definition.target > 0, "Positive target");
            for (int i = 0; i < 3; ++i) {
                check(!definition.title[i].isEmpty(), "Localized title");
                check(!definition.description[i].isEmpty(), "Localized criterion");
            }
        }
        AchievementsModel.State state = AchievementsModel.empty();
        check(state.unlocked() == 0 && state.notifications, "Fresh catalog");
        for (int level = 0; level < 60; ++level) {
            state = AchievementsModel.merge(state, snapshot(0, 1L << level), 1000 + level);
            check(Long.bitCount(state.metrics[0]) == level + 1, "Distinct levels accumulate");
            check(state.unlockedAt[7] == 1000, "First level date remains first");
            check(state.unlockedAt[8] == (level >= 4 ? 1004 : 0), "Five unique levels threshold");
            AchievementsModel.State repeat = AchievementsModel.merge(state, snapshot(0, 1L << level), 99999);
            check(state.same(repeat), "Repeating a clear adds no duplicate");
        }
        check(AchievementsModel.merge(state, snapshot(0, 0), 100000).same(state), "Session restart retains level union");
        for (int level = 0; level < 60; ++level) {
            long[] perfect = snapshot(0, 1L << level); perfect[2] = 1L << level;
            AchievementsModel.State result = AchievementsModel.merge(AchievementsModel.empty(), perfect, 8);
            check(result.metrics[1] == 1L << level && result.unlockedAt[9] == 8, "Perfect evidence index");
        }
        for (int[] pair : new int[][]{{3,20},{4,5},{5,30}}) {
            AchievementsModel.State collected = AchievementsModel.empty();
            for (int count = 0; count <= pair[1]; ++count) {
                long[] input = snapshot(pair[0], count), before = input.clone();
                collected = AchievementsModel.merge(collected, input, 100 + count);
                check(collected.metrics[pair[0]] == count, "Maximum collection progress");
                check(Arrays.equals(input, before), "JNI evidence remains immutable");
                check(AchievementsModel.merge(collected, snapshot(pair[0], 0), 900).same(collected), "Level reset preserves maximum");
            }
            int achievement = pair[0] == 3 ? 3 : pair[0] == 4 ? 1 : 4;
            check(collected.unlockedAt[achievement] == 100 + pair[1], "Threshold unlock time");
        }
        AchievementsModel.State transforms = AchievementsModel.empty();
        for (int bit = 0; bit < 5; ++bit) {
            transforms = AchievementsModel.merge(transforms, snapshot(2, 1 << bit), 7 + bit);
            check(Long.bitCount(transforms.metrics[2]) == bit + 1, "Vehicle types union");
            check(transforms.unlockedAt[5] == 7, "First vehicle date");
        }
        AchievementsModel.State fuzzy = AchievementsModel.merge(AchievementsModel.empty(), snapshot(6, 1), 73);
        check(fuzzy.unlockedAt[6] == 73 && fuzzy.unlocked() == 1, "Natural Fuzzy event");
        check(AchievementsModel.newUnlocks(AchievementsModel.empty(), fuzzy) == 1 << 6, "Notice mask");
        check(AchievementsModel.newUnlocks(fuzzy, fuzzy) == 0, "No old notices");
        check(!AchievementsModel.merge(fuzzy.notifications(false), snapshot(3, 1), 80).notifications, "Notice setting survives progress");

        long[] all = {1, 31, 1, 31, 20, 5, 30, 1};
        AchievementsModel.State complete = AchievementsModel.merge(AchievementsModel.empty(), all, 12345);
        check(complete.unlocked() == 10, "All ten criteria");
        for (long date : complete.unlockedAt) check(date == 12345, "Simultaneous date");
        check(AchievementsModel.merge(complete, snapshot(0, 0), 0).same(complete), "Zero clock/restart cannot erase");
        all[0] = 2; invalid(all); invalid(null); invalid(new long[7]); invalid(new long[9]);
        invalid(snapshot(0, 1L << 60)); invalid(snapshot(1, 1)); invalid(snapshot(2, 32));
        invalid(snapshot(3, -1)); invalid(snapshot(3, 21)); invalid(snapshot(4, 6));
        invalid(snapshot(5, 31)); invalid(snapshot(6, 2));
        long[] metrics = new long[7], dates = new long[10]; dates[0] = 1;
        try { new AchievementsModel.State(metrics, dates, true); throw new AssertionError("Date without evidence accepted"); }
        catch (IllegalArgumentException expected) { ++checks; }
        System.out.println("PASS local achievement model checks=" + checks + ", definitions=" + ids.size());
    }
}
