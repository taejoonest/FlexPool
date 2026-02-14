/*
 * =============================================
 * MQTTHandler.h - Cloud MQTT for Remote Control
 * =============================================
 * 
 * Connects ESP32 to a free public MQTT broker so you can
 * control the pump from ANYWHERE in the world via a web page.
 * 
 * HOW IT WORKS:
 *   ESP32 connects to MQTT broker over WiFi (TCP)
 *   Web page connects to same broker (WebSocket)
 *   Both publish/subscribe to the same "topics"
 *   Messages flow through the broker = remote control!
 * 
 * DEFAULT BROKER: broker.hivemq.com (free, no signup)
 * 
 * TOPICS (using unique device ID from MAC address):
 *   flexpool/{deviceId}/cmd     ← commands TO the ESP32
 *   flexpool/{deviceId}/status  → status FROM the ESP32
 * 
 * REQUIRES: PubSubClient library
 *   Arduino IDE → Sketch → Include Library → Manage Libraries
 *   Search "PubSubClient" by Nick O'Leary → Install
 */

#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <WiFi.h>
#include <PubSubClient.h>

// =============================================
// MQTT BROKER SETTINGS
// =============================================
// HiveMQ public broker - free, no signup required
// Good for testing. For production, use HiveMQ Cloud (free tier)
#define MQTT_BROKER     "broker.hivemq.com"
#define MQTT_PORT       1883           // TCP port (ESP32 uses this)
#define MQTT_WS_PORT    8884           // WebSocket Secure port (browser uses this)

// How often to publish status (milliseconds)
#define MQTT_STATUS_INTERVAL  3000     // Every 3 seconds

// Reconnect interval if disconnected
#define MQTT_RECONNECT_INTERVAL  5000  // Every 5 seconds

// =============================================
// FORWARD DECLARATIONS
// (these functions are defined in Controller.ino)
// =============================================
void sendRemoteControl();
void sendLocalControl();
void sendRunPump(bool start);
void sendStatusQuery();
void sendSetRPM(uint16_t rpm);
void runFullSpeedSequence(uint16_t rpm);
void runFullStopSequence();

// External pump status (defined in Controller.ino)
struct PumpStatus;
extern PumpStatus pumpStatus;
extern bool remoteControlActive;

// =============================================
// MQTTHandler CLASS
// =============================================
class MQTTHandler {
private:
  WiFiClient    _wifiClient;
  PubSubClient  _mqtt;
  
  String _deviceId;
  String _topicCmd;
  String _topicStatus;
  String _topicLWT;
  
  unsigned long _lastStatusPublish = 0;
  unsigned long _lastReconnectAttempt = 0;
  bool _enabled = true;
  
  // Generate unique device ID from MAC address (last 6 chars)
  void generateDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char id[7];
    snprintf(id, sizeof(id), "%02X%02X%02X", mac[3], mac[4], mac[5]);
    _deviceId = String(id);
    
    // Build topic names
    _topicCmd    = "flexpool/" + _deviceId + "/cmd";
    _topicStatus = "flexpool/" + _deviceId + "/status";
    _topicLWT    = "flexpool/" + _deviceId + "/lwt";
  }
  
  // Parse incoming command JSON and execute it
  // Format: {"cmd":"fullstart","rpm":2000}
  void handleCommand(const char* payload, unsigned int length) {
    String msg = String(payload).substring(0, length);
    
    Serial.printf("[MQTT] Received command: %s\n", msg.c_str());
    
    // Simple JSON parsing (no library needed for our simple format)
    if (msg.indexOf("\"fullstart\"") >= 0) {
      int rpm = extractInt(msg, "rpm");
      if (rpm >= 450 && rpm <= 3450) {
        Serial.printf("[MQTT] → Full start at %d RPM\n", rpm);
        runFullSpeedSequence(rpm);
      }
    }
    else if (msg.indexOf("\"fullstop\"") >= 0) {
      Serial.println("[MQTT] → Full stop");
      runFullStopSequence();
    }
    else if (msg.indexOf("\"start\"") >= 0) {
      Serial.println("[MQTT] → Start pump");
      sendRunPump(true);
    }
    else if (msg.indexOf("\"stop\"") >= 0) {
      Serial.println("[MQTT] → Stop pump");
      sendRunPump(false);
    }
    else if (msg.indexOf("\"rpm\"") >= 0) {
      int rpm = extractInt(msg, "value");
      if (rpm >= 450 && rpm <= 3450) {
        Serial.printf("[MQTT] → Set RPM to %d\n", rpm);
        sendSetRPM(rpm);
      }
    }
    else if (msg.indexOf("\"remote\"") >= 0) {
      Serial.println("[MQTT] → Set remote control");
      sendRemoteControl();
    }
    else if (msg.indexOf("\"local\"") >= 0) {
      Serial.println("[MQTT] → Set local control");
      sendLocalControl();
    }
    else if (msg.indexOf("\"query\"") >= 0) {
      Serial.println("[MQTT] → Query status");
      sendStatusQuery();
    }
    else {
      Serial.printf("[MQTT] Unknown command: %s\n", msg.c_str());
    }
    
    // Publish updated status after command
    delay(500);
    publishStatus();
  }
  
  // Extract integer value from simple JSON: "key":1234
  int extractInt(const String& json, const char* key) {
    String search = String("\"") + key + "\"";
    int idx = json.indexOf(search);
    if (idx < 0) return 0;
    
    // Find the colon after the key
    idx = json.indexOf(':', idx);
    if (idx < 0) return 0;
    
    // Skip whitespace
    idx++;
    while (idx < (int)json.length() && json[idx] == ' ') idx++;
    
    // Parse the number
    return json.substring(idx).toInt();
  }

