package org.gbarecomp;

/** No Android framework required: checks the native-to-HUD measurement contract. */
public final class PresentationStatsTest {
    private static int checks;
    private static void require(boolean value, String label) {
        ++checks;
        if (!value) throw new AssertionError(label);
    }
    public static void main(String[] args) {
        PresentationStats value = PresentationStats.parse(
            "SMA3 — 65.2 fps [base=59.7 extra=5.5 prep=0.75 swap=0.25]");
        require(value != null && value.hasTimings(), "new native title recognized");
        require(value.fps.equals("65.2"), "actual low FPS retained");
        require(value.baseFps.equals("59.7") && value.extraFps.equals("5.5"),
                "base and extra measurements remain separate");
        require(value.preparationMs.equals("0.75") && value.swapMs.equals("0.25"),
                "measured timings retain precision");
        value = PresentationStats.parse("SMA3 — 119.5 fps");
        require(value != null && !value.hasTimings() && value.fps.equals("119.5"),
                "old title supported without rounding to target120");
        value = PresentationStats.parse(
            "SMA3 — 119,5 fps [base=59,7 extra=59,7 prep=0,80 swap=0,10]");
        require(value != null && value.fps.equals("119,5") && value.preparationMs.equals("0,80"),
                "native decimal comma supported");
        for (String invalid : new String[]{null, "SMA3", "120 Hz", "SMA3 — NaN fps",
                "SMA3 — -2.5 fps", "SMA3 — 65.2 fps [base=59.7]",
                "SMA3 — 65.2 fps [base=59.7 extra=5.5 prep=0.75 swap=0.25] stale"}) {
            require(PresentationStats.parse(invalid) == null,
                    "incomplete or unrelated title is not a measurement");
        }
        System.out.println("PASS PresentationStats checks=" + checks);
    }
}
