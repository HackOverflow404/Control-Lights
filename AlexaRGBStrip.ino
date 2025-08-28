#include <Arduino.h>
#include <WiFi.h>
#include <Espalexa.h>

// ---------- Wi-Fi ----------
const char* WIFI_SSID = "TP-Link_E827";
const char* WIFI_PASS = "44182816";

// ---------- Pins ----------
const int PIN_R = 4;
const int PIN_G = 16;
const int PIN_B = 17;

// ---------- PWM ----------
const int FREQ_HZ  = 5000;
const int RES_BITS = 8;

Espalexa espalexa;

#if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0))
  #define ESP32_CORE_V3 1
#endif

// ---------- Color-cycle state ----------
volatile bool cycleEnabled = false;
uint8_t  cycleBri = 200;                // 0-255 brightness for the effect
uint16_t cycleHue = 0;                  // 0..359 degrees
unsigned long lastStepMs = 0;
const uint16_t HUE_STEP = 2;            // degrees per step (smaller = smoother)
const uint16_t STEP_INTERVAL_MS = 18;   // ~60 updates/sec

// ========== PWM helpers ==========
#ifdef ESP32_CORE_V3
  inline void pwmAttachPin(int pin) { ledcAttach((uint8_t)pin, FREQ_HZ, RES_BITS); }
  inline void pwmWritePin (int pin, uint32_t duty) { ledcWrite((uint8_t)pin, duty); }

  void setupPWM() {
    Serial.println("[PWM] Setting up PWM pins...");
    pwmAttachPin(PIN_R);
    pwmAttachPin(PIN_G);
    pwmAttachPin(PIN_B);
    pwmWritePin(PIN_R, 0);
    pwmWritePin(PIN_G, 0);
    pwmWritePin(PIN_B, 0);
    Serial.println("[PWM] PWM initialized.");
  }

  inline void writeRGB(uint8_t r, uint8_t g, uint8_t b) {
    Serial.printf("[PWM] Writing RGB: R=%u, G=%u, B=%u\n", r, g, b);
    pwmWritePin(PIN_R, r);
    pwmWritePin(PIN_G, g);
    pwmWritePin(PIN_B, b);
  }
#else
  const int CH_R = 0, CH_G = 1, CH_B = 2;

  void setupPWM() {
    Serial.println("[PWM] Setting up PWM channels...");
    ledcSetup(CH_R, FREQ_HZ, RES_BITS);
    ledcSetup(CH_G, FREQ_HZ, RES_BITS);
    ledcSetup(CH_B, FREQ_HZ, RES_BITS);
    ledcAttachPin(PIN_R, CH_R);
    ledcAttachPin(PIN_G, CH_G);
    ledcAttachPin(PIN_B, CH_B);
    ledcWrite(CH_R, 0);
    ledcWrite(CH_G, 0);
    ledcWrite(CH_B, 0);
    Serial.println("[PWM] PWM initialized.");
  }

  inline void writeRGB(uint8_t r, uint8_t g, uint8_t b) {
    Serial.printf("[PWM] Writing RGB: R=%u, G=%u, B=%u\n", r, g, b);
    ledcWrite(CH_R, r);
    ledcWrite(CH_G, g);
    ledcWrite(CH_B, b);
  }
#endif

// ========== HSV → RGB ==========
static void hsvToRgb(uint16_t h, uint8_t s, uint8_t v, uint8_t &r, uint8_t &g, uint8_t &b) {
  // h: 0-359, s: 0-255, v: 0-255
  uint8_t region = h / 60;
  uint16_t remainder = (h % 60) * 255 / 60;

  uint8_t p = (uint16_t)v * (255 - s) / 255;
  uint8_t q = (uint16_t)v * (255 - (s * remainder / 255)) / 255;
  uint8_t t = (uint16_t)v * (255 - (s * (255 - remainder) / 255)) / 255;

  switch (region) {
    default:
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }
}

// ========== Espalexa callbacks ==========
// Color device: manual color sets and *disables* cycle
void onColor(uint8_t bri, uint32_t rgb) {
  cycleEnabled = false; // manual override cancels effect

  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >>  8) & 0xFF;
  uint8_t b = (rgb      ) & 0xFF;

  Serial.printf("[Espalexa] Raw RGB=%06X, Brightness=%u\n", rgb, bri);
  r = (uint16_t)r * bri / 255;
  g = (uint16_t)g * bri / 255;
  b = (uint16_t)b * bri / 255;

  Serial.printf("[Espalexa] Adjusted RGB: R=%u, G=%u, B=%u\n", r, g, b);
  writeRGB(r, g, b);
}

// Brightness device: toggles/sets the cycle effect
void onCycle(uint8_t bri) {
  // bri > 0 → ON with that brightness; bri == 0 → OFF
  if (bri == 0) {
    cycleEnabled = false;
    Serial.println("[Cycle] DISABLED");
  } else {
    cycleBri = bri;           // remember new brightness
    cycleEnabled = true;
    Serial.printf("[Cycle] ENABLED (bri=%u)\n", cycleBri);
  }
}

// ========== Wi-Fi ==========
void setupWiFi() {
  Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  if (WIFI_PASS && strlen(WIFI_PASS) > 0) WiFi.begin(WIFI_SSID, WIFI_PASS);
  else                                    WiFi.begin(WIFI_SSID);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retries++;
    if (retries > 40) { // ~20s
      Serial.println("\n[WiFi] Connection timed out, restarting...");
      ESP.restart();
    }
  }
  Serial.printf("\n[WiFi] Connected! IP address: %s\n", WiFi.localIP().toString().c_str());
}

// ========== Arduino setup/loop ==========
void setup() {
  Serial.begin(115200);
  Serial.println("\n[Setup] Booting ESP32 RGB Controller...");

  setupPWM();
  setupWiFi();

  Serial.println("[Espalexa] Adding devices...");
  espalexa.addDevice("RGB Strip", onColor);   // Color device
  espalexa.addDevice("RGB Cycle", onCycle);   // Brightness-only device to toggle the rainbow
  espalexa.begin();

  Serial.println("[Espalexa] Ready. Try:");
  Serial.println("  • \"Alexa, set RGB Strip to blue 50%\"");
  Serial.println("  • \"Alexa, turn on RGB Cycle\" (starts rainbow)");
  Serial.println("  • \"Alexa, turn off RGB Cycle\"");
}

void loop() {
  espalexa.loop();

  if (cycleEnabled) {
    unsigned long now = millis();
    if (now - lastStepMs >= STEP_INTERVAL_MS) {
      lastStepMs = now;
      cycleHue = (cycleHue + HUE_STEP) % 360;

      uint8_t r, g, b;
      hsvToRgb(cycleHue, 255, cycleBri, r, g, b); // full saturation
      writeRGB(r, g, b);
    }
  }
}

