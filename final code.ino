#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <esp_wifi.h>
#include <HTTPClient.h>
#include <vector>
#include <string>

// Hardware pin definitions
#define BUZZER_PIN 27
#define RED_PIN 13
#define GREEN_PIN 12
#define BLUE_PIN 14

// Configuration constants
#define EEPROM_SIZE 512
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_MAC_ADDRESSES 10
#define WIFI_TIMEOUT 60000  // 1 minute timeout for WiFi connection
#define INTRUDER_ALERT_DURATION 5000 // 5 seconds alert duration

// AP Mode settings
const char* AP_SSID = "Intruder Detector";
const char* AP_PASSWORD = "12345678";

// Google Sheets Integration (Replace with your Google Script Web App URL)
const char* GOOGLE_SHEETS_URL = "https://script.google.com/macros/s/YOUR_SCRIPT_ID/exec";

// Global variables and objects
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27, 16 column and 2 rows
WebServer server(80);
DNSServer dnsServer;
HTTPClient httpClient;

String storedSSID = "";
String storedPassword = "";
std::vector<String> allowedMacs;

bool isAPMode = true;
unsigned long wifiDisconnectedTime = 0;
unsigned long lastWifiCheckTime = 0;
unsigned long intruderAlertStartTime = 0;

// HTML content for setup page (included from previous implementation)
const char* htmlContent = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Intruder Detector Setup</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 0; padding: 20px; }
        .container { max-width: 500px; margin: 0 auto; }
        h1 { color: #0066cc; }
        label { display: block; margin-top: 15px; }
        input[type=text], input[type=password] { width: 100%; padding: 10px; margin: 8px 0; box-sizing: border-box; }
        textarea { width: 100%; height: 100px; padding: 10px; margin: 8px 0; box-sizing: border-box; }
        button { background-color: #0066cc; color: white; padding: 12px 20px; border: none; cursor: pointer; margin-top: 15px; }
        .info { background-color: #e6f7ff; border-left: 5px solid #0066cc; padding: 10px; margin: 15px 0; }
    </style>
</head>
<body>
    <div class="container">
        <h1>ESP32 Intruder Detector Setup</h1>
        
        <div class="info">
            Enter your WiFi credentials and the MAC addresses you want to allow on your network.
            Any device with a MAC address not on this list will trigger an alert.
        </div>
        
        <form action="/save" method="post">
            <label for="ssid">WiFi SSID:</label>
            <input type="text" id="ssid" name="ssid" required>
            
            <label for="password">WiFi Password:</label>
            <input type="password" id="password" name="password" required>
            
            <label for="macs">Allowed MAC Addresses (one per line):</label>
            <textarea id="macs" name="macs" placeholder="AA:BB:CC:DD:EE:FF&#10;11:22:33:44:55:66"></textarea>
            
            <button type="submit">Save Configuration</button>
        </form>
    </div>
</body>
</html>
)";

// Function prototypes
void startAPMode();
void connectToWifi();
void handleRoot();
void handleSave();
void scanForIntruders();
void handleIntruder(const char* mac);
void setLEDColor(int r, int g, int b);
void logIntruderToSheet(const char* mac, bool connected);

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    
    // Initialize hardware pins
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
    pinMode(BLUE_PIN, OUTPUT);
    
    // Initialize LCD
    Wire.begin();
    lcd.init();
    lcd.backlight();
    
    // Initial startup beep and display
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Intruder Detector");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
    
    // Set LED to blue during startup
    setLEDColor(0, 0, 255);
    
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    
    // Disable WiFi power save mode
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    // Start in AP mode
    startAPMode();
}

void loop() {
    if (isAPMode) {
        // Process DNS and web server requests in AP mode
        dnsServer.processNextRequest();
        server.handleClient();
    } else {
        // WiFi station mode handling
        if (WiFi.status() != WL_CONNECTED) {
            handleWifiDisconnection();
        } else {
            wifiDisconnectedTime = 0;
            scanForIntruders();
            
            // Handle intruder alert duration
            if (intruderAlertStartTime > 0 && 
                (millis() - intruderAlertStartTime) >= INTRUDER_ALERT_DURATION) {
                // Reset alert after duration
                setLEDColor(0, 255, 0);  // Green
                digitalWrite(BUZZER_PIN, LOW);
                intruderAlertStartTime = 0;
            }
            
            delay(1000);  // Scan interval
        }
    }
}

void startAPMode() {
    isAPMode = true;
    
    // Stop previous WiFi activities
    WiFi.disconnect(true);
    delay(500);
    
    // Configure AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    // Start DNS server
    IPAddress apIP = WiFi.softAPIP();
    dnsServer.start(53, "*", apIP);
    
    // Configure web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleRoot);
    server.begin();
    
    // Update display
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("AP: Intruder");
    lcd.setCursor(0, 1);
    lcd.print(apIP.toString());
    
    // Set LED to blue in AP mode
    setLEDColor(0, 0, 255);
    
    Serial.println("AP Mode started");
}

void handleRoot() {
    server.send(200, "text/html", htmlContent);
}

void handleSave() {
    storedSSID = server.arg("ssid");
    storedPassword = server.arg("password");
    String macList = server.arg("macs");

    // Validate input
    if (storedSSID.length() == 0 || storedPassword.length() == 0) {
        server.send(400, "text/plain", "Invalid SSID or password");
        return;
    }

    // Save WiFi credentials to EEPROM
    int addr = 0;
    for (unsigned int i = 0; i < storedSSID.length(); i++) {
        EEPROM.write(addr++, storedSSID[i]);
    }
    EEPROM.write(addr++, 0);  // Null terminator

    for (unsigned int i = 0; i < storedPassword.length(); i++) {
        EEPROM.write(addr++, storedPassword[i]);
    }
    EEPROM.write(addr++, 0);  // Null terminator

    // Save MAC addresses to a separate section of EEPROM
    int macAddrStart = 100; // Start storing MAC addresses at address 100
    for (int i = macAddrStart; i < EEPROM_SIZE; i++) {
        EEPROM.write(i, 0); // Clear previous MAC addresses
    }

    int macAddr = macAddrStart;
    allowedMacs.clear();
    int start = 0;
    int end = macList.indexOf('\n');

    while (end >= 0 && allowedMacs.size() < MAX_MAC_ADDRESSES) {
        String mac = macList.substring(start, end);
        mac.trim();
        if (mac.length() > 0) {
            allowedMacs.push_back(mac);
            for (unsigned int i = 0; i < mac.length(); i++) {
                EEPROM.write(macAddr++, mac[i]);
            }
            EEPROM.write(macAddr++, 0); // Null terminator
        }
        start = end + 1;
        end = macList.indexOf('\n', start);
    }

    // Add last MAC if no trailing newline
    if (start < macList.length() && allowedMacs.size() < MAX_MAC_ADDRESSES) {
        String mac = macList.substring(start);
        mac.trim();
        if (mac.length() > 0) {
            allowedMacs.push_back(mac);
            for (unsigned int i = 0; i < mac.length(); i++) {
                EEPROM.write(macAddr++, mac[i]);
            }
            EEPROM.write(macAddr++, 0); // Null terminator
        }
    }

    EEPROM.commit();

    // Send response
    server.send(200, "text/plain", "Configuration saved. Connecting to WiFi...");

    // Attempt to connect to WiFi
    connectToWifi();
}

void connectToWifi() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    
    lcd.clear();
    lcd.print("Connecting to");
    lcd.setCursor(0, 1);
    lcd.print(storedSSID);

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_TIMEOUT) {
        delay(500);
        Serial.println("Attempting to connect...");
        yield();  // Reset watchdog
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Connected to WiFi!");
        // Keep AP active for scanning
        WiFi.softAP(AP_SSID, AP_PASSWORD);
        setLEDColor(0, 255, 0);  // Green
        isAPMode = false;
        
        lcd.clear();
        lcd.print("Connected &");
        lcd.setCursor(0, 1);
        lcd.print("Scanning...");
    } else {
        Serial.println("Failed to connect to WiFi");
        handleWifiDisconnection();
    }
}

