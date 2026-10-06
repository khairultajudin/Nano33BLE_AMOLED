/*
 * Project: Nano 33 BLE Rain Sound Transmitter
 * Target Board: Arduino Nano 33 BLE (Nordic nRF52840)
 * Environment: Arduino IDE (Arduino Mbed OS Nano Boards)
 * 
 * Description:
 * Generates an internal "rain sound" event entirely in software (no buzzer).
 * Connects via Bluetooth Low Energy (BLE) to the Waveshare ESP32-S3 AMOLED 1.75 display.
 * Sends the rain sound event and count whenever a drop occurs.
 */

#include <ArduinoBLE.h>

// BLE UUIDs for Rain Service and Characteristic
const char* BLE_SERVICE_UUID        = "19B10000-E8F2-537E-4F6C-D104768A1214";
const char* BLE_CHARACTERISTIC_UUID = "19B10001-E8F2-537E-4F6C-D104768A1214";

// Service & Characteristic definitions
BLEService rainService(BLE_SERVICE_UUID);
// 32-bit unsigned integer characteristic: read and notify enabled
BLEUnsignedIntCharacteristic rainChar(BLE_CHARACTERISTIC_UUID, BLERead | BLENotify);

// Application state
uint32_t rainCount = 0;
unsigned long lastRainTime = 0;
unsigned long rainInterval = 3000; // Simulated raindrop interval in milliseconds (3 seconds)
bool autoRain = true;             // Toggle auto simulated rain generation

// LED pin for visual indicator when rain sound event is emitted
const int LED_PIN = LED_BUILTIN; // Pin 13 on Nano 33 BLE

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  // Wait up to 3 seconds for Serial Monitor (optional, board will continue if standalone)
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3000));

  Serial.println("==================================================");
  Serial.println("    Arduino Nano 33 BLE - Rain Sound Generator    ");
  Serial.println("==================================================");

  // Initialize BLE hardware
  if (!BLE.begin()) {
    Serial.println("[ERROR] Failed to initialize BLE hardware!");
    while (1) {
      // Rapid blink to indicate failure
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }

  // Set device name and advertised service
  BLE.setLocalName("Nano33_Rain");
  BLE.setDeviceName("Nano33_Rain");
  BLE.setAdvertisedService(rainService);

  // Add characteristic to service and service to BLE
  rainService.addCharacteristic(rainChar);
  BLE.addService(rainService);

  // Initial characteristic value
  rainChar.writeValue(rainCount);

  // Start advertising
  BLE.advertise();
  Serial.println("[BLE] Advertising started as 'Nano33_Rain'...");
  Serial.println("[BLE] Service UUID: " + String(BLE_SERVICE_UUID));
  Serial.println("[INFO] Rain generator running. Type 'r' in Serial to trigger a rain sound manually.");
  Serial.println("--------------------------------------------------");
}

void loop() {
  // Listen for BLE central devices (e.g. ESP32-S3 AMOLED)
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("[BLE] Connected to central: ");
    Serial.println(central.address());
    digitalWrite(LED_PIN, HIGH); // Solid ON when connected

    while (central.connected()) {
      handleRainGeneration(true);
      handleSerialCommands();
      delay(10);
    }

    Serial.print("[BLE] Disconnected from central: ");
    Serial.println(central.address());
    digitalWrite(LED_PIN, LOW);
  } else {
    // When not connected, still run internal rain generator and allow manual testing
    handleRainGeneration(false);
    handleSerialCommands();
    delay(10);
  }
}

/**
 * Simulates internal rain sound events in code
 */
void handleRainGeneration(bool isConnected) {
  if (autoRain && (millis() - lastRainTime >= rainInterval)) {
    lastRainTime = millis();
    triggerRainEvent(isConnected);

    // Add slight natural randomization to droplet intervals (between 2.0s and 4.0s)
    rainInterval = random(2000, 4200);
  }
}

/**
 * Emits an internal rain sound event
 */
void triggerRainEvent(bool isConnected) {
  rainCount++;

  // Internal software acoustic synthesis simulation:
  // Calculate simulated raindrop acoustic characteristics (frequency in Hz, intensity)
  int dropPitch = random(800, 2200);   // Simulated drop resonance pitch
  int dropIntensity = random(50, 100); // Intensity / volume percentage

  Serial.print("[RAIN EVENT #");
  Serial.print(rainCount);
  Serial.print("] Internal Rain Sound Synthesized: Pitch=");
  Serial.print(dropPitch);
  Serial.print("Hz, Intensity=");
  Serial.print(dropIntensity);
  Serial.print("%");

  if (isConnected) {
    rainChar.writeValue(rainCount);
    Serial.println(" -> Sent to AMOLED via BLE!");
  } else {
    Serial.println(" -> (Not connected, BLE advertising...)");
  }

  // Visual pulse indicator for 100ms
  digitalWrite(LED_PIN, LOW);
  delay(50);
  digitalWrite(LED_PIN, HIGH);
  delay(100);
  if (!isConnected) {
    digitalWrite(LED_PIN, LOW);
  }
}

/**
 * Serial monitor command parser:
 * 'r' = Trigger immediate rain sound
 * 'a' = Toggle auto rain generation
 * 'c' = Reset rain count
 */
void handleSerialCommands() {
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 'r' || cmd == 'R') {
      Serial.println("[CMD] Manual Rain Triggered!");
      triggerRainEvent(BLE.central() && BLE.central().connected());
    } else if (cmd == 'a' || cmd == 'A') {
      autoRain = !autoRain;
      Serial.print("[CMD] Auto Rain Generator: ");
      Serial.println(autoRain ? "ENABLED" : "DISABLED");
    } else if (cmd == 'c' || cmd == 'C') {
      rainCount = 0;
      rainChar.writeValue(rainCount);
      Serial.println("[CMD] Rain Counter Reset to 0.");
    }
  }
}
