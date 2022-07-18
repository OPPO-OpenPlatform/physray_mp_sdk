package com.innopeak.ph.sdk.sample.hub;

import android.content.res.AssetManager;
import android.view.Surface;

public class Native {
    // Used to load the native library on application startup.
    static { System.loadLibrary("hub"); }

    /// create new demo selected by name
    public static native void create(String name, Surface view, AssetManager am, boolean rasterized, boolean hw, boolean animated, boolean useVmaAllocator);

    /// delete demo instance
    public static native void delete();

    /// notify the demo for surface size change.
    public static native void resize();

    /// notify the demo to render a new frame.
    public static native void render();

    /// notify the demo a new touch event just occurred.
    /// TODO: consider pass java struct to native.
    public static native void touch(int[] touchIds, float[] touchPositions);

    /// unit test
    public static native void unitTest();
}
