package org.gbarecomp;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Insets;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.accessibility.AccessibilityManager;
import android.widget.FrameLayout;
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
import org.robolectric.shadows.ShadowChoreographer;
import org.robolectric.util.ReflectionHelpers;

import java.io.File;
import java.io.FileOutputStream;
import java.time.Duration;
import java.util.ArrayList;
import java.util.ArrayDeque;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import static org.junit.Assert.*;

/** Production banner rendering/queue tests. No phone or native game is emulated. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 34, qualifiers = "w420dp-h900dp-xhdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public final class AchievementBannerTest {
    private ActivityController<Activity> controller;
    private Activity activity;
    private AchievementBanner notices;

    @Before public void setup() {
        animationScale(1);
        // Control vsync explicitly: Robolectric otherwise drains an entire
        // ValueAnimator while idling one UI callback, skipping intermediate states.
        ShadowChoreographer.setPaused(true);
        ShadowChoreographer.setFrameDelay(Duration.ofMillis(16));
        controller = Robolectric.buildActivity(Activity.class).setup().visible();
        activity = controller.get();
        advance(32);
        notices = new AchievementBanner(activity, (es,en,pt) -> es);
        notices.resume();
    }

    @After public void cleanup() {
        notices.close();
        animationScale(1);
        controller.pause().stop().destroy();
        Shadows.shadowOf(Looper.getMainLooper()).idle();
    }

    private void advance(long millis) {
        while(millis>0){long step=Math.min(16,millis);Shadows.shadowOf(Looper.getMainLooper()).idleFor(Duration.ofMillis(step));millis-=step;}
    }
    private void animationScale(float scale) {
        ReflectionHelpers.callStaticMethod(ValueAnimator.class, "setDurationScale", ReflectionHelpers.ClassParameter.from(float.class, scale));
    }
    private View banner() { return activity.getWindow().getDecorView().findViewWithTag("achievement-banner"); }
    private String debug() {
        ValueAnimator animator=ReflectionHelpers.getField(notices,"animator");
        return " time="+android.os.SystemClock.uptimeMillis()+" active="+ReflectionHelpers.getField(notices,"active")
            +" pending="+ReflectionHelpers.getField(notices,"pending")+" banner="+ReflectionHelpers.getField(notices,"banner")
            +" animator="+(animator==null?"null":animator.getCurrentPlayTime()+"/"+animator.getDuration());
    }
    private int dp(int value) { return Math.round(value * activity.getResources().getDisplayMetrics().density); }

    private void measure(View view, int width) {
        view.measure(View.MeasureSpec.makeMeasureSpec(dp(width), View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(dp(800), View.MeasureSpec.AT_MOST));
        view.layout(0, 0, view.getMeasuredWidth(), view.getMeasuredHeight());
    }

    private Bitmap render(View view, String name) throws Exception {
        Bitmap bitmap = Bitmap.createBitmap(view.getWidth(), view.getHeight(), Bitmap.Config.ARGB_8888);
        view.draw(new Canvas(bitmap));
        File folder = new File(System.getProperty("sma3.ui.captures"));
        assertTrue(folder.isDirectory() || folder.mkdirs());
        try (FileOutputStream out = new FileOutputStream(new File(folder, name + ".png"))) {
            assertTrue(bitmap.compress(Bitmap.CompressFormat.PNG, 100, out));
        }
        return bitmap;
    }

    private int paintedWidth(Bitmap image) {
        int first=image.getWidth(), last=-1;
        for(int y=0;y<image.getHeight();++y) for(int x=0;x<image.getWidth();++x)
            if(Color.alpha(image.getPixel(x,y))>0){first=Math.min(first,x);last=Math.max(last,x);}
        return last < first ? 0 : last-first+1;
    }

    @Test public void everyUnlockIsQueuedExactlyOnceIncludingYoShiPude() {
        notices.enqueue(AchievementsModel.ALL_IDS_MASK);
        notices.enqueue(AchievementsModel.ALL_IDS_MASK); // Duplicate delivery must not duplicate a celebration.
        Set<String> delivered = new HashSet<>();
        for(int index=0;index<AchievementsModel.DEFINITIONS.length;++index) {
            assertNotNull("Notice " + index + debug(), banner());
            String id="achievement-notice-"+AchievementsModel.DEFINITIONS[index].id;
            assertEquals(id,banner().getTransitionName());assertTrue(delivered.add(id));
            assertEquals("Logro desbloqueado: "+AchievementsModel.DEFINITIONS[index].title[0],banner().getContentDescription());
            // Android starts each ValueAnimator on a vsync; wait through that
            // bounded frame rounding and the intentional gap between notices.
            long remaining=AchievementBanner.MOTION_MILLIS+AchievementBanner.HOLD_MILLIS+AchievementBanner.GAP_MILLIS+250;
            while(remaining>0&&(banner()==null||id.equals(banner().getTransitionName()))){advance(16);remaining-=16;}
        }
        assertEquals(26,delivered.size());assertNull(banner());
        notices.enqueue(AchievementsModel.ALL_IDS_MASK);assertNull(banner());
    }

    @Test public void pauseDisableAndCloseClearCurrentAndQueuedWithoutReplay() {
        notices.enqueue(7);assertNotNull(banner());notices.pause();assertNull(banner());
        advance(20000);notices.resume();notices.enqueue(7);assertNull(banner());
        notices.enqueue(1<<8);assertNotNull(banner());notices.enabled(false);assertNull(banner());
        notices.enqueue(1<<9);assertNull(banner());advance(20000);
        notices.enabled(true);notices.enqueue(1<<10);assertNotNull(banner());
        notices.close();assertNull(banner());advance(20000);notices.resume();notices.enqueue(1<<11);assertNull(banner());
    }

    @Test public void systemDisabledAnimationsStillShowReadableStaticNotice() throws Exception {
        animationScale(0);
        assertFalse(ValueAnimator.areAnimatorsEnabled());
        notices.enqueue(1<<25);assertNotNull(banner());
        measure(banner(),360);
        Bitmap image=render(banner(),"achievement-banner-reduced-motion");
        assertTrue(paintedWidth(image)>dp(300));image.recycle();
        advance(AchievementBanner.HOLD_MILLIS-1);assertNotNull(banner());
        advance(2);assertNull(banner());
    }

    @Test public void holdDoesNotKeepAnimatingAndAccessibilityCanExtendReadingTime() {
        notices.enqueue(1);advance(900);assertNotNull(banner());
        assertNull("Static reading hold must not schedule a ValueAnimator",ReflectionHelpers.getField(notices,"animator"));
        View same=banner();advance(2000);assertSame(same,banner());
        notices.pause();notices.resume();animationScale(0);
        AccessibilityManager manager=(AccessibilityManager)activity.getSystemService(Context.ACCESSIBILITY_SERVICE);
        Shadows.shadowOf(manager).setNonInteractiveUiTimeout(9000);
        notices.enqueue(1<<1);assertNotNull(banner());
        advance(8000);assertNotNull(banner());advance(1001);assertNull(banner());
    }

    @Test public void entryMedallionExpansionHoldCollapseAndExitReallyRender() throws Exception {
        AchievementBanner.BannerView view=new AchievementBanner.BannerView(activity,"yo_shi_pude","Logro desbloqueado","Yo shi pude");
        measure(view,600);
        int[] times={0,220,540,1100,AchievementBanner.HOLD_MILLIS+810,AchievementBanner.HOLD_MILLIS+(int)AchievementBanner.MOTION_MILLIS};
        int[] widths=new int[times.length];
        for(int i=0;i<times.length;++i){
            view.setTime(times[i],AchievementBanner.HOLD_MILLIS);
            Bitmap bitmap=render(view,"achievement-banner-phase-"+i);widths[i]=paintedWidth(bitmap);bitmap.recycle();
        }
        assertEquals(0,widths[0]);assertTrue(widths[1]>=dp(60)&&widths[1]<=dp(66));
        assertTrue(widths[2]>widths[1]);assertTrue(widths[2]<widths[3]);
        assertTrue(widths[3]>=dp(418));assertTrue(widths[4]<widths[3]);assertEquals(0,widths[5]);
    }

    @Test public void cutoutsRotationSmallWidthAndLargeTextStayWithinSafeArea() throws Exception {
        for(int[] size:new int[][]{{360,0,28,0},{600,40,0,28},{264,0,0,0}}) {
            AchievementBanner.BannerView view=new AchievementBanner.BannerView(activity,"twenty_perfect_clears",
                "Conquista desbloqueada","Vinte fases perfeitas");
            WindowInsets insets=new WindowInsets.Builder().setInsets(WindowInsets.Type.systemBars()|WindowInsets.Type.displayCutout(),
                Insets.of(dp(size[1]),dp(size[2]),dp(size[3]),0)).build();
            assertSame(insets,view.onApplyWindowInsets(insets));
            measure(view,size[0]);view.setFrame(1,1,1);
            Bitmap bitmap=render(view,"achievement-banner-safe-"+size[0]);
            for(int y=0;y<bitmap.getHeight();++y)for(int x=0;x<bitmap.getWidth();++x)
                if(x<dp(size[1])||x>=bitmap.getWidth()-dp(size[3])||y<dp(size[2]))assertEquals(0,Color.alpha(bitmap.getPixel(x,y)));
            assertTrue(paintedWidth(bitmap)>dp(190));bitmap.recycle();
            // The same overlay responds to a rotated/narrower surface without restarting its queue.
            measure(view,328);assertEquals(dp(328),view.getWidth());
        }
        Configuration configuration=new Configuration(activity.getResources().getConfiguration());configuration.fontScale=2f;
        Context large=activity.createConfigurationContext(configuration);
        AchievementBanner.BannerView view=new AchievementBanner.BannerView(large,"twenty_perfect_clears",
            "Logro desbloqueado","Veinte niveles perfectos");
        measure(view,264);view.setFrame(1,1,1);
        Bitmap bitmap=render(view,"achievement-banner-large-text-264");
        assertTrue(view.getHeight()>dp(90));assertTrue(view.getHeight()<dp(250));bitmap.recycle();
        assertEquals("Logro desbloqueado: Veinte niveles perfectos",view.getContentDescription());
    }

    @Test public void touchesAndMultitouchReachGameUnderNotice() {
        List<Integer> actions=new ArrayList<>();
        FrameLayout game=new FrameLayout(activity);
        View input=new View(activity){@Override public boolean onTouchEvent(MotionEvent event){actions.add(event.getActionMasked());return true;}};
        game.addView(input,new FrameLayout.LayoutParams(-1,-1));activity.setContentView(game);
        notices.enqueue(1);advance(800);
        View root=activity.getWindow().getDecorView();
        root.measure(View.MeasureSpec.makeMeasureSpec(dp(420),View.MeasureSpec.EXACTLY),View.MeasureSpec.makeMeasureSpec(dp(900),View.MeasureSpec.EXACTLY));
        root.layout(0,0,dp(420),dp(900));
        assertNotNull(debug(),banner());assertFalse(banner().isClickable());assertFalse(banner().isFocusable());
        MotionEvent.PointerProperties first=new MotionEvent.PointerProperties();first.id=0;first.toolType=MotionEvent.TOOL_TYPE_FINGER;
        MotionEvent.PointerProperties second=new MotionEvent.PointerProperties();second.id=1;second.toolType=MotionEvent.TOOL_TYPE_FINGER;
        MotionEvent.PointerCoords a=new MotionEvent.PointerCoords();a.x=dp(180);a.y=dp(60);a.pressure=1;a.size=1;
        MotionEvent.PointerCoords b=new MotionEvent.PointerCoords();b.x=dp(230);b.y=dp(60);b.pressure=1;b.size=1;
        int[] events={MotionEvent.ACTION_DOWN,MotionEvent.ACTION_POINTER_DOWN|(1<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),
            MotionEvent.ACTION_MOVE,MotionEvent.ACTION_POINTER_UP|(1<<MotionEvent.ACTION_POINTER_INDEX_SHIFT),MotionEvent.ACTION_UP};
        for(int i=0;i<events.length;++i){int count=(i==0||i==4)?1:2;
            MotionEvent event=MotionEvent.obtain(0,i*16,events[i],count,new MotionEvent.PointerProperties[]{first,second},
                new MotionEvent.PointerCoords[]{a,b},0,0,1,1,0,0,0,0);
            assertFalse(banner().dispatchTouchEvent(event));assertTrue(root.dispatchTouchEvent(event));event.recycle();}
        assertEquals(java.util.Arrays.asList(MotionEvent.ACTION_DOWN,MotionEvent.ACTION_POINTER_DOWN,MotionEvent.ACTION_MOVE,
            MotionEvent.ACTION_POINTER_UP,MotionEvent.ACTION_UP),actions);
    }

    @Test public void challengeStatusValidationRejectsMalformedNativeMessages() {
        for(long[] invalid:new long[][]{{},{1,1,1},{2,1,1,1},{1,-1,1,1},{1,5,1,1},{1,1,-1,1},{1,1,4,1},{1,1,1,-1},{1,1,1,49}})
            assertNull(AchievementsUi.validatedStatus(invalid));
        assertNull(AchievementsUi.validatedStatus(null));
        for(int state=0;state<=4;++state){long[] value={1,state,3,48};long[] read=AchievementsUi.validatedStatus(value);
            assertArrayEquals(value,read);assertNotSame(value,read);}
    }

    @Test public void delayedStoreCallbackAfterPauseDoesNotReplayAStaleCelebration() {
        ArrayDeque<Runnable> work=new ArrayDeque<>();
        AchievementsUiTest.MemoryDisk disk=new AchievementsUiTest.MemoryDisk();
        AchievementsStore store=new AchievementsStore(disk,work::addLast,new Handler(Looper.getMainLooper()));
        long[] nativeValue={2,0,0,0,0,1,0,0};
        AchievementsUi ui=new AchievementsUi(activity,(es,en,pt)->es,(dialog,parent)->{},()->{},store,()->nativeValue.clone());
        try {
            ui.onResume();advance(32);assertEquals(1,work.size());
            ui.onPause();ui.onResume();advance(32);
            while(!work.isEmpty())work.removeFirst().run();advance(32);
            assertEquals(1,disk.value.unlocked());assertNull(banner());
            nativeValue[4]=1;advance(1000);
            while(!work.isEmpty())work.removeFirst().run();advance(32);
            assertEquals(2,disk.value.unlocked());assertNotNull(banner());
            assertEquals("Logro desbloqueado: Rojo brillante",banner().getContentDescription());
        } finally { ui.close();while(!work.isEmpty())work.removeFirst().run();advance(32); }
    }
}
