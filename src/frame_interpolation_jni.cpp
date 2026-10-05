#include "frame_interpolation.h"
#include <jni.h>
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setInterpolationEnabled(JNIEnv*, jclass, jboolean on) {
    sma3::interpolation_enabled.store(on == JNI_TRUE, std::memory_order_relaxed);
}

#include "controller_options.h"
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setTouchControlsVisible(JNIEnv*, jclass, jboolean on) {
    sma3::touch_visible.store(on == JNI_TRUE);
}
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setControllerButton(JNIEnv*, jclass, jint bit, jint button) {
    if (bit >= 0 && bit < 10 && button >= -1 && button <= 20)
        sma3::controller_buttons[bit].store(button);
}
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setOptionsOpen(JNIEnv*, jclass, jboolean on) {
    sma3::menu_open.store(on == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setVideoOptions(JNIEnv*, jclass, jint quality, jboolean stretch) {
    sma3::video_quality.store(quality == 720 || quality == 1080 ? quality : 0);
    sma3::stretch_screen.store(stretch == JNI_TRUE);
}

#include "localization.h"
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setLanguage(JNIEnv*, jclass, jint language) {
    sma3::language.store(language>=0 && language<=2 ? language : 0);
}
