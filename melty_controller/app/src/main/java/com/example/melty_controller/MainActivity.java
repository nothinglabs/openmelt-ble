package com.example.melty_controller;

import static android.os.SystemClock.uptimeMillis;
import static java.security.AccessController.getContext;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Context;
import android.content.SharedPreferences;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.os.Handler;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.SeekBar;
import android.widget.TextView;


public class MainActivity extends AppCompatActivity implements SensorEventListener, MeltyBleClientCallback {

    static final int TRANSLATE_IDLE = 0;
    static final int TRANSLATE_FORWARD = 1;
    static final int TRANSLATE_REVERSE = 2;

    static final int UI_UPDATE_INTERVAL_MS = 50;
    static final float MAX_ACCELEROMETER_RADIUS = 20.0f; //accelerometer trace radius on the robot
    static final float MIN_ACCELEROMETER_RADIUS = 0.1f; //accelerometer trace radius on the robot

    static final float FORWARD_ACCEL_TILT_THRESH = -0.1f;  //lean forward or back this many G's on phone accelerometer
    static final float BACK_ACCEL_TILT_THRESH = 0.3f;  //lean forward or back this many G's on phone accelerometer

    static int headingLedOffset = 0;

    static final int HEART_BEAT_FIRST_VAL = 10;
    static final int HEART_BEAT_LAST_VAL = 13;
    static final int HEART_BEAT_UPDATE_FREQ_MS = 400;
    static int currentHeartBeat = HEART_BEAT_FIRST_VAL;

    //to determine forward/back/idle translation

    SensorManager sensorManager;
    Sensor accelerometer;
    MeltyBleClient meltyBleClient;

    boolean activityVisible = false;   //used to track if we are in foreground (pause heartbeat if not)


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        loadMeltyParams();

        try {
            meltyBleClient = new MeltyBleClient(MainActivity.this, this);
        } catch (Exception e) {
            Utilities.showError(this, "Could not create BLE Client: " + e.getMessage());
            finish();
        }

        sensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        accelerometer = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
        sensorManager.registerListener((SensorEventListener) this, accelerometer, SensorManager.SENSOR_DELAY_NORMAL);
        startMeltyControlUiUpdateHandler();
    }

    void meltyControlUiUpdate() {
        SeekBar trackingAdjustSeek = (SeekBar) findViewById(R.id.trackingAdjustSeek);
        float trackingAdjust = trackingAdjustSeek.getProgress() / 1000.0f;

        SeekBar fineTrackingAdjustSeek = (SeekBar) findViewById(R.id.fineTrackAdjustSeek);
        float fineTrackingAdjust = ((fineTrackingAdjustSeek.getProgress() - 500) / 1000.0f) * 0.3f;

        SeekBar throttleSeek = (SeekBar) findViewById(R.id.throttleSeek);
        int throttle = throttleSeek.getProgress();

        TextView throttleLabel = (TextView) findViewById(R.id.throttleLabel);
        throttleLabel.setText("Throttle: " + throttle);

        float adjustedRadius;
        adjustedRadius = (MAX_ACCELEROMETER_RADIUS * 1.0f) * trackingAdjust;
        adjustedRadius = adjustedRadius + (adjustedRadius * fineTrackingAdjust);

        byte translateDirection = TRANSLATE_FORWARD;

        saveMeltyParams(adjustedRadius, headingLedOffset);

        if (lastSensorEvent != null) {

            translateDirection = TRANSLATE_IDLE;

            float forwardBackTiltValue = lastSensorEvent.values[0] / 9.8f;  //convert to G's
            float steerByTiltValue = lastSensorEvent.values[1] / 9.8f;  //convert to G's

            if (forwardBackTiltValue > BACK_ACCEL_TILT_THRESH) translateDirection = TRANSLATE_REVERSE;
            if (forwardBackTiltValue < FORWARD_ACCEL_TILT_THRESH) translateDirection = TRANSLATE_FORWARD;

            CheckBox translateTextView = (CheckBox) findViewById(R.id.tiltSteerCheckBox);
            if (translateTextView.isChecked()) {
                //phone tilt control scales by square root
                if (steerByTiltValue > 0) {
                    steerByTiltValue = (float) Math.sqrt(steerByTiltValue);
                } else {
                    steerByTiltValue = (float) Math.sqrt(steerByTiltValue * -1) * -1;
                }

                adjustedRadius = adjustedRadius + (adjustedRadius * steerByTiltValue * 0.08f);
            }
        }

        if (adjustedRadius < MIN_ACCELEROMETER_RADIUS) adjustedRadius = MIN_ACCELEROMETER_RADIUS;

        float finalAdjustedRadius = adjustedRadius;

        TextView radiusTextView = (TextView) findViewById(R.id.radiusText);
        radiusTextView.setText(String.format("%.2f", finalAdjustedRadius) + " cm");

        TextView headingLabel = (TextView) findViewById(R.id.headingLabel);
        headingLabel.setText("Heading: " + headingLedOffset);


        TextView translateTextView = (TextView) findViewById(R.id.translateText);
        if (translateDirection == TRANSLATE_FORWARD) translateTextView.setText("FORWARD");
        if (translateDirection == TRANSLATE_REVERSE) translateTextView.setText("REVERSE");
        if (translateDirection == TRANSLATE_IDLE) translateTextView.setText("IDLE");

        meltyBleClient.updateMeltyConfig(adjustedRadius, translateDirection, throttle, headingLedOffset, getCurrentHeartBeat());

    }

    long lastHeartBeatUpdate = 0;
    private int getCurrentHeartBeat() {

        //don't increment heartbeat if activity is paused - or heartbeat unchecked
        CheckBox heartBeatCheckBox = (CheckBox) findViewById(R.id.heartBeatCheckBox);
        if (!heartBeatCheckBox.isChecked() || !activityVisible) {
            return currentHeartBeat;
        }

        if (uptimeMillis() - lastHeartBeatUpdate > HEART_BEAT_UPDATE_FREQ_MS) {
            currentHeartBeat++;
            if (currentHeartBeat > HEART_BEAT_LAST_VAL) currentHeartBeat = HEART_BEAT_FIRST_VAL;
            lastHeartBeatUpdate = uptimeMillis();
        }
        return currentHeartBeat;
    }


    private void loadMeltyParams(){
        Context context = MainActivity.this;
        SharedPreferences sharedPref = context.getSharedPreferences(
                getString(R.string.app_name), Context.MODE_PRIVATE);

        headingLedOffset = sharedPref.getInt(getString(R.string.heading_preference), 0);

        float radius = sharedPref.getFloat(getString(R.string.radius_preference), (MAX_ACCELEROMETER_RADIUS + MIN_ACCELEROMETER_RADIUS) / 2.0f);
        SeekBar trackingAdjustSeek = (SeekBar) findViewById(R.id.trackingAdjustSeek);
        trackingAdjustSeek.setProgress((int)((radius / MAX_ACCELEROMETER_RADIUS) * 1000));


    }

    private void saveMeltyParams(float radius, int headingOffset){
        Context context = MainActivity.this;
        SharedPreferences sharedPref = context.getSharedPreferences(
                getString(R.string.app_name), Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = sharedPref.edit();

        editor.putInt(getString(R.string.heading_preference), headingOffset);
        editor.apply();

        editor.putFloat(getString(R.string.radius_preference), radius);
        editor.apply();
    }

    public void headingLedDecrease(View view) {
        headingLedOffset--;
        if (headingLedOffset < 0) headingLedOffset = 99;
    }

    public void headingLedIncrease(View view) {
        headingLedOffset++;
        if (headingLedOffset > 99) headingLedOffset = 0;
    }

    void startMeltyControlUiUpdateHandler() {

        Handler handler = new Handler();
        // Define the code block to be executed
        Runnable runnableCode = new Runnable() {
            @Override
            public void run() {
                meltyControlUiUpdate();
                // Repeat this the same runnable code block again another 2 seconds
                // 'this' is referencing the Runnable object
                handler.postDelayed(this, UI_UPDATE_INTERVAL_MS);
            }
        };
        // Start the initial runnable task by posting through the handler
        handler.post(runnableCode);
    }

    public void gotRpmData(int rpm, float batteryVoltage){
        MainActivity.this.runOnUiThread(new Runnable() {
            public void run() {
                TextView textView = (TextView) findViewById(R.id.rpmText);
                if (rpm == 0) textView.setText("---");
                else textView.setText(rpm + " RPM");

                TextView voltageText = (TextView) findViewById(R.id.voltageText);
                voltageText.setText(String.format("%.2f", batteryVoltage) + " v");
            }
        });
    }

    public void connectStatusUpdate(String statusString) {
        //clear RPM - so we don't show prior speed if connection drops
        gotRpmData(0, 0.0f);
        MainActivity.this.runOnUiThread(new Runnable() {
            public void run() {
                TextView textView = (TextView) findViewById(R.id.connectStatusText);
                textView.setText(statusString);
                Button connectButton = (Button) findViewById(R.id.connectButton);
                Button disconnectButton = (Button) findViewById(R.id.disconnectButton);
                if (meltyBleClient.isConnected()) {
                    connectButton.setEnabled(false);
                    disconnectButton.setEnabled(true);
                } else {
                    connectButton.setEnabled(true);
                    disconnectButton.setEnabled(false);
                }
            }
        });
    }

    SensorEvent lastSensorEvent;
    @Override
    public void onSensorChanged(SensorEvent event) {
        lastSensorEvent = event;
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int i) {

    }

    public void scanToConnect(View view) {
        meltyBleClient.scanLeDevice();
    }

    public void disconnect(View view) {
        meltyBleClient.disconnect();
    }

    @Override
    protected void onResume(){
        activityVisible = true;
        super.onResume();
    }

    @Override
    protected void onPause(){
        activityVisible = false;
        super.onResume();
    }
}

