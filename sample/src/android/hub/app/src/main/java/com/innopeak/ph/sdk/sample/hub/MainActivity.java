package com.innopeak.ph.sdk.sample.hub;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;

public class MainActivity extends AppCompatActivity {

    static String TAG = "physray-sdk";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        String internalStorageDir = getApplicationContext().getFilesDir().getAbsolutePath();
        Log.i(TAG, String.format("internal storage path : %s", internalStorageDir));
    }

    public void launchDemo(View view) {
        String name = ((Button) view).getText().toString();
        Intent i    = new Intent(getApplicationContext(), Demo.class);
        i.putExtra("name", name);
        i.putExtra("rayTraced", ((CheckBox) findViewById(R.id.rayTracedCheckbox)).isChecked());
        i.putExtra("hw", false);
        i.putExtra("animated", ((CheckBox) findViewById(R.id.animatedCheckBox)).isChecked());
        startActivity(i);
    }

    public void launchTest(View view) { Native.unitTest(); }
}