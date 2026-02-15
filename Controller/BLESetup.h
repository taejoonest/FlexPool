/*
 * =============================================
 * BLESetup.h - Bluetooth WiFi Provisioning
 * =============================================
 * 
 * Handles one-time WiFi setup via Bluetooth Low Energy (BLE).
 * 
 * HOW IT WORKS:
 *   1. If no WiFi credentials are saved, ESP32 advertises via BLE
 *      as "FlexPool" for up to 5 minutes.
 *   2. User connects from Chrome on PC/Android and sends WiFi
 *      SSID + password via BLE characteristics.
 *   3. ESP32 saves credentials to flash, connects to WiFi, and reboots.
 *   4. On subsequent boots, ESP32 connects to saved WiFi automatically.
 * 
 * NOTE: Web Bluetooth API does NOT work on iPhone/iOS.
 *       Use Chrome on PC or Android for BLE setup.
 * 
 * ALSO PROVIDES:
 *   - WiFi credential storage/retrieval via Preferences (flash memory)
 *   - Static helper methods for checking/connecting/clearing credentials
 * 
 * REQUIRES: Built-in ESP32 BLE libraries (no extra install needed)
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
// BLE SERVICE & CHARACTERISTIC UUIDs
// =============================================
// Custom UUIDs for FlexPool WiFi provisioning
#define FLEXPOOL_SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHAR_SSID_UUID               "12345678-1234-5678-1234-56789abcdef1"
#define CHAR_PASSWORD_UUID           "12345678-1234-5678-1234-56789abcdef2"
#define CHAR_DEVICE_ID_UUID          "12345678-1234-5678-1234-56789abcdef3"
#define CHAR_STATUS_UUID             "12345678-1234-5678-1234-56789abcdef4"
#define CHAR_COMMAND_UUID            "12345678-1234-5678-1234-56789abcdef5"

// =============================================
// PREFERENCES KEYS (flash storage)
// =============================================
#define PREF_NAMESPACE  "flexpool"
#define PREF_SSID       "wifi_ssid"
#define PREF_PASSWORD   "wifi_pass"

// =============================================
// TIMING
// =============================================
#define BLE_TIMEOUT_MS  (5 * 60 * 1000)  // 5 minutes

// =============================================
// BLESetup CLASS
// =============================================
class BLESetup {
private:
  BLEServer* _pServer = nullptr;
  BLECharacteristic* _charSSID = nullptr;
  BLECharacteristic* _charPass = nullptr;
  BLECharacteristic* _charDeviceId = nullptr;
  BLECharacteristic* _charStatus = nullptr;
  BLECharacteristic* _charCommand = nullptr;
  
  bool _deviceConnected = false;
  bool _credentialsReceived = false;
  String _newSSID = "";
  String _newPassword = "";
  
  // ---- BLE Server Callbacks ----
  class ServerCallbacks : public BLEServerCallbacks {
  public:
    BLESetup* parent;
    ServerCallbacks(BLESetup* p) : parent(p) {}
    
    void onConnect(BLEServer* pServer) override {
      parent->_deviceConnected = true;
      Serial.println("[BLE] Device connected!");
      Serial.println("[BLE] Waiting for WiFi credentials...");
    }
    
    void onDisconnect(BLEServer* pServer) override {
      parent->_deviceConnected = false;
      Serial.println("[BLE] Device disconnected");
      // Re-advertise if credentials weren't received
      if (!parent->_credentialsReceived) {
        pServer->startAdvertising();
        Serial.println("[BLE] Re-advertising...");
      }
    }
  };
  
  // ---- Command Characteristic Callback ----
  // When the web page writes "CONNECT" to the command characteristic,
  // it means SSID and password have been written and we should try to connect.
  class CommandCallback : public BLECharacteristicCallbacks {
  public:
    BLESetup* parent;
    CommandCallback(BLESetup* p) : parent(p) {}
    
    void onWrite(BLECharacteristic* pCharacteristic) override {
      std::string val = pCharacteristic->getValue();
      String cmd = String(val.c_str());
      cmd.trim();
      
      if (cmd.equalsIgnoreCase("CONNECT")) {
        // Read SSID and password from their characteristics
        std::string ssidVal = parent->_charSSID->getValue();
        std::string passVal = parent->_charPass->getValue();
        
        parent->_newSSID = String(ssidVal.c_str());
        parent->_newPassword = String(passVal.c_str());
        
        Serial.printf("[BLE] Received SSID: \"%s\"\n", parent->_newSSID.c_str());
        Serial.println("[BLE] Received password: ****");
        
        if (parent->_newSSID.length() > 0) {
          parent->_credentialsReceived = true;
          
          // Update status characteristic
          parent->_charStatus->setValue("CONNECTING");
          parent->_charStatus->notify();
        } else {
          parent->_charStatus->setValue("ERROR: Empty SSID");
          parent->_charStatus->notify();
        }
      }
    }
  };

public:
  BLESetup() {}
  
  // =============================================
  // STATIC: Check if WiFi credentials are saved
  // =============================================
  static bool hasSavedCredentials() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);  // read-only
    String ssid = prefs.getString(PREF_SSID, "");
    prefs.end();
    return ssid.length() > 0;
  }
  
  // =============================================
  // STATIC: Get saved SSID
  // =============================================
  static String getSavedSSID() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    String ssid = prefs.getString(PREF_SSID, "");
    prefs.end();
    return ssid;
  }
  
  // =============================================
  // STATIC: Connect to WiFi using saved credentials
  // =============================================
  static bool connectSaved() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    String ssid = prefs.getString(PREF_SSID, "");
    String pass = prefs.getString(PREF_PASSWORD, "");
    prefs.end();
    
    if (ssid.length() == 0) {
      Serial.println("[WiFi] No saved credentials");
      return false;
    }
    
    Serial.printf("[WiFi] Connecting to \"%s\"...", ssid.c_str());
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    // Wait up to 15 seconds for connection
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf(" Connected!\n");
      Serial.printf("[WiFi] IP address: %s\n", WiFi.localIP().toString().c_str());
      Serial.printf("[WiFi] Signal strength: %d dBm\n", WiFi.RSSI());
      return true;
    } else {
      Serial.println(" Failed!");
      WiFi.disconnect();
      return false;
    }
  }
  
  // =============================================
  // STATIC: Clear saved credentials
  // =============================================
  static void clearCredentials() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.remove(PREF_SSID);
    prefs.remove(PREF_PASSWORD);
    prefs.end();
    Serial.println("[WiFi] Credentials cleared from flash");
  }
  
  // =============================================
  // STATIC: Save WiFi credentials to flash
  // =============================================
  static void saveCredentials(const String& ssid, const String& password) {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putString(PREF_SSID, ssid);
    prefs.putString(PREF_PASSWORD, password);
    prefs.end();
    Serial.println("[WiFi] Credentials saved to flash");
  }
  
  // =============================================
  // RUN PROVISIONING (blocking, reboots when done)
  // =============================================
  // This blocks for up to 5 minutes waiting for BLE credentials.
  // After receiving credentials, it saves them and reboots.
  void runProvisioning() {
    Serial.println("\n[BLE] ========================================");
    Serial.println("[BLE]  FlexPool Bluetooth WiFi Setup");
    Serial.println("[BLE] ========================================");
    Serial.println("[BLE] The ESP32 is now advertising via Bluetooth.");
    Serial.println("[BLE] ");
    Serial.println("[BLE] TO SETUP WIFI:");
    Serial.println("[BLE]   1. Open Chrome on your PC or Android phone");
    Serial.println("[BLE]      (iPhone is NOT supported - Web Bluetooth");
    Serial.println("[BLE]       does not work on iOS/Safari)");
    Serial.println("[BLE]   2. Go to: https://taejoonest.github.io/FlexPool");
    Serial.println("[BLE]   3. Click 'Connect via Bluetooth'");
    Serial.println("[BLE]   4. Select 'FlexPool' from the device list");
    Serial.println("[BLE]   5. Enter your WiFi name and password");
    Serial.println("[BLE] ");
    Serial.println("[BLE] BLE will stay active for 5 minutes.");
    Serial.println("[BLE] ========================================\n");
    
    // Generate device ID from MAC
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char deviceId[7];
    snprintf(deviceId, sizeof(deviceId), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    
    // Initialize BLE
    BLEDevice::init("FlexPool");
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks(this));
    
    // Create service
    BLEService* pService = _pServer->createService(FLEXPOOL_SERVICE_UUID);
    
    // SSID characteristic (read/write)
    _charSSID = pService->createCharacteristic(
      CHAR_SSID_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );
    _charSSID->setValue("");
    
    // Password characteristic (write only)
    _charPass = pService->createCharacteristic(
      CHAR_PASSWORD_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );
    _charPass->setValue("");
    
    // Device ID characteristic (read only) - so the web page can get our ID
    _charDeviceId = pService->createCharacteristic(
      CHAR_DEVICE_ID_UUID,
      BLECharacteristic::PROPERTY_READ
    );
    _charDeviceId->setValue(deviceId);
    
    // Status characteristic (read/notify) - feedback to the web page
    _charStatus = pService->createCharacteristic(
      CHAR_STATUS_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    _charStatus->addDescriptor(new BLE2902());
    _charStatus->setValue("READY");
    
    // Command characteristic (write) - triggers connection attempt
    _charCommand = pService->createCharacteristic(
      CHAR_COMMAND_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );
    _charCommand->setCallbacks(new CommandCallback(this));
    
    // Start service and advertising
    pService->start();
    
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(FLEXPOOL_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.printf("[BLE] Advertising as 'FlexPool' (Device ID: %s)\n", deviceId);
    Serial.println("[BLE] Waiting for connection from Chrome (PC or Android)...\n");
    
    // Wait for credentials (up to 5 minutes)
    unsigned long startTime = millis();
    while (!_credentialsReceived && (millis() - startTime < BLE_TIMEOUT_MS)) {
      delay(100);
      
      // Print periodic reminder every 30 seconds
      static unsigned long lastReminder = 0;
      if (millis() - lastReminder > 30000) {
        unsigned long remaining = (BLE_TIMEOUT_MS - (millis() - startTime)) / 1000;
        Serial.printf("[BLE] Still waiting... (%lu seconds remaining)\n", remaining);
        lastReminder = millis();
      }
    }
    
    // Stop BLE
    BLEDevice::stopAdvertising();
    
    if (_credentialsReceived) {
      Serial.println("\n[BLE] Credentials received! Attempting WiFi connection...");
      
      // Try to connect
      WiFi.mode(WIFI_STA);
      WiFi.begin(_newSSID.c_str(), _newPassword.c_str());
      
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected to \"%s\"!\n", _newSSID.c_str());
        Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
        
        // Notify via BLE
        _charStatus->setValue("CONNECTED");
        _charStatus->notify();
        
        // Also send the GitHub Pages URL with device ID
        Serial.println();
        Serial.println("════════════════════════════════════════════════");
        Serial.println("  WiFi setup complete! Saving and rebooting...");
        Serial.printf( "  Control URL: https://taejoonest.github.io/FlexPool?id=%s\n", deviceId);
        Serial.println("════════════════════════════════════════════════");
        
        delay(2000);  // Give BLE time to send notification
        
        // Save credentials
        saveCredentials(_newSSID, _newPassword);
        
        // Clean up BLE to free memory
        BLEDevice::deinit(true);
        
        delay(500);
        ESP.restart();
      } else {
        Serial.println("\n[WiFi] Connection failed!");
        _charStatus->setValue("FAILED");
        _charStatus->notify();
        
        delay(3000);
        Serial.println("[BLE] Please try again with correct credentials.");
        
        // Clean up and restart provisioning
        BLEDevice::deinit(true);
        delay(500);
        ESP.restart();
      }
    } else {
      // Timeout - no credentials received
      Serial.println("\n[BLE] Timeout - no credentials received in 5 minutes.");
      Serial.println("[BLE] Restarting. Type 'setup' in Serial Monitor to try again.");
      
      BLEDevice::deinit(true);
      delay(500);
      ESP.restart();
    }
  }
};

#endif // BLE_SETUP_H
