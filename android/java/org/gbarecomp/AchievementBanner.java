package org.gbarecomp;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Insets;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.text.Layout;
import android.text.StaticLayout;
import android.text.TextPaint;
import android.text.TextUtils;
import android.view.DisplayCutout;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.accessibility.AccessibilityManager;
import android.view.animation.LinearInterpolator;
import android.widget.FrameLayout;

import java.util.ArrayDeque;

/** A foreground-only, touch-through celebration. All work stays on Android's UI thread. */
final class AchievementBanner {
    interface Translator { String text(String es, String en, String pt); }
    static final int HOLD_MILLIS = 3400;
    static final int ENTER_MILLIS = 220;
    static final int MEDALLION_MILLIS = 160;
    static final int EXPAND_MILLIS = 320;
    static final int COLLAPSE_MILLIS = 260;
    static final int EXIT_MILLIS = 180;
    static final int GAP_MILLIS = 180;
    static final long MOTION_MILLIS = ENTER_MILLIS + MEDALLION_MILLIS
        + EXPAND_MILLIS + COLLAPSE_MILLIS + EXIT_MILLIS;

    private final Activity activity;
    private final Translator translator;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final ArrayDeque<Integer> pending = new ArrayDeque<>();
    private int seenMask;
    private boolean enabled = true, active, closed, nextScheduled;
    private BannerView banner;
    private ValueAnimator animator;
    private ViewGroup host;
    private final Runnable next = () -> { nextScheduled = false; showNext(); };
    private final Runnable finish = this::finishCurrent;
    private Runnable leave;

    AchievementBanner(Activity activity, Translator translator) {
        this.activity = activity;
        this.translator = translator;
    }

    void resume() { if (!closed) active = true; }

    void pause() {
        active = false;
        clear();
    }

    void enabled(boolean value) {
        enabled = value;
        if (!value) clear();
    }

    void close() {
        closed = true;
        pause();
    }

    /** The store supplies only newly committed unlocks, never upgrade recognition. */
    void enqueue(int mask) {
        if (!active || !enabled || closed || activity.isFinishing() || activity.isDestroyed()) return;
        mask &= AchievementsModel.ALL_IDS_MASK & ~seenMask;
        seenMask |= mask;
        for (int index = 0; index < AchievementsModel.DEFINITIONS.length; ++index)
            if ((mask & (1 << index)) != 0) pending.addLast(index);
        if (banner == null && !nextScheduled) showNext();
    }

    private int recommendedHold() {
        AccessibilityManager manager = (AccessibilityManager) activity.getSystemService(Context.ACCESSIBILITY_SERVICE);
        if (Build.VERSION.SDK_INT >= 29 && manager != null)
            return Math.max(HOLD_MILLIS, manager.getRecommendedTimeoutMillis(HOLD_MILLIS,
                AccessibilityManager.FLAG_CONTENT_TEXT | AccessibilityManager.FLAG_CONTENT_ICONS));
        // Android 9 has no timeout API. Give screen-reader users additional reading time.
        return manager != null && manager.isTouchExplorationEnabled() ? HOLD_MILLIS * 2 : HOLD_MILLIS;
    }

