#include "frame_interpolation.h"
#include <jni.h>
extern "C" JNIEXPORT void JNICALL
Java_org_gbarecomp_GbaNative_setInterpolationEnabled(JNIEnv*, jclass, jboolean on) {
    sma3::interpolation_enabled.store(on == JNI_TRUE, std::memory_order_relaxed);
}
