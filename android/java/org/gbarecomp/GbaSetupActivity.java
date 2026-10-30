package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.hardware.Sensor;
import android.hardware.SensorManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.view.Gravity;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * First-launch setup shared by every gbarecomp Android game: the player picks
 * their own cartridge dump (and, optionally or when required, a GBA BIOS)
 * through the Storage Access Framework. Files are size- and SHA-1-verified,
 * then copied into app-private storage. Nothing copyrighted ships in the APK.
 */
public class GbaSetupActivity extends Activity {
    private static final int PICK_BIOS = 1001;
    private static final int PICK_ROM = 1002;
    private static final String SETUP_PREFERENCES = "setup";
    private static final String SKIP_LAUNCHER_ON_BOOT = "skip_launcher_on_boot";
    private static final long BIOS_SIZE = 16 * 1024;
    /** Validation/launcher-shortcut extra: start the game when ready without
     *  changing the player's skip-this-screen preference. */
    public static final String EXTRA_AUTOSTART = "org.gbarecomp.extra.AUTOSTART";

    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private GbaGameConfig config;
    private TextView biosStatus;
    private TextView romStatus;
    private TextView infoStatus;
    private Button playButton;
    private Button biosButton;
    private Button romButton;
    private CheckBox skipLauncherCheck;
    private boolean biosReady;
    private boolean romReady;
    private int displayedLanguage;
    private UpdatesUi updatesUi;
    private BugReportUi bugReportUi;
    private AlertDialog updateDialog;

    private String tr(String es, String en, String pt) {
        int language = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("menu-language", 1);
        return language == 0 ? en : language == 2 ? pt : es;
    }

