# ESP32 Intruder Detector

This project implements an intruder detection system using an ESP32 microcontroller. It monitors WiFi network activity, detects unauthorized devices based on MAC addresses, and provides alerts via RGB LED, buzzer, and LCD display. The system also logs intruder information to Google Sheets and offers a web interface for configuration.

## Features
- **WiFi Monitoring**: Detects devices connected to the network.
- **MAC Address Filtering**: Allows only specified MAC addresses; others trigger alerts.
- **AP Mode Configuration**: Provides a web interface to set WiFi credentials and allowed MAC addresses.
- **Visual and Audible Alerts**: Uses an RGB LED and buzzer to indicate status and alerts.
- **LCD Display**: Shows system status, IP address, and intruder information.
- **Google Sheets Integration**: Logs intruder events to a Google Sheet.
- **Persistent Storage**: Saves configuration to EEPROM.
- **Deauthentication**: Attempts to disconnect unauthorized devices.

## Hardware Requirements
- ESP32 development board
- RGB LED (common cathode/anode based on wiring)
- Buzzer
- 16x2 I2C LCD display (address 0x27)
- Resistors for LED (if needed)
- Breadboard and jumper wires

## Pin Configuration
| Component   | Pin  |
|-------------|------|
| Buzzer      | GPIO27 |
| RGB LED Red | GPIO13 |
| RGB LED Green | GPIO12 |
| RGB LED Blue  | GPIO14 |
| LCD I2C     | SDA/SCL (default I2C pins) |

## Software Requirements
- Arduino IDE with ESP32 board support
- Libraries:
  - `WiFi.h`
  - `WebServer.h`
  - `DNSServer.h`
  - `EEPROM.h`
  - `Wire.h`
  - `LiquidCrystal_I2C.h`
  - `esp_wifi.h`
  - `HTTPClient.h`

## Installation
1. **Set up Arduino IDE**:
   - Install the ESP32 board package.
   - Install required libraries via the Library Manager or manually.

2. **Configure Google Sheets** (optional):
   - Create a Google Sheet.
   - Deploy a Google Apps Script as a web app to handle HTTP POST requests.
   - Update `GOOGLE_SHEETS_URL` in the code with your script's URL.

3. **Upload Code**:
   - Connect the ESP32 to your computer.
   - Open the `.ino` file in Arduino IDE.
   - Update `GOOGLE_SHEETS_URL` if using Google Sheets integration.
   - Upload the code to the ESP32.

## Usage
1. **Initial Setup**:
   - On first boot, the ESP32 starts in Access Point (AP) mode with:
     - SSID: `Intruder Detector`
     - Password: `12345678`
   - Connect to this WiFi network from a device.
   - Access the configuration page at the IP address shown on the LCD (usually `192.168.4.1`).

2. **Configuration**:
   - Enter your WiFi SSID and password.
   - List allowed MAC addresses (one per line, format `AA:BB:CC:DD:EE:FF`).
   - Submit to save settings and connect to your WiFi network.

3. **Operation**:
   - **Green LED**: Connected to WiFi and scanning.
   - **Blue LED**: AP mode or WiFi disconnected.
   - **Red LED + Buzzer**: Intruder detected.
   - The LCD shows status, IP address, or intruder MAC.
   - Intruder events are logged to Google Sheets (if configured).

4. **Intruder Detection**:
   - Scans for devices connected to the AP.
   - If a device's MAC address isn't in the allowed list, it triggers an alert and attempts deauthentication.

## Configuration Details
- **EEPROM Storage**:
  - WiFi credentials stored at address 0.
  - MAC addresses stored starting at address 100.
  - Maximum 10 MAC addresses.
- **Timeouts**:
  - WiFi connection timeout: 60 seconds.
  - Intruder alert duration: 5 seconds.
- **AP Mode**:
  - Automatically restarts if WiFi connection fails for 60 seconds.

## Notes
- Ensure MAC addresses are entered correctly in the format `XX:XX:XX:XX:XX:XX`.
- The deauthentication feature may not work on all devices due to modern WiFi security protocols.
- Google Sheets integration requires a stable internet connection.
- The system keeps the AP active even when connected to WiFi to scan for devices.

## Troubleshooting
- **Can't connect to AP**:
  - Ensure you're within WiFi range.
  - Verify SSID and password.
- **WiFi not connecting**:
  - Check credentials in the web interface.
  - Ensure the WiFi network is 2.4GHz (ESP32 doesn't support 5GHz).
- **No LCD display**:
  - Verify I2C address (0x27) and connections.
- **Intruder not detected**:
  - Ensure the device is connecting to the ESP32's AP.
  - Check allowed MAC addresses.

## License
This project is open-source and available under the MIT License.

## Contributing
Feel free to submit issues or pull requests for improvements or bug fixes.