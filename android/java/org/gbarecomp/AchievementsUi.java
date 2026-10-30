package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

import java.text.DateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicBoolean;

/** Local achievement catalog, lifecycle-safe sampling and foreground notices. */
final class AchievementsUi {
    interface Translator { String text(String es, String en, String pt); }
    interface Presenter { void open(AlertDialog dialog, Runnable parent); }
    private static final long POLL_MILLIS = 1000;

    private final Activity activity;
    private final Translator translator;
    private final Presenter presenter;
    private final Runnable parent;
    private final AchievementsStore store;
    private final AchievementsStore.NativeSnapshot source;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final AtomicBoolean sampling = new AtomicBoolean();
    private boolean closed, resumed, binding;
    private int filter;
    private AlertDialog shown;
    private TextView summary, problem;
    private ProgressBar total;
    private LinearLayout records;
    private Switch notifications;
    private final Button[] filters = new Button[3];
    private Toast toast;
    private AchievementsModel.State rendered;
    private int renderedFilter = -1;

    AchievementsUi(Activity activity, Translator translator, Presenter presenter, Runnable parent) {
        this(activity, translator, presenter, parent, AchievementsStore.get(activity), GbaNative::getLocalAchievements);
    }

    AchievementsUi(Activity activity, Translator translator, Presenter presenter, Runnable parent,
                   AchievementsStore store, AchievementsStore.NativeSnapshot source) {
        this.activity = activity; this.translator = translator; this.presenter = presenter;
        this.parent = parent; this.store = store; this.source = source;
    }

    private String tr(String es, String en, String pt) { return translator.text(es, en, pt); }
    private String tr(String[] value) { return tr(value[0], value[1], value[2]); }
    private boolean alive() { return !closed && !activity.isFinishing() && !activity.isDestroyed(); }
    private int dp(int value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }

