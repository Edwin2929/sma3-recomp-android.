package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.GraphicsMode;

import java.io.File;
import java.io.FileOutputStream;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

import static org.junit.Assert.*;

/** Real Android UI classes on a host framework; JNI fixture, no network or phone. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 34, qualifiers = "w420dp-h900dp-xhdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public final class BugReportUiTest {
    private ActivityController<Activity> controller;
    private Activity activity;
    private AlertDialog shown;
    private BugReportUi ui;
    private int language = 1, backs;

    @Before public void setup() {
        controller = Robolectric.buildActivity(Activity.class).setup().visible();
        activity = controller.get();
        activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit().clear().commit();
        GbaNative.reset();
    }

    @After public void cleanup() {
        GbaNative.release.countDown();
        if (ui != null) ui.close();
        if (shown != null) shown.dismiss();
        controller.pause().stop().destroy(); idle();
    }

    private void idle() { Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(16)); }

    private void open(boolean game) {
        BugReportUi.Translator translator = (es, en, pt) -> language == 0 ? en : language == 2 ? pt : es;
        BugReportUi.Presenter presenter = (dialog, parent) -> {
            if (shown != null) shown.dismiss();
            shown = dialog; dialog.setOnCancelListener(ignored -> parent.run());
            dialog.show(); SettingsPanel.fitWindow(activity, dialog);
        };
        Runnable back = () -> { ++backs; shown.dismiss(); };
        ui = game ? new BugReportUi(activity, translator, presenter, back,
            () -> new BugReportUi.RuntimeInfo(true, PresentationStats.parse("SMA3 — 58.7 fps [base=58.7 extra=0.0 prep=1.2 swap=2.4]"), false))
            : new BugReportUi(activity, translator, presenter, back);
        ui.onResume(); ui.show(); idle();
    }

    private List<View> descendants(View root) {
        List<View> result = new ArrayList<>(); result.add(root);
        if (root instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) root;
            for (int i = 0; i < group.getChildCount(); ++i) result.addAll(descendants(group.getChildAt(i)));
        }
        return result;
    }

    private List<EditText> editors() {
        List<EditText> values = new ArrayList<>();
        for (View view : descendants(shown.getWindow().getDecorView())) if (view instanceof EditText) values.add((EditText)view);
        return values;
    }

    private TextView label(String text) {
        for (View view : descendants(shown.getWindow().getDecorView()))
            if (view instanceof TextView && ((TextView)view).getText().toString().equals(text)) return (TextView)view;
        return null;
    }

    private void click(String text) {
        View target = label(text); assertNotNull("Missing action " + text, target);
        while (!target.isClickable() && target.getParent() instanceof View) target = (View)target.getParent();
        assertTrue("Action is clickable " + text, target.isClickable());
        assertTrue(target.performClick()); idle();
    }

    private String translated(String es, String en, String pt) { return language == 0 ? en : language == 2 ? pt : es; }

    private void prepare() throws Exception {
        click(translated("Vista previa", "Preview", "Prévia"));
        waitPreview();
    }

    private void waitPreview() throws Exception {
        for (int i = 0; i < 300; ++i) {
            idle();
            if (label(translated("Informe completo", "Complete report", "Relatório completo")) != null) return;
            Thread.sleep(2);
        }
        fail("Preview did not arrive");
    }

    private String report() { return editors().get(1).getText().toString(); }

    private String clipboard() {
        ClipboardManager manager = (ClipboardManager) activity.getSystemService(Context.CLIPBOARD_SERVICE);
        assertNotNull(manager.getPrimaryClip());
        return manager.getPrimaryClip().getItemAt(0).getText().toString();
    }

    @Test public void reviewedDraftIsOnlySharedOnTapAndSurvivesReturning() throws Exception {
        open(false);
        List<EditText> inputs = editors();
        assertEquals(4, inputs.size());
        inputs.get(0).setText("Mi problema"); inputs.get(1).setText("Algo pasó");
        inputs.get(2).setText("1. Abrir el juego"); inputs.get(3).setText("Que continúe");
        prepare();
        assertTrue(report().contains("Algo pasó")); assertEquals(0, GbaNative.calls);
        assertNull(Shadows.shadowOf(activity).getNextStartedActivity());
        String reviewed = "Mi informe revisado\nSin datos extra 😃";
        editors().get(1).setText(reviewed);
        click("Volver"); assertFalse(shown.isShowing()); assertEquals(1, backs);
        ui.show(); idle(); assertEquals(reviewed, report());
        click("Compartir…");
        Intent chooser = Shadows.shadowOf(activity).getNextStartedActivity();
        assertEquals(Intent.ACTION_CHOOSER, chooser.getAction());
        Intent send = chooser.getParcelableExtra(Intent.EXTRA_INTENT);
        assertEquals(Intent.ACTION_SEND, send.getAction()); assertEquals("text/plain", send.getType());
        assertEquals(reviewed, send.getStringExtra(Intent.EXTRA_TEXT));
        assertEquals("Mi problema", send.getStringExtra(Intent.EXTRA_SUBJECT));
        assertNull(send.getParcelableExtra(Intent.EXTRA_STREAM));
        ui.onPause(); ui.onResume(); assertEquals(reviewed, report());
        click("Copiar informe"); assertEquals(reviewed, clipboard());
    }

    @Test public void githubPreservesUnicodeAndLargeDraftUsesFullClipboard() throws Exception {
        open(false); prepare();
        editors().get(0).setText("Error & español 😃");
        String shortBody = "Revisado: áéñ + & ? #\nPasos";
        editors().get(1).setText(shortBody); click("Abrir reporte en GitHub");
        Intent open = Shadows.shadowOf(activity).getNextStartedActivity();
        assertEquals(Intent.ACTION_VIEW, open.getAction());
        assertTrue(open.getDataString().startsWith(BugReportModel.ISSUE_URL + "?"));
        assertEquals(shortBody, open.getData().getQueryParameter("body"));
        assertEquals("Error & español 😃", open.getData().getQueryParameter("title"));
        String longBody = "Informe á 😃\n".repeat(1500);
        editors().get(1).setText(longBody); click("Abrir reporte en GitHub");
        Intent fallback = Shadows.shadowOf(activity).getNextStartedActivity();
        assertNull(fallback.getData().getQueryParameter("body"));
        assertEquals(longBody, clipboard());
        assertTrue(fallback.getDataString().length() <= BugReportModel.MAX_ISSUE_URL);
        assertEquals(longBody, report());
    }

    @Test public void automaticDataIsAllowlistedAndModsAreAggregateOnly() throws Exception {
        activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit()
            .putString("account-email", "private@example.invalid").putString("rom-path", "/secret/private.gba")
            .putString("touch-design-1", "secret-layout").putBoolean("interpolation-120", true)
            .putBoolean("time-trial-enabled", true).putBoolean("time-trial-overlay", false).commit();
        GbaNative.catalog = "{\"ok\":true,\"error\":\"secret-error\",\"mods\":["
            + "{\"enabled\":true,\"compatible\":true,\"name\":\"secret-mod\",\"path\":\"/secret/mod\"},"
            + "{\"enabled\":false,\"name\":\"secret-disabled\"}]}";
        open(true); prepare();
        String report = report();
        assertEquals(1, GbaNative.calls);
        assertTrue(report.contains("FPS de presentación solicitados: `120`"));
        assertTrue(report.contains("Última muestra de presentación: `58.7 FPS`"));
        assertTrue(report.contains("Perfiles de imagen instalados / activos: `2 / 1`"));
        assertTrue(report.contains("Contrarreloj: `activado`"));
        assertTrue(report.contains("Cronómetro en pantalla: `desactivado`"));
        assertFalse(report.contains("private@example")); assertFalse(report.contains("secret-"));
        assertFalse(report.contains("/secret/"));
        assertNull(Shadows.shadowOf(activity).getNextStartedActivity());
    }

    @Test public void absentNativeLibraryStillProducesUsefulReport() throws Exception {
        GbaNative.fail = true;
        open(false); prepare();
        assertEquals(0, GbaNative.calls);
        assertTrue(report().contains("Pantalla de origen: `pantalla inicial`"));
        assertTrue(report().contains("Android:"));
        assertFalse(report().contains("null ("));
        ui.close(); shown.dismiss();
        open(true); prepare();
        assertEquals(1, GbaNative.calls);
        assertTrue(report().contains("Perfiles de imagen instalados / activos: `no disponible`"));
    }

    @Test public void abandonedPreparationNeverReopensAnotherScreen() throws Exception {
        GbaNative.block = true; open(true);
        editors().get(1).setText("Mantener mi descripción");
        click("Vista previa"); assertTrue(GbaNative.entered.await(2, TimeUnit.SECONDS));
        click("Volver"); AlertDialog abandoned = shown;
        GbaNative.release.countDown(); assertTrue(GbaNative.finished.await(2, TimeUnit.SECONDS));
        for (int i = 0; i < 10; ++i) { Thread.sleep(2); idle(); }
        assertSame(abandoned, shown); assertFalse(shown.isShowing());
        assertNull(Shadows.shadowOf(activity).getNextStartedActivity());
        ui.show(); idle(); assertEquals(4, editors().size());
        assertEquals("Mantener mi descripción", editors().get(1).getText().toString());
    }

    @Test public void backgroundCompletionWaitsForResumeAndKeepsCapturedSettings() throws Exception {
        GbaNative.block = true; open(true); click("Vista previa");
        assertTrue(GbaNative.entered.await(2, TimeUnit.SECONDS));
        ui.onPause();
        activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit().putBoolean("interpolation-120", true).commit();
        GbaNative.release.countDown(); assertTrue(GbaNative.finished.await(2, TimeUnit.SECONDS));
        for (int i = 0; i < 10; ++i) { Thread.sleep(2); idle(); }
        assertNotNull(label("Preparando informe")); assertNull(label("Informe completo"));
        ui.onResume(); waitPreview();
        assertTrue(report().contains("FPS de presentación solicitados: `60`"));
        assertFalse(report().contains("FPS de presentación solicitados: `120`"));
    }

    @Test public void descriptionsAndActionsUseAllThreeMenuLanguages() throws Exception {
        for (language = 0; language < 3; ++language) {
            activity.getSharedPreferences("sma3-options", Context.MODE_PRIVATE).edit().putInt("menu-language", language).commit();
            open(false); prepare();
            assertTrue(report().startsWith("## " + translated("Qué pasó", "What happened", "O que aconteceu")));
            assertNotNull(label(translated("Compartir…", "Share…", "Compartilhar…")));
            assertNotNull(label(translated("Abrir reporte en GitHub", "Open report on GitHub", "Abrir relato no GitHub")));
            ui.close(); shown.dismiss();
        }
    }

    private View measure(int width, int height) {
        View root = null;
        for (View view : descendants(shown.getWindow().getDecorView()))
            if (view.getClass().getName().endsWith("SettingsPanel$BoundedPanel")) root = view;
        assertNotNull(root);
        float density = activity.getResources().getDisplayMetrics().density;
        root.measure(View.MeasureSpec.makeMeasureSpec(Math.round(width * density), View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(Math.round(height * density), View.MeasureSpec.AT_MOST));
        root.layout(0, 0, root.getMeasuredWidth(), root.getMeasuredHeight());
        return root;
    }

    private void footer(View root) {
        View button = label("Volver"); assertTrue(button instanceof Button);
        int y = 0; View current = button;
        while (current != root) {
            assertFalse(current instanceof ScrollView); y += current.getTop(); current = (View)current.getParent();
        }
        assertTrue(y >= 0 && y + button.getHeight() <= root.getHeight());
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

    @Test public void actualFormAndPreviewKeepBackReachableInBothOrientations() throws Exception {
        open(false);
        View portraitForm = measure(360, 760); footer(portraitForm); capture(portraitForm, "report-form-portrait");
        View landscapeForm = measure(600, 320); footer(landscapeForm); capture(landscapeForm, "report-form-landscape");
        editors().get(0).setText("Error al abrir un nivel");
        editors().get(1).setText("La imagen se quedó quieta al entrar al nivel.");
        prepare();
        View portrait = measure(360, 760); footer(portrait); capture(portrait, "report-preview-portrait");
        View landscape = measure(600, 320); footer(landscape); capture(landscape, "report-preview-landscape");
        assertEquals(2, editors().size());
        for (EditText editor : editors()) assertTrue(editor.getMeasuredWidth() > 0);
    }
}
