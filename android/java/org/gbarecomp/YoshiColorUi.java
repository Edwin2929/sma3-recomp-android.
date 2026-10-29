package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.GridLayout;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

import java.util.function.Consumer;

/** Built-in character-color mod. Editing is local until Apply or Original. */
final class YoshiColorUi {
    static final String ENABLED_KEY = "yoshi-color-enabled";
    static final String RGB_KEY = "yoshi-color-rgb";
    private final Activity activity;
    private final ModsUi.Translator words;
    private final ModsUi.Presenter presenter;
    private final Runnable parent;
    private final Consumer<String> saved;
    private YoshiColorModel.Draft draft;
    private SettingsPanel panel;
    private AlertDialog dialog;
    private EditText hex;
    private TextView error;
    private Button copy;
    private final SeekBar[] sliders = new SeekBar[3];
    private final TextView[] channelLabels = new TextView[3];
    private final TextView[] shadeLabels = new TextView[3];
    private final View[] swatches = new View[3];
    private Button[] presetButtons;
    private boolean updating, closed, saving;

    YoshiColorUi(Activity activity, ModsUi.Translator words, ModsUi.Presenter presenter,
                 Runnable parent, Consumer<String> saved) {
        this.activity = activity;
        this.words = words;
        this.presenter = presenter;
        this.parent = parent;
        this.saved = saved;
    }

    private String tr(String es, String en, String pt) { return words.text(es, en, pt); }
    private boolean alive() { return !closed && !activity.isFinishing() && !activity.isDestroyed(); }
    private boolean showing() { return alive() && dialog != null && dialog.isShowing(); }
    private int dp(int value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }

    static YoshiColorModel.Selection read(Context context) {
        SharedPreferences prefs = context.getSharedPreferences("sma3-options", Context.MODE_PRIVATE);
        try {
            int rgb = prefs.getInt(RGB_KEY, YoshiColorModel.DEFAULT_RGB);
            if (!YoshiColorModel.validRgb(rgb)) return new YoshiColorModel.Selection(false, YoshiColorModel.DEFAULT_RGB);
            return new YoshiColorModel.Selection(prefs.getBoolean(ENABLED_KEY, false), rgb);
        } catch (ClassCastException malformedPreference) {
            return new YoshiColorModel.Selection(false, YoshiColorModel.DEFAULT_RGB);
        }
    }

    static void applySaved(Context context) {
        YoshiColorModel.Selection value = read(context);
        GbaNative.setYoshiColor(value.enabled, value.rgb);
    }

    void show() {
        if (!alive()) return;
        draft = new YoshiColorModel.Draft(read(activity));
        panel = new SettingsPanel(activity, tr("Color de Yoshi", "Yoshi color", "Cor do Yoshi"),
            tr("Mod incorporado · ", "Built-in mod · ", "Mod integrado · ")
                + (draft.initial().enabled ? YoshiColorModel.hex(draft.initial().rgb)
                    : tr("Color original", "Original color", "Cor original")),
            tr("Aplicar color", "Apply color", "Aplicar cor"));
        panel.secondaryNavigation(tr("Cancelar", "Cancel", "Cancelar"), this::back);
        panel.content(new ResponsiveEditor());
        panel.card(SettingsPanel.MODS, tr("Usar color original", "Use original color", "Usar cor original"),
            tr("Desactiva este mod y conserva tu último color guardado.",
               "Disable this mod and keep your last saved color.",
               "Desative este mod e mantenha sua última cor salva."),
            () -> save(draft.originalSelection()));
        panel.paragraph(tr(
            "Se aplica a Yoshi normal durante el juego. Los vehículos, el mapa y las escenas conservan sus colores; tu elección vuelve al recuperar su forma normal.",
            "Applies to normal Yoshi during gameplay. Vehicles, the map and cutscenes keep their colors; your choice returns with his normal form.",
            "Aplica-se ao Yoshi normal durante o jogo. Veículos, mapa e cenas mantêm suas cores; sua escolha retorna com a forma normal."));
        panel.paragraph(tr(
            "El código elegido se guarda tal cual. El juego lo adapta a su paleta de 32 niveles por canal. Las muestras son orientativas: la iluminación, los filtros y los perfiles de imagen activos pueden alterar el resultado.",
            "Your exact code is saved. The game adapts it to its 32-level-per-channel palette. Swatches are a guide: lighting, filters and active image profiles can change the result.",
            "O código escolhido é salvo exatamente. O jogo o adapta à paleta de 32 níveis por canal. As amostras são uma referência: iluminação, filtros e perfis de imagem ativos podem mudar o resultado."));
        dialog = panel.dialog(() -> {
            if (draft.canApply()) save(draft.applySelection());
        });
        refresh(true);
        presenter.open(dialog, this::back);
        Window window = dialog.getWindow();
        if (window != null) window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE
            | WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
    }

