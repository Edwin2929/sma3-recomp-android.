package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;

/** Android document picker and native-backed management of SMA3 color mods. */
final class ModsUi {
    static final int PICK_MOD = 4701;
    private static final long MAX_ARCHIVE_BYTES = 2L * 1024 * 1024;
    private static final String EXAMPLE_ASSET = "mod-examples/tono-calido.gbamod";

    interface Translator { String text(String es, String en, String pt); }
    interface Presenter { void open(AlertDialog dialog, Runnable parent); }
    private interface Task { String run() throws Exception; }

    private final Activity activity;
    private final Translator translator;
    private final Presenter presenter;
    private final Runnable parent;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private volatile boolean closed;
    private List<Mod> lastMods = Collections.emptyList();
    private String pendingWarning = "";
    private String shownWarning = "";
    private YoshiColorUi yoshiColor;

    ModsUi(Activity activity, Translator translator, Presenter presenter, Runnable parent) {
        this.activity = activity;
        this.translator = translator;
        this.presenter = presenter;
        this.parent = parent;
    }

    private String tr(String es, String en, String pt) {
        return translator.text(es, en, pt);
    }

    private boolean alive() {
        return !closed && !activity.isFinishing() && !activity.isDestroyed();
    }

    private SettingsPanel panel(String title, String subtitle) {
        return new SettingsPanel(activity, title, subtitle, tr("Volver", "Back", "Voltar"));
    }

    private AlertDialog open(SettingsPanel panel, Runnable back) {
        AlertDialog dialog = panel.dialog(back);
        presenter.open(dialog, back);
        return dialog;
    }

    void show() {
        runTask(tr("Cargando mods…", "Loading mods…", "Carregando mods…"),
            GbaNative::listMods, null);
    }

    private void runTask(String progress, Task action, String success) {
        if (!alive()) return;
        SettingsPanel loading = panel("Mods", progress);
        loading.paragraph(tr("Puedes volver a Ajustes mientras termina la operación.",
            "You can return to Settings while the operation finishes.",
            "Você pode voltar às configurações enquanto a operação termina."));
        final AlertDialog pending = open(loading, parent);
        try {
            worker.execute(() -> {
                Result result = null;
                String errorKind = null;
                try {
                    if (closed || Thread.currentThread().isInterrupted()) return;
                    result = parse(action.run());
                } catch (ModArchiveCopy.TooLargeException error) {
                    errorKind = "size";
                } catch (SecurityException error) {
                    errorKind = "permission";
                } catch (UnsatisfiedLinkError error) {
                    errorKind = "native";
                } catch (Exception error) {
                    errorKind = "read";
                }
                final Result response = result;
                final String failure = errorKind;
                activity.runOnUiThread(() -> {
                    if (!alive()) return;
                    // A response must not reopen Mods over another page or the
                    // game after the player has left the progress page.
                    if (response != null) {
                        lastMods = response.mods;
                        if (!response.warning.isEmpty() && !response.warning.equals(shownWarning))
                            pendingWarning = response.warning;
                    }
                    if (!pending.isShowing()) return;
                    if (response == null) showCatalog(lastMods, failureMessage(failure), false);
                    else if (!response.ok) showCatalog(lastMods,
                        response.error.isEmpty() ? failureMessage("read") : response.error, false);
                    else showCatalog(lastMods, success, true);
                });
            });
        } catch (RejectedExecutionException ignored) {
            if (alive()) showCatalog(lastMods, failureMessage("read"), false);
        }
    }

    private String failureMessage(String kind) {
        if ("size".equals(kind)) return tr(
            "El archivo supera el límite de 2 MiB. Elige un paquete .gbamod más pequeño.",
            "The file exceeds the 2 MiB limit. Choose a smaller .gbamod package.",
            "O arquivo ultrapassa o limite de 2 MiB. Escolha um pacote .gbamod menor.");
        if ("permission".equals(kind)) return tr(
            "No se pudo abrir el archivo elegido. Vuelve a seleccionarlo y permite su lectura.",
            "The selected file could not be opened. Select it again and allow read access.",
            "Não foi possível abrir o arquivo escolhido. Selecione-o novamente e permita a leitura.");
        if ("native".equals(kind)) return tr(
            "Espera a que aparezca el juego y vuelve a abrir Mods.",
            "Wait for the game to appear, then open Mods again.",
            "Espere o jogo aparecer e abra Mods novamente.");
        return tr("No se pudo completar la operación. Comprueba el archivo y vuelve a intentarlo.",
            "The operation could not be completed. Check the file and try again.",
            "Não foi possível concluir a operação. Confira o arquivo e tente novamente.");
    }

