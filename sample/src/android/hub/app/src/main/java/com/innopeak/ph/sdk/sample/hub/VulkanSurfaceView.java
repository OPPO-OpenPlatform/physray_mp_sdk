package com.innopeak.ph.sdk.sample.hub;

import android.content.Context;
import android.text.method.Touch;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import androidx.annotation.NonNull;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import static java.lang.System.nanoTime;

public class VulkanSurfaceView extends SurfaceView implements SurfaceHolder.Callback {

    static String TAG = "physray-sdk";

    RenderOptions                  _options = new RenderOptions();
    VulkanRenderer                 _renderer;
    AtomicReference<EventListener> _eventListener = new AtomicReference<EventListener>();

    public VulkanSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setAlpha(1.0f);
        getHolder().addCallback(this);
    }

    public RenderOptions get_options() { return _options; }

    public void set_eventListener(EventListener l) { _eventListener.set(l); }

    public interface EventListener {
        public void onFpsUpdated(float fps);
    }

    // TODO: make this method available to VulkanRenderer only.
    public void setFps(float fps) {
        EventListener l = _eventListener.get();
        if (null != l) l.onFpsUpdated(fps);
    }

    @Override
    public void surfaceCreated(@NonNull SurfaceHolder holder) {
        Log.d(TAG, "surfaceCreated");
        _options.surface = holder.getSurface();
        _options.assets  = getContext().getAssets();
        _renderer        = new VulkanRenderer(this, _options);
        _renderer.start();
    }

    @Override
    public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
        Log.d(TAG, "Destroy Android surface...");

        // notify render thread to quit
        _renderer.running.set(false);

        // wait for render thread to finish before returning. If we don't do this, then the surface
        // could get destroyed before vulkan resources are released. It could cause vulkan APi to
        // fail, or even crash when vulkan validation layer is enabled.
        try {
            _renderer.join();
        } catch (InterruptedException ex) { ex.printStackTrace(); }

        Log.d(TAG, "Android surface destroyed.");
    }

    @Override
    public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {
        _renderer.notifySizeChange(format, width, height);
    }

    @Override
    public boolean onTouchEvent(MotionEvent e) {
        // Contains data we will be passing back to android.
        TouchEvent touchEvent = new TouchEvent();
        int        action     = e.getActionMasked();
        if (MotionEvent.ACTION_DOWN == action || MotionEvent.ACTION_MOVE == action || MotionEvent.ACTION_POINTER_DOWN == action ||
            MotionEvent.ACTION_POINTER_UP == action) {

            // Get count of pointers being pressed.
            int pointerCount = e.getPointerCount();

            int actionIndex = e.getActionIndex();

            // Allocate arrays.
            touchEvent.touchIds       = new int[pointerCount];
            touchEvent.touchPositions = new float[2 * pointerCount];

            // Copy data for each pointer to the arrays.
            for (int index = 0; index < pointerCount; ++index) {
                // Check if this is the pointer that is just lift up. If so, then we skip it.
                if (MotionEvent.ACTION_POINTER_UP == action && actionIndex == index) continue;

                // Record the unique id of this pointer.
                touchEvent.touchIds[index] = e.getPointerId(index);

                // Record the position of this pointer.
                touchEvent.touchPositions[index * 2 + 0] = e.getX(index);
                touchEvent.touchPositions[index * 2 + 1] = e.getY(index);
            }
        }
        _renderer.notifyTouchEvent(touchEvent);
        return true;
    }
};

/**
 * Represents a touch we are passing back to native.
 */
class TouchEvent {
    /**
     * Id of each touch.
     */
    int touchIds[];

    /**
     * XY coordinates of each touch.
     * Each touch uses up 2 elements in the array.
     */
    float touchPositions[];
};

class VulkanRenderer extends Thread {

    public AtomicBoolean running = new AtomicBoolean(true);

    public VulkanRenderer(VulkanSurfaceView view, RenderOptions options) {
        _view    = view;
        _options = options;
    }

    public void notifySizeChange(int f, int w, int h) { _resized.set(true); }

    public void notifyTouchEvent(TouchEvent t) {
        _lock.lock();
        _touchEvent = t;
        _lock.unlock();
    }

    public void run() {
        try {

            Native.create(_options.name, _options.surface, _options.assets, _options.rasterized, _options.hw, _options.animated, _options.useVmaAllocator);

            // wait for the initial resize call.
            while (!_resized.get()) {
                try {
                    sleep(100);
                } catch (InterruptedException e) {
                    // do nothing
                }
            }

            Native.resize();

            long lastFrameTime = nanoTime();
            int  frameCounter  = 0;

            while (running.get()) {

                // handle touch event
                TouchEvent t;
                _lock.lock();
                t           = _touchEvent;
                _touchEvent = null;
                _lock.unlock();
                if (null != t) { Native.touch(t.touchIds, t.touchPositions); }

                // do the rendering
                Native.render();

                // calculate FPS
                long now      = nanoTime();
                long duration = now - lastFrameTime;
                ++frameCounter;
                if (duration > 1000000000) { // report FPS once / sec.
                    float fps = (float) frameCounter * 1000000000.0f / duration;
                    _view.setFps(fps);
                    lastFrameTime = now;
                    frameCounter  = 0;
                }
            }

        } finally { Native.delete(); }
    }

    VulkanSurfaceView _view;
    RenderOptions     _options;

    Lock _lock = new ReentrantLock();

    AtomicBoolean _resized = new AtomicBoolean(false);

    TouchEvent _touchEvent;
}