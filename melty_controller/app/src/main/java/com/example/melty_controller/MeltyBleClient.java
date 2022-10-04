package com.example.melty_controller;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.BluetoothLeScanner;
import android.bluetooth.le.ScanCallback;
import android.bluetooth.le.ScanResult;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import androidx.core.app.ActivityCompat;

import java.util.UUID;

public class MeltyBleClient {

    // Stops scanning after 10 seconds.
    private static final long SCAN_PERIOD = 10000;
    private static final long CONNECT_DELAY = 500;

    private static final String DEVICE_NAME = "Melty_Bot";

    private static final UUID CHARACTERISTIC_UPDATE_NOTIFICATION_DESCRIPTOR_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private static final UUID MELTY_DATA_CHARACTERISTIC_UUID = UUID.fromString("00001524-1212-efde-1523-785feabcd123");
    private static final UUID MELTY_CONFIG_CHARACTERISTIC_UUID = UUID.fromString("00001525-1212-efde-1523-785feabcd123");

    //Used to perform gatt connect / disconnect operations on main thread
    //https://github.com/android/connectivity-samples/issues/18
    //Some minor operations not done on main thread (test of moving all gatt operations to main thread didn't resolve 133 errors anyways)
    Handler handler = new Handler(Looper.getMainLooper());

    BluetoothAdapter bluetoothAdapter;
    BluetoothGatt bluetoothGatt;
    BluetoothLeScanner bluetoothLeScanner;

    BluetoothDevice meltyBotDevice = null;

    BluetoothGattCharacteristic meltyDataCharacteristic;
    BluetoothGattCharacteristic meltyConfigCharacteristic;

    MeltyBleClientCallback meltyBleClientCallback;

    Boolean connectedAndInitialized = false;
    Boolean scanning = false;
    Boolean meltyConfigWritePending = false;

    Context context;

    MeltyBleClient(Context passedContext, MeltyBleClientCallback callback) {
        context = passedContext;
        meltyBleClientCallback = callback;

        BluetoothManager bluetoothManager = (BluetoothManager) context.getSystemService(Context.BLUETOOTH_SERVICE);
        bluetoothAdapter = bluetoothManager.getAdapter();

        if (!checkBlePermission()) return;

        bluetoothLeScanner = bluetoothAdapter.getBluetoothLeScanner();

        if (bluetoothLeScanner == null) {
            throw new UnsupportedOperationException("Could not create BLE scanner");
        }

    }

    public Boolean isConnected() {
        return connectedAndInitialized;
    }

    private Boolean checkBlePermission() {
        Boolean hasPermission = (ActivityCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED);
        if (!hasPermission) Utilities.showError(context, "BLE init error");
        return hasPermission;
    }



