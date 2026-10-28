package org.gbarecomp;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Values measured by native presentation; Android UI callbacks are not frames. */
final class PresentationStats {
    private static final String NUMBER = "([0-9]+(?:[.,][0-9]+)?)";
    private static final Pattern TITLE = Pattern.compile("(?:^| — )" + NUMBER + " fps(?: \\[base="
        + NUMBER + " extra=" + NUMBER + " prep=" + NUMBER + " swap=" + NUMBER + "\\])?$");

    final String fps, baseFps, extraFps, preparationMs, swapMs;

    private PresentationStats(Matcher match) {
        fps = match.group(1);
        baseFps = match.group(2);
        extraFps = match.group(3);
        preparationMs = match.group(4);
        swapMs = match.group(5);
    }

    static PresentationStats parse(CharSequence title) {
        if (title == null) return null;
        Matcher match = TITLE.matcher(title);
        return match.find() ? new PresentationStats(match) : null;
    }

    boolean hasTimings() { return baseFps != null; }
}
