package org.gbarecomp;

import android.app.AlertDialog;
import android.graphics.Color;
import android.view.Gravity;
import android.view.Surface;
import android.view.Display;
import android.hardware.display.DisplayManager;
import android.view.SurfaceHolder;
import java.util.Locale;
import android.view.KeyEvent;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.window.OnBackInvokedCallback;
import android.window.OnBackInvokedDispatcher;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import android.content.pm.ActivityInfo;
import android.content.res.AssetManager;
import android.graphics.Insets;
import android.os.Build;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

/**
 * The shared game Activity: installs the packaged payload (game TOML, mod
 * catalog, optional private assets) into private storage, runs immersive and
 * edge-to-edge (drawing under display cutouts), follows the game's
 * orientation policy with live rotation, and reports safe-area insets to the
 * runtime so host chrome avoids cutouts and system-gesture edges.
 */
public class GbaGameActivity extends SDLActivity {
    private GbaGameConfig config;
    private boolean nativeReady;
    private FrameLayout fpsOverlay;
    private TextView fpsLabel;
    private static final Pattern FPS_TITLE = Pattern.compile("([0-9]+(?:[.,][0-9]+)?) fps$");

    private int overlayDp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private void updateFpsVisibility() {
        if (fpsLabel != null) fpsLabel.setVisibility(
            getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("show-fps", false)
                ? View.VISIBLE : View.GONE);
    }

