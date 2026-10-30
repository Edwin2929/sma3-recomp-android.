package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import java.text.DateFormat;
import java.util.Date;
import java.util.Arrays;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

/** Opt-in live timer and local records; sampling never advances the clock. */
final class TimeTrialUi {
    interface Translator { String text(String es, String en, String pt); }
    interface Presenter { void open(AlertDialog dialog, Runnable parent); }
    static final String ENABLED = "time-trial-enabled", OVERLAY = "time-trial-overlay";
    static final String GHOST = "time-trial-ghost";
    private final Activity activity;
    private final Translator translator;
    private final Presenter presenter;
    private final Runnable parent;
    private final FrameLayout overlayHost;
    private final TextView fpsLabel, overlay;
    private final TimeTrialStore store;
    private final TimeTrialStore.NativeSnapshot source;
    private final Consumer<Boolean> enabledSink;
    private final Consumer<Boolean> ghostSink;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final AtomicBoolean sampling = new AtomicBoolean();
    private boolean closed, resumed;
    private int world;
    private AlertDialog shown;
    private TextView current, best, comparison, problem, ghostStatus;
    private LinearLayout records;
    private TimeTrialModel.State rendered;
    private int renderedWorld = -1;
    private long[] renderedGhosts;
    private final Button[] worlds = new Button[6];

    TimeTrialUi(Activity activity, Translator translator, Presenter presenter, Runnable parent,
                FrameLayout overlayHost, TextView fpsLabel) {
        this(activity, translator, presenter, parent, overlayHost, fpsLabel, TimeTrialStore.get(activity),
            GbaNative::getTimeTrial, GbaNative::setTimeTrialEnabled, GbaNative::setTimeTrialGhostEnabled);
    }

    TimeTrialUi(Activity activity, Translator translator, Presenter presenter, Runnable parent,
                FrameLayout overlayHost, TextView fpsLabel, TimeTrialStore store,
                TimeTrialStore.NativeSnapshot source, Consumer<Boolean> enabledSink) {
        this(activity, translator, presenter, parent, overlayHost, fpsLabel, store, source, enabledSink, ignored -> {});
    }

