package com.example.melty_controller;

public interface MeltyBleClientCallback {
    void gotRpmData(int rpm, float batteryVoltage);
    void connectStatusUpdate(String statusString);
}