    private void showNext() {
        if (!active || !enabled || closed || activity.isFinishing() || activity.isDestroyed()) {
            clear();
            return;
        }
        if (banner != null || pending.isEmpty()) return;
        View root = activity.getWindow().getDecorView();
        if (!(root instanceof ViewGroup)) { pending.clear(); return; }
        host = (ViewGroup) root;
        int index = pending.removeFirst();
        AchievementsModel.Definition definition = AchievementsModel.DEFINITIONS[index];
        String title = translator.text(definition.title[0], definition.title[1], definition.title[2]);
        String heading = translator.text("Logro desbloqueado", "Achievement unlocked", "Conquista desbloqueada");
        banner = new BannerView(activity, definition.id, heading, title);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(-1, -2, Gravity.TOP);
        host.addView(banner, params);
        banner.applySafeInsets(root.getRootWindowInsets());
        banner.requestApplyInsets();
        banner.announceForAccessibility(heading + ": " + title);
        final BannerView current = banner;
        int hold = recommendedHold();
        if (!ValueAnimator.areAnimatorsEnabled()) {
            // Removing system animations must not remove the notice itself.
            current.setFrame(1, 1, 1);
            handler.postDelayed(finish, hold);
            return;
        }
        int opening = ENTER_MILLIS + MEDALLION_MILLIS + EXPAND_MILLIS;
        leave = () -> animate(current, opening + hold, MOTION_MILLIS + hold, hold, this::finishCurrent);
        animate(current, 0, opening, hold, () -> {
            current.setFrame(1, 1, 1);
            // No animator or frame invalidation while the title is simply being read.
            // This avoids scheduling a continuous UI animation over the SDL game.
            handler.postDelayed(leave, hold);
        });
    }

    private void animate(BannerView current, long from, long to, int hold, Runnable ended) {
        if (!ValueAnimator.areAnimatorsEnabled()) { finishCurrent(); return; }
        ValueAnimator animation = ValueAnimator.ofFloat(from, to);
        animator = animation;
        animation.setInterpolator(new LinearInterpolator());
        animation.setDuration(to - from);
        animation.addUpdateListener(value -> current.setTime(((Number) value.getAnimatedValue()).floatValue(), hold));
        animation.addListener(new AnimatorListenerAdapter() {
            @Override public void onAnimationEnd(Animator animation) {
                if (banner == current && active && !closed) {
                    animator = null;
                    ended.run();
                }
            }
        });
        animation.start();
    }

    private void stopAnimation() {
        handler.removeCallbacks(finish);
        if (leave != null) handler.removeCallbacks(leave);
        leave = null;
        if (animator != null) {
            animator.removeAllListeners();
            animator.removeAllUpdateListeners();
            animator.cancel();
            animator = null;
        }
    }

    private void detach() {
        if (banner != null && banner.getParent() instanceof ViewGroup)
            ((ViewGroup) banner.getParent()).removeView(banner);
        banner = null;
        host = null;
    }

    private void finishCurrent() {
        stopAnimation();
        detach();
        if (active && enabled && !closed && !pending.isEmpty()) {
            nextScheduled = true;
            handler.postDelayed(next, GAP_MILLIS);
        }
    }

    private void clear() {
        handler.removeCallbacks(next);
        nextScheduled = false;
        stopAnimation();
        pending.clear();
        detach();
    }

    /** Draws one small surface; animation never remeasures the SDL game or its controls. */
    static final class BannerView extends View {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private final TextPaint headingPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        private final TextPaint titlePaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
        private final RectF pill = new RectF();
        private final RectF scratch = new RectF();
        private final Path egg = new Path();
        private final String heading, title;
        private StaticLayout headingLayout, titleLayout;
        private float scale = 0, expansion = 0, opacity = 0;
        private int insetLeft, insetRight, insetTop;
        private float cardLeft, cardWidth, cardHeight, centerY;
        private Shader fill;

        BannerView(Context context, String id, String heading, String title) {
            super(context);
            this.heading = heading;
            this.title = title;
            setTag("achievement-banner");
            setContentDescription(heading + ": " + title);
            setTooltipText(null);
            setClickable(false);
            setLongClickable(false);
            setFocusable(false);
            setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_YES);
            // The ID remains accessible to UI verification without exposing a touch target.
            setTransitionName("achievement-notice-" + id);
        }

        private float dp(float value) { return value * getResources().getDisplayMetrics().density; }
        private float sp(float value) { return value * getResources().getDisplayMetrics().scaledDensity; }

