package org.gbarecomp;

import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.util.Collections;
import java.util.EnumMap;
import java.util.Map;

/** Bounded, local report text. No storage, network or Android dependencies. */
final class BugReportModel {
    static final String ISSUE_URL = "https://github.com/Edwin2929/sma3-recomp-android./issues/new";
    static final int MAX_TITLE = 120, MAX_SECTION = 3000, MAX_REPORT = 24000;
    static final int MAX_ISSUE_URL = 6000;

    enum Field {
        APP("Versión del port", "Port version", "Versão do port"),
        PACKAGE("Paquete", "Package", "Pacote"),
        PAYLOAD("Payload", "Payload", "Payload"),
        ORIGIN("Pantalla de origen", "Source screen", "Tela de origem"),
        DEVICE("Fabricante y modelo", "Manufacturer and model", "Fabricante e modelo"),
        ANDROID("Android", "Android", "Android"),
        ARCH("Arquitecturas", "Architectures", "Arquiteturas"),
        WINDOW("Ventana de la app", "App window", "Janela do app"),
        DISPLAY("Modo físico de pantalla", "Physical display mode", "Modo físico da tela"),
        REFRESH("Refresco informado a la app", "Refresh rate reported to app", "Atualização informada ao app"),
        CONTROLLERS("Mandos detectados", "Detected controllers", "Controles detectados"),
        REQUESTED_FPS("FPS de presentación solicitados", "Requested presentation FPS", "FPS de apresentação solicitados"),
        INTERPOLATION("Mezcla 120 activa al preparar", "120 blending active when preparing", "Mistura 120 ativa ao preparar"),
        OBSERVED_FPS("Última muestra de presentación", "Latest presentation sample", "Última amostra de apresentação"),
        BASE_FPS("Última muestra base", "Latest base sample", "Última amostra base"),
        EXTRA_FPS("Última muestra adicional", "Latest extra sample", "Última amostra adicional"),
        PREPARATION("Último tiempo de preparación", "Latest preparation time", "Último tempo de preparação"),
        SWAP("Último tiempo de presentación", "Latest swap time", "Último tempo de apresentação"),
        IMAGE("Modo de imagen", "Image mode", "Modo de imagem"),
        QUALITY("Tamaño de renderizado", "Rendering size", "Tamanho de renderização"),
        FILTER("Filtro", "Filter", "Filtro"),
        MENU_LANGUAGE("Idioma del menú", "Menu language", "Idioma do menu"),
        GAME_LANGUAGE("Idioma del juego", "Game language", "Idioma do jogo"),
        TOUCH("Controles táctiles", "Touch controls", "Controles de toque"),
        TOUCH_STYLE("Estilo táctil", "Touch style", "Estilo de toque"),
        TOUCH_OPACITY("Opacidad táctil", "Touch opacity", "Opacidade de toque"),
        TOUCH_LAYOUT("Diseño táctil", "Touch layout", "Layout de toque"),
        PAD_MAPPING("Asignaciones de mando personalizadas", "Customized controller bindings", "Mapeamentos de controle personalizados"),
        FPS_COUNTER("Contador FPS", "FPS counter", "Contador FPS"),
        YOSHI_COLOR("Color personalizado de Yoshi", "Custom Yoshi color", "Cor personalizada do Yoshi"),
        TIME_TRIAL("Contrarreloj", "Time trial", "Contrarrelógio"),
        TIME_TRIAL_OVERLAY("Cronómetro en pantalla", "On-screen timer", "Cronômetro na tela"),
        TIME_TRIAL_GHOST("Fantasma del récord", "Personal-best ghost", "Fantasma do recorde"),
        MODS("Perfiles de imagen instalados / activos", "Installed / active image profiles", "Perfis de imagem instalados / ativos");

        final String es, en, pt;
        Field(String es, String en, String pt) { this.es = es; this.en = en; this.pt = pt; }
        String label(int language) { return words(language, es, en, pt); }
    }

    interface Preferences {
        boolean contains(String key);
        boolean flag(String key, boolean fallback);
        int number(String key, int fallback);
    }

