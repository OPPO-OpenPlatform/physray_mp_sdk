/*****************************************************************************
 * Copyright (C) 2020 - 2023 OPPO. All rights reserved.
 *******************************************************************************/

#include "app.h"
#include <android/native_window_jni.h>
#include <android/asset_manager_jni.h>

using namespace ph;

// ---------------------------------------------------------------------------------------------------------------------
//
static std::string to_string(JNIEnv * env, jstring js) {
    jboolean     isCopy;
    const char * convertedValue = env->GetStringUTFChars(js, &isCopy);
    auto         s              = std::string(convertedValue);
    env->ReleaseStringUTFChars(js, convertedValue);
    return s;
}

static AndroidDemoApp::ConstructParameters cp;
static AndroidDemoApp *                    app = nullptr;

// ---------------------------------------------------------------------------------------------------------------------
//
extern "C" JNIEXPORT void JNICALL Java_com_innopeak_ph_sdk_sample_hub_Native_create(JNIEnv * env, jobject, jstring jname, jobject jsurface, jobject jam,
                                                                                    jboolean rasterized, jboolean hw, jboolean animated,
                                                                                    jboolean useVmaAllocator) {
    cp.name            = to_string(env, jname);
    cp.win             = ANativeWindow_fromSurface(env, jsurface);
    cp.rasterized      = rasterized;
    cp.rayQuery        = hw;
    cp.animated        = animated;
    cp.useVmaAllocator = useVmaAllocator;

    // Store the global asset manager pointer
    AssetSystem::setAndroidAssetManager(AAssetManager_fromJava(env, jam));
}

// ---------------------------------------------------------------------------------------------------------------------
//
extern "C" JNIEXPORT void JNICALL Java_com_innopeak_ph_sdk_sample_hub_Native_delete(JNIEnv * env, jobject) {
    try {
        delete app;
        app = nullptr;
    } catch (...) {}
}

// ---------------------------------------------------------------------------------------------------------------------
//
extern "C" JNIEXPORT void JNICALL Java_com_innopeak_ph_sdk_sample_hub_Native_resize(JNIEnv *, jobject) {
    try {
        if (!app) { app = new AndroidDemoApp(cp); }
    } catch (...) {}
}

// ---------------------------------------------------------------------------------------------------------------------
//
extern "C" JNIEXPORT void JNICALL Java_com_innopeak_ph_sdk_sample_hub_Native_render(JNIEnv * env, jobject) {
    try {
        if (app) app->render();
    } catch (...) {}
}

// ---------------------------------------------------------------------------------------------------------------------
//
extern "C" JNIEXPORT void JNICALL Java_com_innopeak_ph_sdk_sample_hub_Native_touch(JNIEnv * env, jobject, jintArray touchIds, jfloatArray touchPositions) {
    try {
        if (app) {
            // List of touch events.
            std::vector<Touch> touches;

            // when all fingers are lift up, touchIds and touchPositions could be NULL.
            if (touchIds) {
                // Get count of touches.
                jsize touchCount = env->GetArrayLength(touchIds);

                // Convert java arrays to C++ pointers.
                jint *   touchIdsPointer       = env->GetIntArrayElements(touchIds, 0);
                jfloat * touchPositionsPointer = env->GetFloatArrayElements(touchPositions, 0);

                for (jsize index = 0; index < touchCount; ++index) {
                    touches.emplace_back(touchIdsPointer[index], touchPositionsPointer[index * 2 + 0], touchPositionsPointer[index * 2 + 1]);
                }
            }

            app->handleTouchEvent({std::move(touches)});
        }
    } catch (...) {}
}

// ---------------------------------------------------------------------------------------------------------------------
//
namespace ph::rt {
void unitTest();
}
extern "C" JNIEXPORT void JNICALL Java_com_innopeak_ph_sdk_sample_hub_Native_unitTest(JNIEnv *, jclass clazz) { ph::rt::unitTest(); }
