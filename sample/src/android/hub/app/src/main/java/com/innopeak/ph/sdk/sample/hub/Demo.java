package com.innopeak.ph.sdk.sample.hub;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.widget.TextView;

public class Demo extends AppCompatActivity {

    private static final String TAG = "phantom-sdk";
    TextView _fpsTextView;
    VulkanSurfaceView           view;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_demo);

        // Hide both status and action bar to make a full screen app.
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN);
        // getSupportActionBar().hide();

        _fpsTextView = findViewById(R.id.fpsTextView);

        Bundle            b    = getIntent().getExtras();
        view                   = findViewById(R.id.mainSurfaceView);
        RenderOptions     ro   = view.get_options();
        ro.name                = b.getString("name");
        ro.rasterized          = !b.getBoolean("rayTraced");
        ro.hw                  = b.getBoolean("hw");
        ro.animated            = b.getBoolean("animated");
        view.set_eventListener(new VulkanSurfaceView.EventListener() {
            @Override
            public void onFpsUpdated(float fps) {
                // note: this method is called from non-ui thread.
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        _fpsTextView.setText(String.format("FPS=%.2f (%.1fms)", fps, 1000.0f / fps));
                    }
                });
            }
        });
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!view.getRenderInitState() && keyCode == KeyEvent.KEYCODE_BACK) {
            Log.e(TAG, "skip respond back key when render init loading");
            return false;
        }
        return super.onKeyDown(keyCode, event);
    }
}
