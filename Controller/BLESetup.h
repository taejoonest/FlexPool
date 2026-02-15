/*
 * =============================================
 * BLESetup.h - Bluetooth LE Provisioning
 * =============================================
 * 
 * ONE-TIME SETUP via Bluetooth:
 *   1. ESP32 boots with no WiFi credentials
 *   2. BLE turns ON, advertises as "FlexPool"
 *   3. User opens the FlexPool webpage (Chrome on PC or Android)
 *   4. Browser connects via BLE
 *   5. Browser sends WiFi SSID + password over BLE
 *   6. ESP32 saves credentials, connects to WiFi
 *   7. ESP32 sends back Device ID over BLE
 *   8. BLE turns OFF, ESP32 reboots into normal mode
 * 
 * NOTE: Web Bluetooth requires Chrome (Windows/Mac/Android).
 *       iPhones do NOT support Web Bluetooth.
 * 
 * EVERY BOOT AFTER:
 *   - BLE never turns on
 *   - ESP32 connects to WiFi using saved credentials
 * 
 * TIMEOUT:
 *   - BLE stays on for 5 minutes
 *   - If nobody connects, ESP32 restarts and tries again
 * 
 * REQUIRES: ESP32 BLE Arduino library (built-in with ESP32 board package)
 */

#ifndef BLE_SETUP_H
#define BLE_SETUP_H

#include <WiFi.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// =============================================
// CONFIGURATION
// =============================================
#define BLE_DEVICE_NAME    "FlexPool"
#define BLE_TIMEOUT_MS     300000   // 5 minutes

#define WIFI_CONNECT_TIMEOUT  20    // seconds

#define PREFS_NAMESPACE    "flexpool"
#define PREFS_KEY_SSID     "ssid"
#define PREFS_KEY_PASS     "pass"

// =============================================
// BLE UUIDs (custom, unique to FlexPool)
// =============================================
// Service: the main FlexPool provisioning service
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

// Characteristic: browser WRITES WiFi credentials here
// Format: JSON string {"ssid":"MyWiFi","pass":"mypassword"}
#define CHAR_WIFI_UUID         "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Characteristic: ESP32 WRITES status/result here, browser reads/gets notified
// Format: JSON string {"ok":true,"id":"A1B2C3","ip":"192.168.1.50"}
//   or on failure:    {"ok":false,"error":"Connection failed"}
#define CHAR_STATUS_UUID       "d1a7e0c2-4f3b-4b1e-9c5d-8a6f2b3e4d5c"

// Characteristic: browser can WRITE "scan" to trigger WiFi network scan
// ESP32 responds on CHAR_STATUS with list of networks
#define CHAR_SCAN_UUID         "e3b0c442-98fc-1c14-b39f-92f6bce1d587"

// =============================================
// FORWARD DECLARATIONS
// =============================================
class BLESetup;

// Global pointer for BLE callbacks to access
static BLESetup* _bleSetupInstance = nullptr;

// =============================================
// BLESetup CLASS
// =============================================
class BLESetup {
private:
  BLEServer*         _server = nullptr;
  BLECharacteristic* _charWifi = nullptr;
  BLECharacteristic* _charStatus = nullptr;
  BLECharacteristic* _charScan = nullptr;
  
  bool _deviceConnected = false;
  bool _shouldRestart = false;
  String _receivedSSID = "";
  String _receivedPass = "";
  bool _credentialsReceived = false;
  bool _scanRequested = false;

  // ---- Simple JSON parser (no library needed) ----
  String extractJsonString(const String& json, const char* key) {
    String search = String("\"") + key + "\"";
    int idx = json.indexOf(search);
    if (idx < 0) return "";
    
    // Find the colon
    idx = json.indexOf(':', idx);
    if (idx < 0) return "";
    
    // Find opening quote
    idx = json.indexOf('"', idx + 1);
    if (idx < 0) return "";
    
    // Find closing quote
    int end = json.indexOf('"', idx + 1);
    if (end < 0) return "";
    
    return json.substring(idx + 1, end);
  }

  // ---- Generate Device ID from MAC address ----
  String getDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char id[7];
    snprintf(id, sizeof(id), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(id);
  }

  // ---- Try connecting to WiFi with given credentials ----
  bool tryConnect(const String& ssid, const String& pass) {
    Serial.printf("[BLE] Trying to connect to \"%s\"...\n", ssid.c_str());
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_TIMEOUT * 2) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();
    
