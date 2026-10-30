package org.gbarecomp;

import android.app.Activity;
import android.app.AlertDialog;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;

import org.json.JSONObject;
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
import org.robolectric.shadows.ShadowChoreographer;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;

import static org.junit.Assert.*;

/** Real Android UI/store under Robolectric; controlled JNI evidence, no phone/game. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 34, qualifiers = "w420dp-h900dp-xhdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public final class TimeTrialUiTest {
    static final class MemoryDisk implements TimeTrialStore.Persistence {
        TimeTrialModel.State value = TimeTrialModel.empty();
        boolean readFail, writeFail; int writes;
        public TimeTrialModel.State read() throws IOException { if (readFail) throw new IOException("read failure"); return value; }
        public void write(TimeTrialModel.State next) throws IOException {
            ++writes; if (writeFail) throw new IOException("write failure"); value = next;
        }
    }
    private ActivityController<Activity> controller;
    private Activity activity;
    private FrameLayout overlayHost;
    private TextView fps;
    private AlertDialog shown;
    private TimeTrialUi ui;
    private TimeTrialStore store;
    private MemoryDisk disk;
    private long[] nativeValue;
    private int reads, language;
    private boolean requested, ghostRequested;
    private long[] ghostValue, lastSyncedBests;
    private int ghostSyncs;

    @Before public void setup() {
        ShadowChoreographer.setPaused(true); ShadowChoreographer.setFrameDelay(Duration.ofMillis(16));
        controller = Robolectric.buildActivity(Activity.class).setup().visible(); activity = controller.get();
        activity.getSharedPreferences("sma3-options",0).edit().clear().commit();
        overlayHost = new FrameLayout(activity); activity.setContentView(overlayHost);
        fps = new TextView(activity); fps.setText("FPS: 60"); fps.setVisibility(View.GONE);
        overlayHost.addView(fps);
        disk = new MemoryDisk(); store = new TimeTrialStore(disk, Runnable::run, new Handler(Looper.getMainLooper()), bests -> {
            ++ghostSyncs; lastSyncedBests=bests.clone(); return ghostValue == null ? null : ghostValue.clone();
        });
        nativeValue = disabled();
        ghostValue = new long[72]; ghostValue[0]=1; ghostValue[2]=1; ghostValue[5]=2;
    }
    @After public void cleanup() {
        if (ui != null) ui.close(); if (shown != null) shown.dismiss();
        controller.pause().stop().destroy(); advance(32);
    }
    private static long[] disabled() {
        long[] value = new long[74]; value[0]=1; value[12]=280896; value[13]=16777216; return value;
    }
    private static long[] result(int rawCourse, long frames, long sequence, long best) {
        return TimeTrialModelTest.snapshot(rawCourse,frames,sequence,best);
    }
    private void advance(long millis) {
        while (millis > 0) { long step=Math.min(16,millis); Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(step)); millis-=step; }
    }
    private long[] sample() { ++reads; return nativeValue.clone(); }
    private void open() {
        ui = new TimeTrialUi(activity,(es,en,pt)->language==0?es:language==1?en:pt,
            (dialog,parent)->{ if(shown!=null)shown.dismiss();shown=dialog;dialog.show();SettingsPanel.fitWindow(activity,dialog); },
            ()->shown.dismiss(),overlayHost,fps,store,this::sample,value->{requested=value;nativeValue[1]=value?1:0;nativeValue[2]=value?1:0;},
            value->{ghostRequested=value;ghostValue[1]=value?1:0;});
        ui.onResume(); ui.show(); advance(32);
    }
    private List<View> descendants(View root) {
        List<View> values=new ArrayList<>();values.add(root);
        if(root instanceof ViewGroup)for(int i=0;i<((ViewGroup)root).getChildCount();++i)values.addAll(descendants(((ViewGroup)root).getChildAt(i)));
        return values;
    }
    private View panel() {
        for(View view:descendants(shown.getWindow().getDecorView()))if(view.getClass().getName().endsWith("SettingsPanel$BoundedPanel"))return view;
        throw new AssertionError("Missing panel");
    }
    private boolean hasText(String wanted) {
        for(View view:descendants(panel()))if(view instanceof TextView&&((TextView)view).getText().toString().contains(wanted))return true;
        return false;
    }
    private int dp(int value) { return Math.round(value*activity.getResources().getDisplayMetrics().density); }
    private View measure(int width,int height) {
        View p=panel();p.measure(View.MeasureSpec.makeMeasureSpec(dp(width),View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(dp(height),View.MeasureSpec.AT_MOST));
        p.layout(0,0,p.getMeasuredWidth(),p.getMeasuredHeight());return p;
    }
    private void footerVisible(View panel) {
        Button footer=null;
        for(View view:descendants(panel))if(view instanceof Button&&((Button)view).getText().toString().equals(language==0?"Volver":language==1?"Back":"Voltar"))footer=(Button)view;
        assertNotNull(footer);int top=0;View at=footer;
        while(at!=panel){assertFalse(at instanceof ScrollView);top+=at.getTop();at=(View)at.getParent();}
        assertTrue(top>=0&&top+footer.getHeight()<=panel.getHeight());assertTrue(footer.getHeight()>=dp(48));
    }
    private void capture(View view,String name) throws Exception {
        File folder=new File(System.getProperty("sma3.ui.captures"));assertTrue(folder.isDirectory()||folder.mkdirs());
        Bitmap image=Bitmap.createBitmap(view.getWidth(),view.getHeight(),Bitmap.Config.ARGB_8888);view.draw(new Canvas(image));
        try(FileOutputStream out=new FileOutputStream(new File(folder,name+".png"))){assertTrue(image.compress(Bitmap.CompressFormat.PNG,100,out));}image.recycle();
    }
    @Test public void defaultsAndAllWorldsRemainReachable() throws Exception {
        open();assertFalse(requested);assertTrue(hasText("Contrarreloj desactivado"));assertTrue(hasText("0 / 60"));
        for(int world=0;world<6;++world){panel().findViewWithTag("time-trial-world-"+world).performClick();advance(16);
            for(int stage=0;stage<10;++stage)assertNotNull(panel().findViewWithTag("time-trial-record-"+(world*10+stage)));}
        View portrait=measure(360,660);footerVisible(portrait);capture(portrait,"time-trial-es-portrait");
        View landscape=measure(720,300);footerVisible(landscape);capture(landscape,"time-trial-es-landscape");
        assertEquals(View.GONE,overlayHost.findViewWithTag("time-trial-overlay").getVisibility());
    }
    @Test public void realSnapshotAloneControlsTimerAndBackgroundStopsPolling() {
        activity.getSharedPreferences("sma3-options",0).edit().putBoolean(TimeTrialUi.ENABLED,true).commit();
        nativeValue=result(0,1000,0,0);open();assertTrue(hasText("0:16.74"));
        advance(500);assertTrue(hasText("0:16.74")); // UI time cannot fabricate game time.
        nativeValue[2]=3;advance(150);assertTrue(hasText("Pausa"));
        TextView overlay=overlayHost.findViewWithTag("time-trial-overlay");assertEquals(View.VISIBLE,overlay.getVisibility());assertFalse(overlay.isClickable());assertFalse(overlay.isFocusable());
        ui.onPause();int stopped=reads;advance(2000);assertEquals(stopped,reads);assertEquals(View.GONE,overlay.getVisibility());
        ui.onResume();advance(32);assertTrue(reads>stopped);
    }
    @Test public void togglesSavePreferencesWithoutClearingRecords() {
        open();List<Switch> toggles=new ArrayList<>();for(View view:descendants(panel()))if(view instanceof Switch)toggles.add((Switch)view);
        assertEquals(3,toggles.size());toggles.get(0).setChecked(true);advance(32);assertTrue(requested);
        assertTrue(activity.getSharedPreferences("sma3-options",0).getBoolean(TimeTrialUi.ENABLED,false));
        toggles.get(1).setChecked(false);advance(16);assertEquals(View.GONE,overlayHost.findViewWithTag("time-trial-overlay").getVisibility());
        toggles.get(0).setChecked(false);advance(16);assertFalse(requested);assertEquals(0,disk.writes);
    }
    @Test public void resultComparesAgainstPreviousBestAndSurvivesRecreation() throws Exception {
        activity.getSharedPreferences("sma3-options",0).edit().putBoolean(TimeTrialUi.ENABLED,true).commit();
        nativeValue=result(0,1000,1,1000);open();assertTrue(hasText("Primera marca"));assertEquals(1000,disk.value.best[0]);
        nativeValue=result(0,900,2,900);advance(150);assertTrue(hasText("Nuevo récord"));assertTrue(hasText("−0:01.67"));assertEquals(900,disk.value.best[0]);
        capture(measure(400,700),"time-trial-record-es");
        ui.close();shown.dismiss();language=1;open();assertTrue(hasText("New best"));footerVisible(measure(400,700));
        capture(measure(400,700),"time-trial-record-en");
        ui.close();shown.dismiss();language=2;open();assertTrue(hasText("Novo recorde"));footerVisible(measure(400,700));
        capture(measure(400,700),"time-trial-record-pt");
        assertEquals(2,disk.writes); // Activity recreation did not rewrite or duplicate results.
    }
    @Test public void pendingWriteRetriesWithoutLosingBetterNativeRecord() {
        disk.writeFail=true;nativeValue=result(0,1000,1,1000);store.poll(this::sample,null);
        assertEquals(TimeTrialStore.Problem.WRITE,store.latest().problem);assertEquals(1000,store.latest().state.best[0]);assertEquals(0,disk.value.best[0]);
        disk.writeFail=false;nativeValue=disabled();store.poll(this::sample,null);
        assertEquals(TimeTrialStore.Problem.NONE,store.latest().problem);assertEquals(1000,disk.value.best[0]);assertEquals(1000,disk.value.last[0]);
    }
    @Test public void missedCompletionsMergeMinimaWithoutInventingRecordOrder() {
        nativeValue=result(0,900,2,800);store.poll(this::sample,null);
        assertEquals(800,disk.value.best[0]);assertEquals(900,disk.value.last[0]);assertEquals(-1,store.latest().previousBest);
        long date=disk.value.recordedAt[0];store.poll(this::sample,null);assertEquals(date,disk.value.recordedAt[0]);assertEquals(1,disk.writes);
    }
    @Test public void exactDiskRoundtripAndMalformedFutureFilesArePreserved() throws Exception {
        TimeTrialModel.State value=TimeTrialModel.merge(TimeTrialModel.empty(),new TimeTrialModel.Snapshot(result(0,1000,1,1000)),1700000000000L,true);
        File path=new File(activity.getFilesDir(),"time-trial-tests/records.json");TimeTrialStore.Disk real=new TimeTrialStore.Disk(path);
        real.write(value);assertTrue(value.same(real.read()));
        String valid=TimeTrialStore.Disk.encode(value);
        for(String bad:new String[]{"broken",new JSONObject(valid).put("schema",2).toString(),new JSONObject(valid).put("clock_hz",60).toString(),new JSONObject(valid).put("extra","x").toString()}){
            Files.write(path.toPath(),bad.getBytes(StandardCharsets.UTF_8));
            TimeTrialStore protectedStore=new TimeTrialStore(real,Runnable::run,new Handler(Looper.getMainLooper()));
            nativeValue=result(0,800,2,800);protectedStore.poll(this::sample,null);
            assertEquals(TimeTrialStore.Problem.READ,protectedStore.latest().problem);assertEquals(bad,Files.readString(path.toPath()));
        }
    }
    @Test public void restoredOrCancelledAttemptCannotChangePersonalBest() {
        nativeValue=result(0,1000,1,1000);store.poll(this::sample,null);
        long[] cancelled=result(0,300,1,1000);cancelled[2]=5;cancelled[8]=1000;cancelled[9]=3;nativeValue=cancelled;
        store.poll(this::sample,null);assertEquals(1000,disk.value.best[0]);assertEquals(1000,disk.value.last[0]);assertEquals(1,disk.writes);
        nativeValue=new long[74];store.poll(this::sample,null);assertEquals(TimeTrialStore.Problem.NATIVE,store.latest().problem);assertEquals(1000,disk.value.best[0]);
    }

    @Test public void ghostDefaultsOffAndTogglePreservesExistingBest() {
        nativeValue=result(0,900,1,900);open();assertFalse(ghostRequested);assertTrue(hasText("Fantasma desactivado"));
        List<Switch> toggles=new ArrayList<>();for(View v:descendants(panel()))if(v instanceof Switch)toggles.add((Switch)v);
        toggles.get(2).setChecked(true);advance(32);assertTrue(ghostRequested);assertTrue(hasText("Activa también el contrarreloj"));
        toggles.get(0).setChecked(true);advance(32);assertTrue(hasText("fantasmas de récord disponibles"));
        toggles.get(2).setChecked(false);advance(32);assertFalse(ghostRequested);assertEquals(900,disk.value.best[0]);
        assertEquals(1,disk.writes);assertFalse(activity.getSharedPreferences("sma3-options",0).getBoolean(TimeTrialUi.GHOST,true));
    }

    @Test public void ghostSyncRequiresReadableRecordsAndUsesMergedBest() {
        disk.readFail=true;nativeValue=result(0,900,1,900);store.poll(this::sample,null);assertEquals(0,ghostSyncs);
        assertEquals(TimeTrialStore.Problem.READ,store.latest().problem);
        disk.readFail=false;store.poll(this::sample,null);assertEquals(1,ghostSyncs);assertEquals(900,lastSyncedBests[0]);
        lastSyncedBests[0]=3;assertEquals(900,store.latest().state.best[0]);
        nativeValue=result(0,800,2,800);disk.writeFail=true;store.poll(this::sample,null);
        assertEquals(800,lastSyncedBests[0]);assertEquals(800,store.latest().state.best[0]);assertEquals(900,disk.value.best[0]);
        assertEquals(TimeTrialStore.Problem.WRITE,store.latest().problem);
    }

    @Test public void ghostShowsSavedReferenceAndPreservesClockAcrossRecreation() throws Exception {
        activity.getSharedPreferences("sma3-options",0).edit().putBoolean(TimeTrialUi.ENABLED,true).putBoolean(TimeTrialUi.GHOST,true).commit();
        nativeValue=result(0,900,1,900);ghostValue[1]=1;ghostValue[4]=1;ghostValue[7]=1;ghostValue[9]=900;ghostValue[11]=1;ghostValue[12]=900;
        nativeValue[2]=TimeTrialModel.RUNNING;nativeValue[4]=450;nativeValue[5]=2;
        open();assertTrue(hasText("Compitiendo contra: 0:15.06"));assertTrue(hasText("Fantasma disponible · 0:15.06"));
        capture(measure(420,820),"ghost-record-es");footerVisible(measure(720,360));capture(measure(720,360),"ghost-landscape-es");
        ui.close();shown.dismiss();language=1;open();assertTrue(hasText("Racing against: 0:15.06"));capture(measure(420,820),"ghost-record-en");
        ui.close();shown.dismiss();language=2;open();assertTrue(hasText("Disputando contra: 0:15.06"));capture(measure(420,820),"ghost-record-pt");
        assertEquals(1,disk.writes);assertEquals(450,store.latest().snapshot.frames);
    }

    @Test public void ghostWarningsDoNotBreakTimerOrEraseRecords() {
        activity.getSharedPreferences("sma3-options",0).edit().putBoolean(TimeTrialUi.ENABLED,true).putBoolean(TimeTrialUi.GHOST,true).commit();
        nativeValue=result(0,900,1,900);ghostValue[1]=1;open();
        ghostValue[5]=4;advance(150);assertTrue(hasText("No se pudo leer un recorrido"));assertEquals(900,disk.value.best[0]);
        ghostValue[5]=5;advance(150);assertTrue(hasText("guardado pendiente"));
        ghostValue[5]=3;ghostValue[10]=1;advance(150);assertTrue(hasText("Guardando el fantasma"));
        ghostValue[5]=2;ghostValue[10]=0;ghostValue[6]=4;advance(150);assertTrue(hasText("límite de grabación"));
        ghostValue[6]=5;advance(150);assertTrue(hasText("no pudo generar un fantasma visible"));
        ghostValue[6]=6;advance(150);assertTrue(hasText("Grabación cancelada"));
        ghostValue=new long[72];advance(150);assertTrue(hasText("fantasma aún no está disponible"));
        assertEquals(TimeTrialStore.Problem.NONE,store.latest().problem);assertEquals(900,disk.value.best[0]);assertEquals(1,disk.writes);
    }

    @Test public void ghostStatusRejectsMalformedBoundaryAndDoesNotPollInBackground() {
        long[] bad=ghostValue.clone();bad[7]=12;
        try { new TimeTrialGhostModel.Snapshot(bad);fail("welcome course accepted"); } catch(IllegalArgumentException expected) {}
        bad=ghostValue.clone();bad[12]=72001;
        try { new TimeTrialGhostModel.Snapshot(bad);fail("oversized ghost accepted"); } catch(IllegalArgumentException expected) {}
        bad=ghostValue.clone();bad[2]=2;
        try { new TimeTrialGhostModel.Snapshot(bad);fail("invalid bool accepted"); } catch(IllegalArgumentException expected) {}
        ghostValue[12]=72000;TimeTrialGhostModel.Snapshot safe=new TimeTrialGhostModel.Snapshot(ghostValue);ghostValue[12]=1;
        assertEquals(72000,safe.best[0]);assertEquals(1,safe.count());
        open();ui.onPause();int stopped=ghostSyncs;advance(2000);assertEquals(stopped,ghostSyncs);ui.onResume();advance(32);assertTrue(ghostSyncs>stopped);
    }
}
