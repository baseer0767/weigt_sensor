#include <HX711.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

// Load cell pins
const int LOADCELL_DOUT_PIN = D2;
const int LOADCELL_SCK_PIN = D1;
const int PWM_OUTPUT_PIN = D5; // GPIO14 (avoid using GPIO9)

// Calibration
const float calibration_factor = 33.60056725;
const float maximum_weight_kg = 60.0;

// Wi-Fi credentials (stored in EEPROM)
char ssid[32] = "";
char password[32] = "";

// Server to POST data
const char* serverURL = "http://172.16.112.148:3000/api/loadcell";

HX711 scale;
WiFiClientSecure client; // Use WiFiClientSecure for HTTPS
ESP8266WebServer server(80);

void handleRoot() {
  String html = "<html><body>";
  html += "<h2>Wi-Fi Configuration</h2>";
  html += "<form action='/setwifi' method='POST'>";
  html += "SSID: <input type='text' name='ssid'><br>";
  html += "Password: <input type='password' name='password'><br>";
  html += "<input type='submit' value='Save Wi-Fi Settings'>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

void handleSetWiFi() {
  if (server.hasArg("ssid") && server.hasArg("password")) {
    String newSSID = server.arg("ssid");
    String newPassword = server.arg("password");

    newSSID.toCharArray(ssid, 32);
    newPassword.toCharArray(password, 32);

    EEPROM.begin(64);
    EEPROM.put(0, ssid);
    EEPROM.put(32, password);
    EEPROM.commit();

    server.send(200, "text/html", "<html><body><h2>Wi-Fi credentials saved. Rebooting...</h2></body></html>");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "<html><body><h2>Error: Missing SSID or Password</h2></body></html>");
  }
}

void setup() {
  Serial.begin(115200);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();

  pinMode(PWM_OUTPUT_PIN, OUTPUT);

  // Read stored Wi-Fi credentials
  EEPROM.begin(64);
  EEPROM.get(0, ssid);
  EEPROM.get(32, password);

  // Disable SSL certificate verification (for testing; use proper CA cert in production)
  client.setInsecure();

  if (strlen(ssid) > 0 && strlen(password) > 0) {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to Wi-Fi");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(1000);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected to Wi-Fi");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      return;
    }
  }

  Serial.println("\nStarting Access Point...");
  WiFi.softAP("LoadCell_Config");
  Serial.println("AP Mode: Connect to 'LoadCell_Config' and visit http://192.168.4.1");

  server.on("/", handleRoot);
  server.on("/setwifi", HTTP_POST, handleSetWiFi);
  server.begin();
}

void loop() {
  server.handleClient();

  static float lastWeight = -1.0; // Initialize with invalid weight
  const float threshold = 0.05;   // Minimum change in kg to trigger update (50 grams)

  if (WiFi.status() == WL_CONNECTED) {
    float weight = scale.get_units(10) / 1000.0; // Convert g to kg
    if (weight < 0) weight = 0;

    // If weight has changed beyond threshold, send update
    if (abs(weight - lastWeight) >= threshold) {
      lastWeight = weight;

      // Calculate PWM and voltage
      int pwmValue = map(weight, 0, maximum_weight_kg, 0, 255);
      pwmValue = constrain(pwmValue, 0, 255);
      analogWrite(PWM_OUTPUT_PIN, pwmValue);

      float voltage = pwmValue * (5.0 / 255.0);
      float percentage = (weight / maximum_weight_kg) * 100.0;
      if (percentage > 100.0) percentage = 100.0;

      // Round to 6 decimal places to match Postman precision
      weight = round(weight * 1000000) / 1000000.0;
      percentage = round(percentage * 1000000) / 1000000.0;

      // Print to serial
      Serial.printf("Weight: %.6f kg | Percentage: %.6f %% | Voltage: %.2f V\n", weight, percentage, voltage);

      // Send HTTP POST
      HTTPClient http;
      http.begin(client, serverURL);
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Accept", "application/json");

      StaticJsonDocument<200> doc;
      doc["weight"] = weight;
      doc["percentage"] = percentage;

      String payload;
      serializeJson(doc, payload);
      Serial.println("Payload:");
      Serial.println(payload);
      int response = http.POST(payload);

      if (response > 0) {
        Serial.printf("Data sent successfully. HTTP Response: %d\n", response);
        Serial.println("Response body:");
        Serial.println(http.getString());
      } else {
        Serial.printf("Error sending data: %d\n", response);
        Serial.println("Error details:");
        Serial.println(http.errorToString(response));
      }

      http.end();
    }
  } else {
    Serial.println("Wi-Fi not connected.");
    delay(3000);
  }

  delay(500); // Small delay to avoid rapid looping
}