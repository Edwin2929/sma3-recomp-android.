package org.gbarecomp;

import android.app.AlertDialog;
import android.graphics.Color;
import android.view.Gravity;
import android.view.Surface;
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

    private boolean activityResumed;
    private String lastFps;
    private SurfaceHolder.Callback refreshCallback;
    private boolean wantsInterpolation() {
        return getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("interpolation-120", false);
    }
    private void applyPresentationMode() {
        boolean enabled = activityResumed && wantsInterpolation();
        float rate = enabled ? 119.455f : 59.7275f;
        WindowManager.LayoutParams attributes = getWindow().getAttributes();
        attributes.preferredRefreshRate = activityResumed ? rate : 0f;
        getWindow().setAttributes(attributes);
        if (Build.VERSION.SDK_INT >= 30 && mSurface != null) {
            Surface surface = mSurface.getHolder().getSurface();
            if (surface != null && surface.isValid()) {
                try { surface.setFrameRate(activityResumed ? rate : 0f, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT); }
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
            ? "Salida: " + fps + " FPS · mezcla 2×\nPantalla: " + String.format(Locale.US, "%.0f", hz) + " Hz"
            : "FPS: " + fps);
    }
    @Override protected void onResume() {
        super.onResume(); activityResumed = true; applyPresentationMode();
    }
    @Override protected void onPause() {
        activityResumed = false; applyPresentationMode(); super.onPause();
    }
    private AlertDialog optionsDialog;
    private OnBackInvokedCallback optionsBackCallback;

    private static final String[] GBA_BUTTONS = {"A", "B", "Select", "Start", "Derecha", "Izquierda", "Arriba", "Abajo", "R", "L"};
    private static final String[] PAD_BUTTONS = {"A / Cruz", "B / Círculo", "X / Cuadrado", "Y / Triángulo", "Back / Share", "Guía", "Start / Options", "Stick izquierdo (L3)", "Stick derecho (R3)", "L1 / LB", "R1 / RB", "Cruceta arriba", "Cruceta abajo", "Cruceta izquierda", "Cruceta derecha", "Misc", "Palanca 1", "Palanca 2", "Palanca 3", "Palanca 4", "Panel táctil", "Sin asignar"};
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
        for (int bit = 0; bit < 10; ++bit) rows[bit] = GBA_BUTTONS[bit] + " → " + PAD_BUTTONS[padBinding(bit) < 0 ? 21 : padBinding(bit)];
        AlertDialog menu = new AlertDialog.Builder(this).setTitle("Mapeo del mando")
            .setItems(rows, (dialog, bit) -> {
                AlertDialog chooser = new AlertDialog.Builder(this)
                    .setTitle("Botón para " + GBA_BUTTONS[bit])
                    .setSingleChoiceItems(PAD_BUTTONS, padBinding(bit) < 0 ? 21 : padBinding(bit), (selection, index) -> {
                        getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("pad-" + bit, index == 21 ? -1 : index).apply();
                        applyInputOptions(); selection.dismiss(); showControllerOptions();
                    }).setNegativeButton("Cancelar", (selection, which) -> showControllerOptions()).create();
                chooser.setOnCancelListener(ignored -> showControllerOptions());
                chooser.show(); setMenuOpen(true);
            })
            .setNeutralButton("Restablecer", (dialog, which) -> {
                android.content.SharedPreferences.Editor editor = getSharedPreferences("sma3-options", MODE_PRIVATE).edit();
                for (int bit = 0; bit < 10; ++bit) editor.remove("pad-" + bit);
                editor.apply(); applyInputOptions(); showControllerOptions();
            })
            .setPositiveButton("Listo", (dialog, which) -> { setMenuOpen(false); enterImmersiveMode(); }).create();
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
        String[] names = {"Automática (resolución de pantalla)", "720p · HD", "1080p · Full HD"};
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
        stretch.setText("Estirar a pantalla completa");
        stretch.setChecked(getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("stretch-screen", false));
        stretch.setOnCheckedChangeListener((button, checked) -> {
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putBoolean("stretch-screen", checked).apply();
            applyInputOptions();
        });
        panel.addView(stretch);
        TextView note = new TextView(this);
        note.setText("Escalado nítido para los gráficos originales de GBA. Estirar llena la pantalla y ensancha la imagen. Desactívalo para conservar las proporciones. 1080p puede consumir más batería.");
        note.setTextSize(13); note.setPadding(0, overlayDp(8), 0, overlayDp(8)); panel.addView(note);
        AlertDialog video = new AlertDialog.Builder(this).setTitle("Calidad de imagen")
            .setView(panel).setPositiveButton("Listo", (dialog, which) -> { setMenuOpen(false); enterImmersiveMode(); }).create();
        video.setOnCancelListener(ignored -> { setMenuOpen(false); enterImmersiveMode(); });
        video.show(); setMenuOpen(true);
    }

    private void showLanguageOptions() {
        if (isFinishing() || isDestroyed()) return;
        int selected = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("game-language", 0);
        AlertDialog languages = new AlertDialog.Builder(this).setTitle("Idioma del juego / Game language")
            .setSingleChoiceItems(new String[]{"English (original)", "Español latinoamericano", "Português do Brasil"}, selected,
                (dialog, which) -> {
                    getSharedPreferences("sma3-options", MODE_PRIVATE).edit().putInt("game-language", which).apply();
                    applyInputOptions();
                })
            .setPositiveButton("OK", (dialog, which) -> { setMenuOpen(false); enterImmersiveMode(); })
            .setNeutralButton("Información", (dialog, which) -> {
                new AlertDialog.Builder(this).setTitle("Traducción experimental")
                    .setMessage("Traduce diálogos, tutoriales, niveles, historia y textos de archivos. Los rótulos gráficos, créditos y Mario Bros conservan el inglés. Se aplica al siguiente mensaje o al volver a abrir la pantalla; un mensaje abierto conserva su idioma.")
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
            .setTitle("Opciones")
            .setMultiChoiceItems(new String[]{"Mostrar FPS", "120 FPS experimentales (mezcla)", "Mostrar controles táctiles", "Idioma / Language / Idioma…"},
                new boolean[]{getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("show-fps", false), wantsInterpolation(), getSharedPreferences("sma3-options", MODE_PRIVATE).getBoolean("touch-visible", true), false},
                (dialog, which, checked) -> {
                    if (which == 3) {
                        dialog.dismiss(); getWindow().getDecorView().post(this::showLanguageOptions); return;
                    }
                    getSharedPreferences("sma3-options", MODE_PRIVATE).edit()
                        .putBoolean(which == 0 ? "show-fps" : which == 1 ? "interpolation-120" : "touch-visible", checked).apply();
                    updateFpsVisibility(); applyInputOptions();
                    if (which == 1) { lastFps = null; applyPresentationMode(); }
                })
            .setNeutralButton("Mapear mando", (dialog, which) -> { dialog.dismiss(); getWindow().getDecorView().post(this::showControllerOptions); })
            .setNegativeButton("Imagen", (dialog, which) -> { dialog.dismiss(); getWindow().getDecorView().post(this::showVideoOptions); })
            .setPositiveButton("Continuar", null)
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
