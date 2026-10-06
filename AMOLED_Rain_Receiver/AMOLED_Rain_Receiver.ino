/*
 * Project: ESP32-S3 Touch AMOLED 1.75 - Rain Sound Receiver & Display
 * Target Board: Waveshare ESP32-S3-Touch-AMOLED-1.75
 * Board Selection in Arduino IDE: "ESP32S3 Dev Module"
 *   - USB CDC On Boot: Enabled
 *   - Flash Size: 16MB (128Mb)
 *   - PSRAM: OPI PSRAM
 *   - Partition Scheme: 16MB (e.g. 3MB App / 9.9MB FATFS or Huge APP)
 * 
 * Required Library:
 *   - "GFX Library for Arduino" by moononournation (via Arduino Library Manager)
 */

#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <driver/i2s.h>
#include <Arduino_GFX_Library.h>

// ==========================================
// 1. PIN DEFINITIONS (Official Waveshare 1.75 AMOLED)
// ==========================================
#define LCD_SDIO0   4
#define LCD_SDIO1   5
#define LCD_SDIO2   6
#define LCD_SDIO3   7
#define LCD_SCLK    38
#define LCD_CS      12
#define LCD_RESET   39
#define LCD_WIDTH   466
#define LCD_HEIGHT  466

// I2C (Touch, PMIC, Audio Codec)
#define IIC_SDA     15
#define IIC_SCL     14

// Audio I2S & Speaker Amplifier (ES8311)
#define I2S_PORT    I2S_NUM_0
#define MCLKPIN     42
#define BCLKPIN     9
#define WSPIN       45
#define DOPIN       8   // I2S Data Out
#define PA_EN_PIN   46  // Speaker amplifier enable pin
#define ES8311_ADDR 0x18

// ==========================================
// 2. DISPLAY DRIVER SETUP (Official Waveshare Configuration)
// ==========================================
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3
);

// Specific offset parameters (6, 0, 0, 0) required for the round CO5300 466x466 AMOLED
Arduino_CO5300 *gfx = new Arduino_CO5300(
  bus, LCD_RESET /* RST */, 0 /* rotation */, LCD_WIDTH /* width */, LCD_HEIGHT /* height */, 6, 0, 0, 0
);

// Colors (Matching the blue theme in the photo)
#define COLOR_BG      0x0019 // Deep royal blue
#define COLOR_TEXT    0xFFFF // Crisp white
#define COLOR_ACCENT  0x03FF // Cyan
#define COLOR_STATUS  0xAD55 // Muted cyan/white

// ==========================================
// 3. BLE CONFIGURATION
// ==========================================
static BLEUUID serviceUUID("19B10000-E8F2-537E-4F6C-D104768A1214");
static BLEUUID    charUUID("19B10001-E8F2-537E-4F6C-D104768A1214");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan    = false;
static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
static BLEAdvertisedDevice* myDevice = nullptr;

// ==========================================
// 4. APPLICATION STATE
// ==========================================
volatile uint32_t rainCount = 0;
volatile bool newRainEvent = false;
String bleStatus = "Searching Nano 33...";

// Forward declarations
void initAudio();
void initES8311Codec();
void playSynthesizedRainSound();
void drawUI();
void updateRainDisplay();
void updateStatusDisplay(String status);
bool connectToServer();

// ==========================================
// 5. BLE CALLBACKS
// ==========================================
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) override {
    connected = true;
    bleStatus = "Connected to Nano 33 BLE";
    Serial.println("[BLE] Connected to Nano 33 BLE Server!");
  }

  void onDisconnect(BLEClient* pclient) override {
    connected = false;
    bleStatus = "Disconnected. Retrying...";
    Serial.println("[BLE] Disconnected from Server. Scanning again...");
    doScan = true;
  }
};

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
  
  if (length >= sizeof(uint32_t)) {
    rainCount = *(reinterpret_cast<uint32_t*>(pData));
  } else if (length == 1) {
    rainCount = pData[0];
  } else {
    rainCount++;
  }

  newRainEvent = true;
  Serial.print("[BLE NOTIFY] Rain count received: ");
  Serial.println(rainCount);
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    Serial.print("[BLE SCAN] Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = false;
      Serial.println("[BLE SCAN] Found Nano 33 BLE by UUID!");
    } else if (advertisedDevice.haveName() && advertisedDevice.getName() == "Nano33_Rain") {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = false;
      Serial.println("[BLE SCAN] Found Nano 33 BLE by Name!");
    }
  }
};