    private void connectToDevice(BluetoothDevice device) {

        //Assures connect happen on main thread + small delay to avoid 133 error (although this didn't solve issue alone)
        //https://github.com/android/connectivity-samples/issues/18
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (!checkBlePermission()) return;
                Log.e("BLE", "Connecting to device: " + device.getName());
                //Setting autoconnect to true seemed to address 133 issue on both Pixel 2 and Pixel 3a
                //https://github.com/android/connectivity-samples/issues/18
                bluetoothGatt = device.connectGatt(context, true, bluetoothGattCallback, BluetoothDevice.TRANSPORT_LE);
                meltyBleClientCallback.connectStatusUpdate("Connecting...");
            }
        }, CONNECT_DELAY);

    }

    private final BluetoothGattCallback bluetoothGattCallback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            if (!checkBlePermission()) return;

            if (newState == BluetoothProfile.STATE_CONNECTED) {
                // successfully connected to the GATT Server
                meltyBleClientCallback.connectStatusUpdate("Discovering Services...");

                handler.post(gatt::discoverServices);

            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                // disconnected from the GATT Server

                handler.post(gatt::close);

                bluetoothGatt = null;
                connectedAndInitialized = false;
                meltyConfigWritePending = false;
                meltyBleClientCallback.connectStatusUpdate("Disconnected (Code " + status + ")");
            }
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic) {
            if (characteristic == meltyDataCharacteristic) {
                float rpm;
                if (characteristic.getValue()[0] == 0) rpm = 0;
                    else rpm = (1.0f / (characteristic.getValue()[0] & 0xff)) * 1000 * 60;

                float batteryVoltage = (characteristic.getValue()[2] & 0xff) / 10.0f;
                Log.e("BLE", "Melty stats (RPM /V): " + rpm + " / " + batteryVoltage);
                meltyBleClientCallback.gotRpmData((int)rpm, batteryVoltage);
            }
            super.onCharacteristicChanged(gatt, characteristic);
        }

        @Override
        public void onCharacteristicWrite(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            meltyConfigWritePending = false;
            super.onCharacteristicWrite(gatt, characteristic, status);
        }

        @Override
        public void onCharacteristicRead(BluetoothGatt gatt, BluetoothGattCharacteristic characteristic, int status) {
            super.onCharacteristicRead(gatt, characteristic, status);
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS) {

                meltyConfigCharacteristic = null;
                meltyDataCharacteristic = null;

                // Loops through available GATT Services.
                for (BluetoothGattService gattService : gatt.getServices()) {

                    for (BluetoothGattCharacteristic gattCharacteristic :
                            gattService.getCharacteristics()) {
                        Log.e("BLE", "Characteristic: " + gattCharacteristic.getUuid());
                        if (gattCharacteristic.getUuid().equals(MELTY_DATA_CHARACTERISTIC_UUID)) {
                            meltyDataCharacteristic = gattCharacteristic;
                            if (!setCharacteristicNotification(bluetoothGatt, meltyDataCharacteristic, true)) {
                                Log.e("BLE", "Failed to setCharacteristicNotification");
                                return;
                            }
                        }
                        if (gattCharacteristic.getUuid().equals(MELTY_CONFIG_CHARACTERISTIC_UUID)) {
                            meltyConfigCharacteristic = gattCharacteristic;
                        }
                    }
                }
                if (meltyConfigCharacteristic != null && meltyDataCharacteristic != null) {
                    connectedAndInitialized = true;
                    meltyBleClientCallback.connectStatusUpdate("Connected");
                }

            } else {
                Log.w("BLE", "onServicesDiscovered received: " + status);
            }
        }
    };


    private boolean setCharacteristicNotification(BluetoothGatt bluetoothGatt, BluetoothGattCharacteristic characteristic, boolean enable) {
        if (!checkBlePermission()) return false;

        Log.e("BLE", "setCharacteristicNotification");
        bluetoothGatt.setCharacteristicNotification(characteristic, enable);
        BluetoothGattDescriptor descriptor = characteristic.getDescriptor(CHARACTERISTIC_UPDATE_NOTIFICATION_DESCRIPTOR_UUID);
        descriptor.setValue(enable ? BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE : new byte[]{0x00, 0x00});
        bluetoothGatt.writeDescriptor(descriptor); //descriptor write operation successfully started?

        return true;
    }


    public void disconnect() {
        if (bluetoothGatt != null) {
            handler.post(bluetoothGatt::disconnect);
        }
    }

    public void scanLeDevice() {

        //skip scan if we already have device
        /*if (meltyBotDevice != null) {
            connectToDevice(meltyBotDevice);
            return;
        }*/

        if (!checkBlePermission()) return;

        if (!scanning) {
            // Stops scanning after a predefined scan period.
            handler.postDelayed(() -> {
                if (scanning == true) {
                    scanning = false;
                    meltyBleClientCallback.connectStatusUpdate("Scan timed out.");
                    bluetoothLeScanner.stopScan(leScanCallback);
                }
            }, SCAN_PERIOD);

            scanning = true;
            bluetoothLeScanner.startScan(leScanCallback);
            meltyBleClientCallback.connectStatusUpdate("Scanning...");
        }
    }

    private final ScanCallback leScanCallback =
            new ScanCallback() {
                @Override
                public void onScanResult(int callbackType, ScanResult result) {
                    if (!checkBlePermission()) return;

                    super.onScanResult(callbackType, result);
                    Log.e("BLE", "Device: " + result.getDevice().getName());
                    if (result.getDevice().getName() != null &&
                            result.getDevice().getName().equals(DEVICE_NAME)) {
                        meltyBleClientCallback.connectStatusUpdate("Found device.");
                        bluetoothLeScanner.stopScan(leScanCallback);
                        scanning = false;

                        meltyBotDevice = result.getDevice();
                        connectToDevice(meltyBotDevice);

                    }
                }
            };

    public void updateMeltyConfig(float radius, byte translateDirection, int throttle, int headingLedOffset, int currentHeartBeat) {
        if (!checkBlePermission()) return;

        radius = radius * 1000;     //value is passed as fixed point (x1000)

        byte[] meltyConfigBytes = {0, 0, 0, 0, 0, 0, 0};

        meltyConfigBytes[0] = (byte)(Math.round(radius) % 256);
        meltyConfigBytes[1] = (byte)(Math.round(radius) / 256);
        meltyConfigBytes[2] = (byte)headingLedOffset;  //led offset
        meltyConfigBytes[3] = (byte)throttle;   //throttle
        meltyConfigBytes[4] = translateDirection;     //translate direction
        meltyConfigBytes[5] = (byte)currentHeartBeat;

        //this is always called from main thread - no need to use handler for gatt
        if (connectedAndInitialized == true && !meltyConfigWritePending) {
            meltyConfigCharacteristic.setValue(meltyConfigBytes);
            if (bluetoothGatt.writeCharacteristic(meltyConfigCharacteristic)) {
                meltyConfigWritePending = true;
            } else {
                Log.e("BLE", "write fail!");
            }
        }
    }
}