    /** Explicit keys only: never enumerate preferences or include custom strings. */
    static Map<Field, String> settings(Preferences prefs) {
        EnumMap<Field, String> values = new EnumMap<>(Field.class);
        values.put(Field.REQUESTED_FPS, prefs.flag("interpolation-120", false) ? "120" : "60");
        int image = prefs.contains("image-mode") ? prefs.number("image-mode", 0)
            : prefs.flag("stretch-screen", false) ? 2 : 0;
        values.put(Field.IMAGE, pick(image, new String[]{"original 3:2", "panorama", "stretch"}, 0));
        int quality = prefs.number("video-quality", 0);
        values.put(Field.QUALITY, quality == 720 ? "720p" : quality == 1080 ? "1080p" : "auto");
        values.put(Field.FILTER, pick(prefs.number("image-filter", 0),
            new String[]{"balanced", "nearest", "bilinear", "Scale2x", "Scale3x", "soft CRT", "cinematic"}, 0));
        String[] languages = {"English", "Español", "Português"};
        values.put(Field.MENU_LANGUAGE, pick(prefs.number("menu-language", 1), languages, 1));
        values.put(Field.GAME_LANGUAGE, pick(prefs.number("game-language", 0), languages, 0));
        values.put(Field.TOUCH, enabled(prefs.flag("touch-visible", true)));
        values.put(Field.TOUCH_STYLE, prefs.number("touch-style", 1) == 0 ? "letters" : "symbols");
        values.put(Field.TOUCH_OPACITY, Math.max(20, Math.min(100, prefs.number("touch-opacity", 75))) + "%");
        int layout = prefs.number("touch-design-active", 0);
        values.put(Field.TOUCH_LAYOUT, layout >= 1 && layout <= 3 ? "slot " + layout : "default");
        int[] defaults = {0, 1, 4, 6, 14, 13, 11, 12, 10, 9};
        int changed = 0;
        for (int bit = 0; bit < defaults.length; ++bit) {
            int saved = prefs.number("pad-" + bit, defaults[bit]);
            if (saved >= -1 && saved <= 20 && saved != defaults[bit]) ++changed;
        }
        values.put(Field.PAD_MAPPING, changed + " / 10");
        values.put(Field.FPS_COUNTER, enabled(prefs.flag("show-fps", false)));
        values.put(Field.YOSHI_COLOR, enabled(prefs.flag("yoshi-color-enabled", false)));
        values.put(Field.TIME_TRIAL, enabled(prefs.flag("time-trial-enabled", false)));
        values.put(Field.TIME_TRIAL_OVERLAY, enabled(prefs.flag("time-trial-overlay", true)));
        values.put(Field.TIME_TRIAL_GHOST, enabled(prefs.flag("time-trial-ghost", false)));
        return Collections.unmodifiableMap(values);
    }

    static String enabled(boolean value) { return value ? "on" : "off"; }
    private static String pick(int value, String[] choices, int fallback) {
        return choices[value >= 0 && value < choices.length ? value : fallback];
    }

    static final class Snapshot {
        final Map<Field, String> values;
        Snapshot(Map<Field, String> source) {
            EnumMap<Field, String> copy = new EnumMap<>(Field.class);
            for (Field key : Field.values()) {
                String value = source.get(key);
                if (value != null) copy.put(key, singleLine(value, 240));
            }
            values = Collections.unmodifiableMap(copy);
        }
        Snapshot mods(int installed, int active) {
            EnumMap<Field, String> copy = new EnumMap<>(Field.class);
            copy.putAll(values);
            copy.put(Field.MODS, installed >= 0 && installed <= 1024 && active >= 0 && active <= installed
                ? installed + " / " + active : "unavailable");
            return new Snapshot(copy);
        }
    }

