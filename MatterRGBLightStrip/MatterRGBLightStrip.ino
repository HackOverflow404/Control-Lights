// Copyright 2025 Espressif
// Licensed under the Apache License, Version 2.0

// ===== Required first so LEDC prototypes exist everywhere =====
#include <Arduino.h>

// ===== Matter / Arduino includes =====
#include <Matter.h>
#if !CONFIG_ENABLE_CHIPOBLE
  // If BLE commissioning is disabled at build time, we'll join Wi-Fi manually
  #include <WiFi.h>
#endif
#include <Preferences.h>

// ──────────────────────────────────────────────────────────────
// Core v3+ detection for Arduino-ESP32 (changes LEDC API shape)
// ──────────────────────────────────────────────────────────────
#if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0))
  #define ESP32_CORE_V3 1
#endif

// ──────────────────────────────────────────────────────────────
// Hardware configuration (YOUR pins)
// ──────────────────────────────────────────────────────────────
static const uint8_t PIN_R = 18;    // PWM -> Red MOSFET gate
static const uint8_t PIN_G = 19;   // PWM -> Green MOSFET gate
static const uint8_t PIN_B = 5;   // PWM -> Blue MOSFET gate

// PWM settings (smooth, inaudible wrt LED strips)
static const uint32_t PWM_FREQ_HZ = 5000;
static const uint8_t  PWM_RES_BITS = 8;   // duty 0..255

#ifndef ESP32_CORE_V3
// v2.x API needs explicit channels
static const int CH_R = 0;
static const int CH_G = 1;
static const int CH_B = 2;
#endif

// Optional: on-board user button
#ifdef BOOT_PIN
  const uint8_t buttonPin = BOOT_PIN;      // most ESP32 devkits
#else
  const uint8_t buttonPin = 0;             // adjust if you wire an external button
#endif

// ──────────────────────────────────────────────────────────────
// Optional Wi-Fi (only used if you build WITHOUT CHIP BLE)
// ──────────────────────────────────────────────────────────────
#if !CONFIG_ENABLE_CHIPOBLE
  const char* ssid     = "TP-Link_E827";
  const char* password = "44182816";
#endif

// ──────────────────────────────────────────────────────────────
// Matter endpoint: Enhanced Color Light (Hue/Sat + XY + CT)
// ──────────────────────────────────────────────────────────────
MatterEnhancedColorLight EnhancedColorLight;

// HSV state we’ll keep synced with the device (0..255 space)
HsvColor_t currentHSVColor = {0, 0, 0};

// Persist last on/off + HSV
Preferences matterPref;
static constexpr const char* NAMESPACE_KEY = "MatterPrefs";
static constexpr const char* ONOFF_KEY     = "OnOff";
static constexpr const char* HSV_KEY       = "HSV";

// Button handling
uint32_t buttonPressTs = 0;
bool     buttonPressed = false;
const uint32_t debounceMs      = 250;
const uint32_t decommissionMs  = 5000;

