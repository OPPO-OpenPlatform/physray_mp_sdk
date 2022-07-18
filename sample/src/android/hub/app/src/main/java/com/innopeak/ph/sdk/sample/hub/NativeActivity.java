package com.innopeak.ph.sdk.sample.hub;

import android.os.Bundle;

public class NativeActivity extends android.app.NativeActivity {
    public String  sceneName  = "";
    public boolean rasterized = false;
    public boolean animated   = true;
    public boolean hw         = true;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Bundle b = getIntent().getExtras();
        if (null == b) return;
        sceneName  = b.getString("name", sceneName);
        rasterized = !b.getBoolean("rayTraced", rasterized);
        animated   = b.getBoolean("animated", animated);
        hw         = b.getBoolean("hw", hw);
    }
}
