package com.example.melty_controller;

import android.content.Context;
import android.widget.Toast;

public class Utilities {
    public static void showError(Context context, String errorText) {
        Toast.makeText(context, errorText,
                Toast.LENGTH_SHORT).show();
    }
}
