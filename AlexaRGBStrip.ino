#include <Arduino.h>
#include <WiFi.h>
#include <Espalexa.h>  // Library Manager

/********** USER CONFIG **********/
const char* WIFI_SSID = "LC-3";   // open network (no password)
const char* WIFI_PASS = "wiphipas";                // leave empty for open SSID
//const char* WIFI_SSID = "Campus Circle";
//const char* WIFI_PASS = "";

// Your wired pins:
const int PIN_R = 4;    // D4 → GPIO4
const int PIN_G = 16;   // D16 → GPIO16
const int PIN_B = 17;   // D17 → GPIO17

// PWM config
const int FREQ_HZ  = 5000;   // 5 kHz
const int RES_BITS = 8;      // 0–255
/*********************************/

Espalexa espalexa;

// Detect ESP32 Arduino core 3.x+
#if defined(ESP_ARDUINO_VERSION) && (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3,0,0))
  #define ESP32_CORE_V3 1
#endif

// ---------- PWM helpers (core-conditional) ----------
#ifdef ESP32_CORE_V3
  // Core 3.x API (no channels)
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
    Serial.printf("[PWM] Writing RGB: R=%d, G=%d, B=%d\n", r, g, b);
    pwmWritePin(PIN_R, r);
    pwmWritePin(PIN_G, g);
    pwmWritePin(PIN_B, b);
  }
#else
  // Core 2.x API (channels)
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
    Serial.printf("[PWM] Writing RGB: R=%d, G=%d, B=%d\n", r, g, b);
    ledcWrite(CH_R, r);
    ledcWrite(CH_G, g);
    ledcWrite(CH_B, b);
  }
#endif
// ----------------------------------------------------

void onColor(uint8_t bri, uint32_t rgb) {
  uint8_t r = (rgb >> 16) & 0xFF;
  uint8_t g = (rgb >>  8) & 0xFF;
  uint8_t b = (rgb      ) & 0xFF;

  Serial.printf("[Espalexa] Raw RGB=%06X, Brightness=%d\n", rgb, bri);

  r = (uint16_t)r * bri / 255;
  g = (uint16_t)g * bri / 255;
  b = (uint16_t)b * bri / 255;

  Serial.printf("[Espalexa] Adjusted RGB: R=%d, G=%d, B=%d\n", r, g, b);
  writeRGB(r, g, b);
}

void setupWiFi() {
  Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  if (WIFI_PASS && strlen(WIFI_PASS) > 0) WiFi.begin(WIFI_SSID, WIFI_PASS);
  else                                    WiFi.begin(WIFI_SSID); // open network

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retries++;
    if (retries > 40) { // 20 seconds
      Serial.println("\n[WiFi] Connection timed out, restarting...");
      ESP.restart();
    }
  }
  Serial.printf("\n[WiFi] Connected! IP address: %s\n", WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n[Setup] Booting ESP32 RGB Controller...");

  setupPWM();
  setupWiFi();

  Serial.println("[Espalexa] Adding RGB Strip device...");
  espalexa.addDevice("RGB Strip", onColor);  // color device
  espalexa.begin();
  Serial.println("[Espalexa] Ready. Say 'Alexa, turn on RGB Strip'.");
}

void loop() {
  espalexa.loop();
}