    private LinearLayout column() {
        LinearLayout value = new LinearLayout(activity);
        value.setOrientation(LinearLayout.VERTICAL);
        return value;
    }

    /** Wide dialogs expose HEX and preset controls together, even in landscape. */
    private final class ResponsiveEditor extends LinearLayout {
        private final LinearLayout details;
        private final View controls;
        private boolean horizontal;

        ResponsiveEditor() {
            super(activity);
            setOrientation(LinearLayout.VERTICAL);
            details = column();
            details.addView(codeEditor(), spaced(-1, -2, 12));
            details.addView(preview(), spaced(-1, -2, 12));
            controls = colorControls();
            addView(details, new LinearLayout.LayoutParams(-1, -2));
            addView(controls, new LinearLayout.LayoutParams(-1, -2));
        }

        @Override protected void onMeasure(int widthSpec, int heightSpec) {
            boolean wide = View.MeasureSpec.getSize(widthSpec) >= dp(480);
            if (wide != horizontal) {
                horizontal = wide;
                setOrientation(wide ? LinearLayout.HORIZONTAL : LinearLayout.VERTICAL);
                LinearLayout.LayoutParams left = wide ? new LinearLayout.LayoutParams(0, -2, 1)
                    : new LinearLayout.LayoutParams(-1, -2);
                left.rightMargin = wide ? dp(16) : 0;
                details.setLayoutParams(left);
                controls.setLayoutParams(wide ? new LinearLayout.LayoutParams(0, -2, 1.2f)
                    : new LinearLayout.LayoutParams(-1, -2));
            }
            super.onMeasure(widthSpec, heightSpec);
        }
    }

    private TextView label(String value, int size, int color) {
        TextView view = new TextView(activity);
        view.setText(value); view.setTextSize(size); view.setTextColor(color);
        view.setLineSpacing(dp(2), 1f);
        return view;
    }

    private GradientDrawable shape(int fill, int stroke, int radius) {
        GradientDrawable value = new GradientDrawable();
        value.setColor(fill); value.setCornerRadius(dp(radius)); value.setStroke(dp(1), stroke);
        return value;
    }

    private LinearLayout.LayoutParams spaced(int width, int height, int bottom) {
        LinearLayout.LayoutParams value = new LinearLayout.LayoutParams(width, height);
        value.bottomMargin = dp(bottom);
        return value;
    }

