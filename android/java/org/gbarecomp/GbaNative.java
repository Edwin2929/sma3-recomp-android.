package org.gbarecomp;

/**
 * JNI entry points exported by the gbarecomp runtime (libmain.so).
 *
 * The native side lives in the engine's src/runtime/host_window.cpp and this
 * project's src/frame_interpolation_jni.cpp. Every method here is safe to call
 * from the UI thread; the runtime reads the values on its own thread.
 */
public final class GbaNative {
    private GbaNative() {}
    public static native float[] getTouchDesign(boolean defaults);
    public static native boolean setTouchDesign(float[] values, boolean enabled);
    public static native void setTouchEditing(boolean editing);
    public static native void setLanguage(int language);
    // Image modes: 0 original 3:2, 1 panoramic, 2 stretch.
    public static native void setVideoOptions(int quality, int imageMode);
    // Image filters: 0 balanced, 1 nearest, 2 bilinear, 3 Scale2x, 4 Scale3x, 5 soft CRT, 6 cinematic.
    public static native void setVideoFilter(int filter);
    // Mod operations return the authoritative installed catalog as JSON and are
    // called serially on the Android mod worker, never on the UI thread.
    public static native String listMods();
    public static native String importMod(String cachedPath);
    public static native String setModEnabled(String id, boolean enabled);
    public static native String removeMod(String id);
    // Built-in character palette mod; RGB is 0xRRGGBB. One atomic update,
    // independent of installed whole-image color profiles.
    public static native void setYoshiColor(boolean enabled, int rgb);
    public static native void setTouchControlsVisible(boolean visible);
    // Styles: 0 classic letters, 1 action symbols. Opacity is clamped to 20..100%.
    public static native void setTouchAppearance(int style, int opacityPercent);
    public static native void setControllerButton(int bit, int button);
    public static native void setOptionsOpen(boolean open);
    public static native void setInterpolationEnabled(boolean enabled);
    public static native void resetPresentationTiming();

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
