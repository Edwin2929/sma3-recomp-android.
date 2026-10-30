package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.res.ColorStateList;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.graphics.drawable.StateListDrawable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.RelativeSizeSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.StyleSpan;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;

import java.util.function.Consumer;
import java.util.function.IntConsumer;

/** Native, scrollable settings with a fixed navigation button and shared styling. */
final class SettingsPanel {
    static final int BACKGROUND = 0xff111b18;
    static final int SURFACE = 0xff1d2c25;
    static final int BORDER = 0xff35483c;
    static final int TEXT = 0xfff2f7ef;
    static final int SECONDARY = 0xffb5c8ba;
    static final int ACCENT = 0xffa5e775;
    private static final int SELECTED = 0xff28432d;
    private static final int WARNING = 0xffffd48a;
    static final int IMAGE = 0, PERFORMANCE = 1, CONTROLS = 2, LANGUAGE = 3, LAYOUT = 4, MODS = 5, UPDATES = 6, ACHIEVEMENTS = 7, TIME_TRIAL = 8, BUG_REPORT = 9;

    private final Activity activity;
    private final LinearLayout root;
    private final LinearLayout body;
    private final Button navigation;
    private Runnable navigationAction;

    SettingsPanel(Activity activity, String title, String subtitle, String buttonText) {
        this.activity = activity;
        root = new BoundedPanel(activity);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(20), dp(20), dp(20), dp(16));
        root.setBackground(shape(BACKGROUND, BORDER, 24));

        TextView heading = text(title, 25, TEXT);
        heading.setTypeface(Typeface.DEFAULT_BOLD);
        heading.setAccessibilityHeading(true);
        root.addView(heading, space(0, 0, 0, 12));

        ScrollView scroll = new ScrollView(activity);
        scroll.setClipToPadding(false);
        scroll.setFillViewport(false);
        scroll.setOverScrollMode(View.OVER_SCROLL_IF_CONTENT_SCROLLS);
        scroll.setVerticalScrollBarEnabled(false);
        body = new LinearLayout(activity);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(0, dp(2), 0, dp(10));
        if (subtitle != null && !subtitle.isEmpty()) {
            TextView description = text(subtitle, 14, SECONDARY);
            body.addView(description, space(0, 0, 0, 14));
        }
        scroll.addView(body, new ScrollView.LayoutParams(-1, -2));
        // The weighted WRAP_CONTENT scroll view shrinks when the screen is short;
        // the header and navigation button remain reachable in either orientation.
        root.addView(scroll, new LinearLayout.LayoutParams(-1, -2, 1));