    TimeTrialUi(Activity activity, Translator translator, Presenter presenter, Runnable parent,
                FrameLayout overlayHost, TextView fpsLabel, TimeTrialStore store,
                TimeTrialStore.NativeSnapshot source, Consumer<Boolean> enabledSink, Consumer<Boolean> ghostSink) {
        this.activity = activity; this.translator = translator; this.presenter = presenter; this.parent = parent;
        this.overlayHost = overlayHost; this.fpsLabel = fpsLabel; this.store = store;
        this.source = source; this.enabledSink = enabledSink; this.ghostSink = ghostSink;
        overlay = text("", 13, SettingsPanel.TEXT);
        overlay.setTag("time-trial-overlay");
        overlay.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        overlay.setMaxWidth(dp(230));
        overlay.setPadding(dp(9), dp(5), dp(9), dp(5));
        overlay.setBackground(shape(0xd0111b18, 0xff45623b));
        overlay.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        overlay.setClickable(false); overlay.setFocusable(false);
        overlay.setVisibility(View.GONE);
        if (overlayHost != null) {
            FrameLayout.LayoutParams size = new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.END);
            size.setMargins(dp(8), dp(8), dp(8), 0); overlayHost.addView(overlay, size);
        }
    }

    static void applySaved(Context context) {
        GbaNative.setTimeTrialEnabled(context.getSharedPreferences("sma3-options", Context.MODE_PRIVATE)
            .getBoolean(ENABLED, false));
        GbaNative.setTimeTrialGhostEnabled(context.getSharedPreferences("sma3-options", Context.MODE_PRIVATE)
            .getBoolean(GHOST, false));
    }
    private String tr(String es, String en, String pt) { return translator.text(es, en, pt); }
    private boolean alive() { return !closed && !activity.isFinishing() && !activity.isDestroyed(); }
    private int dp(int value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }
    private boolean enabled() { return activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).getBoolean(ENABLED, false); }
    private boolean overlayEnabled() { return activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).getBoolean(OVERLAY, true); }
    private boolean ghostEnabled() { return activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).getBoolean(GHOST, false); }
    private TextView text(String value, int size, int color) {
        TextView result = new TextView(activity); result.setText(value); result.setTextSize(size); result.setTextColor(color);
        return result;
    }
    private GradientDrawable shape(int fill, int stroke) {
        GradientDrawable result = new GradientDrawable(); result.setColor(fill); result.setCornerRadius(dp(10));
        result.setStroke(dp(1), stroke); return result;
    }
    private void setText(TextView view, String value) {
        if (view != null && !view.getText().toString().equals(value)) view.setText(value);
    }
    private String level(int index) {
        if (index < 0 || index >= TimeTrialModel.LEVELS) return tr("Sin nivel", "No level", "Sem fase");
        int stage = index % 10;
        return (index / 10 + 1) + "-" + (stage < 8 ? Integer.toString(stage + 1)
            : stage == 8 ? "Extra" : tr("Secreto", "Secret", "Secreto"));
    }

    String summary() {
        return store.latest().state.count() + " / 60 " + tr("récords personales", "personal records", "recordes pessoais");
    }

    private final Runnable tick = new Runnable() {
        public void run() {
            if (!alive() || !resumed) return;
            sample();
            handler.postDelayed(this, enabled() ? 100 : 1000);
        }
    };
    void onResume() {
        if (!alive() || resumed) return;
        resumed = true; handler.removeCallbacks(tick); handler.post(tick); update(store.latest());
    }
    void onPause() {
        if (!resumed) return;
        resumed = false; handler.removeCallbacks(tick); overlay.setVisibility(View.GONE);
        store.poll(source, null);
    }
    void close() {
        if (closed) return;
        onPause(); closed = true; handler.removeCallbacks(tick);
        if (overlayHost != null) overlayHost.removeView(overlay);
        shown = null; records = null; rendered = null;
    }
    private void sample() {
        if (!alive() || !sampling.compareAndSet(false, true)) return;
        store.poll(source, result -> {
            sampling.set(false);
            if (alive()) update(result);
        });
    }

    void show() {
        if (!alive()) return;
        SettingsPanel page = new SettingsPanel(activity, tr("Contrarreloj", "Time trial", "Contra o relógio"),
            tr("Supera tu mejor tiempo en cada nivel.", "Beat your personal best in each level.", "Supere seu melhor tempo em cada fase."),
            tr("Volver", "Back", "Voltar"));
        page.toggle(tr("Activar contrarreloj", "Enable time trials", "Ativar contra o relógio"),
            tr("Empieza al entrar a un nivel desde su inicio.", "Starts when entering a level from its beginning.", "Começa ao entrar em uma fase pelo início."),
            enabled(), value -> {
                activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit().putBoolean(ENABLED, value).apply();
                try { enabledSink.accept(value); } catch (UnsatisfiedLinkError | RuntimeException unavailable) { /* next sample explains availability */ }
                sample(); update(store.latest());
            });
        page.toggle(tr("Mostrar cronómetro", "Show timer", "Mostrar cronômetro"),
            tr("Un contador pequeño durante el juego.", "A small counter while playing.", "Um contador pequeno durante o jogo."),
            overlayEnabled(), value -> {
                activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit().putBoolean(OVERLAY, value).apply();
                update(store.latest());
            });
        page.toggle(tr("Fantasma de mi récord", "Personal-best ghost", "Fantasma do meu recorde"),
            tr("Graba tu mejor recorrido y compite contra su fantasma.",
                "Record your best run and race its ghost.", "Grave seu melhor percurso e dispute com seu fantasma."),
            ghostEnabled(), value -> {
                activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit().putBoolean(GHOST, value).apply();
                try { ghostSink.accept(value); } catch (UnsatisfiedLinkError | RuntimeException unavailable) { /* sampling reports availability */ }
                sample(); update(store.latest());
            });
        ghostStatus = text("", 13, SettingsPanel.SECONDARY);
        ghostStatus.setTag("time-trial-ghost-status"); page.content(ghostStatus);

        LinearLayout live = new LinearLayout(activity); live.setOrientation(LinearLayout.VERTICAL);
        live.setPadding(dp(14), dp(12), dp(14), dp(12)); live.setBackground(shape(0xff263d29, 0xff46613d));
        current = text("", 21, SettingsPanel.TEXT); current.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
        current.setTag("time-trial-current"); live.addView(current);
        best = text("", 14, SettingsPanel.ACCENT); best.setPadding(0, dp(6), 0, 0); live.addView(best);
        comparison = text("", 14, SettingsPanel.SECONDARY); comparison.setPadding(0, dp(5), 0, 0);
        comparison.setTag("time-trial-comparison"); live.addView(comparison); page.content(live);
        problem = text("", 13, 0xffffd48a); problem.setTag("time-trial-problem"); page.content(problem);

        page.section(tr("Récords por mundo", "Records by world", "Recordes por mundo"));
        for (int row = 0; row < 2; ++row) {
            LinearLayout buttons = new LinearLayout(activity);
            for (int column = 0; column < 3; ++column) {
                final int selected = row * 3 + column;
                Button button = new Button(activity); worlds[selected] = button;
                button.setTag("time-trial-world-" + selected);
                button.setText(tr("Mundo ", "World ", "Mundo ") + (selected + 1)); button.setAllCaps(false);
                button.setTextSize(12); button.setTextColor(SettingsPanel.TEXT); button.setMinHeight(dp(48));
                button.setMinWidth(0); button.setMinimumWidth(0); button.setPadding(dp(4), dp(6), dp(4), dp(6));
                button.setOnClickListener(view -> { world = selected; update(store.latest()); });
                LinearLayout.LayoutParams size = new LinearLayout.LayoutParams(0, -2, 1);
                if (column != 0) size.setMargins(dp(4), 0, 0, 0); buttons.addView(button, size);
            }
            page.content(buttons);
        }
        records = new LinearLayout(activity); records.setOrientation(LinearLayout.VERTICAL); page.content(records);
        page.section(tr("Cómo se mide", "Timing rules", "Como o tempo é medido"));
        page.paragraph(tr(
            "Cuenta tiempo del juego, con la misma escala a 60 y 120 FPS. Se detiene en la pausa y el inventario del juego, y termina al confirmar la meta. Incluye los cambios de habitación y la transición de victoria de los jefes.",
            "Measures game time on the same scale at 60 and 120 FPS. Stops in the game's pause and inventory menus and finishes when the goal is confirmed. Room changes and boss victory transitions are included.",
            "Conta tempo de jogo na mesma escala a 60 e 120 FPS. Para na pausa e no inventário do jogo e termina quando a chegada é confirmada. Inclui mudanças de sala e a transição de vitória dos chefes."));
        page.paragraph(tr(
            "Morir, salir del nivel o cargar un estado cancela ese intento. Actívalo antes de entrar al nivel; un reinicio desde un punto intermedio no registra un récord. Las mejores marcas se guardan en este dispositivo para los 60 niveles principales, Extra y Secretos. La bienvenida y Mario Bros. quedan fuera.",
            "Dying, leaving the level or loading a state cancels that attempt. Enable it before entering a level; restarting from a checkpoint cannot set a record. Personal bests are saved on this device for the 60 main, Extra and Secret levels. The welcome course and Mario Bros. are excluded.",
            "Morrer, sair da fase ou carregar um estado cancela a tentativa. Ative antes de entrar na fase; reiniciar de um ponto intermediário não registra recorde. As melhores marcas ficam neste dispositivo para as 60 fases principais, Extra e Secretas. A fase de boas-vindas e Mario Bros. ficam de fora."));
        page.paragraph(tr("Para crear un fantasma, activa el contrarreloj y el fantasma antes de entrar desde el inicio y termina igualando o mejorando tu récord. Los tiempos anteriores no contienen movimientos. El fantasma solo aparece en tu misma sala y se oculta durante transformaciones y escenas sin cobertura. No interactúa con enemigos ni objetos. Desactivarlo conserva los recorridos guardados.",
            "To create a ghost, enable time trials and the ghost before entering from the beginning and finish by matching or beating your best. Earlier times contain no movements. The ghost appears only in your current room and hides during transformations and unsupported scenes. It does not interact with enemies or objects. Turning it off preserves saved runs.",
            "Para criar um fantasma, ative o contra o relógio e o fantasma antes de entrar pelo início e termine igualando ou superando seu recorde. Tempos anteriores não contêm movimentos. O fantasma aparece apenas na sua sala e fica oculto durante transformações e cenas sem suporte. Não interage com inimigos nem objetos. Desativar preserva os percursos salvos."));
        shown = page.dialog(parent); rendered = null; renderedWorld = -1; renderedGhosts = null;
        presenter.open(shown, parent); update(store.latest()); sample();
    }

    private String reason(int reason) {
        switch (reason) {
            case 1: return tr("Yoshi perdió una vida.", "Yoshi lost a life.", "Yoshi perdeu uma vida.");
            case 2: return tr("Saliste del nivel.", "You left the level.", "Você saiu da fase.");
            case 3: return tr("Se cargó un estado guardado.", "A saved state was loaded.", "Um estado salvo foi carregado.");
            default: return tr("El recorrido no se pudo verificar.", "The attempt could not be verified.", "Não foi possível verificar a tentativa.");
        }
    }
    private String status(TimeTrialModel.Snapshot value) {
        if (!enabled()) return tr("Contrarreloj desactivado", "Time trial disabled", "Contra o relógio desativado");
        if (value == null) return tr("Preparando cronómetro…", "Preparing timer…", "Preparando cronômetro…");
        if (value.status == TimeTrialModel.RUNNING || value.status == TimeTrialModel.PAUSED || value.status == TimeTrialModel.FINISHED)
            return level(value.level) + " · " + TimeTrialModel.format(value.frames)
                + (value.status == TimeTrialModel.PAUSED ? tr(" · Pausa", " · Paused", " · Pausa")
                   : value.status == TimeTrialModel.FINISHED ? " ✓" : "");
        if (value.status == TimeTrialModel.INVALIDATED)
            return tr("Intento cancelado", "Attempt cancelled", "Tentativa cancelada");
        return tr("Entra a un nivel para empezar", "Enter a level to start", "Entre em uma fase para começar");
    }
    private String comparison(TimeTrialStore.Result value) {
        TimeTrialModel.Snapshot s = value.snapshot;
        if (s == null || s.completion == 0) return tr("Aún no hay un intento completo.", "No completed attempt yet.", "Ainda não há uma tentativa concluída.");
        String result = tr("Último: ", "Last: ", "Último: ") + level(s.lastLevel) + " · " + TimeTrialModel.format(s.lastFrames);
        if (value.comparedCompletion == s.completion) {
            if (value.previousBest < 0) result += " · " + TimeTrialModel.delta(s.lastFrames, value.state.best[s.lastLevel])
                + tr(" frente al récord", " versus best", " em relação ao recorde");
            else if (value.previousBest == 0) result += tr(" · Primera marca", " · First record", " · Primeira marca");
            else result += " · " + TimeTrialModel.delta(s.lastFrames, value.previousBest)
                + (s.lastFrames < value.previousBest ? tr(" · Nuevo récord", " · New best", " · Novo recorde") : "");
        }
        return result;
    }
    private String ghostStatus(TimeTrialStore.Result value) {
        if (!ghostEnabled()) return tr("Fantasma desactivado. Tus recorridos guardados se conservan.",
            "Ghost disabled. Saved runs are preserved.", "Fantasma desativado. Os percursos salvos são preservados.");
        if (!enabled()) return tr("Activa también el contrarreloj para grabar y ver fantasmas.",
            "Also enable time trials to record and see ghosts.", "Ative também o contra o relógio para gravar e ver fantasmas.");
        TimeTrialGhostModel.Snapshot ghost = value.ghost;
        if (value.ghostUnavailable) return tr("El fantasma aún no está disponible. Se reintentará.",
            "The ghost is not available yet. It will retry.", "O fantasma ainda não está disponível. Uma nova tentativa será feita.");
        if (ghost == null || !ghost.bestsReady || ghost.storage == TimeTrialGhostModel.NOT_CONFIGURED || ghost.storage == TimeTrialGhostModel.LOADING)
            return tr("Preparando recorridos…", "Preparing recorded runs…", "Preparando percursos…");
        if (ghost.storage == TimeTrialGhostModel.READ_ERROR) return tr("No se pudo leer un recorrido. El archivo se conserva; el cronómetro sigue funcionando.",
            "A recorded run could not be read. Its file is preserved; the timer still works.",
            "Não foi possível ler um percurso. O arquivo é preservado; o cronômetro continua funcionando.");
        if (ghost.storage == TimeTrialGhostModel.WRITE_ERROR) return tr("El fantasma tiene un guardado pendiente. Se reintentará sin borrar la ruta anterior.",
            "The ghost has a pending save. It will retry while preserving the previous run.",
            "O fantasma tem um salvamento pendente. Uma nova tentativa preservará o percurso anterior.");
        if (ghost.storage == TimeTrialGhostModel.SAVING || ghost.pending > 0) return tr("Guardando el fantasma de tu récord…",
            "Saving your personal-best ghost…", "Salvando o fantasma do seu recorde…");
        if (ghost.reason == TimeTrialGhostModel.LIMIT) return tr("Este recorrido superó el límite de grabación del fantasma. El tiempo sigue registrándose.",
            "This run exceeded the ghost recording limit. Timing continues.", "Este percurso ultrapassou o limite de gravação do fantasma. O tempo continua sendo registrado.");
        if (ghost.reason == TimeTrialGhostModel.CAPTURE_UNAVAILABLE) return tr("Este recorrido no pudo generar un fantasma visible. Su tiempo se conserva.",
            "This run could not produce a visible ghost. Its time is preserved.", "Este percurso não pôde gerar um fantasma visível. Seu tempo é preservado.");
        if (ghost.reason == TimeTrialGhostModel.BUFFER_UNAVAILABLE || ghost.reason == TimeTrialGhostModel.INVALID_BESTS)
            return tr("No se pudo preparar esta grabación. Vuelve a entrar desde el inicio para intentarlo.",
                "This recording could not be prepared. Enter again from the beginning to retry.",
                "Não foi possível preparar esta gravação. Entre novamente pelo início para tentar.");
        if (ghost.playing) return tr("Compitiendo contra: ", "Racing against: ", "Disputando contra: ")
            + TimeTrialModel.format(ghost.referenceFrames)
            + (ghost.recording ? tr(" · Grabando intento", " · Recording attempt", " · Gravando tentativa") : "");
        if (ghost.recording) return tr("Grabando intento. Iguala o mejora tu récord para guardar su fantasma.",
            "Recording attempt. Match or beat your best to save its ghost.", "Gravando tentativa. Iguale ou supere seu recorde para salvar o fantasma.");
        if (ghost.reason == TimeTrialGhostModel.CANCELLED) return tr("Grabación cancelada. Tus fantasmas completos se conservan.",
            "Recording cancelled. Your completed ghosts are preserved.", "Gravação cancelada. Seus fantasmas concluídos são preservados.");
        return ghost.count() + " / 60 " + tr("fantasmas de récord disponibles. Entra desde el inicio para empezar.",
            "personal-best ghosts available. Enter from the beginning to start.",
            "fantasmas de recorde disponíveis. Entre pelo início para começar.");
    }
    private void update(TimeTrialStore.Result value) {
        TimeTrialModel.Snapshot s = value.snapshot;
        String live = status(s);
        String mark = s != null && s.level >= 0 && value.state.best[s.level] != 0
            ? tr("Récord: ", "Best: ", "Recorde: ") + TimeTrialModel.format(value.state.best[s.level])
            : tr("Sin récord en este nivel", "No record for this level", "Sem recorde nesta fase");
        if (s != null && s.status == TimeTrialModel.INVALIDATED) mark = reason(s.reason);
        if (s == null || s.status == TimeTrialModel.WAITING || s.status == TimeTrialModel.DISABLED)
            mark = summary();
        String issue = "";
        if (value.problem == TimeTrialStore.Problem.LOADING) issue = tr("Cargando récords…", "Loading records…", "Carregando recordes…");
        else if (value.problem == TimeTrialStore.Problem.READ) issue = tr("No se pudieron leer los récords. Se conserva el archivo existente.", "Records could not be read. The existing file is preserved.", "Não foi possível ler os recordes. O arquivo existente foi preservado.");
        else if (value.problem == TimeTrialStore.Problem.WRITE) issue = tr("Guardado pendiente; se reintentará automáticamente.", "Save pending; it will retry automatically.", "Salvamento pendente; uma nova tentativa será feita automaticamente.");
        else if (value.problem == TimeTrialStore.Problem.NATIVE) issue = tr("El cronómetro del juego aún no está disponible.", "The game timer is not available yet.", "O cronômetro do jogo ainda não está disponível.");
        boolean visible = alive() && resumed && enabled() && overlayEnabled();
        overlay.setVisibility(visible ? View.VISIBLE : View.GONE);
        if (visible) {
            setText(overlay, live + "\n" + (issue.isEmpty() ? mark : issue));
            if (overlayHost != null) {
                FrameLayout.LayoutParams position = (FrameLayout.LayoutParams)overlay.getLayoutParams();
                int top = fpsLabel != null && fpsLabel.getVisibility() == View.VISIBLE ? fpsLabel.getHeight() + dp(16) : dp(8);
                if (position.topMargin != top) { position.topMargin = top; overlay.setLayoutParams(position); }
            }
        }
        if (shown == null || !shown.isShowing()) return;
        setText(current, live); setText(best, mark); setText(comparison, comparison(value)); setText(problem, issue);
        setText(ghostStatus, ghostStatus(value));
        problem.setVisibility(issue.isEmpty() ? View.GONE : View.VISIBLE);
        long[] ghosts = value.ghost == null ? null : value.ghost.best;
        if (rendered != value.state || renderedWorld != world || !Arrays.equals(renderedGhosts, ghosts)) renderRecords(value.state, ghosts);
    }

    private void renderRecords(TimeTrialModel.State state, long[] ghosts) {
        if (records == null) return;
        for (int i = 0; i < worlds.length; ++i) {
            worlds[i].setSelected(i == world);
            worlds[i].setBackground(shape(i == world ? 0xff345730 : SettingsPanel.SURFACE,
                i == world ? SettingsPanel.ACCENT : SettingsPanel.BORDER));
        }
        records.removeAllViews();
        for (int stage = 0; stage < 10; ++stage) {
            int index = world * 10 + stage;
            LinearLayout row = new LinearLayout(activity); row.setOrientation(LinearLayout.VERTICAL);
            row.setTag("time-trial-record-" + index); row.setPadding(dp(12), dp(10), dp(12), dp(10));
            row.setBackground(shape(SettingsPanel.SURFACE, SettingsPanel.BORDER));
            TextView title = text(level(index) + "   " + (state.best[index] == 0 ? "—" : TimeTrialModel.format(state.best[index])), 16, SettingsPanel.TEXT);
            title.setTypeface(Typeface.MONOSPACE, Typeface.BOLD); row.addView(title);
            if (state.best[index] != 0) {
                String detail = tr("Récord personal · ", "Personal best · ", "Recorde pessoal · ")
                    + DateFormat.getDateInstance(DateFormat.SHORT).format(new Date(state.recordedAt[index]));
                if (state.last[index] != 0) detail += "\n" + tr("Último: ", "Last: ", "Último: ")
                    + TimeTrialModel.format(state.last[index]) + " · " + TimeTrialModel.delta(state.last[index], state.best[index]);
                row.addView(text(detail, 12, SettingsPanel.SECONDARY));
                if (ghosts != null && ghosts[index] > 0)
                    row.addView(text(tr("Fantasma disponible · ", "Ghost available · ", "Fantasma disponível · ")
                        + TimeTrialModel.format(ghosts[index]), 12, SettingsPanel.ACCENT));
                else row.addView(text(tr("Sin fantasma para esta marca", "No ghost for this record", "Sem fantasma para este recorde"), 12, SettingsPanel.SECONDARY));
            }
            LinearLayout.LayoutParams size = new LinearLayout.LayoutParams(-1, -2); size.setMargins(0, 0, 0, dp(6));
            records.addView(row, size);
        }
        rendered = state; renderedWorld = world; renderedGhosts = ghosts == null ? null : ghosts.clone();
    }
}
