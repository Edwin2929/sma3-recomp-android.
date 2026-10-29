package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.GridLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.GraphicsMode;
import org.robolectric.android.controller.ActivityController;

import java.io.File;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.*;

/** Android host UI checks. JNI is a controlled stand-in; this is not a phone test. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 34, qualifiers = "w420dp-h900dp-xhdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public final class YoshiColorUiTest {
    private ActivityController<Activity> controller;
    private Activity activity;
    private AlertDialog shown;
    private YoshiColorUi ui;
    private int backs, saves;
    private String notice;

    @Before public void setup() {
        controller = Robolectric.buildActivity(Activity.class).setup().visible();
        activity = controller.get();
        activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit().clear().commit();
        GbaNative.reset();
    }

    @After public void cleanup() {
        if (ui != null) ui.close();
        if (shown != null) shown.dismiss();
        controller.pause().stop().destroy();
    }

    private void open() {
        if (ui != null) ui.close();
        if (shown != null) shown.dismiss();
        ui = new YoshiColorUi(activity, (es, en, pt) -> es, (dialog, parent) -> {
            shown = dialog;
            dialog.setOnCancelListener(ignored -> parent.run());
            dialog.show();
            SettingsPanel.fitWindow(activity, dialog);
        }, () -> { ++backs; shown.dismiss(); }, text -> { ++saves; notice = text; shown.dismiss(); });
        ui.show();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    private List<View> descendants(View root) {
        List<View> views = new ArrayList<>();
        views.add(root);
        if (root instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) root;
            for (int index = 0; index < group.getChildCount(); ++index) views.addAll(descendants(group.getChildAt(index)));
        }
        return views;
    }

    private <T extends View> T first(Class<T> type) {
        for (View view : descendants(shown.getWindow().getDecorView())) if (type.isInstance(view)) return type.cast(view);
        throw new AssertionError("Missing " + type.getName());
    }

    private Button button(String text) {
        for (View view : descendants(shown.getWindow().getDecorView()))
            if (view instanceof Button && ((Button) view).getText().toString().equals(text)) return (Button) view;
        throw new AssertionError("Missing button " + text);
    }

    private View panel() {
        for (View view : descendants(shown.getWindow().getDecorView()))
            if (view.getClass().getName().endsWith("SettingsPanel$BoundedPanel")) return view;
        throw new AssertionError("Missing actual settings panel");
    }

    private int dp(int value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }
    private View measure(int widthDp, int heightDp) {
        View root = panel();
        root.measure(View.MeasureSpec.makeMeasureSpec(dp(widthDp), View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(dp(heightDp), View.MeasureSpec.AT_MOST));
        root.layout(0, 0, root.getMeasuredWidth(), root.getMeasuredHeight());
        return root;
    }

    private int topWithin(View view, View root) {
        int value = 0;
        while (view != root) { value += view.getTop(); view = (View) view.getParent(); }
        return value;
    }

    private void assertFooter(View root) {
        for (String name : new String[]{"Cancelar", "Aplicar color"}) {
            Button value = button(name);
            assertTrue(name + " remains inside panel", topWithin(value, root) >= 0
                && topWithin(value, root) + value.getHeight() <= root.getHeight());
            assertTrue(name + " touch height", value.getHeight() >= dp(48));
            View ancestor = value;
            while (ancestor != root) {
                assertFalse(name + " must remain outside scrolling body", ancestor instanceof ScrollView);
                ancestor = (View) ancestor.getParent();
            }
        }
    }

    private void capture(View view, String name) throws Exception {
        File directory = new File(System.getProperty("sma3.ui.captures"));
        assertTrue(directory.isDirectory() || directory.mkdirs());
        Bitmap bitmap = Bitmap.createBitmap(view.getWidth(), view.getHeight(), Bitmap.Config.ARGB_8888);
        view.draw(new Canvas(bitmap));
        try (FileOutputStream output = new FileOutputStream(new File(directory, name + ".png"))) {
            assertTrue(bitmap.compress(Bitmap.CompressFormat.PNG, 100, output));
        }
        bitmap.recycle();
    }

    @Test public void realLayoutsReflowAndKeepFooterOutsideScroll() throws Exception {
        open();
        View root = measure(360, 760);
        assertFooter(root);
        GridLayout grid = first(GridLayout.class);
        assertEquals(48, grid.getChildCount());
        int portraitColumns = grid.getColumnCount();
        for (int index = 0; index < grid.getChildCount(); ++index) {
            assertTrue("palette target width", grid.getChildAt(index).getWidth() >= dp(48));
            assertTrue("palette target height", grid.getChildAt(index).getHeight() >= dp(48));
            assertTrue("preset has spoken HEX", grid.getChildAt(index).getContentDescription().toString().contains("#"));
        }
        capture(root, "yoshi-color-portrait");
        root = measure(600, 328);
        assertFooter(root);
        boolean wideEditor = false;
        for (View view : descendants(root)) if (view.getClass().getName().endsWith("YoshiColorUi$ResponsiveEditor"))
            wideEditor = ((LinearLayout) view).getOrientation() == LinearLayout.HORIZONTAL;
        assertTrue("landscape exposes code and controls in two columns", wideEditor);
        assertTrue("palette adapts to its column width", grid.getColumnCount() != portraitColumns);
        capture(root, "yoshi-color-landscape");
        root = measure(264, 560);
        assertFooter(root);
        assertTrue("palette responds to narrow width", grid.getColumnCount() < portraitColumns);
        for (int index = 0; index < grid.getChildCount(); ++index)
            assertTrue("narrow palette retains target width", grid.getChildAt(index).getWidth() >= dp(48));
        button("Ajustar RGB").performClick();
        root = measure(360, 760);
        assertFooter(root);
        capture(root, "yoshi-color-rgb");
        root = measure(360, 240); // Offered space after a keyboard or split-screen resize.
        assertFooter(root);
        assertTrue(root.getHeight() <= dp(240));
        assertEquals(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE,
            shown.getWindow().getAttributes().softInputMode & WindowManager.LayoutParams.SOFT_INPUT_MASK_ADJUST);
        capture(root, "yoshi-color-reduced-height");
        assertEquals("layout/rotation never publishes a draft", 0, GbaNative.calls);
    }

    @Test public void editCancelApplyAndRestoreUseRealViewCallbacks() {
        open();
        first(EditText.class).setText("#a7f");
        assertTrue(button("Aplicar color").isEnabled());
        assertEquals(0, GbaNative.calls);
        assertFalse(YoshiColorUi.read(activity).enabled);
        button("Cancelar").performClick();
        assertEquals(1, backs);
        assertEquals(0, saves);
        assertEquals(0, GbaNative.calls);
        open();
        assertEquals("#39E739", first(EditText.class).getText().toString());
        first(EditText.class).setText("#12");
        assertFalse(button("Aplicar color").isEnabled());
        assertFalse(button("Copiar").isEnabled());
        shown.cancel();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
        assertEquals("Android Back discards unfinished edit", 2, backs);
        assertEquals(0, GbaNative.calls);
        open();
        first(EditText.class).setText("#a7f");
        button("Aplicar color").performClick();
        assertEquals(1, saves);
        assertEquals(1, GbaNative.calls);
        assertTrue(GbaNative.enabled);
        assertEquals(0xaa77ff, GbaNative.rgb);
        assertTrue(YoshiColorUi.read(activity).enabled);
        assertEquals(0xaa77ff, YoshiColorUi.read(activity).rgb);
        assertTrue(notice.contains("#AA77FF"));
        open();
        assertEquals("#AA77FF", first(EditText.class).getText().toString());
        first(EditText.class).setText("#ff0000");
        for (View view : descendants(shown.getWindow().getDecorView())) {
            if (view.getContentDescription() != null && view.getContentDescription().toString().startsWith("Usar color original.")) {
                view.performClick(); break;
            }
        }
        assertEquals(2, saves);
        assertFalse(YoshiColorUi.read(activity).enabled);
        assertEquals("Original remembers saved color, not unfinished draft", 0xaa77ff, YoshiColorUi.read(activity).rgb);
        assertFalse(GbaNative.enabled);
        assertEquals(0xaa77ff, GbaNative.rgb);
    }

    @Test public void nativeErrorAndDestroyedPageDoNotReportSuccess() {
        open();
        first(EditText.class).setText("#123456");
        GbaNative.fail = true;
        button("Aplicar color").performClick();
        assertEquals(0, saves);
        assertFalse(YoshiColorUi.read(activity).enabled);
        assertTrue(shown.isShowing());
        GbaNative.fail = false;
        ui.close();
        button("Aplicar color").performClick();
        assertEquals("closed helper cannot publish", 1, GbaNative.calls);
        assertEquals(0, saves);
    }

    @Test public void malformedPreferencesFallBackSafelyAndStartupUsesOnlySavedValue() {
        activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit()
            .putString(YoshiColorUi.RGB_KEY, "broken").putBoolean(YoshiColorUi.ENABLED_KEY, true).commit();
        assertFalse(YoshiColorUi.read(activity).enabled);
        assertEquals(YoshiColorModel.DEFAULT_RGB, YoshiColorUi.read(activity).rgb);
        activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit()
            .putInt(YoshiColorUi.RGB_KEY, 0x112233).putBoolean(YoshiColorUi.ENABLED_KEY, true).commit();
        open();
        first(EditText.class).setText("#ee00ff");
        YoshiColorUi.applySaved(activity);
        assertEquals(1, GbaNative.calls);
        assertEquals("resume/startup cannot commit open draft", 0x112233, GbaNative.rgb);
        assertTrue(GbaNative.enabled);
    }
}
