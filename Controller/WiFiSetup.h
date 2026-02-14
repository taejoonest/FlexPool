/*
 * =============================================
 * WiFiSetup.h - WiFi Credential Manager
 * =============================================
 * 
 * Manages WiFi credentials without hardcoding them.
 * 
 * FIRST BOOT (no credentials saved):
 *   1. ESP32 creates a temporary hotspot: "FlexPool-Setup"
 *   2. Connect your phone to it
 *   3. Open 192.168.4.1 in your browser
 *   4. Enter your home WiFi name and password
 *   5. ESP32 saves them to flash and reboots
 * 
 * EVERY BOOT AFTER:
 *   1. ESP32 reads saved credentials from flash
 *   2. Connects to your home WiFi in Station mode
 *   3. Done — no setup hotspot, no AP mode
 * 
 * TO RESET (forget saved WiFi):
 *   - Type "reset" in Serial Monitor, OR
 *   - Go to http://flexpool.local/wifi/reset
 */

#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// =============================================
// CONFIGURATION
// =============================================
#define SETUP_AP_SSID     "FlexPool-Setup"   // Hotspot name during setup
#define SETUP_AP_PASSWORD ""                  // No password (open network) for easy setup
#define WIFI_CONNECT_TIMEOUT  20             // Seconds to wait for connection
#define PREFS_NAMESPACE   "flexpool"         // Flash storage namespace
#define PREFS_KEY_SSID    "ssid"
#define PREFS_KEY_PASS    "pass"

