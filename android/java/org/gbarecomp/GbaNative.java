package org.gbarecomp;

/**
 * JNI entry points exported by the gbarecomp runtime (libmain.so).
 *
 * The native side lives in src/runtime/host_window.cpp. Every method here is
 * safe to call from the UI thread; the runtime reads the values on its own
 * thread at the next present.
 */
public final class GbaNative {
    private GbaNative() {}
    public static native void setLanguage(int language);
    public static native void setVideoOptions(int quality, boolean stretch);
    public static native void setTouchControlsVisible(boolean visible);
    public static native void setControllerButton(int bit, int button);
    public static native void setOptionsOpen(boolean open);
    public static native void setInterpolationEnabled(boolean enabled);

    /**
     * Safe-area insets in physical pixels: display cutouts and mandatory
     * system-gesture regions. Host chrome (touch pad, native buttons) is laid
     * out inside them; the game image may extend underneath.
     */
    public static native void setSafeInsets(int left, int top, int right, int bottom);

    /**
     * Physical pixel density of the panel in the current display mode
     * (DisplayMetrics from getRealMetrics). Touch targets and gesture
     * thresholds are sized in millimetres from it; densityDpi is a user-
     * scalable logical setting and can differ from the glass by 25% or more.
     */
    public static native void setDisplayDpi(float xdpi, float ydpi);
}
