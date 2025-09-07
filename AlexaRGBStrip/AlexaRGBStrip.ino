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

// ---------- Cycle state ----------
volatile bool   cycleEnabled  = false;   // controlled by "RGB Cycle"
uint8_t         cycleBri      = 200;     // 0–255 effect brightness
uint16_t        cycleHue      = 0;       // 0..359 degrees
unsigned long   lastStepMs    = 0;

// Adjustable speed dial (separate device "RGB Speed")
uint8_t         cycleSpeedPct = 40;      // 0–100%
const uint16_t  HUE_STEP      = 1;

const uint16_t  INTERVAL_SLOW_MS = 120;  // 0% speed
const uint16_t  INTERVAL_FAST_MS = 15;   // 100% speed
uint16_t        stepIntervalMs   = 35;

// ========== PWM helpers ==========
#ifdef ESP32_CORE_V3
  inline void pwmAttachPin(int pin) { ledcAttach((uint8_t)pin, FREQ_HZ, RES_BITS); }
  inline void pwmWritePin (int pin, uint32_t duty) { ledcWrite((uint8_t)pin, duty); }

  void setupPWM() {
    Serial.println(F("[PWM] Initializing pins..."));
    pwmAttachPin(PIN_R);
    pwmAttachPin(PIN_G);
    pwmAttachPin(PIN_B);
    pwmWritePin(PIN_R, 0);
    pwmWritePin(PIN_G, 0);
    pwmWritePin(PIN_B, 0);
    Serial.println(F("[PWM] ✅ PWM ready (all channels set to 0)."));
  }

  inline void writeRGB(uint8_t r, uint8_t g, uint8_t b) {
    pwmWritePin(PIN_R, r);
    pwmWritePin(PIN_G, g);
    pwmWritePin(PIN_B, b);
    Serial.printf("[RGB] Applied values → R:%3u G:%3u B:%3u\n", r, g, b);
  }
#else
  const int CH_R = 0, CH_G = 1, CH_B = 2;

  void setupPWM() {
    Serial.println(F("[PWM] Initializing channels..."));
    ledcSetup(CH_R, FREQ_HZ, RES_BITS);
    ledcSetup(CH_G, FREQ_HZ, RES_BITS);
    ledcSetup(CH_B, FREQ_HZ, RES_BITS);
    ledcAttachPin(PIN_R, CH_R);
    ledcAttachPin(PIN_G, CH_G);
    ledcAttachPin(PIN_B, CH_B);
    ledcWrite(CH_R, 0);
    ledcWrite(CH_G, 0);
    ledcWrite(CH_B, 0);
    Serial.println(F("[PWM] ✅ PWM ready (all channels set to 0)."));
  }

  inline void writeRGB(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(CH_R, r);
    ledcWrite(CH_G, g);
    ledcWrite(CH_B, b);
    Serial.printf("[RGB] Applied values → R:%3u G:%3u B:%3u\n", r, g, b);
  }
#endif

// ========== helpers ==========
static inline uint8_t mul8(uint8_t a, uint8_t b) {
  return (uint16_t(a) * uint16_t(b) + 127) / 255;
}

static inline uint16_t lerp_u16(uint16_t a, uint16_t b, uint16_t t /*0..255*/) {
  int32_t da = int32_t(b) - int32_t(a);
  return uint16_t(int32_t(a) + (da * int32_t(t)) / 255);
}

static void updateIntervalFromSpeed() {
  const uint16_t t = (uint16_t)constrain((int)cycleSpeedPct, 0, 100) * 255 / 100;
  const uint16_t v = lerp_u16(INTERVAL_SLOW_MS, INTERVAL_FAST_MS, t);
  const uint16_t minAllowed = min(INTERVAL_SLOW_MS, INTERVAL_FAST_MS);
  const uint16_t maxAllowed = max(INTERVAL_SLOW_MS, INTERVAL_FAST_MS);
  stepIntervalMs = (uint16_t)constrain(v, minAllowed, maxAllowed);

  Serial.printf("[Speed] Set to %3u%% → step interval = %ums\n", cycleSpeedPct, stepIntervalMs);
}