        void applySafeInsets(WindowInsets insets) {
            if (insets == null) return;
            int left, top, right;
            if (Build.VERSION.SDK_INT >= 30) {
                Insets bars = insets.getInsets(WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                left = bars.left; top = bars.top; right = bars.right;
            } else {
                left = insets.getSystemWindowInsetLeft();
                top = insets.getSystemWindowInsetTop();
                right = insets.getSystemWindowInsetRight();
                DisplayCutout cutout = insets.getDisplayCutout();
                if (cutout != null) {
                    left = Math.max(left, cutout.getSafeInsetLeft());
                    top = Math.max(top, cutout.getSafeInsetTop());
                    right = Math.max(right, cutout.getSafeInsetRight());
                }
            }
            if (left != insetLeft || top != insetTop || right != insetRight) {
                insetLeft = left; insetTop = top; insetRight = right;
                requestLayout();
            }
        }

        @Override public WindowInsets onApplyWindowInsets(WindowInsets insets) {
            applySafeInsets(insets);
            return insets;
        }

        // Return false even for pointer-down/move/up. The original game view keeps
        // its multi-touch stream, including touches inside the visible banner.
        @Override public boolean dispatchTouchEvent(MotionEvent event) { return false; }
        @Override public boolean onTouchEvent(MotionEvent event) { return false; }
        @Override public boolean onGenericMotionEvent(MotionEvent event) { return false; }

        @Override protected void onMeasure(int widthSpec, int heightSpec) {
            int width = MeasureSpec.getSize(widthSpec);
            int safeWidth = Math.max(1, width - insetLeft - insetRight);
            cardWidth = Math.max(1, Math.min(dp(420), safeWidth - dp(24)));
            cardLeft = insetLeft + (safeWidth - cardWidth) / 2;
            headingPaint.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
            headingPaint.setTextSize(sp(11)); headingPaint.setColor(0xffbfe6a4);
            titlePaint.setTypeface(Typeface.create("sans-serif", Typeface.BOLD));
            titlePaint.setTextSize(sp(17)); titlePaint.setColor(Color.WHITE);
            int textWidth = Math.max(1, Math.round(cardWidth - dp(86)));
            headingLayout = layout(heading, headingPaint, textWidth, 2);
            titleLayout = layout(title, titlePaint, textWidth, 2);
            cardHeight = Math.max(dp(68), headingLayout.getHeight() + titleLayout.getHeight() + dp(25));
            centerY = insetTop + dp(12) + cardHeight / 2;
            fill = new LinearGradient(cardLeft, 0, cardLeft + cardWidth, cardHeight,
                new int[]{0xff246b37, 0xff123f27, 0xff102e20}, null, Shader.TileMode.CLAMP);
            setMeasuredDimension(width, resolveSize((int)Math.ceil(insetTop + dp(24) + cardHeight), heightSpec));
        }

        private StaticLayout layout(String text, TextPaint paint, int width, int lines) {
            return StaticLayout.Builder.obtain(text, 0, text.length(), paint, width)
                .setAlignment(Layout.Alignment.ALIGN_NORMAL).setIncludePad(false)
                .setMaxLines(lines).setEllipsize(TextUtils.TruncateAt.END)
                .setBreakStrategy(Layout.BREAK_STRATEGY_HIGH_QUALITY).build();
        }

        private static float clamp(float value) { return Math.max(0, Math.min(1, value)); }
        private static float ease(float value) { value = clamp(value); return 1 - (1-value)*(1-value)*(1-value); }

        void setTime(float elapsed, int hold) {
            float entering = ease(elapsed / ENTER_MILLIS);
            float opening = ease((elapsed - ENTER_MILLIS - MEDALLION_MILLIS) / EXPAND_MILLIS);
            float closing = ease((elapsed - ENTER_MILLIS - MEDALLION_MILLIS - EXPAND_MILLIS - hold) / COLLAPSE_MILLIS);
            float leaving = ease((elapsed - ENTER_MILLIS - MEDALLION_MILLIS - EXPAND_MILLIS - hold - COLLAPSE_MILLIS) / EXIT_MILLIS);
            setFrame(entering * (1-leaving), opening * (1-closing), entering * (1-leaving));
        }

        void setFrame(float scale, float expansion, float opacity) {
            scale = clamp(scale); expansion = clamp(expansion); opacity = clamp(opacity);
            if (this.scale == scale && this.expansion == expansion && this.opacity == opacity) return;
            this.scale = scale; this.expansion = expansion; this.opacity = opacity;
            invalidate();
        }

        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            if (opacity <= 0 || cardWidth <= 0 || headingLayout == null) return;
            float diameter = Math.min(dp(64), cardWidth);
            float width = diameter + (cardWidth - diameter) * expansion;
            float height = diameter + (cardHeight - diameter) * expansion;
            float centerX = cardLeft + cardWidth / 2;
            pill.set(centerX - width/2, centerY - height/2, centerX + width/2, centerY + height/2);
            canvas.save();
            canvas.translate(0, -dp(8) * (1-scale));
            canvas.scale(.78f + .22f*scale, .78f + .22f*scale, centerX, centerY);
            paint.setShader(null); paint.setStyle(Paint.Style.FILL);
            paint.setColor(0x44000000); paint.setAlpha((int)(68*opacity));
            scratch.set(pill); scratch.offset(0, dp(3));
            canvas.drawRoundRect(scratch, height/2, height/2, paint);
            paint.setShader(fill); paint.setAlpha((int)(255*opacity));
            canvas.drawRoundRect(pill, height/2, height/2, paint);
            paint.setShader(null); paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(dp(1));
            paint.setColor(0xff78bb69); paint.setAlpha((int)(180*opacity));
            canvas.drawRoundRect(pill, height/2, height/2, paint);
            paint.setStyle(Paint.Style.FILL);

            float iconX = centerX + (cardLeft + dp(36) - centerX) * expansion;
            paint.setColor(0xff245a31); paint.setAlpha((int)(255*opacity));
            canvas.drawCircle(iconX, centerY, dp(25), paint);
            paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(dp(1.4f));
            paint.setColor(0xffa9e477); paint.setAlpha((int)(255*opacity));
            canvas.drawCircle(iconX, centerY, dp(25), paint);
            paint.setStyle(Paint.Style.FILL);
            drawEgg(canvas, iconX, centerY, opacity);

            float textAlpha = clamp((expansion - .4f) / .6f) * opacity;
            if (textAlpha > 0) {
                canvas.save();
                canvas.clipRect(pill.left, pill.top, pill.right - dp(10), pill.bottom);
                // Follow the moving medallion while opening/closing so the
                // revealed title can never overlap the egg in an in-between frame.
                canvas.translate(iconX + dp(36), centerY - (headingLayout.getHeight() + dp(3) + titleLayout.getHeight())/2);
                headingPaint.setAlpha((int)(255*textAlpha)); headingLayout.draw(canvas);
                canvas.translate(0, headingLayout.getHeight()+dp(3));
                titlePaint.setAlpha((int)(255*textAlpha)); titleLayout.draw(canvas);
                canvas.restore();
            }
            canvas.restore();
        }

        private void drawEgg(Canvas canvas, float x, float y, float alpha) {
            canvas.save(); canvas.translate(x, y); canvas.scale(dp(1), dp(1));
            egg.reset(); egg.moveTo(0,-17);
            egg.cubicTo(8,-17,14,-3,14,5); egg.cubicTo(14,14,7,18,0,18);
            egg.cubicTo(-8,18,-14,13,-14,5); egg.cubicTo(-14,-3,-8,-17,0,-17); egg.close();
            paint.setColor(0xfff4fff0); paint.setAlpha((int)(255*alpha)); canvas.drawPath(egg,paint);
            canvas.save(); canvas.clipPath(egg);
            paint.setColor(0xff65ba41); paint.setAlpha((int)(255*alpha));
            scratch.set(-4,-12,5,-2); canvas.drawOval(scratch,paint);
            scratch.set(-15,0,-5,12); canvas.drawOval(scratch,paint);
            scratch.set(5,3,15,14); canvas.drawOval(scratch,paint);
            canvas.restore(); canvas.restore();
        }
    }
}