        navigation = new Button(activity);
        navigation.setText(buttonText);
        navigation.setTextSize(15);
        navigation.setTypeface(Typeface.DEFAULT_BOLD);
        navigation.setAllCaps(false);
        navigation.setTextColor(BACKGROUND);
        navigation.setMinHeight(dp(50));
        navigation.setPadding(dp(16), dp(10), dp(16), dp(10));
        navigation.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33000000),
            shape(ACCENT, ACCENT, 14), null));
        root.addView(navigation, space(0, 8, 0, 0));
    }

    private int dp(int value) {
        return Math.round(value * activity.getResources().getDisplayMetrics().density);
    }

    private TextView text(String value, int size, int color) {
        TextView text = new TextView(activity);
        text.setText(value);
        text.setTextSize(size);
        text.setTextColor(color);
        text.setLineSpacing(dp(2), 1f);
        return text;
    }

    private LinearLayout.LayoutParams space(int left, int top, int right, int bottom) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(-1, -2);
        params.setMargins(dp(left), dp(top), dp(right), dp(bottom));
        return params;
    }

    private GradientDrawable shape(int fill, int stroke, int radius) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(fill);
        drawable.setCornerRadius(dp(radius));
        drawable.setStroke(dp(1), stroke);
        return drawable;
    }

    void section(String title) {
        TextView label = text(title, 13, SECONDARY);
        label.setTypeface(Typeface.DEFAULT_BOLD);
        label.setAccessibilityHeading(true);
        body.addView(label, space(2, 12, 0, 8));
    }

    TextView notice(String title, String message) {
        TextView note = text(title + "\n" + message, 13, WARNING);
        SpannableString content = new SpannableString(title + "\n" + message);
        content.setSpan(new StyleSpan(Typeface.BOLD), 0, title.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        note.setText(content);
        note.setPadding(dp(14), dp(12), dp(14), dp(12));
        note.setBackground(shape(0xff332c1d, 0xff695437, 12));
        body.addView(note, space(0, 0, 0, 10));
        return note;
    }

    void paragraph(String message) {
        TextView text = text(message, 14, SECONDARY);
        body.addView(text, space(2, 4, 2, 12));
    }

    /** A custom editor remains inside the same bounded, scrollable body. */
    void content(View view) {
        body.addView(view, space(0, 0, 0, 10));
    }

    /** Optional second footer action; both buttons remain outside the scroll view. */
    void secondaryNavigation(String title, Runnable action) {
        root.removeView(navigation);
        LinearLayout row = new LinearLayout(activity);
        row.setOrientation(LinearLayout.HORIZONTAL);
        Button secondary = new Button(activity);
        secondary.setText(title);
        secondary.setTextSize(15);
        secondary.setAllCaps(false);
        secondary.setTextColor(TEXT);
        secondary.setMinHeight(dp(50));
        secondary.setPadding(dp(8), dp(10), dp(8), dp(10));
        secondary.setBackground(new RippleDrawable(ColorStateList.valueOf(0x22a5e775),
            shape(SURFACE, BORDER, 14), null));
        secondary.setOnClickListener(v -> action.run());
        LinearLayout.LayoutParams left = new LinearLayout.LayoutParams(0, -2, 1);
        left.setMargins(0, 0, dp(8), 0);
        row.addView(secondary, left);
        row.addView(navigation, new LinearLayout.LayoutParams(0, -2, 1));
        root.addView(row, space(0, 8, 0, 0));
    }

    void setNavigationEnabled(boolean enabled) {
        navigation.setEnabled(enabled);
        navigation.setAlpha(enabled ? 1f : .45f);
    }

    void card(int icon, String title, String description, Runnable action) {
        LinearLayout row = new LinearLayout(activity);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(14), dp(14), dp(14), dp(14));
        row.setMinimumHeight(dp(76));
        row.setBackground(new RippleDrawable(ColorStateList.valueOf(0x22a5e775),
            shape(SURFACE, BORDER, 16), null));
        row.setFocusable(true);
        row.setContentDescription(title + ". " + description);
        row.setOnClickListener(v -> action.run());

        View symbol = new Glyph(activity, icon);
        symbol.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        row.addView(symbol, new LinearLayout.LayoutParams(dp(36), dp(36)));
        LinearLayout labels = new LinearLayout(activity);
        labels.setOrientation(LinearLayout.VERTICAL);
        labels.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        TextView name = text(title, 17, TEXT);
        name.setTypeface(Typeface.DEFAULT_BOLD);
        labels.addView(name);
        if (description != null && !description.isEmpty()) {
            labels.addView(text(description, 13, SECONDARY), space(0, 4, 0, 0));
        }
        LinearLayout.LayoutParams labelSize = new LinearLayout.LayoutParams(0, -2, 1);
        labelSize.setMargins(dp(12), 0, dp(8), 0);
        row.addView(labels, labelSize);
        TextView arrow = text("›", 25, ACCENT);
        arrow.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        row.addView(arrow);
        body.addView(row, space(0, 0, 0, 10));
    }

    void toggle(String title, String description, boolean checked, Consumer<Boolean> change) {
        Switch control = new Switch(activity);
        control.setText(twoLines(title, description));
        control.setTextColor(TEXT);
        control.setTextSize(16);
        control.setShowText(false);
        control.setSwitchPadding(dp(16));
        control.setMinHeight(dp(72));
        control.setPadding(dp(16), dp(14), dp(16), dp(14));
        control.setBackground(new RippleDrawable(ColorStateList.valueOf(0x22a5e775),
            shape(SURFACE, BORDER, 16), null));
        control.setThumbTintList(new ColorStateList(
            new int[][]{new int[]{android.R.attr.state_checked}, new int[]{}},
            new int[]{ACCENT, SECONDARY}));
        control.setTrackTintList(new ColorStateList(
            new int[][]{new int[]{android.R.attr.state_checked}, new int[]{}},
            new int[]{0xff568045, 0xff4a5c50}));
        control.setChecked(checked);
        control.setOnCheckedChangeListener((button, enabled) -> change.accept(enabled));
        body.addView(control, space(0, 0, 0, 10));
    }

    void slider(String title, String description, int minimum, int maximum, int value,
                IntConsumer change) {
        if (minimum > maximum) throw new IllegalArgumentException("Invalid slider range");
        int selected = Math.max(minimum, Math.min(maximum, value));
        LinearLayout card = new LinearLayout(activity);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(16), dp(14), dp(16), dp(8));
        card.setBackground(shape(SURFACE, BORDER, 16));
        TextView label = text(title + " · " + selected + " %", 17, TEXT);
        label.setTypeface(Typeface.DEFAULT_BOLD);
        card.addView(label);
        if (description != null && !description.isEmpty())
            card.addView(text(description, 13, SECONDARY), space(0, 4, 0, 4));
        SeekBar slider = new SeekBar(activity);
        slider.setMin(minimum);
        slider.setMax(maximum);
        slider.setProgress(selected);
        slider.setMinimumHeight(dp(48));
        slider.setThumbTintList(ColorStateList.valueOf(ACCENT));
        slider.setProgressTintList(ColorStateList.valueOf(ACCENT));
        slider.setProgressBackgroundTintList(ColorStateList.valueOf(BORDER));
        slider.setContentDescription(title + ", " + selected + " %");
        slider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onProgressChanged(SeekBar bar, int progress, boolean fromUser) {
                label.setText(title + " · " + progress + " %");
                bar.setContentDescription(title + ", " + progress + " %");
                if (fromUser) change.accept(progress);
            }
            public void onStartTrackingTouch(SeekBar bar) {}
            public void onStopTrackingTouch(SeekBar bar) {}
        });
        card.addView(slider, new LinearLayout.LayoutParams(-1, dp(48)));
        body.addView(card, space(0, 0, 0, 10));
    }

    void choices(String[] titles, String[] descriptions, int selected, IntConsumer choose) {
        RadioGroup group = new RadioGroup(activity);
        group.setOrientation(LinearLayout.VERTICAL);
        for (int index = 0; index < titles.length; ++index) {
            RadioButton button = new RadioButton(activity);
            button.setId(View.generateViewId());
            button.setText(twoLines(titles[index], descriptions[index]));
            button.setTextColor(TEXT);
            button.setTextSize(16);
            button.setGravity(Gravity.CENTER_VERTICAL);
            button.setMinHeight(dp(64));
            button.setPadding(dp(12), dp(13), dp(12), dp(13));
            button.setCompoundDrawablePadding(dp(8));
            button.setButtonTintList(new ColorStateList(
                new int[][]{new int[]{android.R.attr.state_checked}, new int[]{}},
                new int[]{ACCENT, SECONDARY}));
            StateListDrawable surfaces = new StateListDrawable();
            surfaces.addState(new int[]{android.R.attr.state_checked}, shape(SELECTED, ACCENT, 14));
            surfaces.addState(new int[]{}, shape(SURFACE, BORDER, 14));
            button.setBackground(new RippleDrawable(ColorStateList.valueOf(0x22a5e775), surfaces, null));
            RadioGroup.LayoutParams size = new RadioGroup.LayoutParams(-1, -2);
            size.setMargins(0, 0, 0, dp(8));
            group.addView(button, size);
            if (index == selected) group.check(button.getId());
        }
        group.setOnCheckedChangeListener((buttons, id) -> {
            int index = buttons.indexOfChild(buttons.findViewById(id));
            if (index >= 0 && index < titles.length) choose.accept(index);
        });
        body.addView(group, space(0, 0, 0, 2));
    }

    private CharSequence twoLines(String title, String description) {
        if (description == null || description.isEmpty()) return title;
        SpannableString value = new SpannableString(title + "\n" + description);
        value.setSpan(new StyleSpan(Typeface.BOLD), 0, title.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        value.setSpan(new ForegroundColorSpan(SECONDARY), title.length() + 1, value.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        value.setSpan(new RelativeSizeSpan(.8125f), title.length() + 1, value.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return value;
    }

    AlertDialog dialog(Runnable action) {
        navigationAction = action;
        AlertDialog dialog = new AlertDialog.Builder(activity,
            android.R.style.Theme_Material_Dialog_NoActionBar).create();
        dialog.setView(root, 0, 0, 0, 0);
        navigation.setOnClickListener(v -> {
            if (navigationAction != null) navigationAction.run();
            else dialog.dismiss();
        });
        return dialog;
    }

    static void fitWindow(Activity activity, AlertDialog dialog) {
        Window window = dialog.getWindow();
        if (window == null) return;
        float density = activity.getResources().getDisplayMetrics().density;
        Rect area = visibleArea(activity);
        int gap = Math.round(16 * density);
        window.setBackgroundDrawableResource(android.R.color.transparent);
        window.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
        window.setDimAmount(.72f);
        window.setLayout(Math.max(1, Math.min(Math.round(600 * density), area.width() - 2 * gap)),
            ViewGroup.LayoutParams.WRAP_CONTENT);
        window.setGravity(Gravity.CENTER);
    }

    private static Rect visibleArea(Activity activity) {
        Rect area = new Rect();
        activity.getWindow().getDecorView().getWindowVisibleDisplayFrame(area);
        if (area.width() <= 0 || area.height() <= 0) {
            area.set(0, 0, activity.getResources().getDisplayMetrics().widthPixels,
                activity.getResources().getDisplayMetrics().heightPixels);
        }
        return area;
    }

    private static final class BoundedPanel extends LinearLayout {
        private final Activity activity;
        BoundedPanel(Activity activity) { super(activity); this.activity = activity; }
        @Override protected void onMeasure(int widthSpec, int heightSpec) {
            int margin = Math.round(32 * getResources().getDisplayMetrics().density);
            int cap = Math.max(1, visibleArea(activity).height() - margin);
            int mode = MeasureSpec.getMode(heightSpec);
            int offered = MeasureSpec.getSize(heightSpec);
            if (mode != MeasureSpec.UNSPECIFIED) cap = Math.min(cap, offered);
            super.onMeasure(widthSpec, MeasureSpec.makeMeasureSpec(cap, MeasureSpec.AT_MOST));
        }
    }

    /** Small code-drawn symbols avoid platform-dependent emoji or extra image assets. */
    private static final class Glyph extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final int icon;
        Glyph(Activity context, int icon) { super(context); this.icon = icon; }
        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            canvas.save();
            canvas.scale(getWidth() / 36f, getHeight() / 36f);
            paint.setStyle(Paint.Style.FILL);
            paint.setColor(0xff30432a);
            canvas.drawRoundRect(new RectF(0, 0, 36, 36), 11, 11, paint);
            paint.setColor(ACCENT);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(1.7f);
            paint.setStrokeCap(Paint.Cap.ROUND);
            if (icon == IMAGE) {
                canvas.drawRoundRect(new RectF(7, 9, 29, 25), 3, 3, paint);
                canvas.drawLine(14, 29, 22, 29, paint);
                canvas.drawLine(18, 25, 18, 29, paint);
                canvas.drawLine(11, 20, 16, 15, paint);
                canvas.drawLine(16, 15, 22, 21, paint);
                canvas.drawCircle(24, 14, 1, paint);
            } else if (icon == PERFORMANCE) {
                canvas.drawArc(new RectF(7, 8, 29, 30), 160, 220, false, paint);
                canvas.drawLine(18, 21, 24, 13, paint);
                canvas.drawCircle(18, 21, 2, paint);
            } else if (icon == CONTROLS) {
                canvas.drawRoundRect(new RectF(5, 10, 31, 27), 7, 7, paint);
                canvas.drawLine(10, 18, 17, 18, paint);
                canvas.drawLine(13.5f, 14.5f, 13.5f, 21.5f, paint);
                canvas.drawCircle(23, 16, 1, paint);
                canvas.drawCircle(26, 21, 1, paint);
            } else if (icon == LAYOUT) {
                canvas.drawRoundRect(new RectF(6, 8, 30, 28), 3, 3, paint);
                canvas.drawCircle(12, 22, 3, paint);
                canvas.drawCircle(25, 21, 2, paint);
                canvas.drawLine(20, 13, 26, 13, paint);
            } else if (icon == UPDATES) {
                canvas.drawArc(new RectF(7, 7, 29, 29), 35, 280, false, paint);
                canvas.drawLine(29, 9, 28, 16, paint);
                canvas.drawLine(28, 16, 21, 14, paint);
                canvas.drawLine(18, 12, 18, 25, paint);
                canvas.drawLine(13, 20, 18, 25, paint);
                canvas.drawLine(18, 25, 23, 20, paint);
            } else if (icon == ACHIEVEMENTS) {
                canvas.drawArc(new RectF(11, 5, 25, 25), 0, 180, false, paint);
                canvas.drawLine(11, 5, 11, 15, paint);
                canvas.drawLine(25, 5, 25, 15, paint);
                canvas.drawLine(11, 5, 25, 5, paint);
                canvas.drawArc(new RectF(5, 8, 14, 20), 90, 180, false, paint);
                canvas.drawArc(new RectF(22, 8, 31, 20), -90, 180, false, paint);
                canvas.drawLine(18, 25, 18, 30, paint);
                canvas.drawLine(12, 30, 24, 30, paint);
            } else if (icon == TIME_TRIAL) {
                canvas.drawCircle(18, 21, 10, paint);
                canvas.drawLine(18, 7, 18, 11, paint);
                canvas.drawLine(14, 6, 22, 6, paint);
                canvas.drawLine(25, 11, 28, 8, paint);
                canvas.drawLine(18, 21, 18, 15, paint);
                canvas.drawLine(18, 21, 23, 23, paint);
            } else if (icon == BUG_REPORT) {
                canvas.drawRoundRect(new RectF(8, 6, 28, 30), 3, 3, paint);
                canvas.drawLine(18, 11, 18, 19, paint);
                canvas.drawCircle(18, 23, .8f, paint);
            } else if (icon == MODS) {
                canvas.drawRoundRect(new RectF(7, 10, 29, 29), 3, 3, paint);
                canvas.drawLine(7, 16, 29, 16, paint);
                canvas.drawLine(18, 10, 18, 29, paint);
                canvas.drawArc(new RectF(10, 5, 18, 13), 0, -220, false, paint);
                canvas.drawArc(new RectF(18, 5, 26, 13), 180, 220, false, paint);
            } else {
                canvas.drawCircle(18, 18, 11, paint);
                canvas.drawOval(new RectF(13, 7, 23, 29), paint);
                canvas.drawLine(7, 18, 29, 18, paint);
                canvas.drawLine(10, 12, 26, 12, paint);
                canvas.drawLine(10, 24, 26, 24, paint);
            }
            canvas.restore();
        }
    }
}