void handleWifiDisconnection() {
    setLEDColor(0, 0, 255);  // Blue
    
    if (wifiDisconnectedTime == 0) {
        wifiDisconnectedTime = millis();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Disconnected");
        lcd.setCursor(0, 1);
        lcd.print("Reconnecting...");
    }
    
    if (millis() - lastWifiCheckTime > 5000) {
        lastWifiCheckTime = millis();
        WiFi.begin(storedSSID.c_str(), storedPassword.c_str());
    }
    
    if (millis() - wifiDisconnectedTime > WIFI_TIMEOUT) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Reconnect failed");
        lcd.setCursor(0, 1);
        lcd.print("Entering AP mode");
        delay(2000);
        
        startAPMode();
    }
}

void scanForIntruders() {
    wifi_sta_list_t stationList;
    esp_err_t err = esp_wifi_ap_get_sta_list(&stationList);
    
    if (err != ESP_OK) {
        Serial.println("Failed to get station list");
        lcd.clear();
        lcd.print("Scan Error");
        return;
    }

    for (int i = 0; i < stationList.num; i++) {
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 stationList.sta[i].mac[0], stationList.sta[i].mac[1],
                 stationList.sta[i].mac[2], stationList.sta[i].mac[3],
                 stationList.sta[i].mac[4], stationList.sta[i].mac[5]);
        
        if (!isMacAllowed(macStr)) {
            handleIntruder(macStr);
        }
    }
}