    private void installFpsOptions() {
        fpsOverlay = new FrameLayout(this);
        fpsLabel = new TextView(this);
        fpsLabel.setText("FPS: —");
        fpsLabel.setTextSize(14);
        fpsLabel.setTextColor(Color.WHITE);
        fpsLabel.setBackgroundColor(0xB0000000);
        fpsLabel.setPadding(overlayDp(8), overlayDp(4), overlayDp(8), overlayDp(4));
        fpsLabel.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        FrameLayout.LayoutParams counter = new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.START);
        counter.setMargins(overlayDp(8), overlayDp(8), overlayDp(8), 0);
        fpsOverlay.addView(fpsLabel, counter);
        addContentView(fpsOverlay, new android.view.ViewGroup.LayoutParams(-1, -1));
        updateFpsVisibility();
    }

    private String tr(String es, String en, String pt) {
        int lang = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("menu-language", 1);
        return lang == 0 ? en : lang == 2 ? pt : es;
    }
    private void showMenuLanguageOptions() {
        int selected = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("menu-language", 1);
        SettingsPanel panel = settingsPanel(tr("Idioma del menú", "Menu language", "Idioma do menu"),
            tr("Se aplica al instante a los ajustes.", "Applies immediately to settings.", "Aplica-se imediatamente às configurações."));
        panel.choices(new String[]{"English", "Español", "Português"}, new String[]{"", "", ""}, selected, which -> {
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("menu-language", which).apply();
            updateFpsText(); showLanguageMenu();
        });
        openSettings(panel.dialog(this::showLanguageMenu), this::showLanguageMenu);
    }
    private boolean activityResumed;
    private String lastFps;
    private SurfaceHolder.Callback refreshCallback;
    private DisplayManager displayManager;
    private final DisplayManager.DisplayListener displayListener = new DisplayManager.DisplayListener() {
        public void onDisplayAdded(int id) {}
        public void onDisplayRemoved(int id) {}
        public void onDisplayChanged(int id) {
            if (id == getWindowManager().getDefaultDisplay().getDisplayId()) updateFpsText();
        }
    };
    private boolean wantsInterpolation() {
        return getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("interpolation-120", false);
    }
    private void applyPresentationMode() {
        boolean enabled = activityResumed && wantsInterpolation();
        // Keep the exact content cadence on Surface; select the physical 120 Hz mode below.
        float rate = enabled ? 119.455f : 59.7275f;
        Display display = getWindowManager().getDefaultDisplay();
        Display.Mode currentMode = display.getMode();
        int preferredMode = 0;
        float bestDifference = Float.MAX_VALUE;
        if (enabled) for (Display.Mode mode : display.getSupportedModes()) {
            // Never change the user's display resolution.
            if (mode.getPhysicalWidth() != currentMode.getPhysicalWidth()
                    || mode.getPhysicalHeight() != currentMode.getPhysicalHeight()) continue;
            float difference = Math.abs(mode.getRefreshRate() - 120f);
            if (mode.getRefreshRate() >= 119f && difference < bestDifference) {
                preferredMode = mode.getModeId(); bestDifference = difference;
            }
        }
        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.preferredRefreshRate = activityResumed ? (enabled ? 120f : 60f) : 0f;
        attributes.preferredDisplayModeId = preferredMode;
        getWindow().setAttributes(attributes);
        if (Build.VERSION.SDK_INT >= 30 && mSurface != null) {
            Surface surface = mSurface.getHolder().getSurface();
            if (surface != null && surface.isValid()) {
                try {
                    if (Build.VERSION.SDK_INT >= 31) surface.setFrameRate(activityResumed ? rate : 0f,
                        Surface.FRAME_RATE_COMPATIBILITY_DEFAULT, Surface.CHANGE_FRAME_RATE_ALWAYS);
                    else surface.setFrameRate(activityResumed ? rate : 0f, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
                }
                catch (IllegalArgumentException ignored) { /* device retains its mode */ }
            }
        }
        try { GbaNative.setInterpolationEnabled(enabled); }
        catch (UnsatisfiedLinkError notLoadedYet) { /* retried on resume/surface creation */ }
        updateFpsText();
        applyInputOptions();
    }
    private void updateFpsText() {
        if (fpsLabel == null) return;
        String fps = lastFps == null ? "—" : lastFps;
        float hz = getWindowManager().getDefaultDisplay().getRefreshRate();
        fpsLabel.setText(wantsInterpolation()
            ? tr("Salida: ", "Output: ", "Saída: ") + fps + tr(" FPS · mezcla 2×\nPantalla: ", " FPS · 2× blend\nDisplay: ", " FPS · mistura 2×\nTela: ") + String.format(Locale.US, "%.0f", hz) + " Hz"
            : "FPS: " + fps);
        if (wantsInterpolation() && hz < 119f) fpsLabel.append("\n" + tr(
            "Pantalla por debajo de 120 Hz: revisa Pantalla fluida y ahorro de batería.",
            "Display below 120 Hz: check Smooth Display and Battery Saver.",
            "Tela abaixo de 120 Hz: verifique Tela fluida e economia de bateria."));
    }
    @Override protected void onResume() {
        super.onResume(); activityResumed = true; applyPresentationMode();
    }
    @Override protected void onPause() {
        activityResumed = false; applyPresentationMode(); super.onPause();
    }
    private AlertDialog optionsDialog;
    private ModsUi modsUi;
    private OnBackInvokedCallback optionsBackCallback;

    private String[] gbaButtons() { return new String[]{"A", "B", "Select", "Start", tr("Derecha", "Right", "Direita"), tr("Izquierda", "Left", "Esquerda"), tr("Arriba", "Up", "Cima"), tr("Abajo", "Down", "Baixo"), "R", "L"}; }
    private String[] padButtons() { return new String[]{tr("A / Cruz", "A / Cross", "A / Cruz"), tr("B / Círculo", "B / Circle", "B / Círculo"), tr("X / Cuadrado", "X / Square", "X / Quadrado"), tr("Y / Triángulo", "Y / Triangle", "Y / Triângulo"), "Back / Share", tr("Guía", "Guide", "Guia"), "Start / Options", tr("Stick izquierdo (L3)", "Left stick (L3)", "Analógico esquerdo (L3)"), tr("Stick derecho (R3)", "Right stick (R3)", "Analógico direito (R3)"), "L1 / LB", "R1 / RB", tr("Cruceta arriba", "D-pad up", "Direcional cima"), tr("Cruceta abajo", "D-pad down", "Direcional baixo"), tr("Cruceta izquierda", "D-pad left", "Direcional esquerda"), tr("Cruceta derecha", "D-pad right", "Direcional direita"), "Misc", tr("Palanca 1", "Paddle 1", "Alavanca 1"), tr("Palanca 2", "Paddle 2", "Alavanca 2"), tr("Palanca 3", "Paddle 3", "Alavanca 3"), tr("Palanca 4", "Paddle 4", "Alavanca 4"), tr("Panel táctil", "Touchpad", "Painel de toque"), tr("Sin asignar", "Unassigned", "Sem atribuição")}; }
    private static final int[] DEFAULT_PAD = {0,1,4,6,14,13,11,12,10,9};
    private static final int IMAGE_ORIGINAL = 0;
    private static final int IMAGE_PANORAMA = 1;
    private static final int IMAGE_STRETCH = 2;
    // Stable preference/JNI IDs; keep this order in sync with ImageFilter.
    private static final int IMAGE_FILTER_BALANCED = 0;
    private static final int IMAGE_FILTER_COUNT = 7;
    private int imageMode() {
        android.content.SharedPreferences preferences = getSharedPreferences("sma3-options", MODE_PRIVATE);
        int selected;
        if (preferences.contains("image-mode")) {
            selected = preferences.getInt("image-mode", IMAGE_ORIGINAL);
        } else {
            // Preserve the user's existing presentation choice on upgrade.
            // An old stretch preference never silently enables panorama.
            selected = preferences.getBoolean("stretch-screen", false) ? IMAGE_STRETCH : IMAGE_ORIGINAL;
            preferences.edit().putInt("image-mode", selected).apply();
        }
        return selected >= IMAGE_ORIGINAL && selected <= IMAGE_STRETCH ? selected : IMAGE_ORIGINAL;
    }
    private int imageFilter() {
        int selected = getSharedPreferences("sma3-options", MODE_PRIVATE)
            .getInt("image-filter", IMAGE_FILTER_BALANCED);
        return selected >= 0 && selected < IMAGE_FILTER_COUNT ? selected : IMAGE_FILTER_BALANCED;
    }
    private int padBinding(int bit) {
        int button = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("pad-" + bit, DEFAULT_PAD[bit]);
        return button >= -1 && button <= 20 ? button : DEFAULT_PAD[bit];
    }
    private void applyInputOptions() {
        try {
            applyTouchDesign();
            GbaNative.setLanguage(getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("game-language", 0));
            GbaNative.setVideoOptions(getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("video-quality", 0),
                imageMode());
            GbaNative.setVideoFilter(imageFilter());
            GbaNative.setTouchControlsVisible(getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("touch-visible", true));
            for (int bit = 0; bit < 10; ++bit) GbaNative.setControllerButton(bit, padBinding(bit));
        } catch (UnsatisfiedLinkError notLoadedYet) { /* retried with surface */ }
    }
    private void setMenuOpen(boolean open) {
        try { GbaNative.setOptionsOpen(open); } catch (UnsatisfiedLinkError ignored) {}
    }
    private String[] imageModeNames() {
        return new String[]{"Original (3:2)",
            tr("Panorámica (beta)", "Panoramic (beta)", "Panorâmica (beta)"),
            tr("Estirar a pantalla completa", "Stretch to full screen", "Esticar para tela inteira")};
    }
    private String[] imageFilterNames() {
        return new String[]{
            tr("Equilibrado", "Balanced", "Equilibrado"),
            tr("Píxel nítido", "Crisp pixels", "Pixels nítidos"),
            tr("Suave bilineal", "Bilinear smoothing", "Suavização bilinear"),
            "Scale2x", "Scale3x",
            tr("CRT suave", "Soft CRT", "CRT suave"),
            tr("Cinematográfico", "Cinematic", "Cinematográfico")};
    }
    private String qualityName() {
        int quality = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("video-quality", 0);
        return quality == 720 ? "720p · HD" : quality == 1080 ? "1080p · Full HD" : tr("Automática", "Automatic", "Automática");
    }
    private SettingsPanel settingsPanel(String title, String subtitle) {
        return new SettingsPanel(this, title, subtitle, backText());
    }
    private void showControllerOptions() {
        SettingsPanel panel = settingsPanel(tr("Mapeo del mando", "Controller mapping", "Mapeamento do controle"),
            tr("Elige qué botón del mando activa cada acción.", "Choose the controller button for each action.", "Escolha o botão do controle para cada ação."));
        for (int bit = 0; bit < 10; ++bit) {
            final int action = bit;
            panel.card(SettingsPanel.CONTROLS, gbaButtons()[bit], padButtons()[padBinding(bit) < 0 ? 21 : padBinding(bit)],
                () -> showControllerBinding(action));
        }
        panel.section(tr("Configuración inicial", "Default configuration", "Configuração inicial"));
        panel.card(SettingsPanel.CONTROLS, tr("Restablecer mapeo", "Reset mapping", "Restaurar mapeamento"),
            tr("Recupera la asignación de botones predeterminada.", "Restore the default button assignments.", "Restaura as atribuições padrão dos botões."), () -> {
                android.content.SharedPreferences.Editor editor = getSharedPreferences("sma3-options", MODE_PRIVATE).edit();
                for (int bit = 0; bit < 10; ++bit) editor.remove("pad-" + bit);
                editor.apply(); applyInputOptions(); showControllerOptions();
            });
        openSettings(panel.dialog(this::showControlsMenu), this::showControlsMenu);
    }
    private void showControllerBinding(int bit) {
        SettingsPanel panel = settingsPanel(tr("Botón para ", "Button for ", "Botão para ") + gbaButtons()[bit],
            tr("Los cambios se guardan al seleccionar.", "Your selection is saved immediately.", "A seleção é salva imediatamente."));
        String[] buttons = padButtons();
        String[] descriptions = new String[buttons.length];
        java.util.Arrays.fill(descriptions, "");
        panel.choices(buttons, descriptions, padBinding(bit) < 0 ? 21 : padBinding(bit), index -> {
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("pad-" + bit, index == 21 ? -1 : index).apply();
            applyInputOptions(); showControllerOptions();
        });
        openSettings(panel.dialog(this::showControllerOptions), this::showControllerOptions);
    }

    private void showVideoOptions() {
        if (isFinishing() || isDestroyed()) return;
        SettingsPanel panel = settingsPanel(tr("Imagen", "Image", "Imagem"),
            tr("Elige cómo se muestra el juego.", "Choose how the game is displayed.", "Escolha como o jogo é exibido."));
        panel.section(tr("Modo de imagen", "Image mode", "Modo de imagem"));
        String betaWarning = tr(
            "Es una fase temprana y todavía puede presentar errores visuales o de compatibilidad. Si encuentras un problema, usa Original (3:2).",
            "This is an early version and may still have visual or compatibility issues. If you encounter a problem, use Original (3:2).",
            "É uma fase inicial e ainda pode apresentar erros visuais ou de compatibilidade. Se encontrar um problema, use Original (3:2).");
        TextView warning = panel.notice(tr("Panorámica (beta) · Aviso", "Panoramic (beta) · Notice", "Panorâmica (beta) · Aviso"), betaWarning);
        panel.choices(imageModeNames(), new String[]{
            tr("Conserva las proporciones originales, sin deformar la imagen.", "Keeps the original proportions without distortion.", "Mantém as proporções originais, sem deformar a imagem."),
            tr("Muestra más escenario en las escenas compatibles. Puede dejar bandas.", "Shows more scenery in supported scenes. Bars may remain.", "Mostra mais cenário nas cenas compatíveis. Pode deixar faixas."),
            tr("Llena el área de juego. Puede deformar la imagen y no amplía el escenario.", "Fills the game area. May distort the image without showing more scenery.", "Preenche a área do jogo. Pode deformar a imagem sem ampliar o cenário.")
        }, imageMode(), mode -> {
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("image-mode", mode).apply();
            applyInputOptions();
            if (mode == IMAGE_PANORAMA) warning.announceForAccessibility(betaWarning);
        });
        panel.section(tr("Acabado de la imagen", "Image appearance", "Aparência da imagem"));
        panel.card(SettingsPanel.IMAGE, tr("Filtros de imagen", "Image filters", "Filtros de imagem"),
            imageFilterNames()[imageFilter()] + " · " + tr("Elige entre siete estilos.", "Choose from seven styles.", "Escolha entre sete estilos."),
            this::showImageFilterOptions);
        panel.section(tr("Resolución de salida", "Output resolution", "Resolução de saída"));
        panel.card(SettingsPanel.IMAGE, qualityName(),
            tr("Ajusta la resolución y el consumo de batería.", "Adjust resolution and battery usage.", "Ajuste a resolução e o consumo de bateria."), this::showResolutionOptions);
        openSettings(panel.dialog(this::showOptions), this::showOptions);
    }
    private void showImageFilterOptions() {
        SettingsPanel panel = settingsPanel(tr("Filtros de imagen", "Image filters", "Filtros de imagem"),
            tr("Elige un filtro. Se guarda y aplica al instante, sin reiniciar el juego.",
               "Choose a filter. It is saved and applied immediately, without restarting the game.",
               "Escolha um filtro. Ele é salvo e aplicado imediatamente, sem reiniciar o jogo."));
        String[] names = imageFilterNames();
        names[IMAGE_FILTER_BALANCED] += tr(" (recomendado)", " (recommended)", " (recomendado)");
        panel.choices(names, new String[]{
            tr("Mantiene los píxeles definidos y suaviza sus bordes al ajustar la imagen a la pantalla.",
               "Keeps pixels defined while softening their edges when fitting the image to the display.",
               "Mantém os pixels definidos e suaviza suas bordas ao ajustar a imagem à tela."),
            tr("Conserva los bordes duros y el aspecto pixelado. Los escalones se ven más marcados.",
               "Preserves hard edges and the pixelated look. Stair-step edges are more visible.",
               "Preserva as bordas firmes e o aspecto pixelado. Os contornos em degraus ficam mais visíveis."),
            tr("Suaviza toda la imagen. Puede hacer que los textos y detalles pequeños se vean más borrosos.",
               "Smooths the whole image. Text and small details may look blurrier.",
               "Suaviza toda a imagem. Textos e pequenos detalhes podem ficar mais borrados."),
            tr("Reduce los contornos escalonados con un escalado de 2× pensado para gráficos de píxeles.",
               "Reduces stair-step edges with 2× scaling designed for pixel graphics.",
               "Reduz os contornos em degraus com ampliação de 2× feita para gráficos de pixels."),
            tr("Trabaja los contornos a 3×. Puede cambiar detalles pequeños y requiere más procesamiento que Scale2x.",
               "Refines edges at 3×. May alter small details and requires more processing than Scale2x.",
               "Refina os contornos em 3×. Pode alterar pequenos detalhes e exige mais processamento que Scale2x."),
            tr("Añade líneas horizontales suaves y fijas para un estilo retro. Oscurece ligeramente la imagen.",
               "Adds subtle, fixed horizontal lines for a retro look. Makes the image slightly darker.",
               "Adiciona linhas horizontais suaves e fixas para um estilo retrô. Escurece um pouco a imagem."),
            tr("Añade dos franjas negras fijas, arriba y abajo. Cubren parte de la imagen y sus indicadores; los controles siguen visibles.",
               "Adds two fixed black bars, at the top and bottom. They cover part of the image and its HUD; controls remain visible.",
               "Adiciona duas faixas pretas fixas, acima e abaixo. Cobrem parte da imagem e seus indicadores; os controles continuam visíveis.")
        }, imageFilter(), filter -> {
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("image-filter", filter).apply();
            applyInputOptions();
        });
        panel.section(tr("Cómo elegir", "Choosing a filter", "Como escolher"));
        panel.paragraph(tr(
            "Empieza con Equilibrado o prueba Scale2x si prefieres contornos menos escalonados. Si notas menos fluidez, vuelve a Equilibrado o Píxel nítido.",
            "Start with Balanced, or try Scale2x for less jagged edges. If motion feels less smooth, switch back to Balanced or Crisp pixels.",
            "Comece com Equilibrado ou experimente Scale2x para contornos menos serrilhados. Se perceber menos fluidez, volte para Equilibrado ou Pixels nítidos."));
        panel.paragraph(tr(
            "Los filtros cambian el acabado de la imagen; no añaden detalle original ni amplían el campo de visión. CRT es un efecto estético. Funcionan con Original (3:2), Panorámica (beta) y Estirar.",
            "Filters change the image's appearance; they do not add original detail or expand the field of view. CRT is an aesthetic effect. They work with Original (3:2), Panoramic (beta) and Stretch.",
            "Os filtros mudam a aparência da imagem; não acrescentam detalhes originais nem ampliam o campo de visão. CRT é um efeito estético. Funcionam com Original (3:2), Panorâmica (beta) e Esticar."));
        openSettings(panel.dialog(this::showVideoOptions), this::showVideoOptions);
    }
    private void showResolutionOptions() {
        SettingsPanel panel = settingsPanel(tr("Resolución", "Resolution", "Resolução"),
            tr("La resolución de salida no añade detalle al juego original.", "Output resolution does not add detail to the original game.", "A resolução de saída não acrescenta detalhes ao jogo original."));
        int[] values = {0, 720, 1080};
        int saved = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("video-quality", 0);
        int selected = saved == 720 ? 1 : saved == 1080 ? 2 : 0;
        panel.choices(new String[]{tr("Automática", "Automatic", "Automática"), "720p · HD", "1080p · Full HD"}, new String[]{
            tr("Se adapta a la resolución de la pantalla.", "Adapts to the display resolution.", "Adapta-se à resolução da tela."),
            tr("Una opción de menor resolución para reducir el consumo.", "A lower-resolution option to reduce power usage.", "Uma opção de menor resolução para reduzir o consumo."),
            tr("Mayor resolución de salida; puede consumir más batería.", "Higher output resolution; may use more battery.", "Maior resolução de saída; pode consumir mais bateria.")
        }, selected, index -> {
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("video-quality", values[index]).apply();
            applyInputOptions();
        });
        openSettings(panel.dialog(this::showVideoOptions), this::showVideoOptions);
    }

    private void showLanguageOptions() {
        int selected = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("game-language", 0);
        SettingsPanel panel = settingsPanel(tr("Idioma del juego", "Game language", "Idioma do jogo"),
            tr("El cambio se aplica al siguiente mensaje.", "Changes apply to the next message.", "A alteração se aplica à próxima mensagem."));
        panel.choices(new String[]{"English (original)", "Español latinoamericano", "Português do Brasil"}, new String[]{"", "", ""}, selected, which -> {
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("game-language", which).apply();
            applyInputOptions();
        });
        panel.section(tr("Acerca de la traducción", "About the translation", "Sobre a tradução"));
        panel.paragraph(tr(
            "La traducción es parcial: incluye diálogos, tutoriales, niveles, historia y textos de archivos. Los rótulos gráficos, créditos y Mario Bros conservan el inglés.",
            "The translation is partial: it includes dialogue, tutorials, levels, story and file text. Graphic labels, credits and Mario Bros remain in English.",
            "A tradução é parcial: inclui diálogos, tutoriais, fases, história e textos de arquivos. Rótulos gráficos, créditos e Mario Bros continuam em inglês."));
        openSettings(panel.dialog(this::showLanguageMenu), this::showLanguageMenu);
    }

    private String backText() { return tr("Volver", "Back", "Voltar"); }
    private TouchDesignEditor designEditor;
    private boolean settingsDestroyed;
    private void openSettings(AlertDialog next, Runnable parent) {
        if (settingsDestroyed || isFinishing() || isDestroyed()) return;
        AlertDialog old = optionsDialog;
        optionsDialog = next;
        if (old != null) old.dismiss();
        next.setOnCancelListener(dialog -> {
            // Open the parent before the cancelled page's dismiss callback runs;
            // navigating Back must not briefly release game input between pages.
            if (parent != null && !settingsDestroyed && !isFinishing() && !isDestroyed()) parent.run();
        });
        next.setOnDismissListener(dialog -> {
            if (optionsDialog == next) {
                optionsDialog = null;
                setMenuOpen(false);
                if (!settingsDestroyed) enterImmersiveMode();
            }
        });
        next.show();
        SettingsPanel.fitWindow(this, next);
        setMenuOpen(true);
    }
    private void showOptions() {
        SettingsPanel panel = new SettingsPanel(this, tr("Ajustes", "Settings", "Configurações"),
            tr("Tu juego, a tu manera.", "Make the game your own.", "Seu jogo, do seu jeito."),
            tr("Volver al juego", "Return to game", "Voltar ao jogo"));
        panel.card(SettingsPanel.IMAGE, tr("Imagen", "Image", "Imagem"),
            imageModeNames()[imageMode()] + " · " + imageFilterNames()[imageFilter()] + " · " + qualityName(), this::showVideoOptions);
        panel.card(SettingsPanel.PERFORMANCE, tr("Rendimiento y FPS", "Performance and FPS", "Desempenho e FPS"),
            tr("Fluidez, mezcla temporal y contador FPS", "Smoothness, temporal blending and FPS counter", "Fluidez, mistura temporal e contador FPS"), this::showPerformanceMenu);
        panel.card(SettingsPanel.CONTROLS, tr("Controles", "Controls", "Controles"),
            tr("Pantalla táctil, mando y diseños personalizados", "Touchscreen, controller and custom layouts", "Tela de toque, controle e layouts personalizados"), this::showControlsMenu);
        panel.card(SettingsPanel.MODS, "Mods",
            tr("Importar y administrar perfiles de color", "Import and manage color profiles", "Importar e gerenciar perfis de cor"), this::showMods);
        panel.card(SettingsPanel.LANGUAGE, tr("Idiomas", "Languages", "Idiomas"),
            tr("Idioma del menú y traducción del juego", "Menu language and game translation", "Idioma do menu e tradução do jogo"), this::showLanguageMenu);
        openSettings(panel.dialog(null), null);
    }
    private void showMods() {
        ensureModsUi();
        modsUi.show();
    }
    private void ensureModsUi() {
        if (modsUi == null) modsUi = new ModsUi(this, this::tr, this::openSettings, this::showOptions);
    }
    @Override
    protected void onActivityResult(int requestCode, int resultCode, android.content.Intent data) {
        // Android can recreate the Activity while the document picker is open.
        // Recreate the helper before delivering that already-authorized result.
        if (requestCode == ModsUi.PICK_MOD) {
            ensureModsUi();
            modsUi.onActivityResult(requestCode, resultCode, data);
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }
    private void showPerformanceMenu() {
        SettingsPanel panel = settingsPanel(tr("Rendimiento y FPS", "Performance and FPS", "Desempenho e FPS"),
            tr("Ajusta la presentación sin cambiar la velocidad del juego.", "Adjust presentation without changing game speed.", "Ajuste a apresentação sem alterar a velocidade do jogo."));
        panel.toggle(tr("Mostrar FPS", "Show FPS", "Mostrar FPS"),
            tr("Muestra el contador durante la partida.", "Display the counter while playing.", "Mostra o contador durante a partida."),
            getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("show-fps", false), checked -> {
                getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putBoolean("show-fps", checked).apply();
                updateFpsVisibility();
            });
        panel.toggle(tr("Mezcla temporal a 120 FPS", "Temporal blending at 120 FPS", "Mistura temporal a 120 FPS"),
            tr("Añade imágenes intermedias. Requiere una pantalla compatible con 120 Hz.", "Adds intermediate images. Requires a display that supports 120 Hz.", "Acrescenta imagens intermediárias. Requer uma tela compatível com 120 Hz."),
            wantsInterpolation(), checked -> {
                getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putBoolean("interpolation-120", checked).apply();
                lastFps = null; applyPresentationMode();
            });
        panel.section(tr("Cómo funciona", "How it works", "Como funciona"));
        panel.paragraph(tr(
            "La mezcla puede suavizar el movimiento, pero también producir estelas. El juego conserva su ritmo original; 120 FPS se refiere a las imágenes presentadas. El rendimiento depende del dispositivo y del ahorro de batería.",
            "Blending can smooth motion, but may also produce ghosting. The game keeps its original timing; 120 FPS refers to presented images. Performance depends on the device and battery-saving settings.",
            "A mistura pode suavizar o movimento, mas também produzir rastros. O jogo mantém seu ritmo original; 120 FPS refere-se às imagens apresentadas. O desempenho depende do aparelho e da economia de bateria."));
        openSettings(panel.dialog(this::showOptions), this::showOptions);
    }
    private void showControlsMenu() {
        SettingsPanel panel = settingsPanel(tr("Controles", "Controls", "Controles"),
            tr("Juega con pantalla táctil o con un mando.", "Play with touch controls or a controller.", "Jogue com controles de toque ou um controle."));
        panel.toggle(tr("Controles táctiles", "Touch controls", "Controles de toque"),
            tr("Muestra los botones sobre la pantalla del juego.", "Show the buttons over the game screen.", "Mostra os botões sobre a tela do jogo."),
            getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("touch-visible", true), checked -> {
                getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putBoolean("touch-visible", checked).apply();
                applyInputOptions();
            });
        panel.card(SettingsPanel.LAYOUT, tr("Diseños personalizados", "Custom layouts", "Layouts personalizados"),
            tr("Mueve y cambia el tamaño de los controles. Guarda hasta tres diseños.", "Move and resize the controls. Save up to three layouts.", "Mova e redimensione os controles. Salve até três layouts."), this::showDesignMenu);
        panel.card(SettingsPanel.CONTROLS, tr("Mapear mando", "Map controller", "Mapear controle"),
            tr("Asigna los botones de tu mando físico.", "Assign the buttons on your physical controller.", "Atribua os botões do seu controle físico."), this::showControllerOptions);
        openSettings(panel.dialog(this::showOptions), this::showOptions);
    }
    private void showLanguageMenu() {
        SettingsPanel panel = settingsPanel(tr("Idiomas", "Languages", "Idiomas"),
            tr("Configura el menú y el juego por separado.", "Set the menu and game languages separately.", "Configure os idiomas do menu e do jogo separadamente."));
        int menuLanguage = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("menu-language", 1);
        int gameLanguage = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("game-language", 0);
        String[] menus = {"English", "Español", "Português"};
        String[] games = {"English (original)", "Español latinoamericano", "Português do Brasil"};
        panel.card(SettingsPanel.LANGUAGE, tr("Idioma del menú", "Menu language", "Idioma do menu"),
            menus[menuLanguage >= 0 && menuLanguage < 3 ? menuLanguage : 1], this::showMenuLanguageOptions);
        panel.card(SettingsPanel.LANGUAGE, tr("Idioma del juego", "Game language", "Idioma do jogo"),
            games[gameLanguage >= 0 && gameLanguage < 3 ? gameLanguage : 0], this::showLanguageOptions);
        openSettings(panel.dialog(this::showOptions), this::showOptions);
    }
    private float[] storedDesign(int slot){
        try{
            org.json.JSONArray array=new org.json.JSONArray(getSharedPreferences("sma3-options",MODE_PRIVATE).getString("touch-design-"+slot,"[]"));if(array.length()!=28)return null;
            float[] data=new float[28];for(int i=0;i<28;i++){data[i]=(float)array.getDouble(i);if(!Float.isFinite(data[i]))return null;}
            for(int i=0;i<28;i+=4)if(data[i]<0||data[i]>1||data[i+1]<0||data[i+1]>1||data[i+2]<.015f||data[i+2]>1||data[i+3]<.015f||data[i+3]>1)return null;
            return data;
        }catch(org.json.JSONException error){return null;}
    }
    private void applyTouchDesign(){
        int slot=getSharedPreferences("sma3-options",MODE_PRIVATE).getInt("touch-design-active",0);
        float[] data=slot>=1&&slot<=3?storedDesign(slot):null;GbaNative.setTouchDesign(data,data!=null);
    }
    private void showDesignMenu() {
        int active = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("touch-design-active", 0);
        SettingsPanel panel = settingsPanel(tr("Diseños personalizados", "Custom layouts", "Layouts personalizados"),
            tr("Edita una copia y elige Guardar y usar para aplicarla.", "Edit a draft, then choose Save and use to apply it.", "Edite uma cópia e escolha Salvar e usar para aplicá-la."));
        panel.card(SettingsPanel.LAYOUT, tr("Predeterminado", "Default", "Padrão"),
            active == 0 ? tr("En uso", "In use", "Em uso") : tr("Usar la distribución original.", "Use the original arrangement.", "Usar a disposição original."), () -> {
                getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("touch-design-active", 0).apply();
                applyInputOptions(); showDesignMenu();
            });
        for (int slot = 1; slot <= 3; ++slot) {
            final int chosen = slot;
            boolean saved = storedDesign(slot) != null;
            panel.card(SettingsPanel.LAYOUT, tr("Diseño ", "Layout ", "Layout ") + slot,
                active == slot && saved ? tr("En uso · Toca para editar", "In use · Tap to edit", "Em uso · Toque para editar")
                    : saved ? tr("Guardado · Toca para editar", "Saved · Tap to edit", "Salvo · Toque para editar")
                    : tr("Nuevo · Personaliza posición y tamaño", "New · Customize position and size", "Novo · Personalize posição e tamanho"),
                () -> showTouchEditor(chosen));
        }
        openSettings(panel.dialog(this::showControlsMenu), this::showControlsMenu);
    }
    private void showTouchEditor(int slot){
        float[] defaults=GbaNative.getTouchDesign(true);
        if (defaults == null || defaults.length != 28 || defaults[2] <= 0) {
            SettingsPanel panel = settingsPanel(tr("Editor de controles", "Control editor", "Editor de controles"), "");
            panel.paragraph(tr("Espera a que aparezca el juego y vuelve a abrir el editor.", "Wait for the game to appear, then reopen the editor.", "Espere o jogo aparecer e reabra o editor."));
            openSettings(panel.dialog(this::showDesignMenu), this::showDesignMenu);
            return;
        }
        float[] saved=storedDesign(slot);View view=getWindow().getDecorView();
        float width=view.getWidth()-fpsOverlay.getPaddingLeft()-fpsOverlay.getPaddingRight(),height=view.getHeight()-fpsOverlay.getPaddingTop()-fpsOverlay.getPaddingBottom();
        float aspect=width>0&&height>0?width/height:16f/9f;
        designEditor=new TouchDesignEditor(this,this::tr,saved==null?defaults:saved,defaults,aspect,data->{
            try{
                org.json.JSONArray array=new org.json.JSONArray();for(float value:data)array.put((double)value);
                getSharedPreferences("sma3-options",MODE_PRIVATE).edit().putString("touch-design-"+slot,array.toString()).putInt("touch-design-active",slot).apply();applyInputOptions();
            }catch(org.json.JSONException error){android.widget.Toast.makeText(this,tr("No se pudo guardar el diseño","Could not save layout","Não foi possível salvar o layout"),android.widget.Toast.LENGTH_LONG).show();}
        });
        designEditor.setOnDismissListener(d->{designEditor=null;GbaNative.setTouchEditing(false);if(!settingsDestroyed&&!isFinishing()&&!isDestroyed())showDesignMenu();else setMenuOpen(false);});
        if(optionsDialog!=null){optionsDialog.dismiss();optionsDialog=null;}
        GbaNative.setTouchEditing(true);setMenuOpen(true);designEditor.show();
    }
    private void handleSettingsBack(){if(designEditor!=null)designEditor.cancel();else if(optionsDialog!=null&&optionsDialog.isShowing())optionsDialog.cancel();else showOptions();}

    // Consume navigation Back before SDL can translate it into a game key.
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()) handleSettingsBack();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public void onBackPressed() {
        handleSettingsBack();
    }

    @Override
    protected void onDestroy() {
        settingsDestroyed=true;
        if (modsUi != null) modsUi.close();
        if(designEditor!=null)designEditor.dismiss();
        try { GbaNative.setTouchEditing(false); } catch(UnsatisfiedLinkError ignored) {}
        if (Build.VERSION.SDK_INT >= 33 && optionsBackCallback != null) {
            getOnBackInvokedDispatcher().unregisterOnBackInvokedCallback(optionsBackCallback);
        }
        if (displayManager != null) displayManager.unregisterDisplayListener(displayListener);
        if (optionsDialog != null) optionsDialog.dismiss();
        if (mSurface != null && refreshCallback != null) mSurface.getHolder().removeCallback(refreshCallback);
        setMenuOpen(false);
        super.onDestroy();
    }

    // SDL's native presents/sec counter sends the window title to this method
    // on Android's UI thread. Never estimate FPS from UI refresh callbacks.
    @Override
    public void setTitle(CharSequence title) {
        super.setTitle(title);
        if (fpsLabel == null || title == null) return;
        Matcher value = FPS_TITLE.matcher(title);
        if (value.find()) { lastFps = value.group(1); updateFpsText(); }
    }


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        config = GbaGameConfig.load(this);
        setRequestedOrientation(orientationFor(config.orientation));
        try {
            installPayload();
        } catch (IOException error) {
            throw new IllegalStateException("Unable to install game payload", error);
        }
        super.onCreate(savedInstanceState);
        setMenuOpen(false);
        installFpsOptions();
        displayManager = (DisplayManager) getSystemService(DISPLAY_SERVICE);
        if (displayManager != null) displayManager.registerDisplayListener(displayListener, null);
        refreshCallback = new SurfaceHolder.Callback() {
            public void surfaceCreated(SurfaceHolder holder) { applyPresentationMode(); }
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) { applyPresentationMode(); }
            public void surfaceDestroyed(SurfaceHolder holder) {
                try { GbaNative.setInterpolationEnabled(false); } catch (UnsatisfiedLinkError ignored) {}
            }
        };
        if (mSurface != null) mSurface.getHolder().addCallback(refreshCallback);
        if (Build.VERSION.SDK_INT >= 33) {
            optionsBackCallback = this::handleSettingsBack;
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                OnBackInvokedDispatcher.PRIORITY_DEFAULT, optionsBackCallback);
        }
        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams attributes = getWindow().getAttributes();
            attributes.layoutInDisplayCutoutMode = Build.VERSION.SDK_INT >= 30
                ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
                : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(attributes);
        }
        View decor = getWindow().getDecorView();
        decor.setOnApplyWindowInsetsListener((view, insets) -> {
            reportInsets(insets);
            return view.onApplyWindowInsets(insets);
        });
        enterImmersiveMode();
    }

    @Override
    public void onConfigurationChanged(android.content.res.Configuration configuration) {
        super.onConfigurationChanged(configuration);
        getWindow().getDecorView().post(() -> {
            if (optionsDialog != null && optionsDialog.isShowing()) {
                SettingsPanel.fitWindow(this, optionsDialog);
                optionsDialog.getWindow().getDecorView().requestLayout();
            }
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            enterImmersiveMode();
            View decor = getWindow().getDecorView();
            WindowInsets insets = decor.getRootWindowInsets();
            if (insets != null) reportInsets(insets);
        }
    }

    static int orientationFor(String policy) {
        if ("any".equals(policy)) return ActivityInfo.SCREEN_ORIENTATION_FULL_USER;
        if ("portrait".equals(policy)) return ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT;
        return ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE;
    }

    private void reportInsets(WindowInsets insets) {
        int left = 0, top = 0, right = 0, bottom = 0;
        if (Build.VERSION.SDK_INT >= 28) {
            DisplayCutout cutout = insets.getDisplayCutout();
            if (cutout != null) {
                left = cutout.getSafeInsetLeft();
                top = cutout.getSafeInsetTop();
                right = cutout.getSafeInsetRight();
                bottom = cutout.getSafeInsetBottom();
            }
        }
        if (Build.VERSION.SDK_INT >= 29) {
            // Edge swipes starting here belong to the system (home/back).
            Insets gestures = insets.getMandatorySystemGestureInsets();
            left = Math.max(left, gestures.left);
            top = Math.max(top, gestures.top);
            right = Math.max(right, gestures.right);
            bottom = Math.max(bottom, gestures.bottom);
        }
        if (fpsOverlay != null) fpsOverlay.setPadding(left, top, right, bottom);
        try {
            GbaNative.setSafeInsets(left, top, right, bottom);
            DisplayMetrics metrics = new DisplayMetrics();
            getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
            GbaNative.setDisplayDpi(metrics.xdpi, metrics.ydpi);
            nativeReady = true;
            applyInputOptions();
        } catch (UnsatisfiedLinkError notLoadedYet) {
            // libmain loads during super.onCreate; later passes succeed.
            nativeReady = false;
        }
    }

    protected void enterImmersiveMode() {
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars()
                    | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    /**
     * Copies assets/payload into getFilesDir() once per payload version.
     * Player data (saves, config.ini, suspend states, imported ROMs) lives
     * beside the payload and is never overwritten: only files that exist in
     * the payload are replaced, and the payload never contains player data.
     */
    private void installPayload() throws IOException {
        File marker = new File(getFilesDir(), ".payload-version");
        if (marker.isFile()) {
            byte[] version = new byte[(int) marker.length()];
            try (FileInputStream input = new FileInputStream(marker)) {
                int offset = 0;
                while (offset < version.length) {
                    int read = input.read(version, offset, version.length - offset);
                    if (read < 0) break;
                    offset += read;
                }
            }
            String installed = new String(version, StandardCharsets.UTF_8).trim();
            if (config.payloadVersion.equals(installed)) return;
        }
        copyAssetTree(getAssets(), "payload", getFilesDir());
        try (FileOutputStream output = new FileOutputStream(marker)) {
            output.write((config.payloadVersion + "\n").getBytes(StandardCharsets.UTF_8));
        }
    }

    private static void copyAssetTree(AssetManager assets, String assetPath,
                                      File destination) throws IOException {
        String[] children = assets.list(assetPath);
        if (children != null && children.length > 0) {
            if (!destination.isDirectory() && !destination.mkdirs()) {
                throw new IOException("Unable to create " + destination);
            }
            for (String child : children) {
                copyAssetTree(assets, assetPath + "/" + child, new File(destination, child));
            }
            return;
        }
        File parent = destination.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("Unable to create " + parent);
        }
        // Player-owned files next to the payload are never replaced.
        if (destination.isFile() && isPlayerOwned(destination.getName())) return;
        try (InputStream input = assets.open(assetPath);
             FileOutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[1024 * 1024];
            int read;
            while ((read = input.read(buffer)) >= 0) {
                if (read > 0) output.write(buffer, 0, read);
            }
        }
    }

    /** Files the player (or the running game) owns once they exist. */
    private static boolean isPlayerOwned(String name) {
        return name.equals("state.toml") || name.endsWith(".sav") ||
               name.equals("config.ini") || name.equals("keybinds.ini");
    }
}
