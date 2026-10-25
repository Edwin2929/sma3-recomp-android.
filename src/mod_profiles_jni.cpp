#include "mod_profiles.h"

#include <jni.h>
#include <exception>
#include <string>

namespace {
// Native JSON contains only ASCII (metadata is escaped as Unicode), which is
// valid modified UTF-8 too. Never pass arbitrary manifest UTF-8 to NewStringUTF.
template<class Operation>
jstring response(JNIEnv* env, Operation operation) {
    try {
        const std::string result = operation();
        return env->NewStringUTF(result.c_str());
    } catch (const std::exception&) {
        return env->NewStringUTF("{\"ok\":false,\"error\":\"No se pudo completar la operacion de mods.\",\"mods\":[]}");
    }
}

class JavaText {
public:
    JavaText(JNIEnv* env, jstring value) : env_(env), value_(value) {
        if (value) text_ = env->GetStringUTFChars(value, nullptr);
    }
    ~JavaText() { if (text_) env_->ReleaseStringUTFChars(value_, text_); }
    const char* get() const { return text_; }
private:
    JNIEnv* env_;
    jstring value_;
    const char* text_ = nullptr;
};
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_gbarecomp_GbaNative_listMods(JNIEnv* env, jclass) {
    return response(env, [] { return gbarecomp::sma3_mod_list(); });
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_gbarecomp_GbaNative_importMod(JNIEnv* env, jclass, jstring path) {
    JavaText text(env, path);
    if (env->ExceptionCheck()) return nullptr;
    return response(env, [&] {
        return gbarecomp::sma3_mod_import(text.get() ? text.get() : "");
    });
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_gbarecomp_GbaNative_setModEnabled(JNIEnv* env, jclass, jstring id, jboolean enabled) {
    JavaText text(env, id);
    if (env->ExceptionCheck()) return nullptr;
    return response(env, [&] {
        return gbarecomp::sma3_mod_set_enabled(text.get() ? text.get() : "", enabled == JNI_TRUE);
    });
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_gbarecomp_GbaNative_removeMod(JNIEnv* env, jclass, jstring id) {
    JavaText text(env, id);
    if (env->ExceptionCheck()) return nullptr;
    return response(env, [&] {
        return gbarecomp::sma3_mod_remove(text.get() ? text.get() : "");
    });
}