// ========== HSV → RGB ==========
static void hsvToRgb(uint16_t h, uint8_t s, uint8_t v,
                     uint8_t &r, uint8_t &g, uint8_t &b) {
  h = h % 360;
  const uint8_t region = h / 60;
  const uint16_t f = (h % 60) * 255 / 60;

  const uint8_t p = mul8(v, (uint8_t)(255 - s));
  const uint8_t q = mul8(v, (uint8_t)(255 - mul8(s, (uint8_t)f)));
  const uint8_t t = mul8(v, (uint8_t)(255 - mul8(s, (uint8_t)(255 - f))));

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
void onColor(uint8_t bri, uint32_t rgb) {
  if (bri == 0) {
    cycleEnabled = false;
    writeRGB(0, 0, 0);
    Serial.println(F("[Alexa] RGB Strip turned OFF."));
    return;
  }

  cycleEnabled = false;

  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >>  8) & 0xFF;
  uint8_t b = (rgb      ) & 0xFF;

  r = mul8(r, bri);
  g = mul8(g, bri);
  b = mul8(b, bri);
  writeRGB(r, g, b);

  Serial.printf("[Alexa] RGB Strip → Color applied (Brightness=%u)\n", bri);
}

void onCycle(uint8_t bri) {
  if (bri == 0) {
    cycleEnabled = false;
    writeRGB(0, 0, 0);
    Serial.println(F("[Alexa] RGB Cycle turned OFF."));
  } else {
    cycleBri = bri;
    cycleEnabled = true;
    Serial.printf("[Alexa] RGB Cycle ENABLED (Brightness=%u)\n", cycleBri);
  }
}

void onCycleSpeed(uint8_t bri) {
  cycleSpeedPct = (uint8_t)((uint16_t)bri * 100 / 255);
  updateIntervalFromSpeed();
  Serial.printf("[Alexa] RGB Speed set to %u%%\n", cycleSpeedPct);
}

// ========== Wi-Fi ==========
void setupWiFi() {
  Serial.printf("[WiFi] Connecting to SSID: \"%s\"...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("esp32-rgb");

  if (WIFI_PASS && strlen(WIFI_PASS) > 0) WiFi.begin(WIFI_SSID, WIFI_PASS);
  else                                    WiFi.begin(WIFI_SSID);

  const unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    if (millis() - t0 > 30000UL) {
      Serial.println(F("\n[WiFi] ❌ Connection timed out. Restarting..."));
      ESP.restart();
    }
  }
  Serial.printf("\n[WiFi] ✅ Connected! IP Address: %s\n", WiFi.localIP().toString().c_str());
}

inline void ensureWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WiFi] ❌ Lost connection. Reconnecting..."));
    setupWiFi();
  }
}

// ========== Arduino setup/loop ==========
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println(F("\n========== ESP32 RGB Controller =========="));

  setupPWM();
  setupWiFi();
  updateIntervalFromSpeed();

  Serial.println(F("[Espalexa] Registering Alexa devices..."));
  espalexa.addDevice("RGB Strip",  onColor);
  espalexa.addDevice("RGB Cycle",  onCycle);
  espalexa.addDevice("RGB Speed",  onCycleSpeed);

  espalexa.begin();
  Serial.println(F("[Espalexa] ✅ Devices ready. Example commands:"));
  Serial.println(F("   Alexa, set RGB Strip to blue 50%"));
  Serial.println(F("   Alexa, turn on RGB Cycle"));
  Serial.println(F("   Alexa, set RGB Speed to 20% (slower)"));
  Serial.println(F("   Alexa, set RGB Speed to 80% (faster)"));
}

void loop() {
  espalexa.loop();

  static unsigned long lastWiFiCheck = 0;
  const unsigned long now = millis();
  if (now - lastWiFiCheck >= 5000UL) {
    lastWiFiCheck = now;
    ensureWiFi();
  }

  if (cycleEnabled && (uint32_t)(now - lastStepMs) >= stepIntervalMs) {
    lastStepMs = now;

    cycleHue += HUE_STEP;
    if (cycleHue >= 360) cycleHue -= 360;

    uint8_t r, g, b;
    hsvToRgb(cycleHue, 255, cycleBri, r, g, b);
    writeRGB(r, g, b);

    Serial.printf("[Cycle] Hue=%3u → RGB(%3u,%3u,%3u)\n", cycleHue, r, g, b);
  }
}
