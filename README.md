# BLE Rain Sound Communication: Arduino Nano 33 BLE & Waveshare ESP32-S3 AMOLED 1.75

This project establishes wireless communication between an **Arduino Nano 33 BLE** and a **Waveshare ESP32-S3 Touch AMOLED 1.75** using **Bluetooth Low Energy (BLE)** in the **Arduino IDE**.

---

## System Architecture

```
+------------------------------------+          BLE (Wireless)          +--------------------------------------+
|       Arduino Nano 33 BLE          | -------------------------------> |   Waveshare ESP32-S3 AMOLED 1.75     |
|         (BLE Peripheral)           |  Service: 19B10000-...           |            (BLE Central)             |
|                                    |  Char:    19B10001-...           |                                      |
| - Internal Rain Sound Generator    |                                  | - Displays UI:                       |
|   (Software simulated in code)     |                                  |     RAIN: <count>                    |
| - Visual transmission LED pulse    |                                  |     [STREAM AUDIO]                   |
| - Sends event & count over BLE     |                                  | - Receives notification              |
+------------------------------------+                                  | - Increments on-screen counter       |
                                                                        | - Plays synthesized rain drop sound  |
                                                                        |   through onboard speaker (ES8311)   |
                                                                        +--------------------------------------+
```

---

## 1. Device 1: Arduino Nano 33 BLE (Transmitter)

### Sketch Location:
`Nano33BLE_Rain_Transmitter/Nano33BLE_Rain_Transmitter.ino`

### Arduino IDE Configuration:
1. **Board Package**:
   * Open `Tools` -> `Board` -> `Boards Manager...`
   * Search for: **Arduino Mbed OS Nano Boards**
   * Install the latest version.
2. **Select Board**:
   * `Tools` -> `Board` -> `Arduino Mbed OS Nano Boards` -> **Arduino Nano 33 BLE**
   * Select your COM Port under `Tools` -> `Port`.
3. **Required Library**:
   * Open `Sketch` -> `Include Library` -> `Manage Libraries...`
   * Search and install: **ArduinoBLE** (by Arduino).

### Operation:
* The sketch simulates an internal rain sound in code (no external buzzer required).
* Generates randomized rain droplet events (every 2–4 seconds) and updates the BLE characteristic.
* You can also open the Serial Monitor (115200 baud) and type:
  * `r` : Manually trigger an immediate rain drop sound event.
  * `a` : Toggle automatic rain generator on/off.
  * `c` : Reset rain counter to 0.
* Pin 13 LED pulses whenever a rain sound event is transmitted.

---

## 2. Device 2: Waveshare ESP32-S3 Touch AMOLED 1.75 (Receiver & Display)

### Sketch Location:
`AMOLED_Rain_Receiver/AMOLED_Rain_Receiver.ino`

### Arduino IDE Configuration:
1. **Board Package**:
   * Open `Tools` -> `Board` -> `Boards Manager...`
   * Search for: **esp32 by Espressif Systems** (version 2.0.14+ or 3.x).
2. **Select Board**:
   * `Tools` -> `Board` -> `esp32` -> **ESP32S3 Dev Module**
3. **Board Settings in `Tools` Menu**:
   * **USB CDC On Boot**: *Enabled*
   * **Flash Size**: *16MB (128Mb)*
   * **Partition Scheme**: *16MB (3MB APP / 9.9MB FATFS)* or *Huge APP (3MB No OTA / 1MB SPIFFS)*
   * **PSRAM**: *OPI PSRAM*
   * Select your ESP32-S3 COM Port.
4. **Required Libraries**:
   * Open `Sketch` -> `Include Library` -> `Manage Libraries...`
   * Install: **GFX Library for Arduino** by *Moon On Our Nation* (for the CO5300 QSPI AMOLED driver).
   * Note: `BLEDevice` and `driver/i2s.h` are built directly into the ESP32 board package.

### Operation:
* Displays the clean royal blue interface matching your hardware:
  ```
  RAIN: 0
  [STREAM AUDIO]
  Scanning...
  ```
* Scans and automatically pairs with the `Nano33_Rain` peripheral.
* Each time a rain sound event notification arrives:
  1. The counter updates dynamically on the AMOLED screen (`RAIN: 1`, `RAIN: 2`, ...).
  2. The onboard **ES8311** codec + power amplifier (GPIO 46) plays a synthesized rain drop sound through the speaker.

---

## Quick Testing Steps

1. Connect the **Arduino Nano 33 BLE** to your PC and upload `Nano33BLE_Rain_Transmitter.ino`.
2. Connect the **Waveshare ESP32-S3 Touch AMOLED 1.75** to your PC and upload `AMOLED_Rain_Receiver.ino`.
3. Power on both boards:
   * The AMOLED screen will start at `RAIN: 0`.
   * Within seconds, the AMOLED displays `Connected`.
   * As the Nano 33 BLE generates internal rain sound events, the AMOLED will play the rain sound through its speaker and increment the count on screen!
