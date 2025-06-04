#include <Arduino.h>
#include <functional>
#include <map>

const int RED_PIN = 19;
const int GREEN_PIN = 18;
const int BLUE_PIN = 5;
String input = "";

struct RGB {
  int red;
  int green;
  int blue;
};

RGB currentColor = {0, 0, 0};

void setColor(RGB color);
bool isHex(const String& str);
RGB hexToRGB(const String& hexColor);
void changeColor(RGB endColor, int steps, int delayTime);
void listPresets();
bool checkForNewInput();
// void animation(const String& animation);

// Define presets using a map
const std::map<String, RGB> presets = {
  {"sunset", {255, 94, 19}},
  {"ocean", {0, 128, 255}},
  {"forest", {34, 139, 34}},
  {"lavender", {230, 230, 250}},
  {"amber", {255, 69, 0}},
  {"off", {0, 0, 0}}
};

const String animationPresets[] = {"fire", "swirls"};

void setup() {
  Serial.begin(9600);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  changeColor(currentColor, 50, 30);
  listPresets();
}

void loop() {
  Serial.println("Please enter a color (preset name or hex code):");
  
  while (Serial.available() == 0) {
    // Wait for input from the user
  }

  input = Serial.readStringUntil('\n');
  input.trim();

  if (isHex(input)) {
    RGB color = hexToRGB(input);
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

  auto it = presets.find(input);
  if (it != presets.end()) {
    Serial.print("Starting transition to preset: ");
    Serial.println(it->first);
    changeColor(it->second, 50, 30); // 50 steps, 30ms delay per step
    return;
  }
  
  // for (auto it : animationPresets) {
  //   if (input == it) {
  //     animation(input);
  //   }
  // }

  Serial.println("Invalid input. Please enter a valid preset name or a hex color code.");
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

void setColor (RGB color) {
  analogWrite(RED_PIN, color.red);
  analogWrite(GREEN_PIN, color.green);
  analogWrite(BLUE_PIN, color.blue);
  currentColor = color;
}

void changeColor(RGB newColor, int steps, int delayTime) {
  for (int i = 0; i <= steps; i++) {
    if (checkForNewInput()) return; // Exit transition if new input is detected
    int redValue = currentColor.red + (newColor.red - currentColor.red) * i / steps;
    int greenValue = currentColor.green + (newColor.green - currentColor.green) * i / steps;
    int blueValue = currentColor.blue + (newColor.blue - currentColor.blue) * i / steps;
    analogWrite(RED_PIN, redValue);
    analogWrite(GREEN_PIN, greenValue);
    analogWrite(BLUE_PIN, blueValue);
    delay(delayTime);
  }
  currentColor = newColor; // Update the current color after completing the transition
}

bool checkForNewInput() {
  if (Serial.available() > 0) {
    input = Serial.readStringUntil('\n');
    input.trim();
    return true;
  }
  return false;
}

void listPresets() {
  Serial.println("Available presets:");
  for (const auto& preset : presets) {
    Serial.println(preset.first);
  }
  Serial.println("Enter a preset name to start, type a hex color (e.g., 'FFAABB') to set a static color.");
}

// void animation(const String& animType) {
//   switch ():
//     case 
// }