    private void showCatalog(List<Mod> mods, String notice, boolean success) {
        if (!alive()) return;
        SettingsPanel page = panel("Mods", tr(
            "Personaliza a Yoshi y los colores de la imagen.",
            "Customize Yoshi and the colors of the image.",
            "Personalize o Yoshi e as cores da imagem."));
        if (notice != null && !notice.isEmpty()) {
            if (success) page.paragraph(notice);
            else page.notice(tr("No se pudo completar", "Could not complete", "Não foi possível concluir"), notice);
        }
        if (!pendingWarning.isEmpty()) {
            page.notice(tr("Aviso de compatibilidad", "Compatibility notice", "Aviso de compatibilidade"), pendingWarning);
            shownWarning = pendingWarning;
            pendingWarning = "";
        }
        page.section(tr("Incorporado", "Built-in", "Integrado"));
        YoshiColorModel.Selection color = YoshiColorUi.read(activity);
        page.card(SettingsPanel.MODS, tr("Color de Yoshi", "Yoshi color", "Cor do Yoshi"),
            (color.enabled ? tr("Activo · ", "Active · ", "Ativo · ") + YoshiColorModel.hex(color.rgb)
                : tr("Color original", "Original color", "Cor original"))
                + tr(" · Paleta ampliada, RGB y códigos HEX", " · Expanded palette, RGB and HEX codes", " · Paleta ampliada, RGB e códigos HEX"),
            this::showYoshiColor);
        page.section(tr("Perfiles de toda la imagen", "Whole-image profiles", "Perfis de toda a imagem"));
        page.card(SettingsPanel.MODS, tr("Importar mod", "Import mod", "Importar mod"),
            tr("Elegir un archivo .gbamod · máximo 2 MiB", "Choose a .gbamod file · up to 2 MiB", "Escolher um arquivo .gbamod · até 2 MiB"),
            this::pickDocument);
        page.card(SettingsPanel.MODS, tr("Instalar ejemplo", "Install example", "Instalar exemplo"),
            tr("Tono cálido · se añade desactivado para que puedas probarlo.",
               "Warm tone · added disabled so you can try it when ready.",
               "Tom quente · adicionado desativado para você experimentar."),
            this::installExample);
        page.section(tr("Instalados", "Installed", "Instalados") + " · " + mods.size());
        if (mods.isEmpty()) page.paragraph(tr(
            "Todavía no hay mods instalados. Importa un paquete o instala el ejemplo.",
            "No mods are installed yet. Import a package or install the example.",
            "Ainda não há mods instalados. Importe um pacote ou instale o exemplo."));
        for (Mod mod : mods) {
            page.card(SettingsPanel.MODS, mod.name,
                (!mod.compatible ? tr("No compatible", "Incompatible", "Incompatível")
                    : mod.enabled ? tr("Activo", "Active", "Ativo") : tr("Desactivado", "Disabled", "Desativado"))
                    + " · " + mod.version + (mod.author.isEmpty() ? "" : " · " + mod.author),
                () -> showDetails(mod));
        }
        page.section(tr("Compatibilidad", "Compatibility", "Compatibilidade"));
        page.paragraph(tr(
            "Color de Yoshi viene incorporado y no necesita importar un archivo. Los paquetes .gbamod admiten perfiles de toda la imagen para SMA3. Los parches de ROM y los mods de otros juegos no son compatibles.",
            "Yoshi color is built in and needs no imported file. The .gbamod packages support whole-image profiles for SMA3. ROM patches and mods for other games are not compatible.",
            "Cor do Yoshi vem integrado e não precisa importar um arquivo. Os pacotes .gbamod aceitam perfis de toda a imagem para SMA3. Patches de ROM e mods de outros jogos não são compatíveis."));
        page.paragraph(tr(
            "Solo se usa un perfil de imagen a la vez. Activar otro sustituye al anterior. Color de Yoshi se puede usar junto con ese perfil; se configura y desactiva por separado. Los cambios se aplican sin reiniciar y quedan guardados.",
            "One image profile is used at a time. Enabling another replaces the previous one. Yoshi color can be used alongside that profile; configure or disable it separately. Changes apply without restarting and are saved.",
            "Um perfil de imagem é usado por vez. Ativar outro substitui o anterior. Cor do Yoshi pode ser usado junto com esse perfil; configure ou desative separadamente. As mudanças são aplicadas sem reiniciar e ficam salvas."));
        open(page, parent);
    }