public:
  MQTTHandler() : _mqtt(_wifiClient) {}
  
  // Get the device ID (for display in web UI)
  const String& getDeviceId() const { return _deviceId; }
  const String& getTopicCmd() const { return _topicCmd; }
  const String& getTopicStatus() const { return _topicStatus; }
  bool isConnected() const { return _mqtt.connected(); }
  
  // Initialize MQTT
  void begin() {
    generateDeviceId();
    
    _mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    _mqtt.setBufferSize(512);
    
    // Set callback for incoming messages
    _mqtt.setCallback([this](char* topic, byte* payload, unsigned int length) {
      handleCommand((const char*)payload, length);
    });
    
    Serial.println("\n[MQTT] ================================");
    Serial.printf( "[MQTT] Device ID: %s\n", _deviceId.c_str());
    Serial.printf( "[MQTT] Broker:    %s:%d\n", MQTT_BROKER, MQTT_PORT);
    Serial.printf( "[MQTT] Commands:  %s\n", _topicCmd.c_str());
    Serial.printf( "[MQTT] Status:    %s\n", _topicStatus.c_str());
    Serial.println("[MQTT] ================================\n");
    
    connect();
  }
  
  // Connect to MQTT broker
  bool connect() {
    if (_mqtt.connected()) return true;
    
    String clientId = "flexpool-" + _deviceId;
    
    Serial.printf("[MQTT] Connecting to %s...", MQTT_BROKER);
    
    // Set Last Will and Testament (published if ESP32 disconnects unexpectedly)
    bool connected = _mqtt.connect(
      clientId.c_str(),
      NULL, NULL,                          // no username/password for public broker
      _topicLWT.c_str(), 1, true,          // LWT: topic, QoS 1, retain
      "{\"online\":false}"                 // LWT message
    );
    
    if (connected) {
      Serial.println(" Connected!");
      
      // Publish online status
      _mqtt.publish(_topicLWT.c_str(), "{\"online\":true}", true);
      
      // Subscribe to command topic
      _mqtt.subscribe(_topicCmd.c_str());
      Serial.printf("[MQTT] Subscribed to: %s\n", _topicCmd.c_str());
      
      // Publish initial status
      publishStatus();
      
      return true;
    } else {
      Serial.printf(" Failed (rc=%d)\n", _mqtt.state());
      return false;
    }
  }
  
  // Publish current pump status
  void publishStatus() {
    if (!_mqtt.connected()) return;
    
    char json[384];
    snprintf(json, sizeof(json),
      "{"
      "\"running\":%s,"
      "\"rpm\":%d,"
      "\"watts\":%d,"
      "\"gpm\":%d,"
      "\"mode\":%d,"
      "\"error\":%d,"
      "\"remote\":%s,"
      "\"valid\":%s,"
      "\"deviceId\":\"%s\","
      "\"uptime\":%lu,"
      "\"rssi\":%d"
      "}",
      pumpStatus.running ? "true" : "false",
      pumpStatus.rpm,
      pumpStatus.watts,
      pumpStatus.gpm,
      pumpStatus.mode,
      pumpStatus.errCode,
      remoteControlActive ? "true" : "false",
      pumpStatus.valid ? "true" : "false",
      _deviceId.c_str(),
      millis() / 1000,
      WiFi.RSSI()
    );
    
    _mqtt.publish(_topicStatus.c_str(), json);
  }
  
  // Call this in loop()
  void loop() {
    if (!_enabled) return;
    
    // Handle MQTT messages
    if (_mqtt.connected()) {
      _mqtt.loop();
    }
    
    // Reconnect if disconnected
    if (!_mqtt.connected() && millis() - _lastReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
      _lastReconnectAttempt = millis();
      connect();
    }
    
    // Periodically publish status
    if (_mqtt.connected() && millis() - _lastStatusPublish > MQTT_STATUS_INTERVAL) {
      publishStatus();
      _lastStatusPublish = millis();
    }
  }
};

#endif // MQTT_HANDLER_H
