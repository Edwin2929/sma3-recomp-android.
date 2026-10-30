package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;
import java.util.function.Consumer;

/** Reviewable local draft. Sharing and GitHub navigation require an explicit tap. */
final class BugReportUi {
    interface Translator { String text(String es, String en, String pt); }
    interface Presenter { void open(AlertDialog dialog, Runnable parent); }
    interface RuntimeSource { RuntimeInfo capture(); }

    static final class RuntimeInfo {
        final boolean gameScreen, nativeReady;
        final PresentationStats latest;
        final Boolean doubleRateActive;
        RuntimeInfo(boolean nativeReady, PresentationStats latest, Boolean doubleRateActive) {
            this(true, nativeReady, latest, doubleRateActive);
        }
        private RuntimeInfo(boolean gameScreen, boolean nativeReady, PresentationStats latest, Boolean doubleRateActive) {
            this.gameScreen = gameScreen; this.nativeReady = nativeReady;
            this.latest = latest; this.doubleRateActive = doubleRateActive;
        }
        static RuntimeInfo setup() { return new RuntimeInfo(false, false, null, null); }
    }

    private final Activity activity;
    private final Translator translator;
    private final Presenter presenter;
    private final Runnable parent;
    private final RuntimeSource runtime;
    private final ExecutorService worker = Executors.newSingleThreadExecutor();
    private AlertDialog page;
    private AlertDialog loadingPage;
    private volatile boolean closed;
    private boolean resumed = true;
    private int generation;
    private Pending pending;
    private String title = "", happened = "", steps = "", expected = "";
    private String report;
    private String version = "";
    private int reportLanguage = 1;

    BugReportUi(Activity activity, Translator translator, Presenter presenter, Runnable parent) {
        this(activity, translator, presenter, parent, RuntimeInfo::setup);
    }

    BugReportUi(Activity activity, Translator translator, Presenter presenter, Runnable parent, RuntimeSource runtime) {
        this.activity = activity; this.translator = translator; this.presenter = presenter;
        this.parent = parent; this.runtime = runtime;
    }

    private String tr(String es, String en, String pt) { return translator.text(es, en, pt); }
    private boolean alive() { return !closed && !activity.isFinishing() && !activity.isDestroyed(); }
    private boolean showing() { return alive() && page != null && page.isShowing(); }
    private int dp(int value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }
    private String back() { return tr("Volver", "Back", "Voltar"); }

    void onResume() { resumed = true; finishPreparing(); }
    void onPause() { resumed = false; }
    void close() {
        closed = true; resumed = false; ++generation; pending = null;
        worker.shutdownNow(); page = null; loadingPage = null;
    }

    /** Reopening keeps the last draft, including edits made directly in its preview. */
    void show() {
        if (!alive()) return;
        if (report == null) showForm(); else showPreview();
    }

    private SettingsPanel panel(String heading, String description, String navigation) {
        return new SettingsPanel(activity, heading, description, navigation);
    }

