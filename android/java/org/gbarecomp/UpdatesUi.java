package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.Intent;
import android.net.Uri;
import android.provider.Settings;
import android.widget.TextView;
import android.widget.Toast;

import java.text.DateFormat;
import java.util.Date;
import java.util.Locale;

/** Foreground UI for detection, bounded download and the Android package installer. */
final class UpdatesUi {
    static final int INSTALL_PERMISSION = 4702;
    interface Translator { String text(String es, String en, String pt); }
    interface Presenter { void open(AlertDialog dialog, Runnable parent); }
    private final Activity activity;
    private final Translator translator;
    private final Presenter presenter;
    private final Runnable parent;
    private final UpdateManager manager;
    private final UpdateManager.Listener listener = this::changed;
    private AlertDialog page;
    private TextView progressLabel;
    private String shownPhase = "";
    private AlertDialog installPage;
    private boolean installResultPending;
    private String installFailure;
    private AlertDialog permissionPage;
    private AlertDialog permissionReturnPage;
    private static final int PERMISSION_NONE = 0, PERMISSION_SHOW = 1,
        PERMISSION_INSTALL = 2, PERMISSION_DENIED = 3;
    private int permissionReturn = PERMISSION_NONE;
    private boolean confirmInstallAgain;
    private boolean closed, resumed;

    UpdatesUi(Activity activity, Translator translator, Presenter presenter, Runnable parent) {
        this.activity = activity; this.translator = translator;
        this.presenter = presenter; this.parent = parent;
        manager = UpdateManager.get(activity);
        manager.addListener(listener);
    }
    private String tr(String es, String en, String pt) { return translator.text(es, en, pt); }
    private boolean alive() { return !closed && !activity.isFinishing() && !activity.isDestroyed(); }
    String summary() {
        UpdateManager.State state = manager.state();
        if (state.asset != null) return tr("Disponible: ", "Available: ", "Disponível: ") + state.asset.version.text;
        return tr("Buscar nuevas versiones en GitHub", "Check GitHub for new versions", "Buscar novas versões no GitHub");
    }
    void onResume() {
        if (!alive()) return;
        resumed = true;
        finishPermissionReturn();
        if (installResultPending) finishInstallCheck();
        maybeNotify(manager.state());
        manager.maybeCheck();
        if (page != null && page.isShowing() && !shownPhase.equals("install_check") && !shownPhase.equals("notice")) show();
    }
    void onPause() { resumed = false; }
    void close() {
        closed = true; resumed = false; manager.removeListener(listener);
        page = null; installPage = null; permissionPage = null; permissionReturnPage = null; progressLabel = null;
        permissionReturn = PERMISSION_NONE;
    }

    private void maybeNotify(UpdateManager.State state) {
        if (alive() && resumed && manager.automatic() && state.asset != null &&
                !state.busy() && manager.takeNotification(state.asset.id)) {
            Toast.makeText(activity, tr("Nueva versión ", "New version ", "Nova versão ") + state.asset.version.text +
                tr(" disponible. Abre Ajustes → Actualizar.", " available. Open Settings → Updates.",
                   " disponível. Abra Configurações → Atualizar."), Toast.LENGTH_LONG).show();
        }
    }

    private void changed(UpdateManager.State state) {
        if (!alive() || !resumed) return;
        maybeNotify(state);
        // Never reopen a closed page over the game or another settings section.
        if (page == null || !page.isShowing()) return;
        if (shownPhase.equals("install_check") || shownPhase.equals("notice")) return;
        if (state.phase.equals("downloading") && shownPhase.equals("downloading") && progressLabel != null) {
            progressLabel.setText(progress(state)); return;
        }
        show();
    }

    private String progress(UpdateManager.State state) {
        long total = state.asset == null ? 0 : state.asset.size;
        int percent = total == 0 ? 0 : (int) (100 * state.received / total);
        return tr("Descargando… ", "Downloading… ", "Baixando… ") + percent + "%\n" +
            String.format(Locale.getDefault(), "%.1f / %.1f MB", state.received / 1000000.0, total / 1000000.0);
    }