    private void showYoshiColor() {
        if (!alive()) return;
        if (yoshiColor != null) yoshiColor.close();
        yoshiColor = new YoshiColorUi(activity, translator, presenter,
            () -> showCatalog(lastMods, null, true),
            message -> showCatalog(lastMods, message, true));
        yoshiColor.show();
    }

    private void showDetails(Mod mod) {
        if (!alive()) return;
        SettingsPanel page = panel(mod.name,
            tr("Versión ", "Version ", "Versão ") + mod.version
                + (mod.author.isEmpty() ? "" : " · " + mod.author));
        if (!mod.description.isEmpty()) page.paragraph(mod.description);
        if (!mod.compatible) page.notice(tr("Mod no compatible", "Incompatible mod", "Mod incompatível"),
            mod.status.isEmpty() ? tr("Este perfil no puede activarse. Puedes eliminarlo e importar un paquete compatible.",
                "This profile cannot be enabled. You can remove it and import a compatible package.",
                "Este perfil não pode ser ativado. Você pode removê-lo e importar um pacote compatível.") : mod.status);
        else page.toggle(tr("Activar mod", "Enable mod", "Ativar mod"),
            tr("Se aplica al momento. Solo puede haber un perfil de color activo.",
               "Applies immediately. Only one color profile can be active.",
               "Aplicado imediatamente. Apenas um perfil de cor pode ficar ativo."),
            mod.enabled, enabled -> runTask(
                enabled ? tr("Activando mod…", "Enabling mod…", "Ativando mod…")
                        : tr("Desactivando mod…", "Disabling mod…", "Desativando mod…"),
                () -> GbaNative.setModEnabled(mod.id, enabled),
                enabled ? tr("Mod activado.", "Mod enabled.", "Mod ativado.")
                        : tr("Mod desactivado.", "Mod disabled.", "Mod desativado.")));
        page.section(tr("Administrar", "Manage", "Gerenciar"));
        page.card(SettingsPanel.MODS, tr("Eliminar mod", "Remove mod", "Excluir mod"),
            tr("Quita este paquete de la aplicación.", "Remove this package from the app.", "Remove este pacote do aplicativo."),
            () -> confirmRemoval(mod));
        open(page, () -> showCatalog(lastMods, null, true));
    }

    private void confirmRemoval(Mod mod) {
        SettingsPanel page = new SettingsPanel(activity,
            tr("Eliminar mod", "Remove mod", "Excluir mod"),
            "«" + mod.name + "»", tr("Cancelar", "Cancel", "Cancelar"));
        page.paragraph(tr(
            "Este perfil se desactivará y se quitará de la aplicación. Puedes volver a importarlo cuando quieras.",
            "This profile will be disabled and removed from the app. You can import it again whenever you want.",
            "Este perfil será desativado e removido do aplicativo. Você poderá importá-lo novamente quando quiser."));
        page.card(SettingsPanel.MODS, tr("Eliminar este mod", "Remove this mod", "Excluir este mod"),
            mod.name, () -> runTask(tr("Eliminando mod…", "Removing mod…", "Excluindo mod…"),
                () -> GbaNative.removeMod(mod.id), tr("Mod eliminado.", "Mod removed.", "Mod excluído.")));
        open(page, () -> showDetails(mod));
    }