    /** The Activity that runs the game; games may override. */
    protected Class<?> gameActivityClass() {
        return GbaGameActivity.class;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        config = GbaGameConfig.load(this);
        updatesUi = new UpdatesUi(this, this::tr, this::openUpdateDialog, this::dismissUpdates);
        bugReportUi = new BugReportUi(this, this::tr, this::openUpdateDialog, this::dismissUpdates);
        setRequestedOrientation(GbaGameActivity.orientationFor(config.orientation));
        if (hasBundledPrivateAssets()) {
            startActivity(new Intent(this, gameActivityClass()));
            finish();
            return;
        }
        setContentView(buildContent());
        enterImmersiveMode();
        refreshStatus();
        if (!ready()) return;
        if (getIntent().getBooleanExtra(EXTRA_AUTOSTART, false)) {
            startActivity(new Intent(this, gameActivityClass()));
        } else if (skipLauncherOnBoot()) {
            launchGame();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (playButton != null) {
            setContentView(buildContent());
            refreshStatus();
        }
        if (updateDialog != null && updateDialog.isShowing()) SettingsPanel.fitWindow(this, updateDialog);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (updatesUi != null) updatesUi.onResume();
        if (bugReportUi != null) bugReportUi.onResume();
        enterImmersiveMode();
        if (playButton != null) {
            if (displayedLanguage != getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("menu-language", 1)) {
                setContentView(buildContent());
            }
            refreshStatus();
        }
    }

    @Override
    protected void onPause() {
        if (bugReportUi != null) bugReportUi.onPause();
        if (updatesUi != null) updatesUi.onPause();
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        if (bugReportUi != null) bugReportUi.close();
        if (updatesUi != null) updatesUi.close();
        dismissUpdates();
        worker.shutdownNow();
        super.onDestroy();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) enterImmersiveMode();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == UpdatesUi.INSTALL_PERMISSION) {
            if (updatesUi == null)
                updatesUi = new UpdatesUi(this, this::tr, this::openUpdateDialog, this::dismissUpdates);
            updatesUi.onActivityResult(requestCode, resultCode, data);
            return;
        }
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        if (requestCode != PICK_BIOS && requestCode != PICK_ROM) return;
        final boolean bios = requestCode == PICK_BIOS;
        final Uri uri = data.getData();
        setBusy(true, bios ? tr("Comprobando BIOS…", "Checking BIOS…", "Verificando BIOS…") : tr("Comprobando ROM…", "Checking ROM…", "Verificando ROM…"));
        worker.execute(() -> {
            try {
                installVerifiedAsset(uri, bios);
                runOnUiThread(() -> {
                    setBusy(false, null);
                    refreshStatus();
                });
            } catch (Exception error) {
                runOnUiThread(() -> {
                    setBusy(false, null);
                    refreshStatus();
                    showError(bios ? tr("BIOS no compatible", "BIOS not accepted", "BIOS não compatível") : tr("ROM no compatible", "ROM not accepted", "ROM não compatível"),
                        error.getMessage());
                });
            }
        });
    }

    private void openUpdateDialog(AlertDialog next, Runnable parent) {
        if (isFinishing() || isDestroyed()) return;
        AlertDialog old = updateDialog;
        updateDialog = next;
        if (old != null) old.dismiss();
        next.setOnCancelListener(dialog -> {
            if (parent != null && !isFinishing() && !isDestroyed()) parent.run();
        });
        next.setOnDismissListener(dialog -> {
            if (updateDialog == next) {
                updateDialog = null;
                if (!isFinishing() && !isDestroyed()) enterImmersiveMode();
            }
        });
        next.show();
        SettingsPanel.fitWindow(this, next);
    }

    private void dismissUpdates() {
        AlertDialog current = updateDialog;
        updateDialog = null;
        if (current != null) current.dismiss();
    }

    private boolean ready() {
        return romReady && (biosReady || !config.biosRequired);
    }

    private boolean portrait() {
        return getResources().getConfiguration().orientation ==
            Configuration.ORIENTATION_PORTRAIT;
    }

    private View buildContent() {
        final boolean vertical = portrait();
        final boolean stackedFooter = vertical || getResources().getConfiguration().screenWidthDp < 760;
        displayedLanguage = getSharedPreferences("sma3-options", MODE_PRIVATE).getInt("menu-language", 1);
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setBackgroundColor(SettingsPanel.BACKGROUND);

        ScrollView scroll = new ScrollView(this);
        scroll.setFillViewport(true);
        scroll.setBackgroundColor(SettingsPanel.BACKGROUND);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER_HORIZONTAL);
        root.setPadding(dp(vertical ? 20 : 36), dp(vertical ? 36 : 14),
            dp(vertical ? 20 : 36), dp(10));
        scroll.addView(root, new ScrollView.LayoutParams(
            ScrollView.LayoutParams.MATCH_PARENT, ScrollView.LayoutParams.WRAP_CONTENT));
        page.addView(scroll, new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1));

        android.widget.ImageView logo = new android.widget.ImageView(this);
        logo.setImageDrawable(getApplicationInfo().loadIcon(getPackageManager()));
        logo.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        root.addView(logo, margins(dp(vertical ? 64 : 48), dp(vertical ? 64 : 48), 0, 0, 0, dp(10)));

        TextView eyebrow = text(tr("EDICIÓN ANDROID", "ANDROID EDITION", "EDIÇÃO ANDROID"), 12, SettingsPanel.ACCENT);
        eyebrow.setTypeface(Typeface.DEFAULT_BOLD);
        eyebrow.setLetterSpacing(0.16f);
        root.addView(eyebrow);

        TextView title = text(config.title, 27, Color.WHITE);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        title.setGravity(Gravity.CENTER);
        root.addView(title, margins(-1, -2, 0, dp(2), 0, 0));

        TextView subtitle = text(tr("Selecciona tus archivos del juego para comenzar.", "Choose your own game files to begin.", "Selecione seus arquivos do jogo para começar."), 15,
            SettingsPanel.SECONDARY);
        subtitle.setGravity(Gravity.CENTER);
        root.addView(subtitle, margins(-1, -2, 0, 0, 0, dp(10)));

        LinearLayout assets = new LinearLayout(this);
        assets.setOrientation(vertical ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        assets.setGravity(Gravity.CENTER);
        root.addView(assets, margins(-1, -2, 0, 0, 0, dp(8)));

        LinearLayout romCard = assetCard("1", config.romLabel,
            tr("Selecciona tu copia original del cartucho (.gba).", "Choose your clean cartridge dump (.gba).", "Selecione sua cópia original do cartucho (.gba)."));
        romStatus = statusText();
        romButton = actionButton(tr("Elegir ROM", "Choose ROM", "Escolher ROM"));
        romButton.setOnClickListener(view -> pickFile(PICK_ROM));
        romCard.addView(romStatus, margins(-1, -2, 0, dp(4), 0, dp(4)));
        romCard.addView(romButton, margins(-1, dp(48), 0, 0, 0, 0));
        assets.addView(romCard, vertical ? margins(-1, -2, 0, dp(6), 0, dp(6))
                                         : weightedMargins(1, dp(8), 0, dp(8), 0));

        LinearLayout biosCard = assetCard("2",
            config.biosRequired ? "GBA BIOS" : tr("GBA BIOS (opcional)", "GBA BIOS (optional)", "GBA BIOS (opcional)"),
            config.biosRequired
                ? tr("Selecciona tu archivo gba_bios.bin de 16 KiB.", "Choose your clean 16 KiB gba_bios.bin dump.", "Selecione seu arquivo gba_bios.bin de 16 KiB.")
                : tr("Recomendada para mayor precisión. Si no se aporta, se usa un sustituto integrado.", "Recommended for accuracy. Without it, a built-in substitute is used.", "Recomendada para maior precisão. Sem ela, é usado um substituto integrado."));
        biosStatus = statusText();
        biosButton = actionButton(tr("Elegir BIOS", "Choose BIOS", "Escolher BIOS"));
        biosButton.setOnClickListener(view -> pickFile(PICK_BIOS));
        biosCard.addView(biosStatus, margins(-1, -2, 0, dp(4), 0, dp(4)));
        biosCard.addView(biosButton, margins(-1, dp(48), 0, 0, 0, 0));
        assets.addView(biosCard, vertical ? margins(-1, -2, 0, dp(6), 0, dp(6))
                                          : weightedMargins(1, dp(8), 0, dp(8), 0));

        CheckBox fpsCheck = new CheckBox(this);
        fpsCheck.setText(tr("Mostrar FPS durante el juego", "Show FPS while playing", "Mostrar FPS durante o jogo"));
        tintCheckBox(fpsCheck);
        fpsCheck.setTextColor(Color.WHITE);
        fpsCheck.setMinHeight(dp(48));
        fpsCheck.setChecked(getSharedPreferences("sma3-options", MODE_PRIVATE)
            .getBoolean("show-fps", false));
        fpsCheck.setOnCheckedChangeListener((button, checked) ->
            getSharedPreferences("sma3-options", MODE_PRIVATE).edit()
                .putBoolean("show-fps", checked).apply());
        root.addView(fpsCheck, margins(-1, -2, 0, 0, 0, dp(6)));

        Button updateButton = actionButton(tr("Actualizar aplicación", "Update app", "Atualizar aplicativo"));
        updateButton.setOnClickListener(view -> updatesUi.show());
        root.addView(updateButton, margins(-1, dp(48), 0, dp(4), 0, dp(10)));

        Button reportButton = actionButton(tr("Reportar un problema", "Report a problem", "Relatar um problema"));
        reportButton.setOnClickListener(view -> bugReportUi.show());
        root.addView(reportButton, margins(-1, dp(48), 0, 0, 0, dp(10)));

        infoStatus = text("", 13, SettingsPanel.SECONDARY);
        infoStatus.setGravity(Gravity.CENTER);
        root.addView(infoStatus, margins(-1, -2, 0, 0, 0, dp(6)));

        TextView privacy = text(
            tr("Tus archivos se guardan de forma privada en la aplicación y no se incluyen en el APK.",
                "Your files stay in this app’s private storage and are never included in the APK.",
                "Seus arquivos ficam no armazenamento privado do aplicativo e não são incluídos no APK."),
            11, SettingsPanel.SECONDARY);
        privacy.setGravity(Gravity.CENTER);
        root.addView(privacy, margins(-1, -2, 0, 0, 0, 0));

        TextView beta = text(tr(
            "Panorámica (beta): está en una fase temprana y puede presentar errores. Puedes activarla en Ajustes → Imagen.",
            "Panoramic (beta): this is an early version and may have errors. Enable it in Settings → Image.",
            "Panorâmica (beta): está em uma fase inicial e pode apresentar erros. Ative-a em Configurações → Imagem."),
            13, Color.rgb(255, 212, 138));
        beta.setGravity(Gravity.CENTER);
        root.addView(beta, margins(-1, -2, 0, dp(10), 0, dp(4)));

        LinearLayout footer = new LinearLayout(this);
        footer.setOrientation(stackedFooter ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        footer.setGravity(Gravity.CENTER);
        footer.setPadding(dp(vertical ? 20 : 36), dp(8), dp(vertical ? 20 : 36), dp(18));

        skipLauncherCheck = new CheckBox(this);
        skipLauncherCheck.setText(tr("Abrir el juego directamente cuando esté listo", "Start the game directly when ready", "Abrir o jogo diretamente quando estiver pronto"));
        tintCheckBox(skipLauncherCheck);
        skipLauncherCheck.setTextColor(SettingsPanel.TEXT);
        skipLauncherCheck.setTextSize(14);
        skipLauncherCheck.setMinHeight(dp(52));
        skipLauncherCheck.setChecked(skipLauncherOnBoot());
        skipLauncherCheck.setOnCheckedChangeListener((button, checked) ->
            getSharedPreferences(SETUP_PREFERENCES, MODE_PRIVATE)
                .edit().putBoolean(SKIP_LAUNCHER_ON_BOOT, checked).apply());
        footer.addView(skipLauncherCheck, stackedFooter
            ? margins(-2, -2, 0, 0, 0, dp(8)) : weightedMargins(1, 0, 0, dp(18), 0));

        playButton = actionButton(tr("Jugar", "Play", "Jogar"));
        playButton.setTextSize(18);
        playButton.setTypeface(Typeface.DEFAULT_BOLD);
        playButton.setOnClickListener(view -> launchGame());
        footer.addView(playButton, margins(stackedFooter ? -1 : dp(280), dp(58), 0, 0, 0, 0));

        page.addView(footer, new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT));
        return page;
    }

    private LinearLayout assetCard(String number, String title, String body) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(20), dp(12), dp(20), dp(12));
        card.setBackground(roundRect(SettingsPanel.SURFACE, 18, SettingsPanel.BORDER));
        TextView step = text(number + "  " + title, 17, Color.WHITE);
        step.setTypeface(Typeface.DEFAULT_BOLD);
        card.addView(step);
        TextView description = text(body, 13, SettingsPanel.SECONDARY);
        card.addView(description, margins(-1, -2, 0, dp(4), 0, 0));
        return card;
    }

    private TextView statusText() {
        TextView status = text(tr("Comprobando…", "Checking…", "Verificando…"), 13, Color.rgb(255, 211, 78));
        status.setTypeface(Typeface.DEFAULT_BOLD);
        return status;
    }

    private Button actionButton(String label) {
        Button button = new Button(this);
        button.setText(label);
        button.setTextColor(SettingsPanel.BACKGROUND);
        button.setTextSize(13);
        button.setTypeface(Typeface.DEFAULT_BOLD);
        button.setAllCaps(false);
        button.setGravity(Gravity.CENTER);
        button.setPadding(dp(16), 0, dp(16), 0);
        button.setBackground(roundRect(SettingsPanel.ACCENT, 14, Color.TRANSPARENT));
        return button;
    }

    private TextView text(String value, float size, int color) {
        TextView text = new TextView(this);
        text.setText(value);
        text.setTextSize(size);
        text.setTextColor(color);
        text.setLineSpacing(0, 1.12f);
        return text;
    }

    private File biosFile() { return new File(getFilesDir(), "bios/gba_bios.bin"); }
    private File romFile() { return new File(getFilesDir(), "roms/" + config.romFile); }

    private void refreshStatus() {
        biosReady = biosFile().isFile() && biosFile().length() == BIOS_SIZE;
        romReady = romFile().isFile() && romFile().length() == config.romSize;
        setAssetStatus(romStatus, romButton, romReady, "ROM", true);
        setAssetStatus(biosStatus, biosButton, biosReady, "BIOS", config.biosRequired);
        playButton.setEnabled(ready());
        playButton.setAlpha(playButton.isEnabled() ? 1.0f : 0.35f);

        if (config.showGyroStatus) {
            SensorManager manager = (SensorManager) getSystemService(SENSOR_SERVICE);
            Sensor gyro = manager == null ? null
                : manager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
            infoStatus.setText(gyro != null
                ? tr("●  Giroscopio disponible  ·  ", "●  Device gyroscope ready  ·  ", "●  Giroscópio disponível  ·  ") + gyro.getName()
                : tr("Giroscopio no disponible; el movimiento del mando sigue funcionando.",
                    "Device gyroscope unavailable; controller motion still works.",
                    "Giroscópio indisponível; o movimento do controle continua funcionando."));
            infoStatus.setTextColor(gyro != null ? Color.rgb(120, 235, 171)
                                                 : Color.rgb(255, 184, 104));
        } else {
            infoStatus.setText("");
        }
    }

    private void setAssetStatus(TextView status, Button button, boolean ready,
                                String kind, boolean required) {
        status.setText(ready ? tr("●  Verificado y listo", "●  Verified and ready", "●  Verificado e pronto")
            : required ? "○  " + kind + tr(" necesario", " required", " necessário")
            : tr("○  No se ha añadido", "○  Not provided", "○  Não adicionado"));
        status.setTextColor(ready ? Color.rgb(120, 235, 171)
                                  : (required ? Color.rgb(255, 211, 78)
                                              : SettingsPanel.SECONDARY));
        button.setText((ready ? tr("Reemplazar ", "Replace ", "Substituir ") : tr("Elegir ", "Choose ", "Escolher ")) + kind);
    }

    private void pickFile(int requestCode) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        // DocumentsUI often gives .bin/.gba vendor-specific MIME types; filter
        // nothing and let the strict size + SHA-1 check decide.
        intent.setType("*/*");
        startActivityForResult(intent, requestCode);
    }

    private void launchGame() {
        if (!ready()) return;
        getSharedPreferences(SETUP_PREFERENCES, MODE_PRIVATE).edit()
            .putBoolean(SKIP_LAUNCHER_ON_BOOT,
                skipLauncherCheck == null || skipLauncherCheck.isChecked())
            .apply();
        startActivity(new Intent(this, gameActivityClass()));
    }

    private boolean skipLauncherOnBoot() {
        return getSharedPreferences(SETUP_PREFERENCES, MODE_PRIVATE)
            .getBoolean(SKIP_LAUNCHER_ON_BOOT, true);
    }

    private boolean hasBundledPrivateAssets() {
        try (InputStream rom = getAssets().open("payload/roms/" + config.romFile)) {
            if (rom.read() < 0) return false;
        } catch (IOException ignored) {
            return false;
        }
        if (!config.biosRequired) return true;
        try (InputStream bios = getAssets().open("payload/bios/gba_bios.bin")) {
            return bios.read() >= 0;
        } catch (IOException ignored) {
            return false;
        }
    }

    private void installVerifiedAsset(Uri uri, boolean bios)
            throws IOException, NoSuchAlgorithmException {
        String displayName = displayName(uri);
        long expectedSize = bios ? BIOS_SIZE : config.romSize;
        File destination = bios ? biosFile() : romFile();
        File directory = destination.getParentFile();
        if (directory != null && !directory.isDirectory() && !directory.mkdirs()) {
            throw new IOException(tr("No se pudo crear la carpeta privada del juego.", "Could not create the private game directory.", "Não foi possível criar a pasta privada do jogo."));
        }
        File temporary = File.createTempFile("import-", ".tmp", directory);
        MessageDigest digest = MessageDigest.getInstance("SHA-1");
        long total = 0;
        try (InputStream input = getContentResolver().openInputStream(uri);
             FileOutputStream output = new FileOutputStream(temporary)) {
            if (input == null) throw new IOException(tr("Android no pudo abrir ", "Android could not open ", "O Android não conseguiu abrir ") + displayName + ".");
            byte[] buffer = new byte[1024 * 1024];
            int read;
            while ((read = input.read(buffer)) >= 0) {
                if (read == 0) continue;
                total += read;
                if (total > expectedSize) throw new IOException(displayName + tr(" es más grande de lo esperado.", " is larger than expected.", " é maior do que o esperado."));
                digest.update(buffer, 0, read);
                output.write(buffer, 0, read);
            }
            output.getFD().sync();
        } catch (IOException error) {
            temporary.delete();
            throw error;
        }
        String actual = toHex(digest.digest());
        boolean accepted = total == expectedSize &&
            (bios ? config.biosSha1.equals(actual) : config.romAccepted(actual));
        if (!accepted) {
            temporary.delete();
            throw new IOException(String.format(Locale.US,
                tr("%s no es la copia original compatible de %s.\n\nTamaño: %,d bytes (esperado: %,d)\nSHA-1: %s",
                   "%s is not the supported clean %s dump.\n\nSize: %,d bytes (expected %,d)\nSHA-1: %s",
                   "%s não é a cópia original compatível de %s.\n\nTamanho: %,d bytes (esperado: %,d)\nSHA-1: %s"),
                displayName, bios ? "GBA BIOS" : config.romLabel, total, expectedSize, actual));
        }
        if (destination.exists() && !destination.delete()) {
            temporary.delete();
            throw new IOException(tr("No se pudo reemplazar el archivo anterior.", "Could not replace the previously imported file.", "Não foi possível substituir o arquivo anterior."));
        }
        if (!temporary.renameTo(destination)) {
            copyFile(temporary, destination);
            temporary.delete();
        }
    }

    private static void copyFile(File source, File destination) throws IOException {
        try (FileInputStream input = new FileInputStream(source);
             FileOutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[1024 * 1024];
            int read;
            while ((read = input.read(buffer)) >= 0) {
                if (read > 0) output.write(buffer, 0, read);
            }
            output.getFD().sync();
        }
    }

    private String displayName(Uri uri) {
        try (android.database.Cursor cursor = getContentResolver().query(
                uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (column >= 0) return cursor.getString(column);
            }
        } catch (Exception ignored) {
        }
        return tr("archivo seleccionado", "selected file", "arquivo selecionado");
    }

    private void setBusy(boolean busy, String label) {
        biosButton.setEnabled(!busy);
        romButton.setEnabled(!busy);
        playButton.setEnabled(!busy && ready());
        if (busy && label != null) {
            infoStatus.setText(label);
            infoStatus.setTextColor(SettingsPanel.ACCENT);
        }
    }

    private void showError(String title, String message) {
        SettingsPanel panel = new SettingsPanel(this, title, "", tr("Entendido", "OK", "Entendido"));
        panel.paragraph(message == null ? tr("No se pudo leer el archivo seleccionado.", "The selected file could not be read.", "Não foi possível ler o arquivo selecionado.") : message);
        AlertDialog dialog = panel.dialog(null);
        dialog.show();
        SettingsPanel.fitWindow(this, dialog);
    }

    private void tintCheckBox(CheckBox checkBox) {
        checkBox.setButtonTintList(new android.content.res.ColorStateList(
            new int[][]{new int[]{android.R.attr.state_checked}, new int[]{}},
            new int[]{SettingsPanel.ACCENT, SettingsPanel.SECONDARY}));
    }

    private void enterImmersiveMode() {
        if (Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    private GradientDrawable roundRect(int color, int radiusDp, int strokeColor) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(radiusDp));
        if (strokeColor != Color.TRANSPARENT) drawable.setStroke(dp(1), strokeColor);
        return drawable;
    }

    private LinearLayout.LayoutParams margins(int width, int height, int left, int top,
                                               int right, int bottom) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(width, height);
        params.setMargins(left, top, right, bottom);
        return params;
    }

    private LinearLayout.LayoutParams weightedMargins(float weight, int left, int top,
                                                       int right, int bottom) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            0, LinearLayout.LayoutParams.WRAP_CONTENT, weight);
        params.setMargins(left, top, right, bottom);
        return params;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private static String toHex(byte[] bytes) {
        StringBuilder value = new StringBuilder(bytes.length * 2);
        for (byte item : bytes) value.append(String.format(Locale.US, "%02x", item & 0xff));
        return value.toString();
    }
}
