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
        AlertDialog menu = new AlertDialog.Builder(this)
            .setTitle(tr("Idioma del menú", "Menu language", "Idioma do menu"))
            .setSingleChoiceItems(new String[]{"English", "Español", "Português"}, selected, (dialog, which) -> {
                getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("menu-language", which).apply();
                updateFpsText(); dialog.dismiss(); showOptions();
            }).setNegativeButton(tr("Cancelar", "Cancel", "Cancelar"), (dialog, which) -> showOptions()).create();
        menu.setOnCancelListener(dialog -> showOptions());
        menu.show(); setMenuOpen(true);
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
    private OnBackInvokedCallback optionsBackCallback;

    private String[] gbaButtons() { return new String[]{"A", "B", "Select", "Start", tr("Derecha", "Right", "Direita"), tr("Izquierda", "Left", "Esquerda"), tr("Arriba", "Up", "Cima"), tr("Abajo", "Down", "Baixo"), "R", "L"}; }
    private String[] padButtons() { return new String[]{tr("A / Cruz", "A / Cross", "A / Cruz"), tr("B / Círculo", "B / Circle", "B / Círculo"), tr("X / Cuadrado", "X / Square", "X / Quadrado"), tr("Y / Triángulo", "Y / Triangle", "Y / Triângulo"), "Back / Share", tr("Guía", "Guide", "Guia"), "Start / Options", tr("Stick izquierdo (L3)", "Left stick (L3)", "Analógico esquerdo (L3)"), tr("Stick derecho (R3)", "Right stick (R3)", "Analógico direito (R3)"), "L1 / LB", "R1 / RB", tr("Cruceta arriba", "D-pad up", "Direcional cima"), tr("Cruceta abajo", "D-pad down", "Direcional baixo"), tr("Cruceta izquierda", "D-pad left", "Direcional esquerda"), tr("Cruceta derecha", "D-pad right", "Direcional direita"), "Misc", tr("Palanca 1", "Paddle 1", "Alavanca 1"), tr("Palanca 2", "Paddle 2", "Alavanca 2"), tr("Palanca 3", "Paddle 3", "Alavanca 3"), tr("Palanca 4", "Paddle 4", "Alavanca 4"), tr("Panel táctil", "Touchpad", "Painel de toque"), tr("Sin asignar", "Unassigned", "Sem atribuição")}; }
    private static final int[] DEFAULT_PAD = {0,1,4,6,14,13,11,12,10,9};
    private int padBinding(int bit) {
        int button = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("pad-" + bit, DEFAULT_PAD[bit]);
        return button >= -1 && button <= 20 ? button : DEFAULT_PAD[bit];
    }
    private void applyInputOptions() {
        try {
            GbaNative.setLanguage(getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("game-language", 0));
            GbaNative.setVideoOptions(getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("video-quality", 0),
                getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("stretch-screen", false));
            GbaNative.setTouchControlsVisible(getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("touch-visible", true));
            for (int bit = 0; bit < 10; ++bit) GbaNative.setControllerButton(bit, padBinding(bit));
        } catch (UnsatisfiedLinkError notLoadedYet) { /* retried with surface */ }
    }
    private void setMenuOpen(boolean open) {
        try { GbaNative.setOptionsOpen(open); } catch (UnsatisfiedLinkError ignored) {}
    }
    private void showControllerOptions() {
        String[] rows = new String[10];
        for (int bit = 0; bit < 10; ++bit) rows[bit] = gbaButtons()[bit] + " → " + padButtons()[padBinding(bit) < 0 ? 21 : padBinding(bit)];
        AlertDialog menu = new AlertDialog.Builder(this).setTitle(tr("Mapeo del mando", "Controller mapping", "Mapeamento do controle"))
            .setItems(rows, (dialog, bit) -> {
                AlertDialog chooser = new AlertDialog.Builder(this)
                    .setTitle(tr("Botón para ", "Button for ", "Botão para ") + gbaButtons()[bit])
                    .setSingleChoiceItems(padButtons(), padBinding(bit) < 0 ? 21 : padBinding(bit), (selection, index) -> {
                        getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("pad-" + bit, index == 21 ? -1 : index).apply();
                        applyInputOptions(); selection.dismiss(); showControllerOptions();
                    }).setNegativeButton(tr("Cancelar", "Cancel", "Cancelar"), (selection, which) -> showControllerOptions()).create();
                chooser.setOnCancelListener(ignored -> showControllerOptions());
                chooser.show(); setMenuOpen(true);
            })
            .setNeutralButton(tr("Restablecer", "Reset", "Restaurar"), (dialog, which) -> {
                android.content.SharedPreferences.Editor editor = getSharedPreferences("sma3-options", MODE_PRIVATE).edit();
                for (int bit = 0; bit < 10; ++bit) editor.remove("pad-" + bit);
                editor.apply(); applyInputOptions(); showControllerOptions();
            })
            .setPositiveButton(tr("Listo", "Done", "Concluído"), (dialog, which) -> { setMenuOpen(false); enterImmersiveMode(); }).create();
        menu.setOnCancelListener(ignored -> { setMenuOpen(false); enterImmersiveMode(); });
        menu.show(); setMenuOpen(true);
    }

    private void showVideoOptions() {
        if (isFinishing() || isDestroyed()) return;
        android.widget.LinearLayout panel = new android.widget.LinearLayout(this);
        panel.setOrientation(android.widget.LinearLayout.VERTICAL);
        panel.setPadding(overlayDp(20), overlayDp(8), overlayDp(20), 0);
        android.widget.RadioGroup qualities = new android.widget.RadioGroup(this);
        int saved = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("video-quality", 0);
        int[] values = {0, 720, 1080};
        String[] names = {tr("Automática (resolución de pantalla)", "Automatic (display resolution)", "Automática (resolução da tela)"), "720p · HD", "1080p · Full HD"};
        for (int i = 0; i < values.length; ++i) {
            android.widget.RadioButton button = new android.widget.RadioButton(this);
            button.setId(i + 100); button.setText(names[i]); qualities.addView(button);
            if (saved == values[i]) qualities.check(i + 100);
        }
        qualities.setOnCheckedChangeListener((group, id) -> {
            int index = id - 100;
            if (index < 0 || index >= values.length) return;
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("video-quality", values[index]).apply();
            applyInputOptions();
        });
        panel.addView(qualities);
        android.widget.CheckBox stretch = new android.widget.CheckBox(this);
        stretch.setText(tr("Estirar a pantalla completa", "Stretch to full screen", "Esticar para tela inteira"));
        stretch.setChecked(getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("stretch-screen", false));
        stretch.setOnCheckedChangeListener((button, checked) -> {
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putBoolean("stretch-screen", checked).apply();
            applyInputOptions();
        });
        panel.addView(stretch);
        TextView note = new TextView(this);
        note.setText(tr("Escalado nítido para los gráficos originales de GBA. Estirar llena la pantalla y ensancha la imagen. Desactívalo para conservar las proporciones. 1080p puede consumir más batería.", "Sharp scaling of the original GBA graphics. Stretching fills the screen and widens the image. Turn it off to keep the original proportions. 1080p may use more battery.", "Ampliação nítida dos gráficos originais do GBA. Esticar preenche a tela e alarga a imagem. Desative para manter as proporções. 1080p pode consumir mais bateria."));
        note.setTextSize(13); note.setPadding(0, overlayDp(8), 0, overlayDp(8)); panel.addView(note);
        AlertDialog video = new AlertDialog.Builder(this).setTitle(tr("Calidad de imagen", "Image quality", "Qualidade da imagem"))
            .setView(panel).setPositiveButton(tr("Listo", "Done", "Concluído"), (dialog, which) -> { setMenuOpen(false); enterImmersiveMode(); }).create();
        video.setOnCancelListener(ignored -> { setMenuOpen(false); enterImmersiveMode(); });
        video.show(); setMenuOpen(true);
    }

    private void showLanguageOptions() {
        if (isFinishing() || isDestroyed()) return;
        int selected = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("game-language", 0);
        AlertDialog languages = new AlertDialog.Builder(this).setTitle(tr("Idioma del juego / Game language", "Game language", "Idioma do jogo"))
            .setSingleChoiceItems(new String[]{"English (original)", "Español latinoamericano", "Português do Brasil"}, selected,
                (dialog, which) -> {
                    getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("game-language", which).apply();
                    applyInputOptions();
                })
            .setPositiveButton("OK", (dialog, which) -> { setMenuOpen(false); enterImmersiveMode(); })
            .setNeutralButton(tr("Información", "Information", "Informações"), (dialog, which) -> {
                new AlertDialog.Builder(this).setTitle(tr("Traducción experimental", "Experimental translation", "Tradução experimental"))
                    .setMessage(tr("Traduce diálogos, tutoriales, niveles, historia y textos de archivos. Los rótulos gráficos, créditos y Mario Bros conservan el inglés. Se aplica al siguiente mensaje o al volver a abrir la pantalla; un mensaje abierto conserva su idioma.", "Translates dialogue, tutorials, levels, story and file text. Graphic labels, credits and Mario Bros remain in English. Applies to the next message or when reopening a screen; an open message keeps its language.", "Traduz diálogos, tutoriais, fases, história e textos de arquivos. Rótulos gráficos, créditos e Mario Bros continuam em inglês. Aplica-se à próxima mensagem ou ao reabrir a tela; uma mensagem aberta mantém seu idioma."))
                    .setPositiveButton("OK", (info, button) -> { setMenuOpen(false); enterImmersiveMode(); })
                    .setOnCancelListener(info -> setMenuOpen(false)).show();
            }).create();
        languages.setOnCancelListener(dialog -> { setMenuOpen(false); enterImmersiveMode(); });
        languages.show(); setMenuOpen(true);
    }

    private void showOptions() {
        if (isFinishing() || isDestroyed()) return;
        if (optionsDialog != null && optionsDialog.isShowing()) {
            optionsDialog.dismiss();
            return;
        }
        optionsDialog = new AlertDialog.Builder(this)
            .setTitle(tr("Opciones", "Options", "Opções"))
            .setMultiChoiceItems(new String[]{tr("Mostrar FPS", "Show FPS", "Mostrar FPS"), tr("120 FPS experimentales (mezcla)", "Experimental 120 FPS (blend)", "120 FPS experimentais (mistura)"), tr("Mostrar controles táctiles", "Show touch controls", "Mostrar controles de toque"), tr("Idioma del juego…", "Game language…", "Idioma do jogo…"), tr("Idioma del menú…", "Menu language…", "Idioma do menu…")},
                new boolean[]{getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("show-fps", false), wantsInterpolation(), getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("touch-visible", true), false, false},
                (dialog, which, checked) -> {
                    if (which == 4) {
                        dialog.dismiss(); getWindow().getDecorView().post(this::showMenuLanguageOptions); return;
                    }
                    if (which == 3) {
                        dialog.dismiss(); getWindow().getDecorView().post(this::showLanguageOptions); return;
                    }
                    getSharedPreferences("sma3-options", MODE_PRIVATE).edit()
                        .putBoolean(which == 0 ? "show-fps" : which == 1 ? "interpolation-120" : "touch-visible", checked).apply();
                    updateFpsVisibility(); applyInputOptions();
                    if (which == 1) { lastFps = null; applyPresentationMode(); }
                })
            .setNeutralButton(tr("Mapear mando", "Map controller", "Mapear controle"), (dialog, which) -> { dialog.dismiss(); getWindow().getDecorView().post(this::showControllerOptions); })
            .setNegativeButton(tr("Imagen", "Image", "Imagem"), (dialog, which) -> { dialog.dismiss(); getWindow().getDecorView().post(this::showVideoOptions); })
            .setPositiveButton(tr("Continuar", "Continue", "Continuar"), null)
            .create();
        optionsDialog.setOnDismissListener(dialog -> {
            optionsDialog = null;
            setMenuOpen(false);
            if (!isFinishing() && !isDestroyed()) enterImmersiveMode();
        });
        optionsDialog.show();
        setMenuOpen(true);
    }

    // Consume navigation Back before SDL can translate it into a game key.
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
            if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()) showOptions();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public void onBackPressed() {
        showOptions();
    }

    @Override
    protected void onDestroy() {
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
            optionsBackCallback = this::showOptions;
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