// ──────────────────────────────────────────────────────────────
// LEDC PWM init / write helpers (dual-API: v2.x and v3.x)
// ──────────────────────────────────────────────────────────────
static inline void pwmInit() {
#ifdef ESP32_CORE_V3
  // New API (v3.x): attach by pin, no channels needed
  ledcAttach(PIN_R, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttach(PIN_G, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttach(PIN_B, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcWrite(PIN_R, 0);
  ledcWrite(PIN_G, 0);
  ledcWrite(PIN_B, 0);
#else
  // Old API (v2.x): setup channels, then attach pins
  ledcSetup(CH_R, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcSetup(CH_G, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcSetup(CH_B, PWM_FREQ_HZ, PWM_RES_BITS);
  ledcAttachPin(PIN_R, CH_R);
  ledcAttachPin(PIN_G, CH_G);
  ledcAttachPin(PIN_B, CH_B);
  ledcWrite(CH_R, 0);
  ledcWrite(CH_G, 0);
  ledcWrite(CH_B, 0);
#endif
}

static inline void writeRGB(uint8_t r, uint8_t g, uint8_t b) {
#ifdef ESP32_CORE_V3
  ledcWrite(PIN_R, r);
  ledcWrite(PIN_G, g);
  ledcWrite(PIN_B, b);
#else
  ledcWrite(CH_R, r);
  ledcWrite(CH_G, g);
  ledcWrite(CH_B, b);
#endif
}

// ──────────────────────────────────────────────────────────────
// Matter “apply light state” callback
//  - state: on/off
//  - colorHSV: requested HSV (h,s,v) in 0..255 (from stack)
//  - brightness: Matter Level (0..254) – typically mapped into HSV.v by stack
//  - temperature_Mireds: color temp if set (we keep HSV in sync separately)
// Return true for success.
// ──────────────────────────────────────────────────────────────
bool setLightState(bool state, espHsvColor_t colorHSV, uint8_t brightness, uint16_t temperature_Mireds) {
  // Persist last state (use our local HSV, kept in attribute callbacks)
  matterPref.putBool(ONOFF_KEY, state);
  uint32_t packed = (uint32_t(currentHSVColor.h) << 16) |
                    (uint32_t(currentHSVColor.s) << 8)  |
                    (uint32_t(currentHSVColor.v));
  matterPref.putUInt(HSV_KEY, packed);

  if (!state) {
    writeRGB(0, 0, 0);
    return true;
  }

  // Convert our HSV to RGB and drive LEDC
  espRgbColor_t rgb = espHsvColorToRgbColor(currentHSVColor);
  writeRGB(rgb.r, rgb.g, rgb.b);
  return true;
}

// ──────────────────────────────────────────────────────────────
// Setup
// ──────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(buttonPin, INPUT_PULLUP);
  pwmInit();

#if !CONFIG_ENABLE_CHIPOBLE
  // If you built without CHIP BLE, join Wi-Fi first (optional)
  Serial.printf("Connecting to Wi-Fi SSID: %s\n", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wi-Fi connected, IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("Wi-Fi connect timeout; you can still commission over BLE if supported.");
  }
#endif

  // Load last state (defaults: ON, warm white ~10%)
  matterPref.begin(NAMESPACE_KEY, false);
  bool lastOn = matterPref.getBool(ONOFF_KEY, true);
  uint32_t packedHSV = matterPref.getUInt(HSV_KEY, (21u << 16) | (216u << 8) | 25u);
  currentHSVColor = {
    uint8_t(packedHSV >> 16),
    uint8_t(packedHSV >> 8),
    uint8_t(packedHSV)
  };

  // Initialize endpoint with last state
  EnhancedColorLight.begin(lastOn, currentHSVColor);

  // Apply-state callback (drives LEDs + persists)
  EnhancedColorLight.onChange(setLightState);

  // Attribute change callbacks keep our local HSV in sync with controller actions
  EnhancedColorLight.onChangeOnOff([](bool state){
    Serial.printf("OnOff -> %s\n", state ? "ON" : "OFF");
    return true;
  });

  EnhancedColorLight.onChangeColorTemperature([](uint16_t mireds){
    // Map CT → HSV (preserve V)
    espRgbColor_t rgb = espCTToRgbColor(mireds);
    HsvColor_t hsv = espRgbColorToHsvColor(rgb);
    currentHSVColor.h = hsv.h;
    currentHSVColor.s = hsv.s;
    // V stays as-is; Level cluster adjusts it separately
    Serial.printf("ColorTemperature -> %u mireds (HSV now %u,%u,%u)\n",
                  mireds, currentHSVColor.h, currentHSVColor.s, currentHSVColor.v);
    return true;
  });

  EnhancedColorLight.onChangeBrightness([](uint8_t v){
    currentHSVColor.v = v;
    Serial.printf("Brightness -> %u\n", v);
    return true;
  });

  EnhancedColorLight.onChangeColorHSV([](HsvColor_t hsv){
    currentHSVColor.h = hsv.h;
    currentHSVColor.s = hsv.s;
    currentHSVColor.v = hsv.v; // some controllers may set v here too
    Serial.printf("HSV -> (%u,%u,%u)\n", hsv.h, hsv.s, hsv.v);
    return true;
  });

  // Bring up Matter after endpoints are ready
  Matter.begin();

  // If already commissioned, apply the stored/initial state to hardware
  if (Matter.isDeviceCommissioned()) {
    Serial.println("Matter: already commissioned; applying saved state.");
    EnhancedColorLight.updateAccessory(); // invokes setLightState()
  }
}

// ──────────────────────────────────────────────────────────────
// Loop
// ──────────────────────────────────────────────────────────────
void loop() {
  // Commissioning helper (prints code/QR until paired)
  if (!Matter.isDeviceCommissioned()) {
    static uint32_t lastPrint = 0;
    uint32_t now = millis();
    if (now - lastPrint > 2000) {
      lastPrint = now;
      Serial.println("\nMatter device not commissioned.");
      Serial.printf("Manual pairing code: %s\n", Matter.getManualPairingCode().c_str());
      Serial.printf("QR code URL: %s\n", Matter.getOnboardingQRCodeUrl().c_str());
      Serial.println("Use the Alexa app → Devices → + → Add Device → Matter, then scan the QR.");
    }
  }

  // Button: short press toggles, long press decommissions
  if (!buttonPressed && digitalRead(buttonPin) == LOW) {
    buttonPressed = true;
    buttonPressTs = millis();
  }

  if (buttonPressed) {
    uint32_t held = millis() - buttonPressTs;

    // Long-press decommission
    if (held > decommissionMs && digitalRead(buttonPin) == LOW) {
      Serial.println("Decommissioning Matter node...");
      EnhancedColorLight = false;      // turn off
      writeRGB(0,0,0);
      Matter.decommission();           // device will return to pairing mode
      while (digitalRead(buttonPin) == LOW) delay(10); // wait for release
      buttonPressed = false;
    }

    // On release (debounced): toggle
    if (digitalRead(buttonPin) == HIGH && held > debounceMs) {
      buttonPressed = false;
      Serial.println("Button: toggle");
      EnhancedColorLight.toggle();     // notifies controller + invokes callbacks
    }
  }

  delay(1); // keep scheduler happy
}
