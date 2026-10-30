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
import android.widget.ScrollView;
import android.widget.Switch;
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
import org.robolectric.shadows.ShadowToast;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.time.Duration;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import static org.junit.Assert.*;

/** Android host tests; controlled native evidence, no ARM64 game or phone. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 34, qualifiers = "w420dp-h900dp-xhdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public final class AchievementsUiTest {
    private ActivityController<Activity> controller;
    private Activity activity;
    private AlertDialog shown;
    private AchievementsUi ui;
    private MemoryDisk disk;
    private AchievementsStore store;
    private long[] nativeValue;
    private int language;

    static final class MemoryDisk implements AchievementsStore.Persistence {
        AchievementsModel.State value = AchievementsModel.empty();
        boolean failRead, failWrite;
        int writes;
        public AchievementsModel.State read() throws IOException {
            if (failRead) throw new IOException("Controlled read failure"); return value;
        }
        public void write(AchievementsModel.State next) throws IOException {
            if (failWrite) throw new IOException("Controlled write failure"); value = next; ++writes;
        }
    }

    @Before public void setup() {
        controller = Robolectric.buildActivity(Activity.class).setup().visible(); activity = controller.get();
        disk = new MemoryDisk(); store = new AchievementsStore(disk, Runnable::run, new Handler(Looper.getMainLooper()));
        nativeValue = new long[]{1,0,0,0,0,0,0,0}; language = 0; ShadowToast.reset();
    }
    @After public void cleanup() {
        if (ui != null) ui.close(); if (shown != null) shown.dismiss();
        controller.pause().stop().destroy(); idle();
    }
    private void idle() { Shadows.shadowOf(Looper.getMainLooper()).idle(); }
    private void open() {
        if (ui != null) ui.close(); if (shown != null) shown.dismiss();
        ui = new AchievementsUi(activity, (es,en,pt) -> language == 0 ? es : language == 1 ? en : pt,
            (dialog, parent) -> { shown = dialog; dialog.show(); SettingsPanel.fitWindow(activity, dialog); },
            () -> shown.dismiss(), store, () -> nativeValue.clone());
        ui.show(); idle();
    }
    private List<View> descendants(View root) {
        List<View> result = new ArrayList<>(); result.add(root);
        if (root instanceof ViewGroup) for (int i=0;i<((ViewGroup)root).getChildCount();++i)
            result.addAll(descendants(((ViewGroup)root).getChildAt(i)));
        return result;
    }
    private View panel() {
        for (View view : descendants(shown.getWindow().getDecorView()))
            if (view.getClass().getName().endsWith("SettingsPanel$BoundedPanel")) return view;
        throw new AssertionError("Missing production settings panel");
    }
    private int dp(int value) { return Math.round(value*activity.getResources().getDisplayMetrics().density); }
    private View measure(int w,int h) {
        View view=panel(); view.measure(View.MeasureSpec.makeMeasureSpec(dp(w),View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(dp(h),View.MeasureSpec.AT_MOST));
        view.layout(0,0,view.getMeasuredWidth(),view.getMeasuredHeight()); return view;
    }
    private int records() {
        int count=0; for(View view:descendants(panel())) if(view.getTag() instanceof String
            && view.getTag().toString().startsWith("achievement-") && !view.getTag().toString().startsWith("achievement-filter-")) ++count;
        return count;
    }
    private boolean hasText(String wanted) {
        for(View view:descendants(panel())) if(view instanceof TextView && ((TextView)view).getText().toString().contains(wanted)) return true;
        return false;
    }
    private void filter(int which) { panel().findViewWithTag("achievement-filter-"+which).performClick(); idle(); }
    private Switch notices() {
        for(View view:descendants(panel())) if(view instanceof Switch) return (Switch)view;
        throw new AssertionError("Missing notice control");
    }
    private void assertFooter(View root) {
        Button footer=null;
        for(View view:descendants(root)) if(view instanceof Button && ((Button)view).getText().toString().equals("Volver")) footer=(Button)view;
        assertNotNull(footer); int top=0; View view=footer;
        while(view!=root) { assertFalse(view instanceof ScrollView);top+=view.getTop();view=(View)view.getParent(); }
        assertTrue(top>=0 && top+footer.getHeight()<=root.getHeight());assertTrue(footer.getHeight()>=dp(48));
    }
    private void capture(View view,String name) throws Exception {
        File folder=new File(System.getProperty("sma3.ui.captures"));assertTrue(folder.isDirectory()||folder.mkdirs());
        Bitmap bitmap=Bitmap.createBitmap(view.getWidth(),view.getHeight(),Bitmap.Config.ARGB_8888);view.draw(new Canvas(bitmap));
        try(FileOutputStream out=new FileOutputStream(new File(folder,name+".png"))){assertTrue(bitmap.compress(Bitmap.CompressFormat.PNG,100,out));}
        bitmap.recycle();
    }

    @Test public void catalogFiltersAndRealLayouts() throws Exception {
        nativeValue=new long[]{1,3,0,1,3,1,10,0};open();
        assertEquals(10,records());assertTrue(hasText("4 / 10 desbloqueados"));
        for(int[] size:new int[][]{{360,760},{600,328},{264,560},{360,240}}){
            View view=measure(size[0],size[1]);assertFooter(view);
            ViewGroup panel=(ViewGroup)view;
            assertEquals("Panel itself must not scroll its title",0,view.getScrollY());
            assertTrue("Heading remains visible",panel.getChildAt(0).getHeight()>0);
            assertTrue("Scrolling body stays below heading",panel.getChildAt(1).getTop()>=panel.getChildAt(0).getBottom());
            for(int i=0;i<3;++i)assertTrue(view.findViewWithTag("achievement-filter-"+i).getHeight()>=dp(48));
            capture(view,"achievements-"+size[0]+"x"+size[1]);
        }
        filter(1);assertEquals(4,records());assertTrue(hasText("Obtenido:"));
        for(View view:descendants(panel())) if(view.getTag()!=null&&view.getTag().equals("achievement-first_flower"))
            assertTrue(view.getContentDescription().toString().contains("Desbloqueado"));
        filter(2);assertEquals(6,records());assertTrue(hasText("Progreso: 3 / 20"));
        filter(0);assertEquals(10,records());
    }

    @Test public void failedWritesRetainEvidenceAndAnnounceOnlyOnceAfterCommit() {
        disk.failWrite=true;open();ui.onResume();idle();nativeValue[5]=1;
        Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(1001));
        assertEquals(0,disk.value.unlocked());assertEquals(1,store.latest().state.unlocked());
        assertEquals(AchievementsStore.Problem.WRITE,store.latest().problem);
        long originalDate=store.latest().state.unlockedAt[0];assertEquals(0,ShadowToast.shownToastCount());
        nativeValue[4]=1;Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(1000));
        assertEquals(2,store.latest().state.unlocked());disk.failWrite=false;nativeValue=new long[]{1,0,0,0,0,0,0,0};
        Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(1000));
        assertEquals(2,disk.value.unlocked());assertEquals(originalDate,disk.value.unlockedAt[0]);
        assertEquals(1,ShadowToast.shownToastCount());assertTrue(ShadowToast.getTextOfLatestToast().contains("2 logros"));
        Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(1000));assertEquals(1,ShadowToast.shownToastCount());
        notices().setChecked(false);idle();assertFalse(disk.value.notifications);
        nativeValue[6]=30;Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(1000));
        assertEquals(3,disk.value.unlocked());assertEquals(1,ShadowToast.shownToastCount());
    }

    @Test public void recreationLanguageChangesAndPauseDoNotReplayNotices() {
        disk.value=AchievementsModel.merge(AchievementsModel.empty(),new long[]{1,1,0,0,1,1,0,0},77);
        open();ui.onResume();idle();assertEquals(0,ShadowToast.shownToastCount());
        nativeValue[3]=1;ui.onPause();idle();assertEquals(4,disk.value.unlocked());
        assertEquals(0,ShadowToast.shownToastCount());
        store=new AchievementsStore(disk,Runnable::run,new Handler(Looper.getMainLooper()));language=1;open();ui.onResume();idle();
        assertTrue(hasText("Local achievements"));assertTrue(hasText("First flower"));assertEquals(0,ShadowToast.shownToastCount());
        language=2;open();ui.onResume();idle();assertTrue(hasText("Conquistas locais"));assertTrue(hasText("Primeira flor"));
        assertEquals(0,ShadowToast.shownToastCount());ui.close();shown.dismiss();nativeValue[7]=1;
        Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(3000));assertFalse(shown.isShowing());
        assertEquals(0,ShadowToast.shownToastCount());
    }

    @Test public void corruptAndFutureFilesArePreservedThenRecover() throws Exception {
        File path=new File(activity.getFilesDir(),"achievement-test/progress.json");
        AchievementsStore.Disk actual=new AchievementsStore.Disk(path);
        AchievementsModel.State one=AchievementsModel.merge(AchievementsModel.empty(),new long[]{1,0,0,0,0,1,0,0},100);
        actual.write(one);assertTrue(actual.read().same(one));
        byte[] original=Files.readAllBytes(path.toPath());
        for(String invalid:new String[]{"{broken",new String(original,java.nio.charset.StandardCharsets.UTF_8).replace("\"schema\":1","\"schema\":2")}){
            Files.write(path.toPath(),invalid.getBytes(java.nio.charset.StandardCharsets.UTF_8));
            store=new AchievementsStore(actual,Runnable::run,new Handler(Looper.getMainLooper()));
            store.poll(()->new long[]{1,1,0,0,0,0,0,0},null);
            assertEquals(AchievementsStore.Problem.READ,store.latest().problem);
            assertEquals(invalid,new String(Files.readAllBytes(path.toPath()),java.nio.charset.StandardCharsets.UTF_8));
        }
        Files.write(path.toPath(),original);store.poll(()->new long[]{1,1,0,0,0,0,0,0},null);
        assertEquals(AchievementsStore.Problem.NONE,store.latest().problem);assertEquals(2,actual.read().unlocked());
        assertEquals(100,actual.read().unlockedAt[0]);
        Files.copy(path.toPath(),new File(path.getPath()+".bak").toPath());Files.write(path.toPath(),"broken".getBytes());
        assertEquals(2,actual.read().unlocked());
    }

    @Test public void nativeFailuresDoNotEraseSavedProgressOrOverwriteUnreadableData() {
        disk.value=AchievementsModel.merge(AchievementsModel.empty(),new long[]{1,1,0,0,0,0,0,0},91);
        store.poll(()->{throw new UnsatisfiedLinkError("Test unavailable library");},null);
        assertEquals(1,store.latest().state.unlocked());assertEquals(AchievementsStore.Problem.NATIVE,store.latest().problem);
        assertEquals(0,disk.writes);store.poll(()->new long[]{2,0,0,0,0,0,0,0},null);
        assertEquals(1,store.latest().state.unlocked());assertEquals(0,disk.writes);
        disk.failRead=true;store=new AchievementsStore(disk,Runnable::run,new Handler(Looper.getMainLooper()));
        store.notifications(false,null);assertEquals(AchievementsStore.Problem.READ,store.latest().problem);assertEquals(0,disk.writes);
        disk.failRead=false;store.poll(()->new long[]{1,2,0,0,0,0,0,0},null);
        assertEquals(2,Long.bitCount(disk.value.metrics[0]));assertEquals(91,disk.value.unlockedAt[7]);
    }

    @Test public void writesAreSerializedOffTheMainThread() throws Exception {
        ExecutorService worker=Executors.newSingleThreadExecutor();CountDownLatch completed=new CountDownLatch(2);
        List<String> threads=new ArrayList<>();MemoryDisk memory=new MemoryDisk();
        AchievementsStore async=new AchievementsStore(new AchievementsStore.Persistence(){
            public AchievementsModel.State read() throws IOException {assertNotSame(Looper.getMainLooper(),Looper.myLooper());return memory.read();}
            public void write(AchievementsModel.State value)throws IOException{assertNotSame(Looper.getMainLooper(),Looper.myLooper());threads.add(Thread.currentThread().getName());memory.write(value);completed.countDown();}
        },worker,new Handler(Looper.getMainLooper()));
        try{
            async.poll(()->new long[]{1,1,0,0,0,0,0,0},null);
            async.poll(()->new long[]{1,2,0,0,0,0,0,0},null);
            assertTrue(completed.await(5,TimeUnit.SECONDS));assertEquals(2,Long.bitCount(memory.value.metrics[0]));
            assertEquals(2,threads.size());assertEquals(threads.get(0),threads.get(1));
        }finally{worker.shutdownNow();}
    }
}
