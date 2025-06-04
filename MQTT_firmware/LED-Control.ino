#include <WiFi.h>
#include <PubSubClient.h>
#include <functional>
#include <map>

// WiFi
const char* ssid = "Cerebro"; // Enter your Wi-Fi name
const char* password = "Alfred da Butt-ler";  // Enter Wi-Fi password
WiFiClient espClient;

// MQTT Broker and Client
const char *mqtt_broker = "96.63.208.234";
const int mqtt_port = 1883;
const char *topic = "control-lights/changes";
PubSubClient client(espClient);

// GPIO pin initialization
const int RED_PIN = 19;
const int GREEN_PIN = 18;
const int BLUE_PIN = 5;

// Initialize colors
struct RGB {
    int red;
    int green;
    int blue;
};

const std::map<String, RGB> presets = {
  {"sunset", {255, 94, 19}},
  {"ocean", {0, 128, 255}},
  {"forest", {34, 139, 34}},
  {"lavender", {230, 230, 250}},
  {"amber", {255, 69, 0}},
  {"off", {0, 0, 0}}
};

RGB currentColor = {0, 0, 0};

void setup() {
    // Initialize the Serial Monitor
    Serial.begin(115200);
    delay(1000);

    // Set wifi module to station mode and connect to the wifi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("\nConnecting");

    // Print filler strings to indicate trying to connect
    while(WiFi.status() != WL_CONNECTED){
        Serial.println("...");
        delay(100);
    }

    // Print wifi details
    Serial.println("\nConnected to the WiFi network");
    Serial.print("Local ESP32 IP: ");
    Serial.println(WiFi.localIP());

    // Connect to mqtt broker
    client.setServer(mqtt_broker, mqtt_port);
    client.setCallback(callback); // Callback function for when a message is received.
    while (!client.connected()) {
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
        if (client.connect(client_id.c_str())) {
            Serial.println("Hal MQTT broker connected");
        } else {
            Serial.print("failed with state ");
            Serial.print(client.state());
            delay(2000);
        }
    }
    
    // Publish and subscribe to the topic
    char message[] = "Initialized connection from ESP32";
    client.publish(topic, message);
    Serial.printf("Published message: %s\n", message);
    Serial.println("-----------------------");

    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    changeColor(currentColor, 50, 30);
    listPresets();
    client.subscribe(topic);
}

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char) payload[i];
    }
    Serial.println(message);
    Serial.println("-----------------------");
    handleMessage(message);
}

void loop() {
    client.loop();
}

void listPresets() {
  client.publish(topic, "Available presets:");
  for (const auto& preset : presets) {
    client.publish(topic, (preset.first).c_str());
  }
  client.publish(topic, "Enter a preset name to start, type a hex color (e.g., 'FFAABB') to set a static color.");
}

void handleMessage(String message) {
  message.trim();

  if (isHex(message)) {
    RGB color = hexToRGB(message);
    Serial.print("Setting static color: R=");
    Serial.print(color.red);
    Serial.print(" G=");
    Serial.print(color.green);
    Serial.print(" B=");
    Serial.println(color.blue);
    changeColor(color, 50, 5);
    currentColor = color;
    return;
  }

  auto it = presets.find(message);
  if (it != presets.end()) {
    Serial.print("Starting transition to preset: ");
    Serial.println(it->first);
    changeColor(it->second, 50, 30); // 50 steps, 30ms delay per step
    return;
  }

  client.publish(topic, "Invalid input. Please enter a valid preset name or a hex color code.");
}

void setColor(RGB color) {
  analogWrite(RED_PIN, color.red);
  analogWrite(GREEN_PIN, color.green);
  analogWrite(BLUE_PIN, color.blue);
  currentColor = color;
}

void changeColor(RGB newColor, int steps, int delayTime) {
  RGB prevColor = currentColor;
  for (int i = 0; i <= steps; i++) {
    RGB tempColor;
    tempColor.red = prevColor.red + (newColor.red - prevColor.red) * i / steps;
    tempColor.green = prevColor.green + (newColor.green - prevColor.green) * i / steps;
    tempColor.blue = prevColor.blue + (newColor.blue - prevColor.blue) * i / steps;
    setColor(tempColor);
    delay(delayTime);
  }
  currentColor = newColor; // Update the current color after completing the transition
}

bool isHex(const String& str) {
  if (str.length() != 6) return false;
  for (char c : str) {
    if (!isxdigit(c)) return false;
  }
  return true;
}

RGB hexToRGB(const String& hexColor) {
  RGB color = {0, 0, 0};
  color.red = strtol(hexColor.substring(0, 2).c_str(), NULL, 16);
  color.green = strtol(hexColor.substring(2, 4).c_str(), NULL, 16);
  color.blue = strtol(hexColor.substring(4, 6).c_str(), NULL, 16);
  return color;
}