// ==========================================
// 6. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("==================================================");
  Serial.println(" Waveshare ESP32-S3 Touch AMOLED 1.75 - Rain Hub ");
  Serial.println("==================================================");

  // 1. Initialize I2C bus
  Wire.begin(IIC_SDA, IIC_SCL);

  // 2. Initialize AMOLED Display & Power
  if (!gfx->begin()) {
    Serial.println("[ERROR] Display initialization failed!");
  } else {
    Serial.println("[DISPLAY] AMOLED initialized.");
  }

  // CRITICAL: AMOLED panels require brightness to be set to turn ON the screen!
  gfx->setBrightness(220); // 0 = off, 255 = maximum brightness

  // 3. Initialize Audio
  initAudio();

  // 4. Draw UI matching the photo
  drawUI();

  // 5. Initialize BLE Client
  BLEDevice::init("AMOLED_Rain_Hub");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  Serial.println("[BLE] Scanning for Nano 33 BLE transmitter...");
}

// ==========================================
// 7. MAIN LOOP
// ==========================================
void loop() {
  // Connect to target device when found
  if (doConnect) {
    if (connectToServer()) {
      Serial.println("[BLE] Successfully connected to Nano 33 BLE!");
      updateStatusDisplay("Connected");
    } else {
      Serial.println("[BLE] Failed to connect. Retrying scan.");
      updateStatusDisplay("Connection Failed");
      doScan = true;
    }
    doConnect = false;
  }

  // Handle incoming rain sound notifications
  if (newRainEvent) {
    newRainEvent = false;

    // Update screen count
    updateRainDisplay();

    // Play rain sound through onboard speaker
    playSynthesizedRainSound();
  }

  // Restart scan if disconnected
  if (doScan) {
    doScan = false;
    updateStatusDisplay("Scanning...");
    BLEDevice::getScan()->start(5, false);
  }

  delay(20);
}

// ==========================================
// 8. DISPLAY RENDERING FUNCTIONS
// ==========================================
void drawUI() {
  // Fill entire circular screen with deep blue
  gfx->fillScreen(COLOR_BG);

  // Title: "RAIN: 0"
  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
  gfx->setTextSize(4);
  int rainX = 140;
  int rainY = 200;
  gfx->setCursor(rainX, rainY);
  gfx->print("RAIN: ");
  gfx->print(rainCount);

  // Button: "[STREAM AUDIO]"
  gfx->setTextSize(2);
  gfx->setTextColor(COLOR_ACCENT, COLOR_BG);
  int streamX = 150;
  int streamY = 270;
  gfx->setCursor(streamX, streamY);
  gfx->print("[STREAM AUDIO]");

  // Connection status at bottom
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_STATUS, COLOR_BG);
  gfx->setCursor(160, 330);
  gfx->print(bleStatus);
}

void updateRainDisplay() {
  // Clear only the rain count number area to eliminate flicker
  int numX = 275;
  int numY = 195;
  gfx->fillRect(numX, numY, 150, 45, COLOR_BG);

  gfx->setTextColor(COLOR_TEXT, COLOR_BG);
  gfx->setTextSize(4);
  gfx->setCursor(numX + 5, 200);
  gfx->print(rainCount);
}

void updateStatusDisplay(String status) {
  bleStatus = status;
  gfx->fillRect(100, 325, 266, 25, COLOR_BG);
  gfx->setTextSize(1);
  gfx->setTextColor(COLOR_STATUS, COLOR_BG);
  gfx->setCursor(160, 330);
  gfx->print(bleStatus);
}