    return (WiFi.status() == WL_CONNECTED);
  }

  // ---- Save credentials to flash ----
  void saveCredentials(const String& ssid, const String& pass) {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.putString(PREFS_KEY_SSID, ssid);
    prefs.putString(PREFS_KEY_PASS, pass);
    prefs.end();
    Serial.printf("[BLE] Credentials saved for \"%s\"\n", ssid.c_str());
  }

  // ---- Send status back to browser via BLE ----
  void sendStatus(const String& json) {
    if (_charStatus && _deviceConnected) {
      _charStatus->setValue(json.c_str());
      _charStatus->notify();
      Serial.printf("[BLE] Status sent: %s\n", json.c_str());
    }
  }

  // ---- Handle received WiFi credentials ----
  void handleCredentials() {
    _credentialsReceived = false;
    
    Serial.printf("[BLE] Received WiFi credentials: SSID=\"%s\"\n", _receivedSSID.c_str());
    
    // Notify phone: trying to connect
    sendStatus("{\"status\":\"connecting\"}");
    delay(200);
    
    // Try connecting
    if (tryConnect(_receivedSSID, _receivedPass)) {
      // SUCCESS!
      String ip = WiFi.localIP().toString();
      String deviceId = getDeviceId();
      
      Serial.printf("[BLE] Connected! IP: %s, Device ID: %s\n", ip.c_str(), deviceId.c_str());
      
      // Save credentials to flash
      saveCredentials(_receivedSSID, _receivedPass);
      
      // Send success + device ID back to phone
      String response = "{\"ok\":true,\"id\":\"" + deviceId + "\",\"ip\":\"" + ip + "\"}";
      sendStatus(response);
      
      // Wait for phone to receive the response
      delay(2000);
      
      _shouldRestart = true;
      
    } else {
      // FAILED
      Serial.println("[BLE] WiFi connection FAILED");
      
      // Disconnect WiFi and go back to waiting
      WiFi.disconnect();
      
      // Notify phone
      sendStatus("{\"ok\":false,\"error\":\"Could not connect to WiFi. Check password.\"}");
    }
  }

  // ---- Handle WiFi scan request ----
  void handleScanRequest() {
    _scanRequested = false;
    
    Serial.println("[BLE] Scanning WiFi networks...");
    
    // Need to temporarily set WiFi mode for scanning
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    
    int n = WiFi.scanNetworks();
    
    String json = "{\"networks\":[";
    for (int i = 0; i < n && i < 10; i++) {  // max 10 networks (BLE packet size limit)
      if (i > 0) json += ",";
      json += "{\"s\":\"" + WiFi.SSID(i) + "\",\"r\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]}";
    
    WiFi.mode(WIFI_OFF);  // Turn WiFi back off
    
    sendStatus(json);
    Serial.printf("[BLE] Found %d networks\n", n);
  }

public:
  // =============================================
  // BLE SERVER CALLBACKS (friend access)
  // =============================================
  void onConnect() {
    _deviceConnected = true;
    Serial.println("[BLE] Device connected!");
  }
  
  void onDisconnect() {
    _deviceConnected = false;
    Serial.println("[BLE] Device disconnected");
    // Restart advertising so phone can reconnect
    if (_server && !_shouldRestart) {
      _server->startAdvertising();
    }
  }
  
  void onWifiWrite(const String& value) {
    _receivedSSID = extractJsonString(value, "ssid");
    _receivedPass = extractJsonString(value, "pass");
    _credentialsReceived = true;
  }
  
  void onScanWrite() {
    _scanRequested = true;
  }

  // =============================================
  // PUBLIC: Check if credentials exist
  // =============================================
  static bool hasSavedCredentials() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    String ssid = prefs.getString(PREFS_KEY_SSID, "");
    prefs.end();
    return (ssid.length() > 0);
  }

  // =============================================
  // PUBLIC: Get saved SSID
  // =============================================
  static String getSavedSSID() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    String ssid = prefs.getString(PREFS_KEY_SSID, "");
    prefs.end();
    return ssid;
  }

  // =============================================
  // PUBLIC: Connect using saved credentials
  // =============================================
  static bool connectSaved() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    String ssid = prefs.getString(PREFS_KEY_SSID, "");
    String pass = prefs.getString(PREFS_KEY_PASS, "");
    prefs.end();
    
    if (ssid.length() == 0) return false;
    
    Serial.printf("[WiFi] Connecting to \"%s\"", ssid.c_str());
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < WIFI_CONNECT_TIMEOUT * 2) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(" Connected!");
      Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("[WiFi] Signal: %d dBm\n", WiFi.RSSI());
      return true;
    } else {
      Serial.println(" FAILED!");
      return false;
    }
  }

  // =============================================
  // PUBLIC: Clear saved credentials
  // =============================================
  static void clearCredentials() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    Serial.println("[WiFi] Saved credentials cleared.");
  }

  // =============================================
  // PUBLIC: Run BLE provisioning (blocking)
  // Runs for up to 5 minutes, then restarts
  // =============================================
  void runProvisioning() {
    _bleSetupInstance = this;
    
    Serial.println("\n=============================================");
    Serial.println("  BLUETOOTH SETUP MODE");
    Serial.println("=============================================");
    Serial.println("  BLE is ON for 5 minutes.");
    Serial.println("  On your PC or Android, open Chrome:");
    Serial.println("  https://taejoonest.github.io/FlexPool");
    Serial.println("  Then click 'Connect via Bluetooth'.");
    Serial.println("  (iPhone NOT supported for setup)");
    Serial.println("=============================================\n");

    // Initialize BLE
    BLEDevice::init(BLE_DEVICE_NAME);
    
    // Create BLE Server
    _server = BLEDevice::createServer();
    
    // Set server callbacks using a lambda-friendly approach
    class ServerCallbacks : public BLEServerCallbacks {
      void onConnect(BLEServer* server) override {
        if (_bleSetupInstance) _bleSetupInstance->onConnect();
      }
      void onDisconnect(BLEServer* server) override {
        if (_bleSetupInstance) _bleSetupInstance->onDisconnect();
      }
    };
    _server->setCallbacks(new ServerCallbacks());
    
    // Create BLE Service
    BLEService* service = _server->createService(SERVICE_UUID);
    
    // Characteristic: WiFi credentials (phone writes to this)
    _charWifi = service->createCharacteristic(
      CHAR_WIFI_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );
    
    class WifiCallbacks : public BLECharacteristicCallbacks {
      void onWrite(BLECharacteristic* c) override {
        if (_bleSetupInstance) {
          String val = String(c->getValue().c_str());
          _bleSetupInstance->onWifiWrite(val);
        }
      }
    };
    _charWifi->setCallbacks(new WifiCallbacks());
    
    // Characteristic: Status (ESP32 writes, phone reads/gets notified)
    _charStatus = service->createCharacteristic(
      CHAR_STATUS_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    _charStatus->addDescriptor(new BLE2902());
    
    // Set initial status with device ID
    String initStatus = "{\"status\":\"ready\",\"id\":\"" + getDeviceId() + "\"}";
    _charStatus->setValue(initStatus.c_str());
    
    // Characteristic: Scan trigger (phone writes "scan" to request WiFi scan)
    _charScan = service->createCharacteristic(
      CHAR_SCAN_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );
    
    class ScanCallbacks : public BLECharacteristicCallbacks {
      void onWrite(BLECharacteristic* c) override {
        if (_bleSetupInstance) _bleSetupInstance->onScanWrite();
      }
    };
    _charScan->setCallbacks(new ScanCallbacks());
    
    // Start service and advertising
    service->start();
    
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.printf("[BLE] Advertising as \"%s\" ΓÇö waiting for browser connection...\n", BLE_DEVICE_NAME);
    
    // ---- Main loop: wait for phone to connect and send credentials ----
    unsigned long startTime = millis();
    unsigned long lastMsg = 0;
    
    while (!_shouldRestart && (millis() - startTime < BLE_TIMEOUT_MS)) {
      // Handle WiFi credentials if received
      if (_credentialsReceived) {
        handleCredentials();
      }
      
      // Handle WiFi scan request
      if (_scanRequested) {
        handleScanRequest();
      }
      
      // Periodic status message
      if (millis() - lastMsg > 15000) {
        unsigned long remaining = (BLE_TIMEOUT_MS - (millis() - startTime)) / 1000;
        Serial.printf("[BLE] Waiting for connection... (%lu seconds remaining)\n", remaining);
        lastMsg = millis();
      }
      
      delay(50);
    }
    
    // ---- Cleanup ----
    Serial.println("[BLE] Shutting down Bluetooth...");
    BLEDevice::deinit(true);  // true = release memory
    delay(500);
    
    if (_shouldRestart) {
      Serial.println("[BLE] Setup complete! Restarting into normal mode...\n");
    } else {
      Serial.println("[BLE] Timeout (5 min). Restarting to try again...\n");
    }
    
    delay(500);
    ESP.restart();
  }
};

#endif // BLE_SETUP_H