bool isMacAllowed(const char* mac) {
    String macStr = String(mac);

    // Debugging: Print the MAC being checked
    Serial.printf("Checking MAC: %s\n", macStr.c_str());

    for (unsigned int i = 0; i < allowedMacs.size(); i++) {
        Serial.printf("Allowed MAC: %s\n", allowedMacs[i].c_str());
        if (allowedMacs[i].equalsIgnoreCase(macStr)) {
            return true; // MAC is allowed
        }
    }

    return false; // MAC is not allowed
}

void handleIntruder(const char* mac) {
    // Set LED to red
    setLEDColor(0, 50, 30); 
    
    // Sound the buzzer
    digitalWrite(BUZZER_PIN, HIGH);
    
    // Set intruder alert start time
    intruderAlertStartTime = millis();
    
    // Display intruder MAC
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Intruder Detected");
    lcd.setCursor(0, 1);
    lcd.print(mac);
    
    // Log to serial
    Serial.print("Intruder detected: ");
    Serial.println(mac);
    
    // Log to Google Sheets
    logIntruderToSheet(mac, true);
    
    // Attempt to deauthenticate all stations
    esp_wifi_deauth_sta(0);
}

void logIntruderToSheet(const char* mac, bool connected) {
    // Only log if Google Sheets URL is defined
    if (strlen(GOOGLE_SHEETS_URL) > 0) {
        HTTPClient http;
        http.begin(GOOGLE_SHEETS_URL);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        
        // Prepare payload
        String payload = String("mac=") + mac + 
                         "&status=" + (connected ? "Connected" : "Disconnected") +
                         "&timestamp=" + String(millis());
        
        int httpResponseCode = http.POST(payload);
        
        if (httpResponseCode > 0) {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
        } else {
            Serial.println("Error logging to Google Sheets");
        }
        
        http.end();
    }
}

void setLEDColor(int r, int g, int b) {
    analogWrite(RED_PIN, r);
    analogWrite(GREEN_PIN, g);
    analogWrite(BLUE_PIN, b);
}