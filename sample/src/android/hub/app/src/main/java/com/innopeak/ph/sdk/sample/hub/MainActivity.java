package com.innopeak.ph.sdk.sample.hub;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Spinner;

public class MainActivity extends AppCompatActivity implements AdapterView.OnItemSelectedListener {

    static String   TAG = "physray-sdk";
    private Spinner spinner;
    private Button  testButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        String internalStorageDir = getApplicationContext().getFilesDir().getAbsolutePath();
        Log.i(TAG, String.format("internal storage path : %s", internalStorageDir));
        spinner    = findViewById(R.id.spinner);
        testButton = findViewById(R.id.testButton);

        ArrayAdapter<CharSequence> adapter = ArrayAdapter.createFromResource(this, R.array.test_apps, android.R.layout.simple_spinner_item);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);
        spinner.setOnItemSelectedListener(this);
    }

    public void launchDemo(View view) {
        String name = ((Button) view).getText().toString();
        Intent i    = new Intent(getApplicationContext(), Demo.class);
        i.putExtra("name", name);
        i.putExtra("rayTraced", ((CheckBox) findViewById(R.id.rayTracedCheckbox)).isChecked());
        i.putExtra("hw", ((CheckBox) findViewById(R.id.hwCheckBox)).isChecked());
        i.putExtra("animated", ((CheckBox) findViewById(R.id.animatedCheckBox)).isChecked());
        startActivity(i);
    }

    public void launchTest(View view) { Native.unitTest(); }

    @Override
    public void onItemSelected(AdapterView<?> adapterView, View view, int i, long l) {
        String choice = adapterView.getItemAtPosition(i).toString();
        testButton.setText(choice);
    }

    @Override
    public void onNothingSelected(AdapterView<?> adapterView) {}
}