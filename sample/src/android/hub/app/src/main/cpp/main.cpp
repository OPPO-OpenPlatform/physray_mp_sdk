#include "app.h"
#include <android_native_app_glue.h>
#include <algorithm>
#include <cmath>

using namespace ph;

// Note: this cpp file is to launch the app via Android native activity.
// Usually, the demo is launched via JAVA activity. This "pure native" mode
// is only kept as backup, since debugging native app is sometime faster than
// debugging java-native hybrid app on Android platform.

struct AutoJNIEnv {
    android_app * app;
    JNIEnv *      env;
    AutoJNIEnv(android_app * app_): app(app_) { app->activity->vm->AttachCurrentThread(&env, NULL); }
    ~AutoJNIEnv() { app->activity->vm->DetachCurrentThread(); }
    JNIEnv * operator->() const { return env; }
};

AndroidDemoApp * demo      = nullptr;
std::string      sceneName = "Garage"; // launch garage scene by default.

/// Enable fullscreen immersive mode
bool enableImmersiveMode(android_app * app) {
    AutoJNIEnv env(app);

    jclass    activityClass         = env->FindClass("android/app/NativeActivity");
    jclass    windowClass           = env->FindClass("android/view/Window");
    jclass    viewClass             = env->FindClass("android/view/View");
    jmethodID getWindow             = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
    jmethodID getDecorView          = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
    jmethodID setSystemUiVisibility = env->GetMethodID(viewClass, "setSystemUiVisibility", "(I)V");
    jmethodID getSystemUiVisibility = env->GetMethodID(viewClass, "getSystemUiVisibility", "()I");

    jobject windowObj    = env->CallObjectMethod(app->activity->clazz, getWindow);
    jobject decorViewObj = env->CallObjectMethod(windowObj, getDecorView);

    // Get flag ids
    jfieldID id_SYSTEM_UI_FLAG_LAYOUT_STABLE          = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_STABLE", "I");
    jfieldID id_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION", "I");
    jfieldID id_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN      = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN", "I");
    jfieldID id_SYSTEM_UI_FLAG_HIDE_NAVIGATION        = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_HIDE_NAVIGATION", "I");
    jfieldID id_SYSTEM_UI_FLAG_FULLSCREEN             = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_FULLSCREEN", "I");
    jfieldID id_SYSTEM_UI_FLAG_IMMERSIVE_STICKY       = env->GetStaticFieldID(viewClass, "SYSTEM_UI_FLAG_IMMERSIVE_STICKY", "I");

    // Get flags
    const int flag_SYSTEM_UI_FLAG_LAYOUT_STABLE          = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LAYOUT_STABLE);
    const int flag_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
    const int flag_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN      = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN);
    const int flag_SYSTEM_UI_FLAG_HIDE_NAVIGATION        = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_HIDE_NAVIGATION);
    const int flag_SYSTEM_UI_FLAG_FULLSCREEN             = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_FULLSCREEN);
    const int flag_SYSTEM_UI_FLAG_IMMERSIVE_STICKY       = env->GetStaticIntField(viewClass, id_SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

    // Get current immersive status
    const int  currentVisibility                        = env->CallIntMethod(decorViewObj, getSystemUiVisibility);
    const bool is_SYSTEM_UI_FLAG_LAYOUT_STABLE          = (currentVisibility & flag_SYSTEM_UI_FLAG_LAYOUT_STABLE) != 0;
    const bool is_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = (currentVisibility & flag_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION) != 0;
    const bool is_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN      = (currentVisibility & flag_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) != 0;
    const bool is_SYSTEM_UI_FLAG_HIDE_NAVIGATION        = (currentVisibility & flag_SYSTEM_UI_FLAG_HIDE_NAVIGATION) != 0;
    const bool is_SYSTEM_UI_FLAG_FULLSCREEN             = (currentVisibility & flag_SYSTEM_UI_FLAG_FULLSCREEN) != 0;
    const bool is_SYSTEM_UI_FLAG_IMMERSIVE_STICKY       = (currentVisibility & flag_SYSTEM_UI_FLAG_IMMERSIVE_STICKY) != 0;

    const auto isAlreadyImmersive = is_SYSTEM_UI_FLAG_LAYOUT_STABLE && is_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION && is_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN &&
                                    is_SYSTEM_UI_FLAG_HIDE_NAVIGATION && is_SYSTEM_UI_FLAG_FULLSCREEN && is_SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
    PH_LOGV(s("set_immersive data: ") << is_SYSTEM_UI_FLAG_LAYOUT_STABLE << is_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION << is_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                      << is_SYSTEM_UI_FLAG_HIDE_NAVIGATION << is_SYSTEM_UI_FLAG_FULLSCREEN << is_SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    auto success = true;
    if (true) {
        const int flag = flag_SYSTEM_UI_FLAG_LAYOUT_STABLE | flag_SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | flag_SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                         flag_SYSTEM_UI_FLAG_HIDE_NAVIGATION | flag_SYSTEM_UI_FLAG_FULLSCREEN | flag_SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        env->CallVoidMethod(decorViewObj, setSystemUiVisibility, flag);
        if (env->ExceptionCheck()) {
            // Read exception msg
            jthrowable e = env->ExceptionOccurred();
            env->ExceptionClear(); // clears the exception; e seems to remain valid
            jclass       clazz         = env->GetObjectClass(e);
            jmethodID    getMessage    = env->GetMethodID(clazz, "getMessage", "()Ljava/lang/String;");
            jstring      message       = (jstring) env->CallObjectMethod(e, getMessage);
            const char * mstr          = env->GetStringUTFChars(message, NULL);
            const auto   exception_msg = std::string {mstr};
            env->ReleaseStringUTFChars(message, mstr);
            env->DeleteLocalRef(message);
            env->DeleteLocalRef(clazz);
            env->DeleteLocalRef(e);
            PH_LOGW(s("set_immersive exception [") << exception_msg << "]");
            success = false;
        } else {
            PH_LOGI("set_immersive success");
        }
    }
    env->DeleteLocalRef(windowObj);
    env->DeleteLocalRef(decorViewObj);
    return success;
}

std::string getStringField(JNIEnv * env, jobject activity, const char * name) {
    auto        acl   = env->GetObjectClass(activity); // class pointer of the activity
    auto        field = env->GetFieldID(acl, name, "Ljava/lang/String;");
    auto        jstr  = (jstring) env->GetObjectField(activity, field);
    auto        str   = env->GetStringUTFChars(jstr, 0);
    std::string result;
    if (str && *str) result = str;
    env->ReleaseStringUTFChars(jstr, str);
    return result;
}

bool getBooleanField(JNIEnv * env, jobject activity, const char * name) {
    auto acl   = env->GetObjectClass(activity); // class pointer of the activity
    auto field = env->GetFieldID(acl, name, "Z");
    return env->GetBooleanField(activity, field);
}

AndroidDemoApp::ConstructParameters generateDP(struct android_app * app) {
    AutoJNIEnv env(app);
    auto       me = app->activity->clazz;
    auto       cp = AndroidDemoApp::ConstructParameters {
        .name       = getStringField(env.env, me, "sceneName"),
        .win        = app->window,
        .rasterized = getBooleanField(env.env, me, "rasterized"),
        .rayQuery   = getBooleanField(env.env, me, "hw"),
        .animated   = getBooleanField(env.env, me, "animated"),
    };
    if (cp.name.empty()) cp.name = "Cornell Box";
    return cp;
}

/// Process the next main command.
void handleAppCmd(android_app * app, int32_t cmd) {
    switch (cmd) {
    case APP_CMD_WINDOW_RESIZED: {
        if (!demo) { demo = new AndroidDemoApp(generateDP(app)); }
        break;
    }
    case APP_CMD_TERM_WINDOW:
        if (demo) delete demo, demo = nullptr;
        break;

        // TODO: pause resume
    }
}

int32_t handleInputEvent(struct android_app *, AInputEvent * event) {
    // make sure demo app is created.
    if (!demo) return 0;
    return demo->handleInputEvent(event);
}

void android_main(struct android_app * app) {
    // Set the callback to process system events
    app->onAppCmd     = handleAppCmd;
    app->onInputEvent = handleInputEvent;

    // Store the global asset manager pointer
    AssetSystem::setAndroidAssetManager(app->activity->assetManager);

    // Forward cout/cerr to logcat.
    // std::cout.rdbuf(new AndroidBuffer(ANDROID_LOG_INFO));
    // std::cerr.rdbuf(new AndroidBuffer(ANDROID_LOG_ERROR));

    enableImmersiveMode(app);

    // Main loop
    while (app->destroyRequested == 0) {
        // Process system events
        int                   events;
        android_poll_source * source;
        while (ALooper_pollAll(demo ? 0 : -1, nullptr, &events, (void **) &source) >= 0) {
            if (source != NULL) source->process(app, source);
        }

        // do frame animation here
        if (demo) { demo->render(); }
    }
}