    private void open(SettingsPanel panel, Runnable navigation, Runnable backAction) {
        page = panel.dialog(navigation);
        presenter.open(page, backAction);
        if (page.getWindow() != null) page.getWindow().setSoftInputMode(
            WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE | WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
    }

    private void leave() {
        hideKeyboard();
        if (parent != null) parent.run(); else if (page != null) page.dismiss();
    }

    private void showForm() {
        if (!alive()) return;
        SettingsPanel form = panel(tr("Reportar un error", "Report a bug", "Relatar um erro"),
            tr("Describe el problema y revisa el informe antes de compartirlo.",
               "Describe the problem and review the report before sharing it.",
               "Descreva o problema e revise o relatório antes de compartilhá-lo."),
            report == null ? tr("Vista previa", "Preview", "Prévia")
                           : tr("Actualizar vista previa", "Update preview", "Atualizar prévia"));
        form.secondaryNavigation(back(), this::leave);
        form.content(editor(tr("Título breve", "Short title", "Título curto"), title,
            BugReportModel.MAX_TITLE, false, value -> title = value));
        form.content(editor(tr("Qué pasó", "What happened", "O que aconteceu"), happened,
            BugReportModel.MAX_SECTION, true, value -> happened = value));
        form.content(editor(tr("Pasos para reproducirlo", "Steps to reproduce", "Passos para reproduzir"), steps,
            BugReportModel.MAX_SECTION, true, value -> steps = value));
        form.content(editor(tr("Qué esperabas", "What you expected", "O que você esperava"), expected,
            BugReportModel.MAX_SECTION, true, value -> expected = value));
        form.paragraph(tr(
            "Se añaden la versión, el modelo del teléfono, Android y los ajustes utilizados. Podrás editar todo el texto. No se adjuntan archivos del juego ni registros privados.",
            "The version, phone model, Android and current settings are added. You can edit all of the text. Game files and private logs are not attached.",
            "A versão, o modelo do telefone, o Android e as configurações usadas são adicionados. Você pode editar todo o texto. Arquivos do jogo e registros privados não são anexados."));
        if (report != null) form.paragraph(tr(
            "Actualizar vuelve a generar el informe con esta descripción y los ajustes actuales, reemplazando las ediciones de la vista previa.",
            "Updating generates the report again from this description and current settings, replacing edits made in the preview.",
            "Atualizar gera o relatório novamente com esta descrição e as configurações atuais, substituindo as edições da prévia."));
        open(form, this::prepare, this::leave);
    }

    private void prepare() {
        if (!showing() || !resumed) return;
        hideKeyboard();
        RuntimeInfo current;
        try { current = runtime.capture(); }
        catch (RuntimeException unavailable) { current = RuntimeInfo.setup(); }
        if (current == null) current = RuntimeInfo.setup();
        final RuntimeInfo info = current;
        final BugReportModel.Snapshot snapshot = BugReportCollector.capture(activity, info);
        final int language = BugReportCollector.preferences(activity).number("menu-language", 1);
        final String textHappened = happened, textSteps = steps, textExpected = expected;
        final int token = ++generation;
        pending = null;
        SettingsPanel loading = panel(tr("Preparando informe", "Preparing report", "Preparando relatório"),
            tr("Leyendo ajustes del port…", "Reading port settings…", "Lendo configurações do port…"), back());
        open(loading, this::leave, this::leave);
        loadingPage = page;
        final AlertDialog origin = loadingPage;
        try {
            worker.execute(() -> {
                if (closed || Thread.currentThread().isInterrupted()) return;
                BugReportModel.Snapshot complete = BugReportCollector.profiles(snapshot, info.nativeReady);
                activity.runOnUiThread(() -> {
                    if (!alive() || generation != token || page != origin || !origin.isShowing()) return;
                    pending = new Pending(complete, language, textHappened, textSteps, textExpected);
                    finishPreparing();
                });
            });
        } catch (RejectedExecutionException unavailable) {
            if (alive() && page == origin && origin.isShowing()) {
                pending = new Pending(snapshot, language, textHappened, textSteps, textExpected);
                finishPreparing();
            }
        }
    }

    private void finishPreparing() {
        if (!alive() || !resumed || pending == null || page != loadingPage || !page.isShowing()) return;
        Pending ready = pending; pending = null; loadingPage = null;
        reportLanguage = ready.language;
        version = ready.snapshot.values.get(BugReportModel.Field.APP);
        report = BugReportModel.render(reportLanguage, ready.snapshot, ready.happened, ready.steps, ready.expected);
        showPreview();
    }

    private void showPreview() {
        if (!alive()) return;
        SettingsPanel preview = panel(tr("Revisar informe", "Review report", "Revisar relatório"),
            tr("Este es el texto que se copiará o compartirá. Puedes editarlo antes de continuar.",
               "This is the text that will be copied or shared. You can edit it before continuing.",
               "Este é o texto que será copiado ou compartilhado. Você pode editá-lo antes de continuar."), back());
        preview.content(editor(tr("Título", "Title", "Título"),
            BugReportModel.title(reportLanguage, title, version), BugReportModel.MAX_TITLE, false, value -> title = value));
        preview.content(editor(tr("Informe completo", "Complete report", "Relatório completo"),
            report, BugReportModel.MAX_REPORT, true, value -> report = value));
        preview.card(SettingsPanel.BUG_REPORT, tr("Copiar informe", "Copy report", "Copiar relatório"),
            tr("Copiar el texto revisado al portapapeles.", "Copy the reviewed text to the clipboard.",
               "Copiar o texto revisado para a área de transferência."), this::copy);
        preview.card(SettingsPanel.BUG_REPORT, tr("Compartir…", "Share…", "Compartilhar…"),
            tr("Elegir una aplicación para compartir el texto.", "Choose an app to share the text.",
               "Escolher um aplicativo para compartilhar o texto."), this::share);
        preview.card(SettingsPanel.BUG_REPORT, tr("Abrir reporte en GitHub", "Open report on GitHub", "Abrir relato no GitHub"),
            tr("Abre un borrador de issue. Revisa y publica desde GitHub.",
               "Opens an issue draft. Review and submit it on GitHub.",
               "Abre um rascunho de issue. Revise e publique no GitHub."), this::github);
        preview.card(SettingsPanel.CONTROLS, tr("Editar descripción y actualizar datos", "Edit description and refresh details", "Editar descrição e atualizar dados"),
            tr("Volver al formulario para generar otra vista previa.", "Return to the form to generate another preview.",
               "Voltar ao formulário para gerar outra prévia."), this::showForm);
        preview.paragraph(tr("Nada se envía automáticamente. El borrador se conserva al volver a este apartado.",
            "Nothing is sent automatically. The draft is kept when you return to this section.",
            "Nada é enviado automaticamente. O rascunho é mantido ao voltar a esta seção."));
        open(preview, this::leave, this::leave);
    }

    private String reviewed() {
        String value = BugReportModel.clean(report, BugReportModel.MAX_REPORT);
        if (value.trim().isEmpty()) {
            toast(tr("El informe está vacío.", "The report is empty.", "O relatório está vazio."));
            return null;
        }
        return value;
    }

    private String reportTitle() { return BugReportModel.title(reportLanguage, title, version); }

    private boolean clipboard(String text) {
        try {
            ClipboardManager clipboard = (ClipboardManager) activity.getSystemService(Context.CLIPBOARD_SERVICE);
            if (clipboard == null) throw new IllegalStateException("Clipboard unavailable");
            clipboard.setPrimaryClip(ClipData.newPlainText(reportTitle(), text));
            return true;
        } catch (RuntimeException unavailable) {
            toast(tr("No se pudo copiar. Puedes seleccionar el texto del informe.",
                "Could not copy. You can select the report text.",
                "Não foi possível copiar. Você pode selecionar o texto do relatório."));
            return false;
        }
    }

    private void copy() {
        if (!showing() || !resumed) return;
        String text = reviewed();
        if (text != null && clipboard(text)) toast(tr("Informe copiado.", "Report copied.", "Relatório copiado."));
    }

    private void share() {
        if (!showing() || !resumed) return;
        String text = reviewed();
        if (text == null) return;
        hideKeyboard();
        Intent send = new Intent(Intent.ACTION_SEND).setType("text/plain")
            .putExtra(Intent.EXTRA_SUBJECT, reportTitle()).putExtra(Intent.EXTRA_TEXT, text);
        try { activity.startActivity(Intent.createChooser(send, tr("Compartir informe", "Share report", "Compartilhar relatório"))); }
        catch (ActivityNotFoundException | SecurityException unavailable) {
            toast(tr("No se pudo abrir Compartir. Usa Copiar informe.", "Could not open Share. Use Copy report.",
                "Não foi possível abrir Compartilhar. Use Copiar relatório."));
        }
    }

    private void github() {
        if (!showing() || !resumed) return;
        String text = reviewed();
        if (text == null) return;
        BugReportModel.IssueLink link = BugReportModel.issueLink(reportTitle(), text);
        if (link.needsPaste && !clipboard(text)) return;
        hideKeyboard();
        try {
            activity.startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(link.url)).addCategory(Intent.CATEGORY_BROWSABLE));
            if (link.needsPaste) toast(tr("Informe copiado. Pégalo en el cuerpo del issue de GitHub.",
                "Report copied. Paste it into the GitHub issue body.",
                "Relatório copiado. Cole-o no corpo da issue do GitHub."));
        } catch (ActivityNotFoundException | SecurityException unavailable) {
            toast(tr("No se pudo abrir GitHub. Copia el informe y abre el repositorio en tu navegador.",
                "Could not open GitHub. Copy the report and open the repository in your browser.",
                "Não foi possível abrir o GitHub. Copie o relatório e abra o repositório no navegador."));
        }
    }

    private View editor(String label, String text, int maximum, boolean multiline, Consumer<String> update) {
        LinearLayout group = new LinearLayout(activity);
        group.setOrientation(LinearLayout.VERTICAL);
        TextView caption = new TextView(activity);
        caption.setText(label); caption.setTextSize(14); caption.setTextColor(SettingsPanel.SECONDARY);
        caption.setTypeface(Typeface.DEFAULT_BOLD);
        group.addView(caption, new LinearLayout.LayoutParams(-1, -2));
        EditText input = new EditText(activity);
        input.setId(View.generateViewId()); caption.setLabelFor(input.getId());
        input.setTextColor(SettingsPanel.TEXT); input.setHintTextColor(SettingsPanel.SECONDARY);
        input.setTextSize(14); input.setPadding(dp(12), dp(10), dp(12), dp(10));
        input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_CAP_SENTENCES
            | (multiline ? InputType.TYPE_TEXT_FLAG_MULTI_LINE : 0));
        input.setImeOptions(EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING);
        input.setSingleLine(!multiline);
        if (multiline) { input.setMinLines(maximum == BugReportModel.MAX_REPORT ? 10 : 2); input.setMaxLines(18); }
        input.setFilters(new InputFilter[]{new InputFilter.LengthFilter(maximum)});
        GradientDrawable background = new GradientDrawable();
        background.setColor(SettingsPanel.SURFACE); background.setCornerRadius(dp(10));
        background.setStroke(dp(1), SettingsPanel.BORDER); input.setBackground(background);
        input.setText(text == null ? "" : text);
        input.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            public void onTextChanged(CharSequence s, int start, int before, int count) { update.accept(s.toString()); }
            public void afterTextChanged(Editable s) {}
        });
        LinearLayout.LayoutParams size = new LinearLayout.LayoutParams(-1, -2);
        size.setMargins(0, dp(6), 0, 0); group.addView(input, size);
        return group;
    }

    private void hideKeyboard() {
        InputMethodManager keyboard = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (keyboard != null && page != null && page.getWindow() != null)
            keyboard.hideSoftInputFromWindow(page.getWindow().getDecorView().getWindowToken(), 0);
    }

    private void toast(String text) { if (alive()) Toast.makeText(activity, text, Toast.LENGTH_LONG).show(); }

    private static final class Pending {
        final BugReportModel.Snapshot snapshot;
        final int language;
        final String happened, steps, expected;
        Pending(BugReportModel.Snapshot snapshot, int language, String happened, String steps, String expected) {
            this.snapshot = snapshot; this.language = language;
            this.happened = happened; this.steps = steps; this.expected = expected;
        }
    }
}
