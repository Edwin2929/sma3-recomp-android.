package org.gbarecomp;

import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.EnumMap;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Pure report/URL/privacy checks; does not launch or submit a GitHub issue. */
public final class BugReportModelTest {
    private static int checks;
    private static void require(boolean ok, String reason) {
        ++checks;
        if (!ok) throw new AssertionError(reason);
    }
    private static final class Preferences implements BugReportModel.Preferences {
        final Map<String, Object> values = new HashMap<>();
        final Set<String> reads = new HashSet<>();
        public boolean contains(String key) { reads.add(key); return values.containsKey(key); }
        public boolean flag(String key, boolean fallback) {
            reads.add(key); Object value = values.get(key); return value instanceof Boolean ? (Boolean)value : fallback;
        }
        public int number(String key, int fallback) {
            reads.add(key); Object value = values.get(key); return value instanceof Integer ? (Integer)value : fallback;
        }
    }
    private static String decode(String value) { return URLDecoder.decode(value, StandardCharsets.UTF_8); }

    public static void main(String[] args) {
        Preferences prefs = new Preferences();
        prefs.values.put("account-email", "private@example.invalid");
        prefs.values.put("rom-path", "/private/player/game.gba");
        prefs.values.put("touch-design-1", "private-layout-json");
        Map<BugReportModel.Field, String> settings = BugReportModel.settings(prefs);
        Set<String> allowed = new HashSet<>(Arrays.asList("interpolation-120", "image-mode", "stretch-screen",
            "video-quality", "image-filter", "menu-language", "game-language", "touch-visible", "touch-style",
            "touch-opacity", "touch-design-active", "show-fps", "yoshi-color-enabled", "time-trial-enabled", "time-trial-overlay"));
        for (int i = 0; i < 10; ++i) allowed.add("pad-" + i);
        require(prefs.reads.equals(allowed), "only the complete explicit settings allowlist is read");
        require(settings.get(BugReportModel.Field.REQUESTED_FPS).equals("60"), "default request is 60, no measurement invented");
        require(settings.get(BugReportModel.Field.PAD_MAPPING).equals("0 / 10"), "default controller settings");
        require(!settings.toString().contains("private"), "unknown string preferences never enter the report");
        prefs.values.put("image-mode", 99); prefs.values.put("image-filter", -8);
        prefs.values.put("touch-opacity", 1000); prefs.values.put("video-quality", 999);
        prefs.values.put("pad-0", 9); prefs.values.put("pad-1", 42); prefs.values.put("pad-2", -1);
        prefs.values.put("interpolation-120", true); prefs.values.put("time-trial-enabled", true);
        prefs.values.put("time-trial-overlay", false);
        settings = BugReportModel.settings(prefs);
        require(settings.get(BugReportModel.Field.IMAGE).equals("original 3:2"), "invalid image ID follows actual fallback");
        require(settings.get(BugReportModel.Field.FILTER).equals("balanced"), "invalid filter fallback");
        require(settings.get(BugReportModel.Field.QUALITY).equals("auto"), "invalid resolution fallback");
        require(settings.get(BugReportModel.Field.TOUCH_OPACITY).equals("100%"), "opacity clamped");
        require(settings.get(BugReportModel.Field.PAD_MAPPING).equals("2 / 10"), "counts valid custom/unbound values only");
        require(settings.get(BugReportModel.Field.TIME_TRIAL).equals("on")
            && settings.get(BugReportModel.Field.TIME_TRIAL_OVERLAY).equals("off"), "new settings included distinctly");
        prefs.values.remove("image-mode"); prefs.values.put("stretch-screen", true);
        require(BugReportModel.settings(prefs).get(BugReportModel.Field.IMAGE).equals("stretch"), "legacy image preference understood read-only");
        prefs.values.put("menu-language", 999); prefs.values.put("game-language", -1);
        require(BugReportModel.settings(prefs).get(BugReportModel.Field.MENU_LANGUAGE).equals("Español")
            && BugReportModel.settings(prefs).get(BugReportModel.Field.GAME_LANGUAGE).equals("English"), "language defaults");

        EnumMap<BugReportModel.Field, String> facts = new EnumMap<>(BugReportModel.Field.class);
        facts.putAll(settings); facts.put(BugReportModel.Field.APP, "0.10.16-beta (27)");
        facts.put(BugReportModel.Field.OBSERVED_FPS, "58.7 FPS");
        facts.put(BugReportModel.Field.DEVICE, "Vendor\n`\u0000Model");
        BugReportModel.Snapshot snapshot = new BugReportModel.Snapshot(facts).mods(3, 1);
        facts.clear();
        require(snapshot.values.size() > 5, "snapshot is a defensive immutable copy");
        try { snapshot.values.clear(); throw new AssertionError("snapshot mutable"); }
        catch (UnsupportedOperationException expected) { ++checks; }
        for (int language = 0; language < 3; ++language) {
            String report = BugReportModel.render(language, snapshot, "Cayó Yoshi 😃", "1. Saltar\r\n2. Esperar", "Seguir jugando");
            require(report.contains("Cayó Yoshi 😃") && report.contains("1. Saltar\n2. Esperar"), "Unicode text and normalized line endings");
            require(report.contains("120") && report.contains("58.7 FPS"), "request and actual sample remain separate");
            require(report.contains("3 / 1"), "aggregate mods included");
            require(!report.contains("Vendor\n") && !report.contains("\u0000") && !report.contains("`Model"), "device metadata cannot inject a block or fence");
            require(!report.contains("private-layout-json") && !report.contains("private@example"), "no unexpected preference values");
            String title = BugReportModel.title(language, "", "0.10.16-beta");
            require(title.contains("SMA3 0.10.16-beta"), "localized default title retains version");
        }
        require(BugReportModel.render(0, snapshot, "", "", "").contains("[To complete]"), "blank description explicitly incomplete");
        require(BugReportModel.render(1, snapshot, "", "", "").contains("[Por completar]"), "Spanish template");
        require(BugReportModel.render(2, snapshot, "", "", "").contains("[A preencher]"), "Portuguese template");
        require(snapshot.mods(-1, 0).values.get(BugReportModel.Field.MODS).equals("unavailable"), "negative mod count rejected");
        require(snapshot.mods(1, 2).values.get(BugReportModel.Field.MODS).equals("unavailable"), "active cannot exceed installed");
        require(snapshot.mods(1025, 0).values.get(BugReportModel.Field.MODS).equals("unavailable"), "catalog count bounded");

        for (String text : new String[]{"a&b?c#d", "Español / Português", "😃😀❤️", "line1\nline2", "a+b %=é"}) {
            BugReportModel.IssueLink link = BugReportModel.issueLink(text, text);
            require(!link.needsPaste && link.url.startsWith(BugReportModel.ISSUE_URL + "?title="), "fixed repository; short report prefilled");
            String[] query = link.url.substring(link.url.indexOf('?') + 1).split("&");
            require(query.length == 2, "user text cannot inject issue parameters");
            require(decode(query[0].substring(6)).equals(BugReportModel.singleLine(text, 120)), "title round-trips UTF-8");
            require(decode(query[1].substring(5)).equals(text), "body round-trips UTF-8");
        }
        String enormous = "é😃".repeat(7000);
        BugReportModel.IssueLink longLink = BugReportModel.issueLink("T".repeat(500), enormous);
        require(longLink.needsPaste && !longLink.url.contains("&body="), "large URL uses full clipboard draft instead of truncating issue body");
        require(longLink.url.length() <= BugReportModel.MAX_ISSUE_URL, "fallback URL stays bounded");
        int before = 0;
        for (int length = 5500; length <= 6100; ++length) {
            BugReportModel.IssueLink link = BugReportModel.issueLink("x", "x".repeat(length));
            require(link.url.length() <= BugReportModel.MAX_ISSUE_URL, "every URL at the boundary stays within limit");
            if (link.needsPaste) ++before;
        }
        require(before > 0 && before < 601, "both sides of URL threshold exercised");
        require(BugReportModel.clean("a\u0000b\u202Ec\u2066d\uD800e\r\nf", 20).equals("abcde\nf"), "unsafe controls and isolated surrogate stripped");
        require(BugReportModel.clean("ab😃", 3).equals("ab"), "character bound never splits supplementary glyph");
        require(BugReportModel.clean("ab😃", 4).equals("ab😃"), "complete supplementary glyph retained");
        require(BugReportModel.singleLine("a\nb\tc", 20).equals("a b c"), "title one-line normalization");
        require(BugReportModel.clean(null, 2).isEmpty(), "missing text is safe");
        for (BugReportModel.Field field : BugReportModel.Field.values()) facts.put(field, "é".repeat(240));
        String maximum = BugReportModel.render(2, new BugReportModel.Snapshot(facts),
            "x".repeat(8000), "y".repeat(8000), "z".repeat(8000));
        require(maximum.length() <= BugReportModel.MAX_REPORT, "maximum allowed user and automatic fields fit report cap");
        require(!maximum.contains("x".repeat(3001)), "description fields cannot exceed their bounds");
        System.out.println("PASS BugReportModel checks=" + checks);
    }
}