    static String render(int language, Snapshot snapshot, String happened, String steps, String expected) {
        StringBuilder body = new StringBuilder();
        body.append("## ").append(words(language, "Qué pasó", "What happened", "O que aconteceu")).append('\n');
        section(body, happened, language);
        body.append("\n## ").append(words(language, "Pasos para reproducirlo", "Steps to reproduce", "Passos para reproduzir")).append('\n');
        section(body, steps, language);
        body.append("\n## ").append(words(language, "Qué esperaba", "Expected behavior", "Comportamento esperado")).append('\n');
        section(body, expected, language);
        body.append("\n## ").append(words(language, "Datos del port y del dispositivo", "Port and device details", "Dados do port e do dispositivo")).append('\n');
        for (Field key : Field.values()) {
            String value = snapshot.values.get(key);
            if (value != null) body.append("- ").append(key.label(language)).append(": `")
                .append(translatedValue(language, value).replace('`', '\'')).append("`\n");
        }
        body.append('\n').append(words(language,
            "Los FPS solicitados son una preferencia. La última muestra observada no es una media de la sesión ni una medición sostenida; los FPS adicionales son presentación interpolada.",
            "Requested FPS are a preference. The latest observed sample is not a session average or a sustained measurement; extra FPS are interpolated presentation.",
            "Os FPS solicitados são uma preferência. A última amostra observada não é uma média da sessão nem uma medição sustentada; os FPS extras são apresentação interpolada.")).append('\n');
        body.append('\n').append(words(language,
            "Informe preparado localmente para revisión del jugador. Los datos automáticos no incluyen ROM, BIOS, partidas, nombres de archivos, identificadores únicos ni registros privados.",
            "Report prepared locally for the player to review. Automatic details exclude ROM, BIOS, saves, file names, unique identifiers and private logs.",
            "Relatório preparado localmente para revisão do jogador. Os dados automáticos não incluem ROM, BIOS, saves, nomes de arquivos, identificadores únicos ou registros privados.")).append('\n');
        if (body.length() > MAX_REPORT) throw new IllegalStateException("Report exceeds bounded format");
        return body.toString();
    }

    private static void section(StringBuilder body, String text, int language) {
        String value = clean(text, MAX_SECTION).trim();
        body.append(value.isEmpty() ? words(language, "[Por completar]", "[To complete]", "[A preencher]") : value).append('\n');
    }

    static String title(int language, String draft, String version) {
        String value = singleLine(draft, MAX_TITLE).trim();
        if (!value.isEmpty()) return value;
        return singleLine("SMA3 " + singleLine(version, 60) + " — "
            + words(language, "Reporte de error", "Bug report", "Relato de erro"), MAX_TITLE);
    }

    static final class IssueLink {
        final String url;
        final boolean needsPaste;
        IssueLink(String url, boolean needsPaste) { this.url = url; this.needsPaste = needsPaste; }
    }

    static IssueLink issueLink(String title, String report) {
        String prefix = ISSUE_URL + "?title=" + encode(singleLine(title, MAX_TITLE));
        String url = prefix + "&body=" + encode(clean(report, MAX_REPORT));
        return url.length() <= MAX_ISSUE_URL ? new IssueLink(url, false) : new IssueLink(prefix, true);
    }

    private static String encode(String value) {
        try { return URLEncoder.encode(value, "UTF-8"); }
        catch (UnsupportedEncodingException impossible) { throw new IllegalStateException(impossible); }
    }

    static String clean(String value, int maximum) {
        if (value == null || maximum <= 0) return "";
        StringBuilder result = new StringBuilder(Math.min(value.length(), maximum));
        for (int i = 0; i < value.length() && result.length() < maximum;) {
            int code = value.codePointAt(i);
            i += Character.charCount(code);
            if (code == '\r') { if (i < value.length() && value.charAt(i) == '\n') continue; code = '\n'; }
            if ((code < 32 && code != '\n' && code != '\t') || code == 127
                    || (code >= 0x202a && code <= 0x202e) || (code >= 0x2066 && code <= 0x2069)
                    || (code >= 0xd800 && code <= 0xdfff)) continue;
            if (result.length() + Character.charCount(code) > maximum) break;
            result.appendCodePoint(code);
        }
        return result.toString();
    }

    static String singleLine(String value, int maximum) {
        return clean(value, maximum).replace('\n', ' ').replace('\t', ' ');
    }

    static String words(int language, String es, String en, String pt) {
        return language == 0 ? en : language == 2 ? pt : es;
    }

    private static String translatedValue(int language, String value) {
        if (value.equals("on")) return words(language, "activado", "on", "ativado");
        if (value.equals("off")) return words(language, "desactivado", "off", "desativado");
        if (value.equals("unavailable")) return words(language, "no disponible", "unavailable", "indisponível");
        if (value.equals("game")) return words(language, "juego", "game", "jogo");
        if (value.equals("setup")) return words(language, "pantalla inicial", "setup screen", "tela inicial");
        return value;
    }
}
