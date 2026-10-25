package org.gbarecomp;

import android.app.Activity;
import android.os.Build;
import android.view.Display;
import android.view.Surface;
import android.view.WindowManager;

/**
 * UI-thread-only refresh requests. The platform chooses the actual mode.
 * Activity supplies lifecycle and display/surface callbacks; this class never
 * posts a per-frame request or mistakes a requested rate for measured FPS.
 */
final class DisplayRefreshController {
    static final class State {
        final int requestedFps;
        final float physicalHz, appHz;
        final boolean supports120, doubleRateActive, requestApplied;
        State(int requestedFps, float physicalHz, float appHz, boolean supports120,
              boolean doubleRateActive, boolean requestApplied) {
            this.requestedFps = requestedFps;
            this.physicalHz = physicalHz;
            this.appHz = appHz;
            this.supports120 = supports120;
            this.doubleRateActive = doubleRateActive;
            this.requestApplied = requestApplied;
        }
    }

    private final Activity activity;
    private Surface lastSurface;
    private float lastRate = Float.NaN;

    DisplayRefreshController(Activity activity) { this.activity = activity; }

    // A SurfaceHolder may reuse its Java Surface wrapper after recreation.
    // The Activity calls this from surfaceCreated; no hidden Surface APIs.
    void invalidateSurface() { lastSurface = null; lastRate = Float.NaN; }

    State apply(boolean resumed, boolean requested120, Surface surface) {
        final int requestedFps = requested120 ? 120 : 60;
        Display display = activity.getWindowManager().getDefaultDisplay();
        if (display == null) return new State(requestedFps, 0, 0, false, false, false);
        Display.Mode current = display.getMode();
        Display.Mode[] supported = display.getSupportedModes();
        RefreshRatePolicy.Mode[] modes = new RefreshRatePolicy.Mode[supported.length];
        for (int i = 0; i < supported.length; ++i) {
            Display.Mode mode = supported[i];
            modes[i] = new RefreshRatePolicy.Mode(mode.getModeId(), mode.getPhysicalWidth(),
                mode.getPhysicalHeight(), mode.getRefreshRate());
        }
        final boolean supports120 = RefreshRatePolicy.supportsDouble(
            current.getPhysicalWidth(), current.getPhysicalHeight(), modes);
        final float physicalHz = current.getRefreshRate();
        // Android may return an application-specific frame-rate override here
        // even while Display.Mode describes a faster physical panel mode.
        final float appHz = display.getRefreshRate();
        final int preferredMode = resumed ? RefreshRatePolicy.selectMode(requestedFps,
            current.getPhysicalWidth(), current.getPhysicalHeight(), modes) : 0;
        final float preferredRate = resumed ? requestedFps : 0f;
        WindowManager.LayoutParams attributes = activity.getWindow().getAttributes();
        if (attributes.preferredDisplayModeId != preferredMode ||
                attributes.preferredRefreshRate != preferredRate) {
            attributes.preferredDisplayModeId = preferredMode;
            attributes.preferredRefreshRate = preferredRate;
            activity.getWindow().setAttributes(attributes);
        }
        final boolean validSurface = surface != null && surface.isValid();
        boolean applied = Build.VERSION.SDK_INT < 30;
        if (Build.VERSION.SDK_INT >= 30 && validSurface) {
            final float rate = !resumed ? 0f : RefreshRatePolicy.GBA_HZ *
                (requested120 && supports120 ? 2f : 1f);
            if (lastSurface == surface && lastRate == rate) {
                applied = true;
            } else {
                try {
                    if (Build.VERSION.SDK_INT >= 31) {
                        surface.setFrameRate(rate, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT,
                            Surface.CHANGE_FRAME_RATE_ONLY_IF_SEAMLESS);
                    } else {
                        surface.setFrameRate(rate, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT);
                    }
                    lastSurface = surface; lastRate = rate;
                    applied = true;
                } catch (IllegalArgumentException | IllegalStateException unavailable) {
                    invalidateSurface();
                }
            }
        } else if (!validSurface) {
            invalidateSurface();
        }
        return new State(requestedFps, physicalHz, appHz, supports120,
            RefreshRatePolicy.doubleRateActive(resumed, validSurface, requested120,
                                               physicalHz, appHz), applied);
    }
}