    String summary() {
        return store.latest().state.unlocked() + " / " + AchievementsModel.DEFINITIONS.length
            + tr(" desbloqueados · guardados en este dispositivo", " unlocked · saved on this device",
                 " desbloqueados · salvos neste dispositivo");
    }

    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!alive() || !resumed) return;
            sample();
            handler.postDelayed(this, POLL_MILLIS);
        }
    };

    void onResume() {
        if (!alive() || resumed) return;
        resumed = true;
        handler.removeCallbacks(tick);
        handler.post(tick);
    }

    void onPause() {
        if (!resumed) return;
        resumed = false;
        handler.removeCallbacks(tick);
        if (toast != null) toast.cancel();
        // A final queued sample is independent of any in-flight UI request.
        // The process-owned worker can finish it after this Activity closes.
        store.poll(source, null);
    }

    void close() {
        if (closed) return;
        onPause();
        closed = true;
        handler.removeCallbacks(tick);
        if (toast != null) toast.cancel();
        shown = null; rendered = null;
    }

    private void sample() {
        if (!alive() || !sampling.compareAndSet(false, true)) return;
        store.poll(source, result -> {
            sampling.set(false);
            if (!alive()) return;
            update(result);
            if (resumed && result.state.notifications && result.newUnlocks != 0) announce(result.newUnlocks);
        });
    }

    private void announce(int mask) {
        int count = Integer.bitCount(mask);
        String message;
        if (count == 1) {
            int index = Integer.numberOfTrailingZeros(mask);
            message = tr("Logro desbloqueado: ", "Achievement unlocked: ", "Conquista desbloqueada: ")
                + tr(AchievementsModel.DEFINITIONS[index].title);
        } else {
            message = count + tr(" logros desbloqueados. Ver en Ajustes → Logros.",
                " achievements unlocked. View in Settings → Achievements.",
                " conquistas desbloqueadas. Veja em Configurações → Conquistas.");
        }
        if (toast != null) toast.cancel();
        toast = Toast.makeText(activity, message, Toast.LENGTH_SHORT);
        toast.show();
    }

    void show() {
        if (!alive()) return;
        SettingsPanel page = new SettingsPanel(activity,
            tr("Logros locales", "Local achievements", "Conquistas locais"),
            null,
            tr("Volver", "Back", "Voltar"));
        LinearLayout overview = new Overview();
        overview.setPadding(dp(16), dp(14), dp(16), dp(14));
        overview.setBackground(shape(0xff263d29, 0xff46613d, 16));
        summary = text("", 21, SettingsPanel.TEXT);
        summary.setTypeface(Typeface.DEFAULT_BOLD);
        summary.setAccessibilityHeading(true);
        overview.addView(summary);
        total = progress(AchievementsModel.DEFINITIONS.length);
        LinearLayout.LayoutParams track = new LinearLayout.LayoutParams(-1, dp(8));
        track.setMargins(0, dp(12), 0, dp(2)); overview.addView(total, track);
        page.content(overview);

        LinearLayout tabs = new LinearLayout(activity);
        tabs.setOrientation(LinearLayout.HORIZONTAL);
        tabs.setBaselineAligned(false);
        String[] names = {tr("Todos", "All", "Todas"), tr("Logrados", "Unlocked", "Obtidas"),
            tr("Pendientes", "Pending", "Pendentes")};
        for (int i = 0; i < filters.length; ++i) {
            final int selected = i;
            Button button = new Button(activity);
            filters[i] = button;
            button.setTag("achievement-filter-" + i);
            button.setText(names[i]); button.setAllCaps(false); button.setTextSize(12);
            button.setMinHeight(dp(48)); button.setMinimumWidth(0); button.setMinWidth(0);
            button.setPadding(dp(4), dp(8), dp(4), dp(8));
            button.setOnClickListener(view -> { filter = selected; update(store.latest()); });
            LinearLayout.LayoutParams size = new LinearLayout.LayoutParams(0, -2, 1);
            if (i != 0) size.setMargins(dp(6), 0, 0, 0);
            tabs.addView(button, size);
        }
        page.content(tabs);

        problem = text("", 13, 0xffffd48a);
        problem.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        page.content(problem);
        records = column();
        page.content(records);
        page.section(tr("Preferencias", "Preferences", "Preferências"));
        notifications = new Switch(activity);
        notifications.setText(tr("Avisos al desbloquear", "Unlock notices", "Avisos ao desbloquear"));
        notifications.setTextColor(SettingsPanel.TEXT); notifications.setTextSize(16);
        notifications.setMinHeight(dp(56)); notifications.setSwitchPadding(dp(14));
        notifications.setPadding(dp(14), dp(10), dp(14), dp(10));
        notifications.setBackground(shape(SettingsPanel.SURFACE, SettingsPanel.BORDER, 14));
        notifications.setThumbTintList(new ColorStateList(
            new int[][]{new int[]{android.R.attr.state_checked}, new int[]{}},
            new int[]{SettingsPanel.ACCENT, SettingsPanel.SECONDARY}));
        notifications.setTrackTintList(ColorStateList.valueOf(0xff4a5c50));
        notifications.setOnCheckedChangeListener((view, enabled) -> {
            if (binding) return;
            store.notifications(enabled, result -> { if (alive()) update(result); });
        });
        page.content(notifications);
        page.paragraph(tr(
            "Se obtienen mediante acciones detectadas durante el juego desde esta versión. Cargar una partida avanzada no importa sus logros. El progreso es local, independiente de la partida y no requiere una cuenta.",
            "Earned through actions detected during play from this version onward. Loading an advanced save does not import achievements. Progress is local, separate from your game save, and needs no account.",
            "São obtidas por ações detectadas durante o jogo a partir desta versão. Carregar uma partida avançada não importa conquistas. O progresso é local, separado da partida e não exige uma conta."));
        rendered = null; renderedFilter = -1;
        shown = page.dialog(parent);
        presenter.open(shown, parent);
        update(store.latest());
        sample();
    }

    private void update(AchievementsStore.Result result) {
        if (shown == null || !shown.isShowing()) return;
        AchievementsModel.State state = result.state;
        summary.setText(state.unlocked() + " / " + AchievementsModel.DEFINITIONS.length
            + tr(" desbloqueados", " unlocked", " desbloqueadas"));
        total.setProgress(state.unlocked());
        String message = "";
        if (result.problem == AchievementsStore.Problem.LOADING)
            message = tr("Cargando logros…", "Loading achievements…", "Carregando conquistas…");
        else if (result.problem == AchievementsStore.Problem.READ)
            message = tr("No se pudo leer el progreso local. Se conserva el archivo y se reintentará.",
                "Local progress could not be read. The file is preserved and will be retried.",
                "Não foi possível ler o progresso local. O arquivo foi preservado e haverá nova tentativa.");
        else if (result.problem == AchievementsStore.Problem.WRITE)
            message = tr("Hay progreso pendiente de guardar. Se reintentará automáticamente.",
                "Some progress is waiting to be saved. Saving will retry automatically.",
                "Há progresso aguardando para ser salvo. Uma nova tentativa será feita automaticamente.");
        else if (result.problem == AchievementsStore.Problem.NATIVE)
            message = tr("El registro comenzará cuando el juego esté listo. Tus logros guardados siguen disponibles.",
                "Tracking starts when the game is ready. Your saved achievements remain available.",
                "O registro começará quando o jogo estiver pronto. Suas conquistas salvas continuam disponíveis.");
        problem.setText(message); problem.setVisibility(message.isEmpty() ? View.GONE : View.VISIBLE);
        binding = true;
        notifications.setChecked(state.notifications);
        notifications.setEnabled(result.problem != AchievementsStore.Problem.LOADING
            && result.problem != AchievementsStore.Problem.READ);
        binding = false;
        for (int i = 0; i < filters.length; ++i) {
            filters[i].setSelected(i == filter);
            filters[i].setTextColor(i == filter ? SettingsPanel.BACKGROUND : SettingsPanel.TEXT);
            filters[i].setBackground(new RippleDrawable(ColorStateList.valueOf(0x22a5e775),
                shape(i == filter ? SettingsPanel.ACCENT : SettingsPanel.SURFACE, SettingsPanel.BORDER, 12), null));
        }
        if (state.same(rendered) && renderedFilter == filter) return;
        rendered = state; renderedFilter = filter;
        records.removeAllViews();
        for (int i = 0; i < AchievementsModel.DEFINITIONS.length; ++i) {
            boolean unlocked = state.unlockedAt[i] > 0;
            if ((filter == 1 && !unlocked) || (filter == 2 && unlocked)) continue;
            addRecord(i, state);
        }
        if (records.getChildCount() == 0) {
            records.addView(text(filter == 1
                ? tr("Todavía no hay logros desbloqueados. Tu próxima aventura puede ser la primera.",
                     "No achievements unlocked yet. Your next adventure could be the first.",
                     "Ainda não há conquistas desbloqueadas. Sua próxima aventura pode ser a primeira.")
                : tr("¡Completaste todos los logros disponibles!", "You completed all available achievements!",
                     "Você completou todas as conquistas disponíveis!"), 15, SettingsPanel.SECONDARY));
        }
    }

    private void addRecord(int index, AchievementsModel.State state) {
        AchievementsModel.Definition def = AchievementsModel.DEFINITIONS[index];
        boolean unlocked = state.unlockedAt[index] > 0;
        LinearLayout card = column();
        card.setTag("achievement-" + def.id);
        card.setPadding(dp(14), dp(14), dp(14), dp(14));
        card.setBackground(shape(SettingsPanel.SURFACE, unlocked ? 0xff5d7e4b : SettingsPanel.BORDER, 16));
        card.setFocusable(true);
        LinearLayout heading = new LinearLayout(activity);
        heading.setGravity(Gravity.CENTER_VERTICAL);
        Badge badge = new Badge(unlocked);
        heading.addView(badge, new LinearLayout.LayoutParams(dp(34), dp(34)));
        TextView title = text(tr(def.title), 17, SettingsPanel.TEXT);
        title.setTypeface(Typeface.DEFAULT_BOLD);
        LinearLayout.LayoutParams name = new LinearLayout.LayoutParams(0, -2, 1);
        name.setMargins(dp(10), 0, 0, 0); heading.addView(title, name);
        card.addView(heading);
        TextView description = text(tr(def.description), 14, SettingsPanel.SECONDARY);
        LinearLayout.LayoutParams gap = new LinearLayout.LayoutParams(-1, -2);
        gap.setMargins(0, dp(8), 0, dp(10)); card.addView(description, gap);
        int value = def.progress(state);
        String status = unlocked ? tr("Desbloqueado", "Unlocked", "Desbloqueada")
            : tr("Progreso", "Progress", "Progresso") + ": " + value + " / " + def.target;
        card.addView(text(status, 13, unlocked ? SettingsPanel.ACCENT : SettingsPanel.SECONDARY));
        ProgressBar progress = progress(def.target); progress.setProgress(value);
        LinearLayout.LayoutParams track = new LinearLayout.LayoutParams(-1, dp(6));
        track.setMargins(0, dp(7), 0, 0); card.addView(progress, track);
        String date = "";
        if (unlocked) {
            DateFormat format = DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT,
                new Locale(tr("es", "en", "pt")));
            date = tr("Obtenido: ", "Earned: ", "Obtida: ") + format.format(new Date(state.unlockedAt[index]));
            TextView when = text(date, 12, SettingsPanel.SECONDARY);
            LinearLayout.LayoutParams dateGap = new LinearLayout.LayoutParams(-1, -2);
            dateGap.setMargins(0, dp(8), 0, 0); card.addView(when, dateGap);
        }
        card.setContentDescription(tr(def.title) + ". " + tr(def.description) + ". " + status + ". " + date);
        for (int i = 0; i < card.getChildCount(); ++i)
            card.getChildAt(i).setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        LinearLayout.LayoutParams margin = new LinearLayout.LayoutParams(-1, -2);
        margin.setMargins(0, 0, 0, dp(10)); records.addView(card, margin);
    }

    private LinearLayout column() {
        LinearLayout column = new LinearLayout(activity); column.setOrientation(LinearLayout.VERTICAL); return column;
    }
    private TextView text(String value, int size, int color) {
        TextView text = new TextView(activity); text.setText(value); text.setTextSize(size);
        text.setTextColor(color); text.setLineSpacing(dp(2), 1f); return text;
    }
    private GradientDrawable shape(int fill, int stroke, int radius) {
        GradientDrawable drawable = new GradientDrawable(); drawable.setColor(fill);
        drawable.setCornerRadius(dp(radius)); drawable.setStroke(dp(1), stroke); return drawable;
    }
    private ProgressBar progress(int maximum) {
        ProgressBar bar = new ProgressBar(activity, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(maximum); bar.setProgressTintList(ColorStateList.valueOf(SettingsPanel.ACCENT));
        bar.setProgressBackgroundTintList(ColorStateList.valueOf(SettingsPanel.BORDER));
        bar.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO); return bar;
    }

    /** Keep the overview short on wide displays so the first achievement is visible. */
    private final class Overview extends LinearLayout {
        private Boolean wide;
        Overview() { super(activity); setOrientation(LinearLayout.VERTICAL); }
        @Override protected void onMeasure(int widthSpec, int heightSpec) {
            boolean next = View.MeasureSpec.getSize(widthSpec) >= dp(480);
            if (wide == null || wide != next) {
                wide = next;
                setOrientation(next ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
                setGravity(Gravity.CENTER_VERTICAL);
                setPadding(dp(16), dp(next ? 10 : 14), dp(16), dp(next ? 10 : 14));
                if (getChildCount() == 2) {
                    getChildAt(0).setLayoutParams(new LinearLayout.LayoutParams(next ? 0 : -1, -2, next ? 1.2f : 0));
                    LinearLayout.LayoutParams track = new LinearLayout.LayoutParams(next ? 0 : -1, dp(8), next ? 1 : 0);
                    track.setMargins(dp(next ? 16 : 0), dp(next ? 0 : 12), 0, dp(next ? 0 : 2));
                    getChildAt(1).setLayoutParams(track);
                }
            }
            super.onMeasure(widthSpec, heightSpec);
        }
    }

    private final class Badge extends View {
        private final boolean earned;
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        Badge(boolean earned) { super(activity); this.earned = earned;
            setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO); }
        @Override protected void onDraw(Canvas canvas) {
            canvas.save(); canvas.scale(getWidth() / 34f, getHeight() / 34f);
            paint.setColor(earned ? 0xff324b2a : 0xff29382f); paint.setStyle(Paint.Style.FILL);
            canvas.drawCircle(17, 17, 17, paint);
            Path star = new Path();
            for (int i = 0; i < 10; ++i) {
                double angle = -Math.PI / 2 + i * Math.PI / 5;
                float radius = i % 2 == 0 ? 11 : 4.7f;
                float x = 17 + (float)Math.cos(angle) * radius, y = 17 + (float)Math.sin(angle) * radius;
                if (i == 0) star.moveTo(x, y); else star.lineTo(x, y);
            }
            star.close(); paint.setColor(earned ? SettingsPanel.ACCENT : 0xff738b79);
            paint.setStyle(earned ? Paint.Style.FILL : Paint.Style.STROKE); paint.setStrokeWidth(1.6f);
            canvas.drawPath(star, paint); canvas.restore();
        }
    }
}