// =============================================
// SETUP PORTAL HTML
// =============================================
const char SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>FlexPool WiFi Setup</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #0f172a; color: #e2e8f0;
      display: flex; justify-content: center; align-items: center;
      min-height: 100vh; padding: 20px;
    }
    .card {
      background: #1e293b; border-radius: 20px; padding: 32px;
      width: 100%; max-width: 400px; border: 1px solid #334155;
    }
    h1 { color: #38bdf8; font-size: 1.4rem; text-align: center; margin-bottom: 4px; }
    .subtitle { color: #64748b; text-align: center; font-size: 0.85rem; margin-bottom: 24px; }
    label { display: block; color: #94a3b8; font-size: 0.85rem; margin-bottom: 6px; margin-top: 16px; }
    input[type="text"], input[type="password"] {
      width: 100%; padding: 12px 16px; border-radius: 10px;
      border: 1px solid #475569; background: #0f172a; color: #e2e8f0;
      font-size: 1rem; outline: none;
    }
    input:focus { border-color: #38bdf8; }
    .btn {
      width: 100%; padding: 14px; border: none; border-radius: 12px;
      background: #2563eb; color: white; font-size: 1rem; font-weight: 600;
      cursor: pointer; margin-top: 24px;
    }
    .btn:active { background: #1d4ed8; }
    .scan { margin-top: 16px; }
    .network {
      background: #0f172a; border-radius: 8px; padding: 10px 14px;
      margin-top: 6px; cursor: pointer; border: 1px solid #334155;
      font-size: 0.9rem;
    }
    .network:hover { border-color: #38bdf8; }
    .signal { color: #64748b; font-size: 0.75rem; float: right; }
    .info { color: #64748b; font-size: 0.75rem; text-align: center; margin-top: 16px; }
    .show-pass { color: #64748b; font-size: 0.8rem; cursor: pointer; margin-top: 6px; }
  </style>
</head>
<body>
  <div class="card">
    <h1>FlexPool Setup</h1>
    <p class="subtitle">Connect your pool controller to WiFi</p>
    
    <div class="scan" id="networks">
      <label>Available Networks:</label>
      <div id="netList"><div class="network">Scanning...</div></div>
    </div>

    <form action="/save" method="POST">
      <label for="ssid">WiFi Network Name</label>
      <input type="text" id="ssid" name="ssid" placeholder="Your WiFi name" required>
      
      <label for="pass">WiFi Password</label>
      <input type="password" id="pass" name="pass" placeholder="Your WiFi password">
      <div class="show-pass" onclick="togglePass()">Show password</div>
      
      <button type="submit" class="btn">Connect</button>
    </form>
    
    <p class="info">Credentials are saved to the ESP32's flash memory.<br>
    They are never sent anywhere else.</p>
  </div>

  <script>
    function togglePass() {
      const p = document.getElementById('pass');
      p.type = p.type === 'password' ? 'text' : 'password';
    }
    function pickNetwork(name) {
      document.getElementById('ssid').value = name;
      document.getElementById('pass').focus();
    }
    // Fetch scanned networks
    fetch('/scan').then(r => r.json()).then(nets => {
      const list = document.getElementById('netList');
      if (nets.length === 0) {
        list.innerHTML = '<div class="network">No networks found</div>';
        return;
      }
      list.innerHTML = '';
      nets.forEach(n => {
        const div = document.createElement('div');
        div.className = 'network';
        div.onclick = () => pickNetwork(n.ssid);
        div.innerHTML = n.ssid + '<span class="signal">' + n.rssi + ' dBm</span>';
        list.appendChild(div);
      });
    }).catch(() => {
      document.getElementById('netList').innerHTML = '<div class="network">Scan failed</div>';
    });
  </script>
</body>
</html>
)rawliteral";

const char SAVED_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>FlexPool - Saved</title>
  <style>
    body {
      font-family: -apple-system, sans-serif; background: #0f172a; color: #e2e8f0;
      display: flex; justify-content: center; align-items: center; min-height: 100vh;
    }
    .card {
      background: #1e293b; border-radius: 20px; padding: 40px; text-align: center;
      max-width: 400px; border: 1px solid #334155;
    }
    h1 { color: #22c55e; margin-bottom: 16px; }
    p { color: #94a3b8; line-height: 1.6; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Saved!</h1>
    <p>WiFi credentials saved. The ESP32 will now restart and connect to your network.</p>
    <p style="margin-top:16px; color:#64748b;">
      After it restarts, reconnect your phone to your home WiFi and open:<br>
      <strong style="color:#38bdf8;">http://flexpool.local</strong>
    </p>
  </div>
</body>
</html>
)rawliteral";

// =============================================
// WiFiSetup CLASS
// =============================================
class WiFiSetup {
public:
  // Check if we have saved credentials
  static bool hasSavedCredentials() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);  // read-only
    String ssid = prefs.getString(PREFS_KEY_SSID, "");
    prefs.end();
    return (ssid.length() > 0);
  }
  
  // Get saved SSID
  static String getSavedSSID() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, true);
    String ssid = prefs.getString(PREFS_KEY_SSID, "");
    prefs.end();
    return ssid;
  }
  
  // Try to connect using saved credentials.
  // Returns true if connected, false if failed.
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
      Serial.println("[WiFi] Could not connect with saved credentials.");
      return false;
    }
  }
  
  // Clear saved credentials
  static void clearCredentials() {
    Preferences prefs;
    prefs.begin(PREFS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    Serial.println("[WiFi] Saved credentials cleared.");
  }
  
  // Run the setup portal (blocking - runs until user saves credentials)
  // This starts an AP and web server for configuration.
  static void runSetupPortal() {
    Serial.println("\n=============================================");
    Serial.println("  WIFI SETUP MODE");
    Serial.println("=============================================");
    Serial.printf( "  1. On your phone, connect to WiFi: \"%s\"\n", SETUP_AP_SSID);
    Serial.println("  2. Open http://192.168.4.1 in your browser");
    Serial.println("  3. Select your home WiFi and enter password");
    Serial.println("  4. ESP32 will save and restart automatically");
    Serial.println("=============================================\n");
    
    // Start AP
    WiFi.mode(WIFI_AP);
    WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASSWORD);
    delay(100);
    
    Serial.printf("[WiFi] Setup hotspot active: \"%s\"\n", SETUP_AP_SSID);
    Serial.printf("[WiFi] Setup page: http://%s\n", WiFi.softAPIP().toString().c_str());
    
    // Start temporary web server for setup
    WebServer setupServer(80);
    bool shouldRestart = false;
    
    // Serve setup page
    setupServer.on("/", HTTP_GET, [&setupServer]() {
      setupServer.send_P(200, "text/html", SETUP_HTML);
    });
    
    // WiFi network scan
    setupServer.on("/scan", HTTP_GET, [&setupServer]() {
      int n = WiFi.scanNetworks();
      String json = "[";
      for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
      }
      json += "]";
      setupServer.send(200, "application/json", json);
    });
    
    // Save credentials
    setupServer.on("/save", HTTP_POST, [&setupServer, &shouldRestart]() {
      String ssid = setupServer.arg("ssid");
      String pass = setupServer.arg("pass");
      
      if (ssid.length() == 0) {
        setupServer.send(400, "text/plain", "SSID is required");
        return;
      }
      
      // Save to flash
      Preferences prefs;
      prefs.begin(PREFS_NAMESPACE, false);
      prefs.putString(PREFS_KEY_SSID, ssid);
      prefs.putString(PREFS_KEY_PASS, pass);
      prefs.end();
      
      Serial.printf("[WiFi] Saved credentials for \"%s\"\n", ssid.c_str());
      
      setupServer.send_P(200, "text/html", SAVED_HTML);
      shouldRestart = true;
    });
    
    setupServer.begin();
    
    // Run setup portal until credentials are saved
    unsigned long lastBlink = 0;
    while (!shouldRestart) {
      setupServer.handleClient();
      
      // Periodic reminder in Serial Monitor
      if (millis() - lastBlink > 10000) {
        Serial.printf("[WiFi] Waiting for setup... Connect to \"%s\" and open http://192.168.4.1\n",
                      SETUP_AP_SSID);
        lastBlink = millis();
      }
      delay(10);
    }
    
    // Give the browser time to receive the response
    unsigned long saved = millis();
    while (millis() - saved < 3000) {
      setupServer.handleClient();
      delay(10);
    }
    
    Serial.println("[WiFi] Restarting to connect with new credentials...\n");
    delay(500);
    ESP.restart();
  }
};

#endif // WIFI_SETUP_H
