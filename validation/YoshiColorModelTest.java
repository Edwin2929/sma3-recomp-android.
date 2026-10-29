package org.gbarecomp;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/** Host checks for the production editor, palette conversion and save boundary. */
public final class YoshiColorModelTest {
    private static int checks;
    private static void require(boolean condition, String name) {
        ++checks;
        if (!condition) throw new AssertionError(name);
    }

    private static boolean same(YoshiColorModel.Selection a, YoshiColorModel.Selection b) {
        return a.enabled == b.enabled && a.rgb == b.rgb;
    }

    private static final class Store implements YoshiColorModel.Store {
        YoshiColorModel.Selection current;
        final List<YoshiColorModel.Selection> writes = new ArrayList<>();
        boolean failFirst, failAll, throwFirst;
        Store(YoshiColorModel.Selection value) { current = value; }
        @Override public boolean save(YoshiColorModel.Selection value) {
            writes.add(value);
            current = value; // SharedPreferences also changes memory on disk failure.
            if (throwFirst && writes.size() == 1) throw new IllegalStateException("disk failure");
            return !failAll && !(failFirst && writes.size() == 1);
        }
    }

    private static final class Publisher implements YoshiColorModel.Publisher {
        YoshiColorModel.Selection current;
        final List<YoshiColorModel.Selection> writes = new ArrayList<>();
        boolean failFirst, failRollback;
        Publisher(YoshiColorModel.Selection value) { current = value; }
        @Override public void publish(YoshiColorModel.Selection value) {
            writes.add(value);
            if ((failFirst && writes.size() == 1) || (failRollback && writes.size() > 1))
                throw new UnsatisfiedLinkError("library unavailable");
            current = value;
        }
    }