    private void pickDocument() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        // Providers assign vendor-specific types to .gbamod; native validation
        // verifies the archive and its supported profile instead of trusting MIME.
        intent.setType("*/*");
        try { activity.startActivityForResult(intent, PICK_MOD); }
        catch (ActivityNotFoundException error) {
            showCatalog(lastMods, tr("No hay un selector de archivos disponible.",
                "No file picker is available.", "Não há um seletor de arquivos disponível."), false);
        }
    }

    boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != PICK_MOD) return false;
        if (!alive()) return true;
        if (resultCode != Activity.RESULT_OK || data == null || data.getData() == null) {
            show();
            return true;
        }
        final Uri uri = data.getData();
        runTask(tr("Importando mod…", "Importing mod…", "Importando mod…"), () -> {
            try (InputStream input = activity.getContentResolver().openInputStream(uri)) {
                if (input == null) throw new IOException("Document provider returned no stream");
                return importStream(input);
            }
        }, tr("Mod importado y desactivado. Toca su nombre para activarlo.",
              "Mod imported and disabled. Tap its name to enable it.",
              "Mod importado e desativado. Toque no nome para ativá-lo."));
        return true;
    }

    private void installExample() {
        runTask(tr("Instalando ejemplo…", "Installing example…", "Instalando exemplo…"), () -> {
            try (InputStream input = activity.getAssets().open(EXAMPLE_ASSET)) {
                return importStream(input);
            }
        }, tr("Ejemplo instalado y desactivado. Toca su nombre para probarlo.",
              "Example installed and disabled. Tap its name to try it.",
              "Exemplo instalado e desativado. Toque no nome para experimentar."));
    }

    private String importStream(InputStream input) throws IOException {
        File cached = File.createTempFile("sma3-mod-", ".gbamod", activity.getCacheDir());
        try {
            try (FileOutputStream output = new FileOutputStream(cached)) {
                ModArchiveCopy.copy(input, output, MAX_ARCHIVE_BYTES);
            }
            if (closed || Thread.currentThread().isInterrupted()) throw new IOException("Import cancelled");
            return GbaNative.importMod(cached.getAbsolutePath());
        } finally {
            // The engine owns validated installed bytes; the picker copy is
            // temporary even when parsing, validation, or installation fails.
            if (!cached.delete()) cached.deleteOnExit();
        }
    }

    private static Result parse(String json) throws JSONException {
        if (json == null || json.length() > 1024 * 1024) throw new JSONException("Invalid mod response");
        JSONObject object = new JSONObject(json);
        boolean ok = object.getBoolean("ok");
        String error = bounded(object.optString("error", ""), 2048);
        String warning = bounded(object.optString("warning", ""), 2048);
        JSONArray array = object.getJSONArray("mods");
        if (array.length() > 1024) throw new JSONException("Too many mods");
        List<Mod> mods = new ArrayList<>();
        for (int i = 0; i < array.length(); ++i) {
            JSONObject item = array.getJSONObject(i);
            String id = item.getString("id");
            if (id.isEmpty() || id.length() > 128) throw new JSONException("Invalid mod ID");
            mods.add(new Mod(id, bounded(item.getString("name"), 160),
                bounded(item.getString("version"), 80), bounded(item.optString("author", ""), 160),
                bounded(item.optString("description", ""), 4096), item.getBoolean("enabled"),
                item.optBoolean("compatible", true), bounded(item.optString("status", ""), 2048)));
        }
        return new Result(ok, error, warning, Collections.unmodifiableList(mods));
    }

    private static String bounded(String value, int maximum) {
        if (value.length() <= maximum) return value;
        int end = maximum;
        if (end > 0 && Character.isHighSurrogate(value.charAt(end - 1))) --end;
        return value.substring(0, end) + "…";
    }

    void close() {
        closed = true;
        if (yoshiColor != null) yoshiColor.close();
        worker.shutdownNow();
    }

    private static final class Result {
        final boolean ok;
        final String error, warning;
        final List<Mod> mods;
        Result(boolean ok, String error, String warning, List<Mod> mods) {
            this.ok = ok; this.error = error; this.warning = warning; this.mods = mods;
        }
    }

    private static final class Mod {
        final String id, name, version, author, description, status;
        final boolean enabled, compatible;
        Mod(String id, String name, String version, String author, String description, boolean enabled,
                boolean compatible, String status) {
            this.id = id; this.name = name; this.version = version;
            this.author = author; this.description = description; this.enabled = enabled;
            this.compatible = compatible; this.status = status;
        }
    }
}
