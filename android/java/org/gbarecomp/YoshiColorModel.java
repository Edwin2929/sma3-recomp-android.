package org.gbarecomp;

import java.util.Locale;

/** Color editing and save transaction, independent of the Android view lifecycle. */
final class YoshiColorModel {
    static final int DEFAULT_RGB = 0x39e739;
    private static final int[] PRESETS = {
        0x39e739, 0x80ff80, 0x20b858, 0x147d42, 0x087050, 0xb5ef61,
        0x00e5b0, 0x74f5d0, 0x25c5be, 0x008f99, 0x005f73, 0xb7ffff,
        0x29b6ff, 0x90d8ff, 0x337dff, 0x3656c8, 0x172c80, 0xd4e8ff,
        0x875cff, 0xc3a0ff, 0x7940c9, 0xa53fd1, 0x5a227a, 0xe8cfff,
        0xff4fbc, 0xffa8dc, 0xe33487, 0xff6e8f, 0xa02254, 0xffd4e5,
        0xff4141, 0xff9292, 0xdc2c38, 0xff7a29, 0xb9421f, 0xffc49a,
        0xffdf33, 0xfff299, 0xe7ae28, 0xc87c35, 0x8f542e, 0xffe3ba,
        0xffffff, 0xd8dde3, 0xa1aab8, 0x697586, 0x343d4b, 0x000000
    };

    private YoshiColorModel() {}

    static boolean validRgb(int rgb) { return rgb >= 0 && rgb <= 0xffffff; }

    static Integer parseHex(String text) {
        if (text == null) return null;
        String value = text.trim();
        if (value.startsWith("#")) value = value.substring(1);
        if (value.length() != 3 && value.length() != 6) return null;
        int result = 0;
        for (int i = 0; i < value.length(); ++i) {
            char ch = value.charAt(i);
            int digit = ch >= '0' && ch <= '9' ? ch - '0'
                : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10
                : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
            if (digit < 0) return null;
            result = value.length() == 3 ? (result << 8) | digit * 17 : (result << 4) | digit;
        }
        return result;
    }

    static String hex(int rgb) {
        if (!validRgb(rgb)) throw new IllegalArgumentException("RGB must have 24 bits");
        return String.format(Locale.ROOT, "#%06X", rgb);
    }

    static int[] presets() { return PRESETS.clone(); }

    /** Matches the native RGB555 palette rounding and shade construction. */
    static int previewRgb(int rgb, int shade) {
        if (!validRgb(rgb) || shade < -1 || shade > 1)
            throw new IllegalArgumentException("Invalid color preview");
        int result = 0;
        for (int shift : new int[]{16, 8, 0}) {
            int component = (((rgb >> shift) & 255) * 31 + 127) / 255;
            if (shade < 0) component = (component * 18 + 12) / 25;
            if (shade > 0) component += ((31 - component) * 7 + 12) / 25;
            result |= ((component << 3) | (component >> 2)) << shift;
        }
        return result;
    }

    static final class Selection {
        final boolean enabled;
        final int rgb;
        Selection(boolean enabled, int rgb) {
            if (!validRgb(rgb)) throw new IllegalArgumentException("RGB must have 24 bits");
            this.enabled = enabled;
            this.rgb = rgb;
        }
    }

    static final class Draft {
        private final Selection initial;
        private int rgb;
        private boolean valid = true;

        Draft(Selection initial) { this.initial = initial; rgb = initial.rgb; }
        int rgb() { return rgb; }
        boolean canApply() { return valid; }
        Selection initial() { return initial; }

        boolean editHex(String text) {
            Integer value = parseHex(text);
            valid = value != null;
            if (valid) rgb = value;
            return valid;
        }

        void choose(int value) {
            if (!validRgb(value)) throw new IllegalArgumentException("RGB must have 24 bits");
            rgb = value;
            valid = true;
        }

        void channel(int channel, int value) {
            if (channel < 0 || channel > 2 || value < 0 || value > 255)
                throw new IllegalArgumentException("Invalid RGB channel");
            int shift = (2 - channel) * 8;
            choose((rgb & ~(255 << shift)) | (value << shift));
        }

        Selection applySelection() {
            if (!valid) throw new IllegalStateException("Incomplete color code");
            return new Selection(true, rgb);
        }

        // Original means disabling the mod, not overwriting the remembered
        // custom color with a green value or accidentally saving an unfinished edit.
        Selection originalSelection() { return new Selection(false, initial.rgb); }
    }

    interface Store { boolean save(Selection value); }
    interface Publisher { void publish(Selection value); }
    enum SaveResult { SAVED, NATIVE_UNAVAILABLE, SAVE_FAILED, RESTORE_FAILED }

    /** The UI only calls this on Apply or Original; previews never publish. */
    static SaveResult save(Selection requested, Selection previous, Store store, Publisher publisher) {
        try {
            publisher.publish(requested);
        } catch (RuntimeException | LinkageError unavailable) {
            return SaveResult.NATIVE_UNAVAILABLE;
        }
        try {
            if (store.save(requested)) return SaveResult.SAVED;
        } catch (RuntimeException failedWrite) { /* restore the old live and saved selection */ }
        boolean restored = true;
        try { publisher.publish(previous); }
        catch (RuntimeException | LinkageError unavailable) { restored = false; }
        // SharedPreferences.commit() can update in-memory values even when its
        // disk write returns false, so roll back both storage and the renderer.
        try { restored = store.save(previous) && restored; }
        catch (RuntimeException failedWrite) { restored = false; }
        return restored ? SaveResult.SAVE_FAILED : SaveResult.RESTORE_FAILED;
    }
}