    public static void main(String[] args) {
        require(YoshiColorModel.parseHex("#39E739") == 0x39e739, "full HEX accepted");
        require(YoshiColorModel.parseHex("  aB3dEF \n") == 0xab3def, "case, optional # and edge spaces");
        require(YoshiColorModel.parseHex("#09f") == 0x0099ff, "short HEX expands each nibble");
        require(YoshiColorModel.parseHex("000") == 0, "black is valid, not missing");
        require(YoshiColorModel.parseHex("fff") == 0xffffff, "short white");
        for (String value : new String[]{null, "", "#", "0", "12", "1234", "12345", "1234567",
                "#AABBCCDD", "0x123456", "##123456", "#12 3456", "#gg00ff", "+12345", "-12345",
                "#１２３４５６", "rgb(1,2,3)", "#\u0000123456"})
            require(YoshiColorModel.parseHex(value) == null, "reject malformed/alpha/non-ASCII: " + value);

        Locale originalLocale = Locale.getDefault();
        try {
            Locale.setDefault(Locale.forLanguageTag("ar-EG"));
            require(YoshiColorModel.hex(0x00aa0f).equals("#00AA0F"), "HEX is stable across locales");
            for (int value = 0; value <= 0xffffff; value += 4097)
                require(YoshiColorModel.parseHex(YoshiColorModel.hex(value)) == value, "24-bit roundtrip");
        } finally { Locale.setDefault(originalLocale); }
        require(!YoshiColorModel.validRgb(-1) && !YoshiColorModel.validRgb(0x1000000)
            && YoshiColorModel.validRgb(0) && YoshiColorModel.validRgb(0xffffff), "RGB bounds");

        int[] palette = YoshiColorModel.presets();
        Set<Integer> unique = new HashSet<>();
        for (int color : palette) {
            require(YoshiColorModel.validRgb(color), "every preset is RGB24");
            require(unique.add(color), "presets are distinct");
        }
        require(palette.length == 48 && unique.contains(0) && unique.contains(0xffffff), "48 presets include extremes");
        palette[0] = 0;
        require(YoshiColorModel.presets()[0] == YoshiColorModel.DEFAULT_RGB, "preset callers cannot mutate catalog");

        require(YoshiColorModel.previewRgb(0x39e739, 0) == 0x39e739, "native RGB555 green exact");
        require(YoshiColorModel.previewRgb(0x39e739, -1) == 0x29a529, "native shadow palette");
        require(YoshiColorModel.previewRgb(0x39e739, 1) == 0x73ef73, "native light palette");
        require(YoshiColorModel.previewRgb(0x000000, 1) == 0x4a4a4a, "black retains a visible highlight");
        require(YoshiColorModel.previewRgb(0xffffff, -1) == 0xb5b5b5, "white retains a shadow");
        int previous = -1;
        for (int value = 0; value < 256; ++value) {
            int color = value * 0x010101;
            int base = YoshiColorModel.previewRgb(color, 0) & 255;
            require(base >= previous, "palette rounding monotonic");
            require(Math.abs(base - value) <= 4, "base rounding error within four RGB units");
            require((YoshiColorModel.previewRgb(color, -1) & 255) <= base
                && (YoshiColorModel.previewRgb(color, 1) & 255) >= base, "shadow/base/light order");
            previous = base;
        }

        YoshiColorModel.Selection initial = new YoshiColorModel.Selection(false, 0x39e739);
        YoshiColorModel.Draft draft = new YoshiColorModel.Draft(initial);
        Store store = new Store(initial);
        Publisher publisher = new Publisher(initial);
        draft.editHex("#a7f");
        draft.channel(0, 42);
        require(draft.rgb() == 0x2a77ff, "channel adjustment only changes requested component");
        require(store.writes.isEmpty() && publisher.writes.isEmpty(), "editing never saves or publishes");
        require(same(initial, draft.initial()), "draft never mutates original selection");
        require(same(initial, new YoshiColorModel.Draft(initial).initial()), "reopen after cancel restores saved selection");
        require(same(initial, draft.originalSelection()), "Original discards uncommitted color but retains saved choice");
        draft.editHex("#AA");
        require(!draft.canApply() && draft.rgb() == 0x2a77ff, "partial code leaves last preview and blocks Apply");
        try {
            draft.applySelection(); throw new AssertionError("invalid color was applied");
        } catch (IllegalStateException expected) { ++checks; }
        draft.choose(0xabcdef);
        require(draft.canApply(), "preset recovers invalid text draft");
        YoshiColorModel.Selection requested = draft.applySelection();
        require(requested.enabled && requested.rgb == 0xabcdef, "Apply enables chosen exact RGB");
        require(YoshiColorModel.save(requested, initial, store, publisher) == YoshiColorModel.SaveResult.SAVED,
            "valid explicit save succeeds");
        require(store.writes.size() == 1 && publisher.writes.size() == 1
            && same(store.current, publisher.current), "one saved/live transaction");
        YoshiColorModel.Draft reopened = new YoshiColorModel.Draft(store.current);
        require(reopened.rgb() == 0xabcdef && reopened.initial().enabled, "reopen keeps enabled exact HEX");
        YoshiColorModel.Selection disabled = reopened.originalSelection();
        require(!disabled.enabled && disabled.rgb == requested.rgb, "disable retains last custom choice");

        store = new Store(initial); publisher = new Publisher(initial); publisher.failFirst = true;
        require(YoshiColorModel.save(requested, initial, store, publisher) == YoshiColorModel.SaveResult.NATIVE_UNAVAILABLE,
            "unavailable library never reports applied");
        require(store.writes.isEmpty() && same(store.current, initial), "native failure cannot overwrite saved preference");

        store = new Store(initial); publisher = new Publisher(initial); store.failFirst = true;
        require(YoshiColorModel.save(requested, initial, store, publisher) == YoshiColorModel.SaveResult.SAVE_FAILED,
            "commit failure explicitly reported");
        require(store.writes.size() == 2 && publisher.writes.size() == 2
            && same(store.current, initial) && same(publisher.current, initial), "failed commit rolls back live and in-memory values");

        store = new Store(initial); publisher = new Publisher(initial); store.throwFirst = true;
        require(YoshiColorModel.save(requested, initial, store, publisher) == YoshiColorModel.SaveResult.SAVE_FAILED
            && same(store.current, initial) && same(publisher.current, initial), "exception during commit also rolls back");

        store = new Store(initial); publisher = new Publisher(initial); store.failAll = true;
        require(YoshiColorModel.save(requested, initial, store, publisher) == YoshiColorModel.SaveResult.RESTORE_FAILED,
            "unconfirmed rollback never reports restored or saved");
        require(same(publisher.current, initial), "restore renderer even if both disk writes fail");

        store = new Store(initial); publisher = new Publisher(initial); store.failFirst = true; publisher.failRollback = true;
        require(YoshiColorModel.save(requested, initial, store, publisher) == YoshiColorModel.SaveResult.RESTORE_FAILED,
            "failed renderer rollback is disclosed");
        require(same(store.current, initial), "storage rollback still attempted after renderer failure");
        System.out.println("PASS YoshiColorModel checks=" + checks);
    }
}
