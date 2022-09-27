package com.example.melty_controller;

public interface MeltyBleClientCallback {
    void gotRpmData(int rpm);
    void connectStatusUpdate(String statusString);
}