// ==========================================
// 9. BLE CONNECTION LOGIC
// ==========================================
bool connectToServer() {
  Serial.print("[BLE] Connecting to: ");
  Serial.println(myDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (!pClient->connect(myDevice)) {
    return false;
  }

  // Obtain reference to the remote service
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("[BLE ERROR] Failed to find Rain Service UUID!");
    pClient->disconnect();
    return false;
  }

  // Obtain reference to the characteristic
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("[BLE ERROR] Failed to find Rain Characteristic UUID!");
    pClient->disconnect();
    return false;
  }

  // Subscribe to notifications
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("[BLE] Subscribed to Rain notifications!");
  }

  return true;
}

// ==========================================
// 10. AUDIO CODEC & SYNTHESIZER
// ==========================================
void writeES8311Reg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ES8311_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void initES8311Codec() {
  writeES8311Reg(0x00, 0x80); // Reset
  delay(10);
  writeES8311Reg(0x00, 0x00);
  writeES8311Reg(0x01, 0x3F); // Clock Manager
  writeES8311Reg(0x09, 0x0C); // System Control
  writeES8311Reg(0x14, 0x1A); // Audio Format: I2S 16-bit
  writeES8311Reg(0x17, 0xB8); // Enable DAC
  writeES8311Reg(0x32, 0xC0); // Output Volume (0x00 max, 0xC0 medium, 0xFF mute)
}

void initAudio() {
  // Enable onboard Speaker Power Amplifier (GPIO 46)
  pinMode(PA_EN_PIN, OUTPUT);
  digitalWrite(PA_EN_PIN, HIGH);

  // Initialize ES8311 Codec over I2C
  initES8311Codec();

  // Configure ESP32 I2S Peripheral
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = BCLKPIN,
    .ws_io_num = WSPIN,
    .data_out_num = DOPIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);

  Serial.println("[AUDIO] I2S & ES8311 audio initialized.");
}

/**
 * Synthesizes a soothing raindrop / splash acoustic waveform
 * entirely in software math without needing an SD card or MP3 file!
 */
void playSynthesizedRainSound() {
  const int sampleRate = 16000;
  const int durationMs = 120; // 120ms droplet sound
  const int numSamples = (sampleRate * durationMs) / 1000;
  int16_t buffer[256 * 2]; // Stereo interleaved

  float startFreq = random(1400, 2000); // Plink high frequency
  float endFreq   = random(600, 900);   // Fall frequency
  float phase = 0;

  int sampleIndex = 0;
  while (sampleIndex < numSamples) {
    int chunkSize = min(256, numSamples - sampleIndex);

    for (int i = 0; i < chunkSize; i++) {
      int cur = sampleIndex + i;
      float progress = (float)cur / numSamples;

      // Frequency drops over time (droplet pitch glide)
      float currentFreq = startFreq + (endFreq - startFreq) * progress;

      // Exponential decay amplitude envelope
      float envelope = exp(-5.0f * progress);

      // Add a touch of pink/white noise splash to the droplet sine wave
      float noise = ((float)random(-100, 100) / 100.0f) * 0.25f * (1.0f - progress);
      float tone = sin(phase);
      float sampleVal = (tone * 0.75f + noise) * envelope * 18000.0f;

      phase += 2.0f * PI * currentFreq / sampleRate;
      if (phase >= 2.0f * PI) phase -= 2.0f * PI;

      int16_t pcm = (int16_t)sampleVal;
      buffer[i * 2]     = pcm; // Left
      buffer[i * 2 + 1] = pcm; // Right
    }

    size_t bytesWritten = 0;
    i2s_write(I2S_PORT, buffer, chunkSize * 4, &bytesWritten, portMAX_DELAY);
    sampleIndex += chunkSize;
  }

  // Clear DMA after playback
  i2s_zero_dma_buffer(I2S_PORT);
}
