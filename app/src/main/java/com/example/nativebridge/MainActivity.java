package com.example.nativebridge;

import android.app.Activity;
import android.os.Bundle;

public class MainActivity extends Activity {
    static { System.loadLibrary("native-lib"); }
    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
    }
}