    private View preview() {
        LinearLayout content = column();
        content.addView(label(tr("Vista previa de la paleta", "Palette preview", "Prévia da paleta"),
            13, SettingsPanel.SECONDARY), spaced(-1, -2, 8));
        LinearLayout row = new LinearLayout(activity);
        row.setOrientation(LinearLayout.HORIZONTAL);
        String[] names = {tr("Sombra", "Shadow", "Sombra"), tr("Base", "Base", "Base"),
            tr("Luz", "Highlight", "Luz")};
        for (int index = 0; index < 3; ++index) {
            LinearLayout item = column();
            swatches[index] = new View(activity);
            swatches[index].setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
            item.addView(swatches[index], new LinearLayout.LayoutParams(-1, dp(42)));
            TextView name = label(names[index], 12, SettingsPanel.SECONDARY);
            name.setGravity(Gravity.CENTER);
            item.addView(name, spaced(-1, -2, 0));
            shadeLabels[index] = label("", 12, SettingsPanel.TEXT);
            shadeLabels[index].setTypeface(Typeface.MONOSPACE);
            shadeLabels[index].setGravity(Gravity.CENTER);
            item.addView(shadeLabels[index]);
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, -2, 1);
            if (index < 2) params.rightMargin = dp(8);
            row.addView(item, params);
        }
        content.addView(row);
        return content;
    }

    private View codeEditor() {
        LinearLayout content = column();
        content.setFocusableInTouchMode(true); // The keyboard only opens after a deliberate tap.
        TextView title = label(tr("Código HEX", "HEX code", "Código HEX"), 14, SettingsPanel.TEXT);
        content.addView(title, spaced(-1, -2, 6));
        LinearLayout row = new LinearLayout(activity);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        hex = new EditText(activity);
        hex.setId(View.generateViewId()); title.setLabelFor(hex.getId());
        hex.setTextSize(19); hex.setTextColor(SettingsPanel.TEXT);
        hex.setHintTextColor(SettingsPanel.SECONDARY);
        hex.setTypeface(Typeface.MONOSPACE); hex.setSingleLine(true);
        hex.setSelectAllOnFocus(true); hex.setHint("#RRGGBB");
        hex.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD
            | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS | InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS);
        hex.setFilters(new InputFilter[]{new InputFilter.LengthFilter(32)});
        hex.setImeOptions(EditorInfo.IME_ACTION_DONE);
        hex.setPadding(dp(12), dp(8), dp(12), dp(8));
        hex.setBackground(shape(SettingsPanel.SURFACE, SettingsPanel.BORDER, 12));
        row.addView(hex, new LinearLayout.LayoutParams(0, dp(50), 1));
        copy = button(tr("Copiar", "Copy", "Copiar"));
        LinearLayout.LayoutParams copySize = new LinearLayout.LayoutParams(-2, dp(50));
        copySize.leftMargin = dp(8);
        row.addView(copy, copySize);
        copy.setOnClickListener(v -> copyCode());
        content.addView(row);
        content.addView(label(tr("Acepta #RRGGBB o #RGB, con o sin #.",
            "Accepts #RRGGBB or #RGB, with or without #.",
            "Aceita #RRGGBB ou #RGB, com ou sem #."), 12, SettingsPanel.SECONDARY), spaced(-1, -2, 2));
        error = label("", 13, 0xffffd48a);
        error.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        error.setVisibility(View.GONE);
        content.addView(error);
        hex.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                if (updating || !alive()) return;
                draft.editHex(s.toString());
                refresh(false);
            }
            public void afterTextChanged(Editable value) {}
        });
        hex.setOnEditorActionListener((view, action, event) -> {
            if (action != EditorInfo.IME_ACTION_DONE) return false;
            hideKeyboard();
            hex.clearFocus();
            return true;
        });
        content.requestFocus();
        return content;
    }

    private Button button(String title) {
        Button value = new Button(activity);
        value.setText(title); value.setTextSize(14); value.setAllCaps(false);
        value.setTextColor(SettingsPanel.TEXT); value.setMinHeight(dp(48));
        value.setMinWidth(0); value.setMinimumWidth(0);
        value.setPadding(dp(12), dp(8), dp(12), dp(8));
        value.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33a5e775),
            shape(SettingsPanel.SURFACE, SettingsPanel.BORDER, 12), null));
        return value;
    }

    private View colorControls() {
        LinearLayout content = column();
        LinearLayout tabs = new LinearLayout(activity);
        tabs.setOrientation(LinearLayout.HORIZONTAL);
        Button palette = button(tr("Paleta · 48", "Palette · 48", "Paleta · 48"));
        Button custom = button(tr("Ajustar RGB", "Adjust RGB", "Ajustar RGB"));
        LinearLayout.LayoutParams first = new LinearLayout.LayoutParams(0, dp(48), 1);
        first.rightMargin = dp(8);
        tabs.addView(palette, first);
        tabs.addView(custom, new LinearLayout.LayoutParams(0, dp(48), 1));
        content.addView(tabs, spaced(-1, -2, 8));
        View grid = paletteGrid();
        View channels = rgbControls();
        content.addView(grid); content.addView(channels);
        Runnable showPalette = () -> {
            grid.setVisibility(View.VISIBLE); channels.setVisibility(View.GONE);
            tabState(palette, true); tabState(custom, false); hideKeyboard();
        };
        Runnable showChannels = () -> {
            grid.setVisibility(View.GONE); channels.setVisibility(View.VISIBLE);
            tabState(palette, false); tabState(custom, true); hideKeyboard();
        };
        palette.setOnClickListener(v -> showPalette.run());
        custom.setOnClickListener(v -> showChannels.run());
        grid.setVisibility(View.VISIBLE); channels.setVisibility(View.GONE);
        tabState(palette, true); tabState(custom, false);
        return content;
    }

    private void tabState(Button button, boolean selected) {
        button.setSelected(selected);
        button.setTypeface(selected ? Typeface.DEFAULT_BOLD : Typeface.DEFAULT);
        button.setTextColor(selected ? SettingsPanel.ACCENT : SettingsPanel.SECONDARY);
        button.setBackground(new RippleDrawable(ColorStateList.valueOf(0x22a5e775),
            shape(selected ? 0xff28432d : SettingsPanel.SURFACE,
                selected ? SettingsPanel.ACCENT : SettingsPanel.BORDER, 12), null));
    }

    private View paletteGrid() {
        int[] colors = YoshiColorModel.presets();
        presetButtons = new Button[colors.length];
        for (int index = 0; index < colors.length; ++index) {
            final int color = colors[index];
            Button item = button("");
            item.setTextSize(22); item.setTypeface(Typeface.DEFAULT_BOLD);
            item.setPadding(0, 0, 0, 0);
            item.setContentDescription(tr("Elegir color ", "Choose color ", "Escolher cor ") + YoshiColorModel.hex(color));
            item.setOnClickListener(v -> {
                if (!showing()) return;
                draft.choose(color); refresh(true); hideKeyboard();
            });
            presetButtons[index] = item;
        }
        return new PresetGrid();
    }

    /** Reflow to retain 48 dp touch targets on narrow screens and after rotation. */
    private final class PresetGrid extends GridLayout {
        private int columns;
        PresetGrid() { super(activity); setUseDefaultMargins(false); }
        @Override protected void onMeasure(int widthSpec, int heightSpec) {
            int available = View.MeasureSpec.getSize(widthSpec);
            int count = Math.max(1, Math.min(8, available / dp(52)));
            if (columns != count) {
                removeAllViews(); columns = count; setColumnCount(columns);
                for (int index = 0; index < presetButtons.length; ++index) {
                    GridLayout.LayoutParams size = new GridLayout.LayoutParams(
                        GridLayout.spec(index / columns), GridLayout.spec(index % columns, 1f));
                    size.width = 0; size.height = dp(48);
                    size.setMargins(dp(2), dp(2), dp(2), dp(2));
                    addView(presetButtons[index], size);
                }
            }
            super.onMeasure(widthSpec, heightSpec);
        }
    }

    private String channelName(int index) {
        return index == 0 ? tr("Rojo", "Red", "Vermelho")
            : index == 1 ? tr("Verde", "Green", "Verde") : tr("Azul", "Blue", "Azul");
    }

    private View rgbControls() {
        LinearLayout content = column();
        content.setPadding(dp(12), dp(12), dp(12), dp(8));
        content.setBackground(shape(SettingsPanel.SURFACE, SettingsPanel.BORDER, 14));
        int[] accents = {0xffff8b8b, 0xff91e39f, 0xff8dbdff};
        for (int index = 0; index < 3; ++index) {
            final int channel = index;
            channelLabels[index] = label("", 15, SettingsPanel.TEXT);
            content.addView(channelLabels[index]);
            SeekBar bar = new SeekBar(activity);
            bar.setId(View.generateViewId()); channelLabels[index].setLabelFor(bar.getId());
            bar.setMin(0); bar.setMax(255);
            bar.setThumbTintList(ColorStateList.valueOf(accents[index]));
            bar.setProgressTintList(ColorStateList.valueOf(accents[index]));
            bar.setProgressBackgroundTintList(ColorStateList.valueOf(SettingsPanel.BORDER));
            bar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                public void onProgressChanged(SeekBar value, int progress, boolean fromUser) {
                    if (!fromUser || updating || !showing()) return;
                    draft.channel(channel, progress); refresh(true);
                }
                public void onStartTrackingTouch(SeekBar value) { hideKeyboard(); }
                public void onStopTrackingTouch(SeekBar value) {}
            });
            content.addView(bar, new LinearLayout.LayoutParams(-1, dp(48)));
            sliders[index] = bar;
        }
        return content;
    }

    private void refresh(boolean replaceHex) {
        updating = true;
        try {
            if (replaceHex) { hex.setText(YoshiColorModel.hex(draft.rgb())); hex.setSelection(hex.length()); }
            for (int index = 0; index < 3; ++index) {
                int channel = (draft.rgb() >> ((2 - index) * 8)) & 255;
                channelLabels[index].setText(channelName(index) + " · " + channel);
                sliders[index].setProgress(channel);
                sliders[index].setContentDescription(channelName(index) + ", " + channel + " / 255");
                int sample = YoshiColorModel.previewRgb(draft.rgb(), index - 1);
                swatches[index].setBackground(shape(0xff000000 | sample, SettingsPanel.BORDER, 10));
                shadeLabels[index].setText(YoshiColorModel.hex(sample));
            }
            int[] colors = YoshiColorModel.presets();
            for (int index = 0; index < colors.length; ++index) {
                int color = colors[index];
                boolean selected = draft.canApply() && draft.rgb() == color;
                Button item = presetButtons[index];
                item.setSelected(selected); item.setText(selected ? "✓" : "");
                int luminance = ((color >> 16) & 255) * 299 + ((color >> 8) & 255) * 587 + (color & 255) * 114;
                int contrast = luminance >= 150000 ? 0xff101810 : 0xffffffff;
                item.setTextColor(contrast);
                GradientDrawable fill = shape(0xff000000 | color, selected ? contrast : 0x558a9c8e, 10);
                if (selected) fill.setStroke(dp(3), contrast);
                item.setBackground(new RippleDrawable(ColorStateList.valueOf(0x55ffffff), fill, null));
            }
            panel.setNavigationEnabled(draft.canApply());
            copy.setEnabled(draft.canApply()); copy.setAlpha(draft.canApply() ? 1f : .45f);
            showError(draft.canApply() ? "" : tr("Escribe 3 o 6 dígitos hexadecimales (0–9, A–F).",
                "Enter 3 or 6 hexadecimal digits (0–9, A–F).", "Digite 3 ou 6 dígitos hexadecimais (0–9, A–F)."));
        } finally { updating = false; }
    }

    private void showError(String value) {
        error.setText(value); error.setVisibility(value.isEmpty() ? View.GONE : View.VISIBLE);
    }

    private void save(YoshiColorModel.Selection requested) {
        if (!showing() || saving) return;
        saving = true;
        SharedPreferences prefs = activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE);
        YoshiColorModel.SaveResult result = YoshiColorModel.save(requested, draft.initial(),
            value -> prefs.edit().putBoolean(ENABLED_KEY, value.enabled).putInt(RGB_KEY, value.rgb).commit(),
            value -> GbaNative.setYoshiColor(value.enabled, value.rgb));
        saving = false;
        if (result == YoshiColorModel.SaveResult.SAVED) {
            hideKeyboard();
            saved.accept(requested.enabled
                ? tr("Color de Yoshi aplicado: ", "Yoshi color applied: ", "Cor do Yoshi aplicada: ") + YoshiColorModel.hex(requested.rgb)
                : tr("Color original de Yoshi restaurado.", "Yoshi's original color restored.", "Cor original do Yoshi restaurada."));
        } else if (result == YoshiColorModel.SaveResult.NATIVE_UNAVAILABLE) {
            showError(tr("Espera a que aparezca el juego y vuelve a intentarlo. El color no se guardó.",
                "Wait for the game to appear and try again. The color was not saved.",
                "Espere o jogo aparecer e tente novamente. A cor não foi salva."));
        } else {
            showError(result == YoshiColorModel.SaveResult.SAVE_FAILED
                ? tr("No se pudo guardar el color. Se ha restaurado la selección anterior.",
                     "The color could not be saved. The previous selection was restored.",
                     "Não foi possível salvar a cor. A seleção anterior foi restaurada.")
                : tr("No se pudo confirmar el guardado. Revisa el espacio disponible y vuelve a aplicar tu elección.",
                     "Saving could not be confirmed. Check available storage and apply your choice again.",
                     "Não foi possível confirmar o salvamento. Confira o espaço disponível e aplique sua escolha novamente."));
        }
    }

    private void copyCode() {
        if (!showing() || !draft.canApply()) return;
        ClipboardManager clipboard = (ClipboardManager) activity.getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard == null) return;
        clipboard.setPrimaryClip(ClipData.newPlainText(tr("Color de Yoshi", "Yoshi color", "Cor do Yoshi"),
            YoshiColorModel.hex(draft.rgb())));
        Toast.makeText(activity, tr("Código copiado", "Code copied", "Código copiado"), Toast.LENGTH_SHORT).show();
    }

    private void hideKeyboard() {
        if (hex == null) return;
        InputMethodManager keyboard = (InputMethodManager) activity.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (keyboard != null) keyboard.hideSoftInputFromWindow(hex.getWindowToken(), 0);
    }

    private void back() {
        if (!alive()) return;
        hideKeyboard(); parent.run();
    }

    void close() { closed = true; }
}
