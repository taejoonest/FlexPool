/*
 * =============================================
 * WebUI.h - Web Interface for FlexPool Controller
 * =============================================
 * 
 * Mobile-friendly HTML control panel.
 * Uses MQTT over WebSocket so it works from ANYWHERE:
 *   - On your home WiFi (local)
 *   - On cellular data (remote)
 *   - From another country (cloud)
 * 
 * MAIN INTERFACE:
 *   Speed 1 - Speed 4 buttons (each sends full start sequence)
 *   Custom RPM slider
 *   Stop button
 *   Status display
 * 
 * The ESP32 serves this page at http://flexpool.local
 * The page connects to the MQTT broker via WebSocket
 * Commands and status flow through MQTT
 */

#ifndef WEBUI_H
#define WEBUI_H

const char WEBUI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>FlexPool Controller</title>
  <script src="https://unpkg.com/mqtt/dist/mqtt.min.js"></script>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #0f172a;
      color: #e2e8f0;
      min-height: 100vh;
      padding: 16px;
    }
    h1 {
      text-align: center;
      font-size: 1.5rem;
      padding: 16px 0 4px;
      color: #38bdf8;
      letter-spacing: 1px;
    }
    h1 span { color: #94a3b8; font-weight: 300; font-size: 0.9rem; display: block; }

    /* Connection banner */
    .conn-bar {
      display: flex; justify-content: center; align-items: center; gap: 8px;
      padding: 8px; margin-bottom: 12px; border-radius: 10px;
      font-size: 0.8rem; text-align: center;
    }
    .conn-bar.connected { background: #052e16; color: #4ade80; }
    .conn-bar.connecting { background: #422006; color: #fbbf24; }
    .conn-bar.disconnected { background: #450a0a; color: #f87171; }
    .conn-dot { width: 8px; height: 8px; border-radius: 50%; }
    .conn-bar.connected .conn-dot { background: #22c55e; box-shadow: 0 0 6px #22c55e; }
    .conn-bar.connecting .conn-dot { background: #f59e0b; box-shadow: 0 0 6px #f59e0b; }
    .conn-bar.disconnected .conn-dot { background: #ef4444; box-shadow: 0 0 6px #ef4444; }

    /* Device ID input (for remote access) */
    .device-section {
      background: #1e293b; border-radius: 12px; padding: 14px;
      margin-bottom: 12px; border: 1px solid #334155;
      display: none;
    }
    .device-section.show { display: block; }
    .device-section label { font-size: 0.8rem; color: #94a3b8; }
    .device-row { display: flex; gap: 8px; margin-top: 6px; }
    .device-row input {
      flex: 1; padding: 10px; border-radius: 8px; border: 1px solid #475569;
      background: #0f172a; color: #e2e8f0; font-size: 1rem;
      font-family: 'Courier New', monospace; letter-spacing: 2px; text-transform: uppercase;
    }
    .device-row button {
      padding: 10px 16px; border-radius: 8px; border: none;
      background: #2563eb; color: white; font-weight: 600; cursor: pointer;
    }

    /* Status card */
    .status-card {
      background: #1e293b; border-radius: 16px; padding: 20px;
      margin-bottom: 16px; border: 1px solid #334155;
    }
    .status-header {
      display: flex; justify-content: space-between; align-items: center;
      margin-bottom: 16px;
    }
    .status-dot {
      width: 12px; height: 12px; border-radius: 50%;
      display: inline-block; margin-right: 8px;
    }
    .dot-running { background: #22c55e; box-shadow: 0 0 8px #22c55e; }
    .dot-stopped { background: #ef4444; box-shadow: 0 0 8px #ef4444; }
    .dot-unknown { background: #f59e0b; box-shadow: 0 0 8px #f59e0b; }
    .status-label { font-size: 1.1rem; font-weight: 600; }
    .status-grid {
      display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px;
    }
    .stat-item {
      background: #0f172a; border-radius: 10px; padding: 12px; text-align: center;
    }
    .stat-value { font-size: 1.6rem; font-weight: 700; color: #38bdf8; }
    .stat-label {
      font-size: 0.75rem; color: #94a3b8;
      text-transform: uppercase; letter-spacing: 1px; margin-top: 2px;
    }

    .section-title {
      font-size: 0.8rem; color: #64748b;
      text-transform: uppercase; letter-spacing: 2px; margin: 20px 0 10px;
    }

    /* ======== SPEED BUTTONS (primary interface) ======== */
    .speed-section {
      background: #1e293b; border-radius: 16px; padding: 20px;
      margin-bottom: 16px; border: 1px solid #334155;
    }
    .speed-grid {
      display: grid; grid-template-columns: repeat(2, 1fr); gap: 12px;
    }
    .speed-btn {
      border: none; border-radius: 14px; padding: 20px 8px;
      cursor: pointer; transition: all 0.15s; color: white;
      text-align: center; position: relative; overflow: hidden;
    }
    .speed-btn:active { transform: scale(0.96); }
    .speed-btn.active { box-shadow: 0 0 0 3px #38bdf8, 0 0 20px rgba(56,189,248,0.3); }
    .speed-name {
      font-size: 1.2rem; font-weight: 700; display: block;
    }
    .speed-rpm {
      font-size: 0.85rem; opacity: 0.85; margin-top: 4px; display: block;
    }
    .speed-watts {
      font-size: 0.7rem; opacity: 0.6; margin-top: 2px; display: block;
    }
    .speed-1 { background: linear-gradient(135deg, #0891b2, #06b6d4); }
    .speed-2 { background: linear-gradient(135deg, #2563eb, #3b82f6); }
    .speed-3 { background: linear-gradient(135deg, #7c3aed, #8b5cf6); }
    .speed-4 { background: linear-gradient(135deg, #c2410c, #ea580c); }

    /* STOP button */
    .stop-section { margin-bottom: 16px; }
    .btn-bigstop {
      width: 100%; border: none; border-radius: 14px; padding: 18px;
      font-size: 1.15rem; font-weight: 700; cursor: pointer;
      background: linear-gradient(135deg, #dc2626, #b91c1c);
      color: white; transition: all 0.15s; letter-spacing: 1px;
    }
    .btn-bigstop:active { transform: scale(0.97); }

    /* RPM custom slider */
    .rpm-section {
      background: #1e293b; border-radius: 16px; padding: 20px;
      margin-bottom: 16px; border: 1px solid #334155;
    }
    .rpm-display {
      text-align: center; font-size: 2.5rem; font-weight: 700;
      color: #38bdf8; margin: 8px 0;
    }
    .rpm-display span { font-size: 1rem; color: #94a3b8; }
    input[type="range"] {
      width: 100%; height: 8px; -webkit-appearance: none;
      background: #334155; border-radius: 4px; outline: none; margin: 12px 0;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none; width: 28px; height: 28px;
      border-radius: 50%; background: #38bdf8; cursor: pointer;
    }
    .rpm-presets {
      display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; margin-top: 10px;
    }
    .rpm-preset {
      border: 1px solid #475569; background: transparent; color: #e2e8f0;
      border-radius: 8px; padding: 8px 4px; font-size: 0.8rem; cursor: pointer;
    }
    .rpm-preset:active { background: #334155; }
    .btn-setcustom {
      width: 100%; margin-top: 12px; border: none; border-radius: 12px;
      padding: 14px; font-size: 1rem; font-weight: 600; cursor: pointer;
      background: linear-gradient(135deg, #16a34a, #0891b2); color: white;
    }
    .btn-setcustom:active { transform: scale(0.97); }

    /* Buttons */
    .btn-row { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin-bottom: 10px; }
    .btn {
      border: none; border-radius: 12px; padding: 14px 8px; font-size: 0.95rem;
      font-weight: 600; cursor: pointer; transition: all 0.15s; color: white; text-align: center;
    }
    .btn:active { transform: scale(0.96); }
    .btn-start  { background: #16a34a; }
    .btn-stop   { background: #dc2626; }
    .btn-remote { background: #2563eb; }
    .btn-local  { background: #7c3aed; }
    .btn-query  { background: #0891b2; }
    .btn:disabled { opacity: 0.4; cursor: not-allowed; }

    /* Collapsible advanced section */
    .advanced-toggle {
      width: 100%; background: #1e293b; border: 1px solid #334155;
      border-radius: 12px; padding: 12px; color: #64748b;
      font-size: 0.85rem; cursor: pointer; text-align: center;
      margin-bottom: 12px;
    }
    .advanced-toggle:active { background: #334155; }
    .advanced-panel { display: none; }
    .advanced-panel.show { display: block; }

    /* Settings section */
    .settings-section {
      background: #1e293b; border-radius: 16px; padding: 20px;
      margin-bottom: 16px; border: 1px solid #334155;
    }
    .setting-row {
      display: flex; align-items: center; justify-content: space-between;
      padding: 10px 0; border-bottom: 1px solid #334155;
    }
    .setting-row:last-child { border-bottom: none; }
    .setting-label { font-size: 0.9rem; color: #94a3b8; }
    .setting-input {
      width: 90px; padding: 8px; border-radius: 8px; border: 1px solid #475569;
      background: #0f172a; color: #38bdf8; font-size: 1rem;
      text-align: center; font-weight: 600;
    }
    .btn-save-settings {
      width: 100%; margin-top: 12px; border: none; border-radius: 12px;
      padding: 12px; font-size: 0.9rem; font-weight: 600; cursor: pointer;
      background: #475569; color: white;
    }

    /* Log */
    .log-section {
      background: #1e293b; border-radius: 16px; padding: 16px;
      margin-top: 16px; border: 1px solid #334155;
    }
    .log-box {
      background: #0f172a; border-radius: 8px; padding: 10px;
      max-height: 200px; overflow-y: auto;
      font-family: 'Courier New', monospace; font-size: 0.75rem;
      color: #94a3b8; line-height: 1.5;
    }
    .log-entry { border-bottom: 1px solid #1e293b; padding: 2px 0; }
    .log-ok { color: #22c55e; }
    .log-err { color: #ef4444; }

    .toast {
      position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%);
      background: #334155; color: #e2e8f0; padding: 12px 24px;
      border-radius: 12px; font-size: 0.9rem; opacity: 0;
      transition: opacity 0.3s; pointer-events: none; z-index: 100;
    }
    .toast.show { opacity: 1; }
    .info-bar {
      text-align: center; color: #475569; font-size: 0.7rem; margin-top: 12px; line-height: 1.6;
    }
  </style>
</head>
<body>
  <h1>FlexPool <span>Pentair Pump Controller</span></h1>

  <!-- MQTT CONNECTION STATUS -->
  <div class="conn-bar disconnected" id="connBar">
    <div class="conn-dot"></div>
    <span id="connLabel">Connecting to cloud...</span>
  </div>

  <!-- DEVICE ID (shown when opened remotely without an embedded ID) -->
  <div class="device-section" id="deviceSection">
    <label>Enter your Device ID to connect remotely:</label>
    <div class="device-row">
      <input type="text" id="deviceInput" placeholder="e.g. A1B2C3" maxlength="6">
      <button onclick="manualConnect()">Connect</button>
    </div>
  </div>

  <!-- STATUS CARD -->
  <div class="status-card">
    <div class="status-header">
      <div>
        <span class="status-dot dot-unknown" id="statusDot"></span>
        <span class="status-label" id="statusLabel">Waiting for data...</span>
      </div>
      <button class="btn btn-query" style="padding:8px 16px; font-size:0.8rem;" onclick="sendCmd('query')">Refresh</button>
    </div>
    <div class="status-grid">
      <div class="stat-item">
        <div class="stat-value" id="rpmVal">--</div>
        <div class="stat-label">RPM</div>
      </div>
      <div class="stat-item">
        <div class="stat-value" id="wattsVal">--</div>
        <div class="stat-label">Watts</div>
      </div>
      <div class="stat-item">
        <div class="stat-value" id="gpmVal">--</div>
        <div class="stat-label">GPM</div>
      </div>
      <div class="stat-item">
        <div class="stat-value" id="modeVal">--</div>
        <div class="stat-label">Mode</div>
      </div>
    </div>
  </div>

  <!-- ======== SPEED BUTTONS (PRIMARY INTERFACE) ======== -->
  <div class="speed-section">
    <div class="section-title" style="margin-top:0">Select Speed</div>
    <div class="speed-grid">
      <button class="speed-btn speed-1" id="speedBtn1" onclick="setSpeed(1)">
        <span class="speed-name">Speed 1</span>
        <span class="speed-rpm" id="speedLabel1">750 RPM</span>
      </button>
      <button class="speed-btn speed-2" id="speedBtn2" onclick="setSpeed(2)">
        <span class="speed-name">Speed 2</span>
        <span class="speed-rpm" id="speedLabel2">1500 RPM</span>
      </button>
      <button class="speed-btn speed-3" id="speedBtn3" onclick="setSpeed(3)">
        <span class="speed-name">Speed 3</span>
        <span class="speed-rpm" id="speedLabel3">2350 RPM</span>
      </button>
      <button class="speed-btn speed-4" id="speedBtn4" onclick="setSpeed(4)">
        <span class="speed-name">Speed 4</span>
        <span class="speed-rpm" id="speedLabel4">3110 RPM</span>
      </button>
    </div>
  </div>

  <!-- STOP BUTTON -->
  <div class="stop-section">
    <button class="btn-bigstop" onclick="sendCmd('fullstop')">STOP PUMP</button>
  </div>

  <!-- CUSTOM RPM -->
  <div class="rpm-section">
    <div class="section-title" style="margin-top:0">Custom RPM</div>
    <div class="rpm-display"><span id="rpmTarget">1000</span> <span>RPM</span></div>
    <input type="range" id="rpmSlider" min="450" max="3450" step="50" value="1000"
           oninput="document.getElementById('rpmTarget').textContent=this.value">
    <div class="rpm-presets">
      <button class="rpm-preset" onclick="setSlider(600)">600</button>
      <button class="rpm-preset" onclick="setSlider(1100)">1100</button>
      <button class="rpm-preset" onclick="setSlider(2000)">2000</button>
      <button class="rpm-preset" onclick="setSlider(3000)">3000</button>
    </div>
    <button class="btn-setcustom" onclick="fullStartCustom()">Start at Custom RPM</button>
  </div>

  <!-- ADVANCED COMMANDS (collapsible) -->
  <button class="advanced-toggle" onclick="toggleAdvanced()">
    ▼ Advanced / Manual Commands
  </button>
  <div class="advanced-panel" id="advancedPanel">

    <!-- Speed Settings (configurable RPM for each speed) -->
    <div class="settings-section">
      <div class="section-title" style="margin-top:0">Speed Settings (RPM)</div>
      <div class="setting-row">
        <span class="setting-label">Speed 1</span>
        <input type="number" class="setting-input" id="cfg1" min="450" max="3450" step="50" value="750">
      </div>
      <div class="setting-row">
        <span class="setting-label">Speed 2</span>
        <input type="number" class="setting-input" id="cfg2" min="450" max="3450" step="50" value="1500">
      </div>
      <div class="setting-row">
        <span class="setting-label">Speed 3</span>
        <input type="number" class="setting-input" id="cfg3" min="450" max="3450" step="50" value="2350">
      </div>
      <div class="setting-row">
        <span class="setting-label">Speed 4</span>
        <input type="number" class="setting-input" id="cfg4" min="450" max="3450" step="50" value="3110">
      </div>
      <button class="btn-save-settings" onclick="saveSettings()">Save Speed Settings</button>
    </div>

    <!-- Manual Commands -->
    <div class="section-title">Manual Commands</div>
    <div class="btn-row">
      <button class="btn btn-start"  onclick="sendCmd('start')">Start Motor</button>
      <button class="btn btn-stop"   onclick="sendCmd('stop')">Stop Motor</button>
    </div>
    <div class="btn-row">
      <button class="btn btn-remote" onclick="sendCmd('remote')">Remote Control</button>
      <button class="btn btn-local"  onclick="sendCmd('local')">Local Control</button>
    </div>
    <div class="btn-row">
      <button class="btn btn-query" onclick="sendCmd('query')">Query Status</button>
      <button class="btn" style="background:#475569" onclick="setRPMOnly()">Set RPM Only</button>
    </div>
  </div>

  <!-- LOG -->
  <div class="log-section">
    <div class="section-title" style="margin-top:0">Activity Log</div>
    <div class="log-box" id="logBox"></div>
  </div>

  <div class="info-bar" id="infoBar"></div>
  <div class="toast" id="toast"></div>

  <script>
    // =============================================
    // SPEED PRESETS (RPM values for Speed 1-4)
    // Saved in browser localStorage so they persist
    // =============================================
    const DEFAULT_SPEEDS = [750, 1500, 2350, 3110];
    let speeds = [...DEFAULT_SPEEDS];
    let activeSpeed = 0;  // 0 = none, 1-4 = active speed

    // Load saved speeds from localStorage
    function loadSettings() {
      const saved = localStorage.getItem('flexpool_speeds');
      if (saved) {
        try {
          const arr = JSON.parse(saved);
          if (arr.length === 4) speeds = arr;
        } catch(e) {}
      }
      // Update UI
      for (let i = 1; i <= 4; i++) {
        document.getElementById('speedLabel' + i).textContent = speeds[i-1] + ' RPM';
        document.getElementById('cfg' + i).value = speeds[i-1];
      }
    }

    function saveSettings() {
      for (let i = 1; i <= 4; i++) {
        const val = parseInt(document.getElementById('cfg' + i).value);
        if (val >= 450 && val <= 3450) speeds[i-1] = val;
      }
      localStorage.setItem('flexpool_speeds', JSON.stringify(speeds));
      loadSettings();
      showToast('Speed settings saved!');
      addLog('Speed settings saved: ' + speeds.join(', ') + ' RPM', true);
    }

    // =============================================
    // MQTT CONFIGURATION
    // =============================================
    const MQTT_BROKER = 'wss://broker.hivemq.com:8884/mqtt';

    // Device ID: injected by ESP32 when served locally, or entered manually
    let DEVICE_ID = '%%DEVICE_ID%%';
    let mqttClient = null;
    let topicCmd = '';
    let topicStatus = '';

    // =============================================
    // INITIALIZATION
    // =============================================
    window.addEventListener('load', () => {
      loadSettings();

      // Check if device ID was injected by ESP32 (local access)
      if (DEVICE_ID && DEVICE_ID !== '%%' + 'DEVICE_ID' + '%%') {
        addLog('Device ID: ' + DEVICE_ID, null);
        connectMQTT(DEVICE_ID);
      } else {
        // No device ID — check localStorage
        const saved = localStorage.getItem('flexpool_device_id');
        if (saved) {
          DEVICE_ID = saved;
          addLog('Using saved Device ID: ' + DEVICE_ID, null);
          connectMQTT(DEVICE_ID);
        } else {
          document.getElementById('deviceSection').classList.add('show');
          setConnStatus('disconnected', 'Enter Device ID to connect');
        }
      }
    });

    function manualConnect() {
      const id = document.getElementById('deviceInput').value.trim().toUpperCase();
      if (id.length < 4) {
        showToast('Enter a valid Device ID (shown in Serial Monitor)');
        return;
      }
      DEVICE_ID = id;
      localStorage.setItem('flexpool_device_id', id);
      document.getElementById('deviceSection').classList.remove('show');
      connectMQTT(id);
    }

    // =============================================
    // MQTT CONNECTION
    // =============================================
    function connectMQTT(deviceId) {
      topicCmd    = 'flexpool/' + deviceId + '/cmd';
      topicStatus = 'flexpool/' + deviceId + '/status';
      const topicLWT = 'flexpool/' + deviceId + '/lwt';

      setConnStatus('connecting', 'Connecting to cloud...');
      addLog('Connecting to MQTT broker...', null);

      const clientId = 'flexpool-web-' + Math.random().toString(16).substr(2, 6);

      mqttClient = mqtt.connect(MQTT_BROKER, {
        clientId: clientId,
        clean: true,
        connectTimeout: 10000,
        reconnectPeriod: 3000,
      });

      mqttClient.on('connect', () => {
        setConnStatus('connected', 'Connected — Device: ' + deviceId);
        addLog('Connected to MQTT broker', true);
        mqttClient.subscribe(topicStatus);
        mqttClient.subscribe(topicLWT);
        addLog('Listening for pump status...', null);
      });

      mqttClient.on('message', (topic, message) => {
        try {
          const data = JSON.parse(message.toString());
          if (topic === topicStatus) {
            updateStatus(data);
          } else if (topic === topicLWT) {
            if (data.online === false) {
              setConnStatus('disconnected', 'ESP32 is offline');
              addLog('ESP32 went offline', false);
            }
          }
        } catch (e) {
          addLog('Bad message: ' + message.toString(), false);
        }
      });

      mqttClient.on('error', (err) => {
        setConnStatus('disconnected', 'Connection error');
        addLog('MQTT error: ' + err.message, false);
      });

      mqttClient.on('close', () => {
        setConnStatus('connecting', 'Reconnecting...');
      });

      mqttClient.on('reconnect', () => {
        setConnStatus('connecting', 'Reconnecting...');
      });
    }

    // =============================================
    // SEND COMMANDS VIA MQTT
    // =============================================
    function sendCmd(cmd, extra) {
      if (!mqttClient || !mqttClient.connected) {
        showToast('Not connected');
        addLog('Cannot send — not connected', false);
        return;
      }

      let payload = { cmd: cmd };
      if (extra) Object.assign(payload, extra);

      const msg = JSON.stringify(payload);
      mqttClient.publish(topicCmd, msg);
      addLog('Sent: ' + msg, true);
      showToast('Command sent: ' + cmd);
    }

    // =============================================
    // SPEED 1-4 BUTTONS
    // =============================================
    function setSpeed(num) {
      const rpm = speeds[num - 1];
      activeSpeed = num;
      highlightSpeed(num);
      sendCmd('fullstart', { rpm: rpm });
      addLog('Speed ' + num + ' selected → ' + rpm + ' RPM', true);
    }

    function highlightSpeed(num) {
      for (let i = 1; i <= 4; i++) {
        document.getElementById('speedBtn' + i).classList.remove('active');
      }
      if (num > 0) {
        document.getElementById('speedBtn' + num).classList.add('active');
      }
    }

    // =============================================
    // CUSTOM RPM
    // =============================================
    function fullStartCustom() {
      const rpm = parseInt(document.getElementById('rpmSlider').value);
      activeSpeed = 0;
      highlightSpeed(0);
      sendCmd('fullstart', { rpm: rpm });
    }

    function setRPMOnly() {
      const rpm = parseInt(document.getElementById('rpmSlider').value);
      sendCmd('rpm', { value: rpm });
    }

    // =============================================
    // UPDATE STATUS DISPLAY
    // =============================================
    function updateStatus(s) {
      const dot = document.getElementById('statusDot');
      const label = document.getElementById('statusLabel');

      if (s.running) {
        dot.className = 'status-dot dot-running';
        label.textContent = 'Running';
      } else {
        dot.className = 'status-dot dot-stopped';
        label.textContent = 'Stopped';
        activeSpeed = 0;
        highlightSpeed(0);
      }

      document.getElementById('rpmVal').textContent = s.rpm >= 0 ? s.rpm : '--';
      document.getElementById('wattsVal').textContent = s.watts >= 0 ? s.watts : '--';
      document.getElementById('gpmVal').textContent = s.gpm >= 0 ? s.gpm : '--';

      const modes = ['Filter','Manual','Speed 1','Speed 2','Speed 3','Speed 4'];
      document.getElementById('modeVal').textContent = modes[s.mode] || 'Mode ' + s.mode;

      // Auto-detect which speed button matches current RPM
      if (s.running && s.rpm > 0) {
        let matched = 0;
        for (let i = 0; i < 4; i++) {
          if (Math.abs(s.rpm - speeds[i]) < 30) { matched = i + 1; break; }
        }
        if (matched > 0 && activeSpeed !== matched) {
          activeSpeed = matched;
          highlightSpeed(matched);
        }
      }

      // Update info bar
      const info = document.getElementById('infoBar');
      info.textContent = 'Device: ' + (s.deviceId || DEVICE_ID) +
        ' | RSSI: ' + (s.rssi || '?') + ' dBm' +
        ' | Uptime: ' + formatUptime(s.uptime || 0);
    }

    function formatUptime(sec) {
      if (sec < 60) return sec + 's';
      if (sec < 3600) return Math.floor(sec/60) + 'm';
      return Math.floor(sec/3600) + 'h ' + Math.floor((sec%3600)/60) + 'm';
    }

    // =============================================
    // UI HELPERS
    // =============================================
    function setSlider(v) {
      document.getElementById('rpmSlider').value = v;
      document.getElementById('rpmTarget').textContent = v;
    }

    function toggleAdvanced() {
      const panel = document.getElementById('advancedPanel');
      const btn = panel.previousElementSibling;
      panel.classList.toggle('show');
      btn.textContent = panel.classList.contains('show')
        ? '▲ Advanced / Manual Commands'
        : '▼ Advanced / Manual Commands';
    }

    function setConnStatus(state, text) {
      const bar = document.getElementById('connBar');
      bar.className = 'conn-bar ' + state;
      document.getElementById('connLabel').textContent = text;
    }

    function showToast(msg) {
      const t = document.getElementById('toast');
      t.textContent = msg;
      t.classList.add('show');
      setTimeout(() => t.classList.remove('show'), 2000);
    }

    function addLog(msg, ok) {
      const box = document.getElementById('logBox');
      const cls = ok === true ? 'log-ok' : (ok === false ? 'log-err' : '');
      const time = new Date().toLocaleTimeString();
      box.innerHTML = '<div class="log-entry ' + cls + '">[' + time + '] ' + msg + '</div>' + box.innerHTML;
      if (box.children.length > 50) box.removeChild(box.lastChild);
    }
  </script>
</body>
</html>
)rawliteral";

#endif // WEBUI_H