    void show() {
        if (!alive()) return;
        UpdateManager.State state = manager.state();
        SettingsPanel panel = new SettingsPanel(activity, tr("Actualizaciones", "Updates", "Atualizações"),
            tr("Versión instalada: ", "Installed version: ", "Versão instalada: ") + manager.installedName(),
            tr("Volver", "Back", "Voltar"));
        shownPhase = state.phase; progressLabel = null;
        panel.toggle(tr("Buscar automáticamente", "Check automatically", "Buscar automaticamente"),
            tr("Comprueba GitHub al abrir o volver al juego, como máximo cada 6 horas.",
               "Checks GitHub when opening or returning to the game, at most every 6 hours.",
               "Verifica o GitHub ao abrir ou voltar ao jogo, no máximo a cada 6 horas."),
            manager.automatic(), manager::automatic);
        if (confirmInstallAgain) panel.notice(tr("Continuar actualización", "Continue update", "Continuar atualização"),
            tr("La pantalla se ha vuelto a crear. Pulsa Instalar actualización para continuar.",
               "This screen was recreated. Tap Install update to continue.",
               "A tela foi recriada. Toque em Instalar atualização para continuar."));
        if (state.phase.equals("checking")) {
            panel.paragraph(tr("Buscando nuevas versiones…", "Checking for new versions…", "Buscando novas versões…"));
        } else if (state.phase.equals("downloading")) {
            progressLabel = panel.notice(tr("Descarga en curso", "Download in progress", "Download em andamento"), "");
            progressLabel.setText(progress(state));
            panel.card(SettingsPanel.UPDATES, tr("Cancelar descarga", "Cancel download", "Cancelar download"),
                tr("Se eliminará el archivo parcial.", "The partial file will be removed.", "O arquivo parcial será removido."), manager::cancelDownload);
            panel.paragraph(tr("Puedes volver al juego mientras se descarga. Regresa aquí para instalarla.",
                "You can return to the game while downloading. Come back here to install it.",
                "Você pode voltar ao jogo durante o download. Volte aqui para instalar."));
        } else if (state.phase.equals("verifying")) {
            panel.paragraph(tr("Verificando archivo, versión y firma…", "Verifying file, version and signature…", "Verificando arquivo, versão e assinatura…"));
        } else {
            if (!state.error.isEmpty()) panel.notice(tr("Aviso", "Notice", "Aviso"), error(state.error));
            if (state.asset != null) {
                panel.section(tr("Nueva versión disponible", "New version available", "Nova versão disponível"));
                panel.paragraph(state.asset.version.text + " · " +
                    String.format(Locale.getDefault(), "%.1f MB", state.asset.size / 1000000.0));
            } else if (state.phase.equals("current")) {
                panel.paragraph(tr("No hay un APK compatible más nuevo en Descarga.",
                    "There is no newer compatible APK in Descarga.", "Não há um APK compatível mais recente em Descarga."));
            }
            UpdatePolicy.Asset ready = manager.readyAsset();
            if (ready != null) {
                panel.card(SettingsPanel.UPDATES, tr("Instalar actualización", "Install update", "Instalar atualização"),
                    ready.version.text + " · " + tr("Android pedirá confirmar la instalación. Tus datos se conservan al actualizar.",
                       "Android will ask you to confirm installation. Updating preserves your data.",
                       "O Android pedirá confirmação da instalação. A atualização mantém seus dados."), this::requestInstall);
            }
            if (state.asset != null) {
                panel.card(SettingsPanel.UPDATES, tr("Descargar actualización", "Download update", "Baixar atualização"),
                    state.asset.name, manager::download);
            }
            panel.card(SettingsPanel.UPDATES, tr("Buscar ahora", "Check now", "Buscar agora"),
                tr("Consultar la release Descarga en GitHub.", "Check the Descarga release on GitHub.",
                   "Consultar a release Descarga no GitHub."), manager::checkNow);
        }
        if (state.checkedAt > 0) panel.paragraph(tr("Última comprobación: ", "Last checked: ", "Última verificação: ") +
            DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT).format(new Date(state.checkedAt)));
        panel.section(tr("Origen de las actualizaciones", "Update source", "Origem das atualizações"));
        panel.paragraph("GitHub · Edwin2929/sma3-recomp-android. · Descarga");
        panel.paragraph(tr("La detección es automática; la descarga y la instalación se inician desde este apartado. Android puede pedir permitir instalaciones desde esta aplicación y siempre muestra su confirmación.",
            "Detection is automatic; start the download and installation here. Android may ask you to allow installs from this app and always shows its confirmation.",
            "A detecção é automática; inicie o download e a instalação aqui. O Android pode pedir permissão para instalar por este aplicativo e sempre mostra a confirmação."));
        page = panel.dialog(parent);
        presenter.open(page, parent);
    }

    private void requestInstall() {
        if (!alive() || !resumed || page == null || !page.isShowing()) return;
        confirmInstallAgain = false;
        if (!activity.getPackageManager().canRequestPackageInstalls()) {
            // Keep a token only in this Activity instance and tie it to the
            // page that received the explicit Install tap.
            permissionPage = page;
            try {
                activity.startActivityForResult(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                    Uri.parse("package:" + activity.getPackageName())), INSTALL_PERMISSION);
            } catch (ActivityNotFoundException | SecurityException error) {
                permissionPage = null; notice("install_permission");
            }
            return;
        }
        verifyThenInstall();
    }

    boolean onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode != INSTALL_PERMISSION) return false;
        if (!alive()) return true;
        AlertDialog origin = permissionPage;
        permissionPage = null;
        if (origin != null) {
            if (origin != page || !origin.isShowing()) return true;
            permissionReturnPage = origin;
            permissionReturn = activity.getPackageManager().canRequestPackageInstalls() ?
                PERMISSION_INSTALL : PERMISSION_DENIED;
        } else if (page == null) {
            // Android may recreate the Activity while Settings is open. A
            // result alone does not restore an installation request token.
            permissionReturn = PERMISSION_SHOW;
        } else return true;
        // Activity results arrive before onResume. Defer even the dialog;
        // never open a window or installer from a paused Activity.
        if (resumed) finishPermissionReturn();
        return true;
    }

    private void finishPermissionReturn() {
        if (!alive() || !resumed) return;
        int action = permissionReturn;
        permissionReturn = PERMISSION_NONE;
        AlertDialog origin = permissionReturnPage;
        permissionReturnPage = null;
        if (action != PERMISSION_NONE && action != PERMISSION_SHOW &&
                (origin == null || origin != page || !origin.isShowing())) return;
        if (action == PERMISSION_INSTALL) verifyThenInstall();
        else if (action == PERMISSION_DENIED) notice("install_permission");
        else if (action == PERMISSION_SHOW) { confirmInstallAgain = true; show(); }
    }

    private void verifyThenInstall() {
        if (!alive() || !resumed) return;
        SettingsPanel loading = new SettingsPanel(activity, tr("Actualizaciones", "Updates", "Atualizações"),
            tr("Verificando antes de instalar…", "Verifying before installation…", "Verificando antes de instalar…"),
            tr("Volver", "Back", "Voltar"));
        final AlertDialog pending = loading.dialog(parent);
        page = pending; installPage = pending; shownPhase = "install_check";
        installResultPending = false; installFailure = null;
        presenter.open(pending, parent);
        manager.prepareInstall((verified, problem) -> {
            if (!alive() || !pending.isShowing()) return;
            installResultPending = true;
            installFailure = verified ? null : problem;
            if (resumed) finishInstallCheck();
        });
    }

    private void finishInstallCheck() {
        if (!installResultPending || !alive() || !resumed) return;
        installResultPending = false;
        if (installPage == null || page != installPage || !installPage.isShowing()) return;
        installPage = null;
        if (installFailure != null) { notice(installFailure); return; }
        Uri uri = UpdateApkProvider.uri(activity);
        Intent install = new Intent(Intent.ACTION_VIEW).setDataAndType(uri, "application/vnd.android.package-archive")
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        install.setClipData(ClipData.newRawUri("SMA3 update", uri));
        try { activity.startActivity(install); }
        catch (ActivityNotFoundException | SecurityException error) { notice("installer"); return; }
        // Leave a usable page underneath the system installer, including when
        // the user cancels. A permission return can arrive before onResume;
        // defer installation until this Activity is actually foreground.
        show();
    }

    private void notice(String kind) {
        if (!alive()) return;
        SettingsPanel panel = new SettingsPanel(activity, tr("Actualizaciones", "Updates", "Atualizações"),
            error(kind), tr("Volver", "Back", "Voltar"));
        shownPhase = "notice";
        page = panel.dialog(this::show);
        presenter.open(page, this::show);
    }

    private String error(String kind) {
        if ("cancelled".equals(kind)) return tr("Descarga cancelada. Puedes volver a intentarlo.", "Download cancelled. You can try again.", "Download cancelado. Você pode tentar novamente.");
        if ("rate".equals(kind)) return tr("GitHub ha limitado temporalmente las consultas. Vuelve a intentarlo más tarde.", "GitHub temporarily limited requests. Try again later.", "O GitHub limitou as consultas temporariamente. Tente mais tarde.");
        if ("missing".equals(kind)) return tr("No se encontró la release Descarga o su archivo. El mantenedor puede estar reemplazándolo.", "The Descarga release or its file was not found. The maintainer may be replacing it.", "A release Descarga ou seu arquivo não foi encontrado. O mantenedor pode estar substituindo-o.");
        if ("digest".equals(kind) || "size".equals(kind)) return tr("La descarga está incompleta o no coincide con el archivo publicado. No se instalará; vuelve a descargarla.", "The download is incomplete or does not match the published file. It will not be installed; download again.", "O download está incompleto ou não corresponde ao arquivo publicado. Não será instalado; baixe novamente.");
        if ("signature".equals(kind)) return tr("La firma del APK no coincide con esta aplicación. La actualización se ha rechazado.", "The APK signature does not match this app. The update was rejected.", "A assinatura do APK não corresponde a este aplicativo. A atualização foi rejeitada.");
        if ("package".equals(kind) || "platform".equals(kind) || "version".equals(kind) || "older".equals(kind)) return tr("El APK no es una actualización compatible de esta aplicación: comprueba el paquete, la versión, Android y ARM64.", "The APK is not a compatible update for this app: check its package, version, Android requirement and ARM64 support.", "O APK não é uma atualização compatível deste aplicativo: confira pacote, versão, Android e ARM64.");
        if ("install_permission".equals(kind)) return tr("Permite instalar aplicaciones desde SMA3 en la pantalla de Android y pulsa Instalar actualización otra vez. Puedes cancelar y seguir jugando.", "Allow installs from SMA3 in Android settings and tap Install update again. You can cancel and keep playing.", "Permita instalar por SMA3 nas configurações do Android e toque em Instalar atualização novamente. Você pode cancelar e continuar jogando.");
        if ("installer".equals(kind)) return tr("No se pudo abrir el instalador de Android. Comprueba las restricciones del dispositivo.", "The Android installer could not be opened. Check device restrictions.", "Não foi possível abrir o instalador do Android. Confira as restrições do dispositivo.");
        if ("storage".equals(kind) || "permission".equals(kind)) return tr("No se pudo guardar la descarga. Comprueba el espacio disponible y vuelve a intentarlo.", "The download could not be saved. Check free space and try again.", "Não foi possível salvar o download. Confira o espaço livre e tente novamente.");
        if ("missing_download".equals(kind)) return tr("El archivo descargado ya no está disponible. Descarga la actualización otra vez.", "The downloaded file is no longer available. Download the update again.", "O arquivo baixado não está mais disponível. Baixe a atualização novamente.");
        if ("metadata".equals(kind) || "source".equals(kind)) return tr("La respuesta de GitHub o la dirección de descarga no es válida. No se ha instalado ningún archivo.", "The GitHub response or download address is invalid. No file was installed.", "A resposta do GitHub ou o endereço de download não é válido. Nenhum arquivo foi instalado.");
        if ("busy".equals(kind)) return tr("Espera a que termine la operación actual y vuelve a intentarlo.", "Wait for the current operation and try again.", "Espere a operação atual terminar e tente novamente.");
        return tr("No se pudo conectar con GitHub. Comprueba Internet y vuelve a intentarlo.", "Could not connect to GitHub. Check your connection and try again.", "Não foi possível conectar ao GitHub. Confira a conexão e tente novamente.");
    }
}
