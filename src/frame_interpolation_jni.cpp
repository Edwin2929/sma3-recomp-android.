#include "frame_interpolation.h"
#include "presentation_pacing.h"
#include "yoshi_color.h"
#include "local_achievements.h"
#include <bit>
#include <jni.h>
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setInterpolationEnabled(JNIEnv*, jclass, jboolean on) {
    sma3::interpolation_enabled.store(on == JNI_TRUE, std::memory_order_relaxed);
}
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_resetPresentationTiming(JNIEnv*, jclass) {
    sma3::presentation_epoch.fetch_add(1, std::memory_order_relaxed);
}

#include "controller_options.h"
#include "touch_appearance.h"
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setTouchControlsVisible(JNIEnv*, jclass, jboolean on) {
    sma3::touch_visible.store(on == JNI_TRUE);
}
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setTouchAppearance(JNIEnv*, jclass, jint style, jint opacity) {
    sma3::set_touch_appearance(style, opacity);
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
Java_org_gbarecomp_GbaNative_setVideoOptions(JNIEnv*, jclass, jint quality, jint image_mode) {
    sma3::video_quality.store(quality == 720 || quality == 1080 ? quality : 0);
    const auto mode = sma3::validated_image_mode(image_mode);
    if (sma3::image_mode.exchange(mode) != mode)
        sma3::presentation_epoch.fetch_add(1, std::memory_order_relaxed);
}

extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setVideoFilter(JNIEnv*, jclass, jint filter) {
    sma3::image_filter.store(sma3::validated_image_filter(filter), std::memory_order_relaxed);
}

extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setYoshiColor(JNIEnv*, jclass, jboolean enabled, jint rgb) {
    // Java accepts only a 24-bit color. Reject invalid JNI callers as well;
    // alpha/sign bits must never become an unintended palette configuration.
    if (rgb < 0 || rgb > 0xffffff) return;
    sma3::set_yoshi_color(enabled == JNI_TRUE, static_cast<uint32_t>(rgb));
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_org_gbarecomp_GbaNative_getLocalAchievements(JNIEnv* env, jclass) {
    // The game thread owns observation. JNI only copies a small, coherent
    // published snapshot; it never reads game memory or consumes events.
    const auto progress = sma3::local_achievements_snapshot();
    std::array<jlong, 8> values{};
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = std::bit_cast<jlong>(progress[i]);
    auto output = env->NewLongArray(static_cast<jsize>(values.size()));
    if (output)
        env->SetLongArrayRegion(output, 0, static_cast<jsize>(values.size()), values.data());
    return output;
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_org_gbarecomp_GbaNative_getYoShiPudeStatus(JNIEnv* env, jclass) {
    const auto status = sma3::local_achievements_challenge_status();
    std::array<jlong, 4> values{};
    for (std::size_t i = 0; i < values.size(); ++i)
        values[i] = std::bit_cast<jlong>(status[i]);
    auto output = env->NewLongArray(static_cast<jsize>(values.size()));
    if (output)
        env->SetLongArrayRegion(output, 0, static_cast<jsize>(values.size()), values.data());
    return output;
}

#include "localization.h"
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setLanguage(JNIEnv*, jclass, jint language) {
    sma3::language.store(language>=0 && language<=2 ? language : 0);
}

#include "touch_layout.h"
extern "C" JNIEXPORT jfloatArray JNICALL
Java_org_gbarecomp_GbaNative_getTouchDesign(JNIEnv* env,jclass,jboolean defaults){
 auto data=sma3::get_touch_design(defaults==JNI_TRUE);jfloatArray out=env->NewFloatArray(28);
 if(out)env->SetFloatArrayRegion(out,0,28,data.data());return out;
}
extern "C" JNIEXPORT jboolean JNICALL
Java_org_gbarecomp_GbaNative_setTouchDesign(JNIEnv* env,jclass,jfloatArray values,jboolean enabled){
 sma3::TouchDesign data{};if(enabled&&(!values||env->GetArrayLength(values)!=28))return JNI_FALSE;
 if(values&&env->GetArrayLength(values)==28)env->GetFloatArrayRegion(values,0,28,data.data());
 if(env->ExceptionCheck())return JNI_FALSE;
 return sma3::set_touch_design(data,enabled==JNI_TRUE)?JNI_TRUE:JNI_FALSE;
}
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setTouchEditing(JNIEnv*,jclass,jboolean on){sma3::touch_editing.store(on==JNI_TRUE);}
