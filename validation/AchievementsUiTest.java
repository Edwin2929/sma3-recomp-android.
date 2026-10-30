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
import org.json.JSONObject;
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
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Arrays;
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
    // Produced by the REAL 0.10.9 Disk.encode(), compiled with Model/Store from
    // commit 2c5dbc94a707460da3ae1aac1b93cb6d2fcdaa32. Keep bytes/order/newline.
    private static final String LEGACY_RECORD = "{\"schema\":1,\"notifications\":false,\"evidence\":{\"completed_levels\":16777215,\"perfect_levels\":511,\"transformations\":21,\"red_coins\":10,\"flowers\":3,\"stars\":30,\"events\":1},\"unlocked_at\":{\"first_flower\":1700000000100,\"five_flowers\":0,\"first_red_coin\":1700000000200,\"twenty_red_coins\":0,\"thirty_stars\":1700000000300,\"first_transformation\":1700000000400,\"fuzzy_recovery\":1700000000500,\"first_clear\":1700000000600,\"five_clears\":1700000000700,\"perfect_clear\":1700000000800}}\n";
    private static final long[] LEGACY_METRICS = {16777215,511,21,10,3,30,1};
    private static final long[] LEGACY_DATES = {1700000000100L,0,1700000000200L,0,1700000000300L,
        1700000000400L,1700000000500L,1700000000600L,1700000000700L,1700000000800L};
    private static final int RECOGNIZED = (1<<10)|(1<<11)|(1<<12)|(1<<13)|(1<<14)|(1<<17)|(1<<20)|(1<<22)|(1<<24);
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
        public AchievementsStore.ReadResult read() throws IOException {
            if (failRead) throw new IOException("Controlled read failure");
            return new AchievementsStore.ReadResult(value, false);
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
    private File recordFile(String folder,String record) throws Exception {
        File file=new File(activity.getFilesDir(),folder+"/progress.json");
        Files.createDirectories(file.toPath().getParent());
        Files.write(file.toPath(),record.getBytes(StandardCharsets.UTF_8));return file;
    }
    private JSONObject reversed(JSONObject original) throws Exception {
        JSONObject result=new JSONObject();
        for(int i=original.length()-1;i>=0;--i){String key=original.names().getString(i);Object value=original.get(key);
            result.put(key,value instanceof JSONObject?reversed((JSONObject)value):value);}
        return result;
    }
    private void assertLegacyPreserved(AchievementsModel.State value) {
        assertEquals(17,value.unlocked());assertFalse(value.notifications);
        assertEquals(RECOGNIZED,value.recognizedOnUpgrade);assertEquals(9,Integer.bitCount(value.recognizedOnUpgrade));
        assertArrayEquals(LEGACY_METRICS,value.metrics);
        assertArrayEquals(LEGACY_DATES,Arrays.copyOf(value.unlockedAt,10));
        for(int i=10;i<25;++i)assertEquals((RECOGNIZED&(1<<i))!=0,value.unlockedAt[i]>0);
    }
    private void scrollToRecord(String id) {
        View anchor=panel().findViewWithTag("achievement-"+id);assertNotNull(anchor);
        int top=0;View item=anchor;
        while(!(item.getParent() instanceof ScrollView)){top+=item.getTop();item=(View)item.getParent();}
        ((ScrollView)item.getParent()).scrollTo(0,top);
    }

    @Test public void realPreviousEncoderFileMigratesSilentlyAndKeepsDatesAcrossRestart() throws Exception {
        StringBuilder digest=new StringBuilder();
        for(byte value:java.security.MessageDigest.getInstance("SHA-256").digest(LEGACY_RECORD.getBytes(StandardCharsets.UTF_8)))
            digest.append(String.format("%02x",value&255));
        assertEquals("e66c5b7e85bf7102fbe35b7918912cf160e9c68d92ef9a880c47f9337f104cc8",digest.toString());
        File path=recordFile("legacy-real-encoder",LEGACY_RECORD);AchievementsStore.Disk actual=new AchievementsStore.Disk(path);
        store=new AchievementsStore(actual,Runnable::run,new Handler(Looper.getMainLooper()));
        List<AchievementsStore.Result> callbacks=new ArrayList<>();long before=System.currentTimeMillis();
        store.poll(()->{throw new UnsatisfiedLinkError("No game library during upgrade");},callbacks::add);idle();
        assertEquals(AchievementsStore.Problem.NATIVE,store.latest().problem);
        assertEquals(1,callbacks.size());assertEquals(0,callbacks.get(0).newUnlocks);
        AchievementsModel.State upgraded=actual.read().state;assertLegacyPreserved(upgraded);
        assertFalse(actual.read().needsRewrite);
        for(int i=10;i<25;++i)if(upgraded.unlockedAt[i]>0){assertTrue(upgraded.unlockedAt[i]>=before);
            assertTrue(upgraded.unlockedAt[i]<=System.currentTimeMillis());assertEquals(upgraded.unlockedAt[10],upgraded.unlockedAt[i]);}
        String persisted=Files.readString(path.toPath());JSONObject json=new JSONObject(persisted);
        assertEquals(2,json.getInt("schema"));assertEquals(25,json.getJSONObject("unlocked_at").length());
        assertEquals(7,json.getJSONObject("evidence").length());
        store=new AchievementsStore(actual,Runnable::run,new Handler(Looper.getMainLooper()));callbacks.clear();
        store.poll(()->new long[]{1,0,0,0,0,0,0,0},callbacks::add);idle();
        assertTrue(upgraded.same(store.latest().state));assertEquals(0,callbacks.get(0).newUnlocks);
        assertEquals(persisted,Files.readString(path.toPath()));
        assertTrue(AchievementsStore.Disk.decode(reversed(json).toString(),before+100000).state.same(upgraded));
        AchievementsStore.ReadResult reordered=AchievementsStore.Disk.decode(reversed(new JSONObject(LEGACY_RECORD)).toString(),123456);
        assertTrue(reordered.needsRewrite);assertLegacyPreserved(reordered.state);assertEquals(123456,reordered.state.unlockedAt[24]);
        open();assertEquals(25,records());assertTrue(hasText("17 / 25 desbloqueados"));
        assertTrue(hasText("Reconocido al ampliar los logros:"));assertTrue(hasText("Obtenido:"));assertFalse(notices().isChecked());
        filter(1);assertEquals(17,records());filter(2);assertEquals(8,records());filter(0);
        View view=measure(360,760);scrollToRecord("three_flowers");capture(view,"achievements-upgrade-360x760");
        language=1;open();assertTrue(hasText("Recognized when achievements expanded:"));
        assertTrue(hasText("Car"));assertTrue(hasText("Submarine"));
        language=2;open();assertTrue(hasText("Reconhecida ao ampliar as conquistas:"));
        assertTrue(hasText("Carro"));assertTrue(hasText("Submarino"));assertEquals(0,ShadowToast.shownToastCount());

        JSONObject empty=new JSONObject(LEGACY_RECORD);empty.put("notifications",true);
        for(String metric:AchievementsModel.METRIC_KEYS)empty.getJSONObject("evidence").put(metric,0);
        for(int i=0;i<10;++i)empty.getJSONObject("unlocked_at").put(AchievementsModel.DEFINITIONS[i].id,0);
        AchievementsStore.Disk emptyDisk=new AchievementsStore.Disk(recordFile("legacy-empty",empty.toString()));
        AchievementsStore emptyStore=new AchievementsStore(emptyDisk,Runnable::run,new Handler(Looper.getMainLooper()));
        callbacks.clear();emptyStore.poll(()->new long[]{1,0,0,0,0,0,0,0},callbacks::add);idle();
        assertFalse(emptyDisk.read().needsRewrite);assertEquals(0,emptyDisk.read().state.unlocked());
        assertTrue(emptyDisk.read().state.notifications);assertEquals(0,callbacks.get(0).newUnlocks);
    }

    @Test public void migrationFailureRetriesSameRecognitionDateAndSurvivesRestart() throws Exception {
        File path=recordFile("legacy-retry",LEGACY_RECORD);AchievementsStore.Disk actual=new AchievementsStore.Disk(path);
        final boolean[] fail={true};final int[] attempts={0};
        AchievementsStore.Persistence unstable=new AchievementsStore.Persistence(){
            public AchievementsStore.ReadResult read() throws IOException{return actual.read();}
            public void write(AchievementsModel.State next) throws IOException{
                ++attempts[0];if(fail[0])throw new IOException("Controlled migration write failure");actual.write(next);}
        };
        store=new AchievementsStore(unstable,Runnable::run,new Handler(Looper.getMainLooper()));
        List<AchievementsStore.Result> callbacks=new ArrayList<>();
        store.poll(()->null,callbacks::add);idle();assertEquals(AchievementsStore.Problem.WRITE,store.latest().problem);
        AchievementsModel.State pending=store.latest().state;assertLegacyPreserved(pending);
        assertEquals(LEGACY_RECORD,Files.readString(path.toPath()));assertEquals(0,callbacks.get(0).newUnlocks);
        store.poll(()->null,callbacks::add);idle();assertTrue(pending.same(store.latest().state));
        assertEquals(LEGACY_RECORD,Files.readString(path.toPath()));assertEquals(2,attempts[0]);
        fail[0]=false;store.poll(()->{throw new UnsatisfiedLinkError("Still unavailable");},callbacks::add);idle();
        assertEquals(3,attempts[0]);assertTrue(pending.same(actual.read().state));assertFalse(actual.read().needsRewrite);
        for(AchievementsStore.Result result:callbacks)assertEquals(0,result.newUnlocks);
        store=new AchievementsStore(actual,Runnable::run,new Handler(Looper.getMainLooper()));callbacks.clear();
        store.poll(()->null,callbacks::add);idle();assertTrue(pending.same(store.latest().state));assertEquals(0,callbacks.get(0).newUnlocks);

        // A process lost BEFORE commit keeps the intact old file. The replacement
        // process recognizes at its own successful upgrade time, never an invented old date.
        Files.writeString(path.toPath(),LEGACY_RECORD);fail[0]=true;
        store=new AchievementsStore(unstable,Runnable::run,new Handler(Looper.getMainLooper()));store.poll(()->null,null);
        assertEquals(AchievementsStore.Problem.WRITE,store.latest().problem);assertEquals(LEGACY_RECORD,Files.readString(path.toPath()));
        store=new AchievementsStore(actual,Runnable::run,new Handler(Looper.getMainLooper()));
        store.poll(()->null,null);assertLegacyPreserved(actual.read().state);assertFalse(actual.read().needsRewrite);

        // Silent recognition also applies to the user's enabled-notice preference.
        Files.writeString(path.toPath(),LEGACY_RECORD.replace("\"notifications\":false","\"notifications\":true"));
        store=new AchievementsStore(actual,Runnable::run,new Handler(Looper.getMainLooper()));callbacks.clear();
        store.poll(()->new long[]{1,0,0,0,0,0,0,0},callbacks::add);idle();
        assertTrue(actual.read().state.notifications);assertEquals(0,callbacks.get(0).newUnlocks);
    }

    @Test public void incompleteExpandedAndDisguisedLegacyFilesAreNeverOverwritten() throws Exception {
        AchievementsModel.State expanded=AchievementsStore.Disk.decode(LEGACY_RECORD,12345).state;
        String good=AchievementsStore.Disk.encode(expanded);List<String> invalid=new ArrayList<>();
        for(String missing:new String[]{"first_flower","submarine_transformation"}){
            JSONObject value=new JSONObject(good);value.getJSONObject("unlocked_at").remove(missing);invalid.add(value.toString());}
        JSONObject disguised=new JSONObject(good);disguised.put("schema",1);disguised.remove("recognized_on_upgrade");invalid.add(disguised.toString());
        for(String missing:new String[]{"first_flower","perfect_clear"}){
            JSONObject value=new JSONObject(LEGACY_RECORD);value.getJSONObject("unlocked_at").remove(missing);invalid.add(value.toString());}
        JSONObject extra=new JSONObject(LEGACY_RECORD);extra.getJSONObject("unlocked_at").put("future_id",0);invalid.add(extra.toString());
        JSONObject future=new JSONObject(good);future.put("schema",3);invalid.add(future.toString());
        JSONObject missingOrigin=new JSONObject(good);missingOrigin.remove("recognized_on_upgrade");invalid.add(missingOrigin.toString());
        for(Object mask:new Object[]{1,1<<9,1<<21,1<<25,1L<<40,-1,"16777216"}){
            JSONObject value=new JSONObject(good);value.put("recognized_on_upgrade",mask);invalid.add(value.toString());}
        JSONObject invalidDate=new JSONObject(good);invalidDate.getJSONObject("unlocked_at").put("three_flowers",0);invalid.add(invalidDate.toString());
        File path=recordFile("malformed-upgrade",good);AchievementsStore.Disk actual=new AchievementsStore.Disk(path);
        assertEquals(16,invalid.size());
        for(String value:invalid){
            Files.writeString(path.toPath(),value);store=new AchievementsStore(actual,Runnable::run,new Handler(Looper.getMainLooper()));
            store.poll(()->new long[]{1,(1L<<40)-1,(1L<<20)-1,31,20,5,30,1},null);
            assertEquals(AchievementsStore.Problem.READ,store.latest().problem);assertEquals(value,Files.readString(path.toPath()));
            store.notifications(true,null);assertEquals(AchievementsStore.Problem.READ,store.latest().problem);
            assertEquals(value,Files.readString(path.toPath()));
        }
        Files.writeString(path.toPath(),good);store.poll(()->new long[]{1,0,0,0,0,0,0,0},null);
        assertEquals(AchievementsStore.Problem.NONE,store.latest().problem);assertTrue(expanded.same(actual.read().state));
    }

    @Test public void allTwentyFiveNoticesAreGroupedAndFiltersRemainComplete() throws Exception {
        open();ui.onResume();idle();nativeValue=new long[]{1,(1L<<40)-1,(1L<<20)-1,31,20,5,30,1};
        Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(1001));
        assertEquals(25,disk.value.unlocked());assertEquals(0x1ffffff,AchievementsModel.newUnlocks(AchievementsModel.empty(),disk.value));
        assertEquals(0,disk.value.recognizedOnUpgrade);assertEquals(1,ShadowToast.shownToastCount());
        assertTrue(ShadowToast.getTextOfLatestToast().contains("25 logros"));assertTrue(hasText("25 / 25 desbloqueados"));
        filter(0);assertEquals(25,records());filter(1);assertEquals(25,records());
        filter(2);assertEquals(0,records());assertTrue(hasText("¡Completaste todos los logros disponibles!"));filter(0);
        View view=measure(600,328);scrollToRecord("submarine_transformation");capture(view,"achievements-all25-600x328");
        open();ui.onResume();Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(1001));
        assertEquals(1,ShadowToast.shownToastCount());
        AchievementsStore.Disk actual=new AchievementsStore.Disk(new File(activity.getFilesDir(),"all25/progress.json"));
        actual.write(disk.value);assertTrue(actual.read().state.same(disk.value));

        long[] legacyAll=Arrays.copyOf(disk.value.unlockedAt,10);
        AchievementsModel.State allRecognized=AchievementsModel.expandLegacy(disk.value.metrics,legacyAll,true,55555);
        assertEquals(25,allRecognized.unlocked());assertEquals(AchievementsModel.EXPANDED_IDS_MASK,allRecognized.recognizedOnUpgrade);
        actual.write(allRecognized);assertTrue(actual.read().state.same(allRecognized));
        assertEquals(55555,actual.read().state.unlockedAt[24]);
    }

    @Test public void catalogFiltersAndRealLayouts() throws Exception {
        nativeValue=new long[]{1,3,0,1,3,1,10,0};open();
        assertEquals(25,records());assertTrue(hasText("5 / 25 desbloqueados"));
        for(int[] size:new int[][]{{360,760},{600,328},{264,560},{360,240}}){
            View view=measure(size[0],size[1]);assertFooter(view);
            ViewGroup panel=(ViewGroup)view;
            assertEquals("Panel itself must not scroll its title",0,view.getScrollY());
            assertTrue("Heading remains visible",panel.getChildAt(0).getHeight()>0);
            assertTrue("Scrolling body stays below heading",panel.getChildAt(1).getTop()>=panel.getChildAt(0).getBottom());
            for(int i=0;i<3;++i)assertTrue(view.findViewWithTag("achievement-filter-"+i).getHeight()>=dp(48));
            capture(view,"achievements-"+size[0]+"x"+size[1]);
        }
        filter(1);assertEquals(5,records());assertTrue(hasText("Obtenido:"));
        for(View view:descendants(panel())) if(view.getTag()!=null&&view.getTag().equals("achievement-first_flower"))
            assertTrue(view.getContentDescription().toString().contains("Desbloqueado"));
        filter(2);assertEquals(20,records());assertTrue(hasText("Progreso: 3 / 20"));
        filter(0);assertEquals(25,records());
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
        nativeValue[3]=1;ui.onPause();idle();assertEquals(5,disk.value.unlocked());
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
        actual.write(one);assertTrue(actual.read().state.same(one));
        byte[] original=Files.readAllBytes(path.toPath());
        for(String invalid:new String[]{"{broken",new String(original,StandardCharsets.UTF_8).replace("\"schema\":2","\"schema\":3")}){
            Files.write(path.toPath(),invalid.getBytes(java.nio.charset.StandardCharsets.UTF_8));
            store=new AchievementsStore(actual,Runnable::run,new Handler(Looper.getMainLooper()));
            store.poll(()->new long[]{1,1,0,0,0,0,0,0},null);
            assertEquals(AchievementsStore.Problem.READ,store.latest().problem);
            assertEquals(invalid,new String(Files.readAllBytes(path.toPath()),java.nio.charset.StandardCharsets.UTF_8));
        }
        Files.write(path.toPath(),original);store.poll(()->new long[]{1,1,0,0,0,0,0,0},null);
        assertEquals(AchievementsStore.Problem.NONE,store.latest().problem);assertEquals(2,actual.read().state.unlocked());
        assertEquals(100,actual.read().state.unlockedAt[0]);
        Files.copy(path.toPath(),new File(path.getPath()+".bak").toPath());Files.write(path.toPath(),"broken".getBytes());
        assertEquals(2,actual.read().state.unlocked());
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
            public AchievementsStore.ReadResult read() throws IOException {assertNotSame(Looper.getMainLooper(),Looper.myLooper());return memory.read();}
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
