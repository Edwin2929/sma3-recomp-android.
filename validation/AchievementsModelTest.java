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
        String[] expectedIds = {"first_flower","five_flowers","first_red_coin","twenty_red_coins","thirty_stars",
            "first_transformation","fuzzy_recovery","first_clear","five_clears","perfect_clear","three_flowers",
            "five_red_coins","ten_red_coins","ten_clears","twenty_clears","thirty_clears","forty_clears",
            "five_perfect_clears","ten_perfect_clears","twenty_perfect_clears","car_transformation",
            "mole_transformation","helicopter_transformation","train_transformation","submarine_transformation"};
        check(AchievementsModel.DEFINITIONS.length == 25, "Exactly 25 definitions");
        for (int i = 0; i < expectedIds.length; ++i)
            check(AchievementsModel.DEFINITIONS[i].id.equals(expectedIds[i]), "Stable catalog append order");
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
            for (int[] threshold : new int[][]{{13,10},{14,20},{15,30},{16,40}})
                check(state.unlockedAt[threshold[0]] == (level + 1 >= threshold[1] ? 999 + threshold[1] : 0),
                    "Expanded unique level threshold and first date");
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

        for (int mask = 0; mask <= 31; ++mask) {
            AchievementsModel.State vehicle = AchievementsModel.merge(AchievementsModel.empty(), snapshot(2, mask), 600);
            check(vehicle.unlocked() == (mask == 0 ? 0 : Integer.bitCount(mask) + 1), "Only the observed vehicle types unlock");
            for (int bit = 0; bit < 5; ++bit) {
                int index = 20 + bit, expected = (mask & (1 << bit)) != 0 ? 1 : 0;
                check(AchievementsModel.DEFINITIONS[index].progress(vehicle) == expected, "Vehicle UI predicate");
                check(vehicle.unlockedAt[index] == expected * 600, "Vehicle merge predicate");
            }
        }
        AchievementsModel.State perfects = AchievementsModel.empty();
        for (int level = 0; level < 60; ++level) {
            long[] value = snapshot(0, 1L << level); value[2] = 1L << level;
            perfects = AchievementsModel.merge(perfects, value, 5000 + level);
            for (int[] threshold : new int[][]{{9,1},{17,5},{18,10},{19,20}}) {
                check(AchievementsModel.DEFINITIONS[threshold[0]].progress(perfects) == Math.min(level + 1, threshold[1]),
                    "Perfect level UI count");
                check(perfects.unlockedAt[threshold[0]] == (level + 1 >= threshold[1] ? 4999 + threshold[1] : 0),
                    "Perfect level threshold and original date");
            }
            check(AchievementsModel.merge(perfects, value, 99999).same(perfects), "Repeating a perfect level adds no duplicate");
        }
        for (int[] threshold : new int[][]{{10,4,3},{11,3,5},{12,3,10}}) {
            for (int amount = 0; amount <= (threshold[1] == 3 ? 20 : 5); ++amount) {
                AchievementsModel.State value = AchievementsModel.merge(AchievementsModel.empty(), snapshot(threshold[1], amount), 700);
                check(AchievementsModel.DEFINITIONS[threshold[0]].progress(value) == Math.min(threshold[2], amount), "Expanded collection UI progress");
                check(value.unlockedAt[threshold[0]] == (amount >= threshold[2] ? 700 : 0), "Expanded collection threshold");
            }
        }
        long[] all = {1, (1L<<40)-1, (1L<<20)-1, 31, 20, 5, 30, 1};
        AchievementsModel.State complete = AchievementsModel.merge(AchievementsModel.empty(), all, 12345);
        check(complete.unlocked() == 25, "All 25 criteria");
        check(AchievementsModel.newUnlocks(AchievementsModel.empty(), complete) == 0x1ffffff, "All 25 notice bits including bit 24");
        check(complete.recognizedOnUpgrade == 0, "Fresh play has earned dates");
        for (long date : complete.unlockedAt) check(date == 12345, "Simultaneous date");
        check(AchievementsModel.merge(complete, snapshot(0, 0), 0).same(complete), "Zero clock/restart cannot erase");
        all[0] = 2; invalid(all); invalid(null); invalid(new long[7]); invalid(new long[9]);
        invalid(snapshot(0, 1L << 60)); invalid(snapshot(1, 1)); invalid(snapshot(2, 32));
        invalid(snapshot(3, -1)); invalid(snapshot(3, 21)); invalid(snapshot(4, 6));
        invalid(snapshot(5, 31)); invalid(snapshot(6, 2));
        long[] metrics = new long[7], dates = new long[25]; dates[0] = 1;
        try { new AchievementsModel.State(metrics, dates, true); throw new AssertionError("Date without evidence accepted"); }
        catch (IllegalArgumentException expected) { ++checks; }
        long[] legacyMetrics = {(1L<<24)-1, (1L<<9)-1, 21, 10, 3, 30, 1};
        long[] legacyDates = {100,0,200,0,300,400,500,600,700,800};
        long[] beforeMetrics = legacyMetrics.clone(), beforeDates = legacyDates.clone();
        AchievementsModel.State migrated = AchievementsModel.expandLegacy(legacyMetrics, legacyDates, false, 10000);
        int[] newlyRecognized = {10,11,12,13,14,17,20,22,24};
        int recognized = 0; for (int index : newlyRecognized) recognized |= 1 << index;
        check(migrated.unlocked() == 17, "Legacy partial evidence recognizes nine additions");
        check(migrated.recognizedOnUpgrade == recognized, "Recognition origin includes submarine bit 24");
        check(!migrated.notifications, "Legacy muted preference preserved");
        check(Arrays.equals(legacyMetrics,beforeMetrics) && Arrays.equals(legacyDates,beforeDates), "Legacy input immutability");
        check(Arrays.equals(migrated.metrics,legacyMetrics), "No evidence synthesized by migration");
        for (int i=0;i<10;++i) check(migrated.unlockedAt[i] == legacyDates[i], "All ten original dates preserved");
        for (int i=10;i<25;++i) check(migrated.unlockedAt[i] == ((recognized & (1<<i)) != 0 ? 10000 : 0), "Recognition date is expansion time");
        check(migrated.notifications(true).recognizedOnUpgrade == recognized, "Preference edit preserves date origin");
        check(AchievementsModel.merge(migrated,snapshot(0,0),20000).same(migrated), "Old snapshot keeps recognized dates");
        AchievementsModel.State next = AchievementsModel.merge(migrated,snapshot(2,2),20000);
        check(next.unlockedAt[21] == 20000 && next.recognizedOnUpgrade == recognized, "New gameplay uses earned date");
        check(AchievementsModel.newUnlocks(migrated,next) == 1<<21, "Later vehicle notice is isolated");
        for (int invalidMask : new int[]{1, 1<<9, 1<<25, -1}) {
            try { new AchievementsModel.State(complete.metrics,complete.unlockedAt,true,invalidMask); throw new AssertionError("Invalid origin accepted"); }
            catch (IllegalArgumentException expected) { ++checks; }
        }
        try { new AchievementsModel.State(migrated.metrics,migrated.unlockedAt,false,1<<21); throw new AssertionError("Unearned recognition accepted"); }
        catch (IllegalArgumentException expected) { ++checks; }
        try { AchievementsModel.expandLegacy(legacyMetrics,new long[25],false,30000); throw new AssertionError("25 dates disguised as legacy"); }
        catch (IllegalArgumentException expected) { ++checks; }
        try { AchievementsModel.expandLegacy(legacyMetrics,new long[10],false,30000); throw new AssertionError("Missing original date accepted"); }
        catch (IllegalArgumentException expected) { ++checks; }
        System.out.println("PASS local achievement model checks=" + checks + ", definitions=" + ids.size());
    }
}
