package com.innopeak.ph.sdk.sample.hub;

import android.content.res.AssetManager;
import android.view.Surface;

public class RenderOptions {
    public String       name;
    public Surface      surface;
    public AssetManager assets;
    public boolean      rasterized      = false; // set to true to disable ray tracing pipeline and fall back to rasterized rendering.
    public boolean      hw              = true;  // set to true to use Vulkan RT extension.
    public boolean      animated        = true;
    public boolean      useVmaAllocator = true;
}
