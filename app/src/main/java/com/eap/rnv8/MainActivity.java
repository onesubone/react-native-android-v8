package com.eap.rnv8;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.View;

public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_demo);
    }

    public void gotoDemo(View view) {
        Log.d("MyV8Test", "gotoDemo");
        startActivity(new Intent(this, DemoActivity.class));
    }
}
