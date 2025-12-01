#include <Wire.h>             // I2C communication
#include <Adafruit_GFX.h>     // Core graphics library
#include <Adafruit_SSD1306.h> // OLED control
#include <Adafruit_AHTX0.h>   // AHT20 Temp/Humidity Sensor
#include <Adafruit_BMP280.h>  // BMP280 Pressure Sensor
#include <WiFi.h>             // Standard ESP32 Wi-Fi library
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h> // Asynchronous Web Server for speed and stability
#include <Preferences.h>      // Non-Volatile Storage (NVS) for saving config
#include <ArduinoOTA.h>       // Over-The-Air Updates
#include <ArduinoJson.h>       // For robust JSON handling

// --- DEVELOPMENT & AI FLAGS ---
// Set this to 1 to enable a special mode for collecting touch sensor data for ML model training.
// The device will only print touch data to the Serial Monitor and will not run the main Mochi application.
#define DATA_COLLECTION_MODE 0

// Set this to 1 when you have an I2S microphone connected and are ready to work on Phase 3.
#define ENABLE_VOICE_RECOGNITION 0

// --- WIFI & NVS CONFIGURATION ---
const char* AP_SSID = "Smart-Nav-Mitra-Setup";
const char* AP_PASS = "mochisetup";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress NET_MASK(255, 255, 255, 0);

// NVS Keys
const char* PREFS_NAMESPACE = "nav_mitra_cfg";
const char* KEY_SSID = "wifi_ssid";
const char* KEY_PASS = "wifi_pass";
const char* KEY_DEV_NAME = "dev_name";
const char* KEY_TEMP_HIGH = "temp_high";
const char* KEY_TEMP_LOW = "temp_low";
const char* KEY_BUZZER_EN = "buzzer_en";
const char* KEY_TZ_OFFSET = "tz_offset";
const char* KEY_OLED_TO = "oled_to";
const char* KEY_QUIET_START = "quiet_start";
const char* KEY_QUIET_END = "quiet_end";
const char* KEY_ALARM_EN = "alarm_en";
const char* KEY_ALARM_HR = "alarm_hr";
const char* KEY_ALARM_MIN = "alarm_min";

// Defaults and In-Memory Storage
String deviceName = "nav-mitra";
String staSsid = "";
String staPass = "";

// --- HARDWARE & THRESHOLD CONFIGURATION ---
#define OLED_RESET -1       // Reset pin
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define I2C_SDA_PIN 8       // GPIO 8 for SDA (Adjust based on your ESP32 board)
#define I2C_SCL_PIN 9       // GPIO 9 for SCL (Adjust based on your ESP32 board)
#define TOUCH_PIN 7         // GPIO 7 for Touch Input
#define BUZZER_PIN 6        // GPIO 6 for Active Buzzer

// Environment Thresholds
float tempAlertHigh = 30.0; // Celsius
float tempAlertLow = 18.0;  // Celsius

// --- CUSTOMIZABLE SETTINGS (with defaults) ---
bool buzzerEnabled = true;
uint16_t oledTimeoutMins = 10; // 0 = always on
long gmtOffset_sec = 0;
uint8_t quietHourStart = 22; // 10 PM
uint8_t quietHourEnd = 7;    // 7 AM
bool alarmEnabled = false;
uint8_t alarmHour = 7;
uint8_t alarmMinute = 30;
bool alarmHasTriggeredToday = false;

// --- OBJECT INSTANCES ---
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp; // I2C
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences preferences;

// --- TIME CONFIGURATION ---
const char* ntpServer = "pool.ntp.org";
const int   daylightOffset_sec = 3600;

#if ENABLE_VOICE_RECOGNITION
// --- PHASE 3: VOICE RECOGNITION HARDWARE (PLACEHOLDER) ---
// #include <driver/i2s.h>
// #define I2S_MIC_WS    1 // Example Pin
// #define I2S_MIC_SD    2 // Example Pin
// #define I2S_MIC_SCK   3 // Example Pin
#endif

// --- STATE MANAGEMENT ---
enum MochiState { HAPPY, ALERT_HIGH, ALERT_LOW, TOUCHED, UPDATING };
MochiState currentState = HAPPY;
MochiState lastState = HAPPY; // To track state changes for display updates

unsigned long lastSensorReadTime = 0;
unsigned long lastMinuteCheck = 0; // For checking the alarm once per minute
const long sensorInterval = 5000; // Read sensors every 5 seconds
unsigned long touchTimer = 0;
const long touchDisplayDuration = 2000; // Show TOUCHED state for 2 seconds
unsigned long lastActivityTime = 0;
bool isDisplayOff = false;

// Sensor Readings (Global for easy access)
float tempC = 0.0;
float humidity = 0.0;
float pressure_hPa = 0.0;

// --- HISTORICAL DATA FOR CHARTING ---
struct DataPoint {
  unsigned long time;
  float temp;
  float humidity;
};
const int DATA_HISTORY_SIZE = 60; // Store 60 points (e.g., 5 minutes of data at 5s intervals)
DataPoint dataHistory[DATA_HISTORY_SIZE];
int historyIndex = 0;

// --- FUNCTION PROTOTYPES ---
void loadConfig();
void saveConfig(const String& ssid, const String& pass, const String& name);
void saveSettings(float high, float low, bool buzzer, long tz, uint16_t timeout, uint8_t qStart, uint8_t qEnd, bool almEn, uint8_t almHr, uint8_t almMin);
void setupOTA();
bool connectToWiFi();
void startCaptivePortal();
void handleRoot(AsyncWebServerRequest *request);
void handleData(AsyncWebServerRequest *request);
void handleHistory(AsyncWebServerRequest *request);
void handleConfig(AsyncWebServerRequest *request);
void handleSaveConfig(AsyncWebServerRequest *request);
void handleSettings(AsyncWebServerRequest *request);
void handleSaveSettings(AsyncWebServerRequest *request);
void handleReboot(AsyncWebServerRequest *request);
void drawMochiFace(MochiState state);
void handleUpdate(AsyncWebServerRequest *request);
void handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUpdateSuccess(AsyncWebServerRequest *request);
bool isQuietHours();
void checkAlarm();
void readSensors();
void checkTouchAndEnvironment();
void buzzAlert(int durationMs);

#if DATA_COLLECTION_MODE == 0 // This wraps the main application logic

// ------------------------------------
// 1. EMBEDDED HTML/CSS/JS FOR CONFIGURATION PORTAL
// ------------------------------------
const char* CONFIG_HTML = R"raw(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart-Nav-Mitra Configuration</title>
    <style>
        :root {
            --primary: #FF69B4; /* Hot Pink (Mochi color) */
            --secondary: #6A5ACD; /* Slate Blue */
            --bg: #F0F4F8; /* Light Blue/Gray */
            --card-bg: #FFFFFF;
            --text-color: #333;
        }
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: var(--bg);
            color: var(--text-color);
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        .container {
            background: var(--card-bg);
            padding: 30px;
            border-radius: 16px;
            box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1);
            width: 100%;
            max-width: 400px;
        }
        h1 {
            color: var(--primary);
            text-align: center;
            margin-bottom: 20px;
            font-size: 1.8em;
        }
        .mochi-face {
            text-align: center;
            font-size: 3rem;
            margin-bottom: 20px;
            animation: pulse 1.5s infinite;
        }
        @keyframes pulse {
            0% { transform: scale(1); opacity: 0.8; }
            50% { transform: scale(1.1); opacity: 1; }
            100% { transform: scale(1); opacity: 0.8; }
        }
        label {
            display: block;
            margin-bottom: 8px;
            font-weight: bold;
            color: var(--secondary);
        }
        input[type="text"], input[type="password"] {
            width: 100%;
            padding: 12px;
            margin-bottom: 15px;
            border: 2px solid #ddd;
            border-radius: 8px;
            box-sizing: border-box;
            transition: border-color 0.3s;
        }
        input[type="text"]:focus, input[type="password"]:focus {
            border-color: var(--primary);
            outline: none;
        }
        button {
            width: 100%;
            padding: 12px;
            background-color: var(--primary);
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 1.1em;
            cursor: pointer;
            transition: background-color 0.3s, transform 0.1s;
        }
        button:hover {
            background-color: #E05AA0;
        }
        button:active {
            transform: scale(0.99);
        }
        p.note {
            margin-top: 20px;
            font-size: 0.9em;
            color: #666;
            text-align: center;
            border-top: 1px dashed #ddd;
            padding-top: 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Smart-Nav-Mitra Configuration Portal</h1>
        <div class="mochi-face">🍥</div>
        <form action="/saveconfig" method="post">
            <label for="devicename">Device Name (mDNS: [name].local)</label>
            <input type="text" id="devicename" name="devicename" value="%DEVICENAME%" required>

            <label for="ssid">Wi-Fi SSID</label>
            <input type="text" id="ssid" name="ssid" required>

            <label for="password">Wi-Fi Password</label>
            <input type="password" id="password" name="password">

            <button type="submit">Connect & Save</button>
        </form>
        <p class="note">Once saved, Smart-Nav-Mitra will reboot and try to connect to your network.</p>
    </div>
</body>
</html>
)raw";

// ------------------------------------
// 2. EMBEDDED HTML/CSS/JS FOR MAIN DEVICE INTERFACE
// ------------------------------------
const char* MAIN_HTML = R"raw(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>%DEVICENAME% Interface</title>
    <style>
        :root {
            --primary: #FF69B4; /* Hot Pink */
            --secondary: #6A5ACD; /* Slate Blue */
            --bg: #F0F4F8;
            --card-bg: #FFFFFF;
            --text-color: #333;
            --success: #32CD32; /* Lime Green */
            --danger: #FF4500; /* Orange Red */
            --cold: #00BFFF; /* Deep Sky Blue */
            --warning: #FFA500; /* Orange */
        }
        .top-right-info {
            position: absolute;
            top: 20px;
            right: 20px;
            text-align: right;
            color: #555;
        }
        body {
            font-family: 'Inter', sans-serif;
            margin: 0;
            padding: 20px;
            background-color: var(--bg);
            color: var(--text-color);
            display: flex;
            flex-direction: column;
            align-items: center;
            min-height: 100vh;
        }
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        h1 {
            color: var(--primary);
            font-size: 2.5em;
            margin-bottom: 5px;
        }
        .mochi-display {
            display: flex;
            flex-direction: column;
            align-items: center;
            margin-bottom: 40px;
            padding: 20px;
            background: var(--card-bg);
            border-radius: 16px;
            box-shadow: 0 8px 20px rgba(0, 0, 0, 0.1);
            width: 100%;
            max-width: 600px;
            transition: background-color 0.5s;
        }
        .mochi-face {
            font-size: 8rem;
            animation: breathe 4s ease-in-out infinite;
        }
        @keyframes breathe {
            0%, 100% { transform: scale(1); }
            50% { transform: scale(1.05); }
        }
        .emotion-text {
            font-size: 1.5em;
            font-weight: bold;
            margin-top: 10px;
        }
        .info-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
            gap: 20px;
            width: 100%;
            max-width: 1000px;
        }
        .card {
            background: var(--card-bg);
            padding: 20px;
            border-radius: 12px;
            box-shadow: 0 4px 15px rgba(0, 0, 0, 0.05);
            transition: transform 0.2s;
        }
        .card:hover {
            transform: translateY(-3px);
        }
        .card h2 {
            margin-top: 0;
            font-size: 1.3em;
            color: var(--primary);
            border-bottom: 2px solid var(--bg);
            padding-bottom: 8px;
            margin-bottom: 15px;
        }
        .card p {
            margin: 5px 0;
            line-height: 1.5;
        }
        .status-badge {
            display: inline-block;
            padding: 4px 10px;
            border-radius: 6px;
            font-size: 0.9em;
            font-weight: bold;
            margin-left: 10px;
        }
        .status-badge.online {
            background-color: var(--success);
            color: white;
        }
        .actions {
            display: flex;
            gap: 10px;
            margin-top: 20px;
        }
        .action-btn {
            padding: 8px 16px;
            border: none;
            border-radius: 8px;
            color: white;
            font-weight: bold;
            cursor: pointer;
            transition: opacity 0.2s;
        }
        .action-btn:hover { opacity: 0.85; }
        .btn-settings { background-color: var(--secondary); }
        .btn-reboot { background-color: var(--warning); }

        @media (max-width: 650px) {
            .info-grid {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>Hello.. %GREETING%</h1>
        <h2>I'm %DEVICENAME%!</h2>
        <p style="margin-top: -15px;">Your friendly companion is online and connected.</p>
    </div>
    <div class="top-right-info">
        <p id="live-datetime" style="margin:0; font-weight: bold;"></p>
    </div>

    <div id="mochi-display" class="mochi-display">
        <div id="mochi-face" class="mochi-face">😊</div>
        <div id="emotion-text" class="emotion-text" style="color: var(--secondary);">Happy and Ready!</div>
    </div>

    <div class="info-grid">
        <!-- PARAMETER CARD 1: Sensor Data -->
        <div class="card parameter-card">
            <h2>Environment State</h2>
            <p><strong>Temperature:</strong> <span id="temp">%TEMP_C%</span> °C</p>
            <p><strong>Humidity:</strong> <span id="humidity">%HUMIDITY%</span> %</p>
            <p><strong>Pressure:</strong> <span id="pressure">%PRESSURE%</span> hPa</p>
        </div>

        <!-- PARAMETER CARD 2: System Status -->
        <div class="card parameter-card">
            <h2>System Health</h2>
            <p><strong>Current State:</strong> <span id="current-state">%STATE%</span></p>
            <p><strong>Uptime:</strong> <span id="uptime">Loading...</span></p>
            <p><strong>Free Heap:</strong> <span id="heap">%FREE_HEAP%</span> bytes</p>
            <p><strong>Current Time:</strong> <span id="currentTime">Loading...</span></p>
            <div class="actions">
                <button class="action-btn btn-settings" onclick="window.location.href='/settings'">Settings</button>
                <button class="action-btn btn-reboot" onclick="rebootDevice()">Reboot</button>
            </div>
        </div>

        <!-- INFO CARD 3: Network Information -->
        <div class="card info-card">
            <h2>Network Info <span class="status-badge online">Online</span></h2>
            <p><strong>Local IP:</strong> <span id="ip">%LOCAL_IP%</span></p>
            <p><strong>mDNS URL:</strong> http://%DEVICENAME%.local</p>
            <p><strong>Wi-Fi SSID:</strong> %WIFI_SSID%</p>
            <p><strong>Signal Strength:</strong> <span id="rssi">%RSSI%</span> dBm</p>
        </div>

        <!-- INFO CARD 4: Device Details -->
        <div class="card info-card">
            <h2>Device Info</h2>
            <p><strong>Firmware Ver:</strong> 2.0.0</p>
            <p><strong>Chip Model:</strong> ESP32</p>
            <p><strong>MAC Address:</strong> %MAC_ADDRESS%</p>
            <p><strong>Configured Name:</strong> %DEVICENAME%</p>
        </div>
    </div>
    
    <!-- CHART CARD -->
    <div class="card" style="width: 100%; max-width: 1000px; margin-top: 20px;">
        <h2>Live Environment Data</h2>
        <canvas id="sensorChart"></canvas>
    </div>

    <script>
        let sensorChart;

        // Mapping MochiState enum to Strings for display
        const stateMap = {
            0: 'HAPPY',
            1: 'ALERT_HIGH (Too Hot)',
            2: 'ALERT_LOW (Too Cold)',
            3: 'TOUCHED',
            4: 'UPDATING (OTA)',
        };

        // Function to fetch dynamic data and update the cards
        function updateData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    // Update Sensor Data
                    document.getElementById('temp').innerText = data.tempC.toFixed(1);
                    document.getElementById('humidity').innerText = data.humidity.toFixed(0);
                    document.getElementById('pressure').innerText = data.pressure_hPa.toFixed(0);
                    // document.getElementById('lux').innerText = data.lux.toFixed(1); // Uncomment when lux sensor is added

                    // Update System Data
                    document.getElementById('current-state').innerText = stateMap[data.state];
                    document.getElementById('uptime').innerText = formatUptime(data.uptime);
                    document.getElementById('heap').innerText = data.heap;
                    document.getElementById('currentTime').innerText = data.time;
                    document.getElementById('live-datetime').innerText = data.time;

                    // Update Chart
                    updateChart(data.time, data.tempC, data.humidity);

                    // Update Mochi Face and Display Color
                    updateMochiFace(data.state, data.tempC);
                })
                .catch(error => console.error('Error fetching data:', error));
        }

        function updateMochiFace(state, temp) {
            const faceElement = document.getElementById('mochi-face');
            const emotionElement = document.getElementById('emotion-text');
            const displayElement = document.getElementById('mochi-display');
            let face, emotion, color, bgColor;

            switch (state) {
                case 1: // ALERT_HIGH
                    face = '🥵';
                    emotion = 'It\'s getting warm!';
                    color = 'var(--danger)';
                    bgColor = '#FFEDED';
                    break;
                case 2: // ALERT_LOW
                    face = '🥶';
                    emotion = 'A bit chilly!';
                    color = 'var(--cold)';
                    bgColor = '#EDF6FF';
                    break;
                case 3: // TOUCHED
                    face = '😉';
                    emotion = 'Thanks for the touch!';
                    color = 'var(--primary)';
                    bgColor = 'var(--card-bg)';
                    break;
                case 4: // UPDATING
                    face = '🔄';
                    emotion = 'Updating...';
                    color = 'orange';
                    bgColor = 'var(--card-bg)';
                    break;
                case 0: // HAPPY
                default:
                    face = '😊';
                    emotion = 'Happy and Ready!';
                    color = 'var(--secondary)';
                    bgColor = 'var(--card-bg)';
                    break;
            }

            faceElement.innerText = face;
            emotionElement.innerText = emotion;
            emotionElement.style.color = color;
            displayElement.style.backgroundColor = bgColor;
        }

        // Helper function to format uptime from seconds
        function formatUptime(ms) {
            let totalSeconds = Math.floor(ms / 1000);
            const hours = Math.floor(totalSeconds / 3600);
            totalSeconds %= 3600;
            const minutes = Math.floor(totalSeconds / 60);
            const seconds = totalSeconds % 60;
            return `${hours}h ${minutes}m ${seconds}s`;
        }

        function rebootDevice() {
            if (confirm('Are you sure you want to reboot Mochi?')) {
                fetch('/reboot', { method: 'POST' })
                    .then(() => {
                        alert('Reboot command sent. The device will now restart.');
                        // Disable page interaction
                        document.body.style.pointerEvents = 'none';
                        document.body.style.opacity = '0.5';
                    })
                    .catch(error => console.error('Error sending reboot command:', error));
            }
        }

        function initChart(history) {
            const ctx = document.getElementById('sensorChart').getContext('2d');
            sensorChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: history.labels,
                    datasets: [{
                        label: 'Temperature (°C)',
                        data: history.temps,
                        borderColor: 'rgba(255, 99, 132, 1)',
                        backgroundColor: 'rgba(255, 99, 132, 0.2)',
                        yAxisID: 'yTemp',
                    }, {
                        label: 'Humidity (%)',
                        data: history.hums,
                        borderColor: 'rgba(54, 162, 235, 1)',
                        backgroundColor: 'rgba(54, 162, 235, 0.2)',
                        yAxisID: 'yHum',
                    }]
                },
                options: {
                    scales: {
                        yTemp: {
                            type: 'linear',
                            display: true,
                            position: 'left',
                            title: { display: true, text: 'Temperature (°C)' }
                        },
                        yHum: {
                            type: 'linear',
                            display: true,
                            position: 'right',
                            title: { display: true, text: 'Humidity (%)' },
                            grid: { drawOnChartArea: false } // only draw grid for temp axis
                        }
                    }
                }
            });
        }

        function updateChart(label, temp, hum) {
            if (!sensorChart) return;
            sensorChart.data.labels.push(label.split(' ')[1]); // Just time
            sensorChart.data.datasets[0].data.push(temp);
            sensorChart.data.datasets[1].data.push(hum);

            // Limit data points
            if (sensorChart.data.labels.length > 60) {
                sensorChart.data.labels.shift();
                sensorChart.data.datasets.forEach(dataset => dataset.data.shift());
            }
            sensorChart.update('none'); // 'none' for no animation
        }

        // Fetch historical data on page load to populate chart
        fetch('/history')
            .then(response => response.json())
            .then(history => initChart(history))
            .catch(error => console.error('Error fetching history:', error));

        // Update data every 3 seconds
        setInterval(updateData, 3000);
    </script>
</body>
</html>
)raw";

// ------------------------------------
// 3. FIRMWARE LOGIC (Implementation)
// ------------------------------------

const char* SETTINGS_HTML = R"raw(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Mochi Settings</title>
    <style>
        :root {
            --primary: #FF69B4;
            --secondary: #6A5ACD;
            --bg: #F0F4F8;
            --card-bg: #FFFFFF;
            --text-color: #333;
        }
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: var(--bg); color: var(--text-color); display: flex; justify-content: center; align-items: center; min-height: 100vh; }
        .container { background: var(--card-bg); padding: 30px; border-radius: 16px; box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1); width: 100%; max-width: 400px; }
        h1 { color: var(--primary); text-align: center; margin-bottom: 20px; }
        label { display: block; margin: 15px 0 8px; font-weight: bold; color: var(--secondary); }
        input[type="number"], select { width: 100%; padding: 12px; border: 2px solid #ddd; border-radius: 8px; box-sizing: border-box; background-color: white; }
        input[type="number"]:focus, select:focus { border-color: var(--primary); outline: none; }
        .checkbox-group { display: flex; align-items: center; gap: 10px; margin-top: 20px; }
        input[type="range"] { width: 100%; }
        button { width: 100%; padding: 12px; margin-top: 20px; background-color: var(--primary); color: white; border: none; border-radius: 8px; font-size: 1.1em; cursor: pointer; }
        button:hover { background-color: #E05AA0; }
        .note { margin-top: 20px; font-size: 0.9em; color: #666; text-align: center; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Mochi Settings</h1>
        <form action="/save-settings" method="post">
            <label for="temp_high">High Temperature Alert (°C)</label>
            <input type="number" id="temp_high" name="temp_high" step="0.1" value="%TEMP_HIGH%" required>

            <label for="temp_low">Low Temperature Alert (°C)</label>
            <input type="number" id="temp_low" name="temp_low" step="0.1" value="%TEMP_LOW%" required>

            <label for="timezone">Time Zone</label>
            <select id="timezone" name="timezone">
                <option value="-43200">UTC-12:00</option>
                <option value="-39600">UTC-11:00</option>
                <option value="-36000">UTC-10:00 (HST)</option>
                <option value="-32400">UTC-09:00 (AKST)</option>
                <option value="-28800">UTC-08:00 (PST)</option>
                <option value="-25200">UTC-07:00 (MST)</option>
                <option value="-21600">UTC-06:00 (CST)</option>
                <option value="-18000">UTC-05:00 (EST)</option>
                <option value="-14400">UTC-04:00 (AST)</option>
                <option value="-10800">UTC-03:00</option>
                <option value="-7200">UTC-02:00</option>
                <option value="-3600">UTC-01:00</option>
                <option value="0">UTC±00:00 (GMT)</option>
                <option value="3600">UTC+01:00 (CET)</option>
                <option value="7200">UTC+02:00 (EET)</option>
                <option value="10800">UTC+03:00 (MSK)</option>
                <option value="14400">UTC+04:00</option>
                <option value="18000">UTC+05:00</option>
                <option value="19800">UTC+05:30 (IST)</option>
                <option value="21600">UTC+06:00</option>
                <option value="25200">UTC+07:00</option>
                <option value="28800">UTC+08:00 (CST)</option>
                <option value="32400">UTC+09:00 (JST)</option>
                <option value="34200">UTC+09:30</option>
                <option value="36000">UTC+10:00 (AEST)</option>
                <option value="39600">UTC+11:00</option>
                <option value="43200">UTC+12:00</option>
            </select>

            <label for="oled_timeout">OLED Timeout (minutes, 0=always on)</label>
            <input type="number" id="oled_timeout" name="oled_timeout" min="0" value="%OLED_TO%">

            <hr style="margin: 20px 0; border: 1px dashed #ddd;">

            <label for="quiet_start">Quiet Hours Start (0-23)</label>
            <input type="number" id="quiet_start" name="quiet_start" min="0" max="23" value="%QUIET_START%">
            <label for="quiet_end">Quiet Hours End (0-23)</label>
            <input type="number" id="quiet_end" name="quiet_end" min="0" max="23" value="%QUIET_END%">

            <label for="alarm_hr">Alarm Time (Hour, 0-23)</label>
            <input type="number" id="alarm_hr" name="alarm_hr" min="0" max="23" value="%ALARM_HR%">
            <label for="alarm_min">Alarm Time (Minute, 0-59)</label>
            <input type="number" id="alarm_min" name="alarm_min" min="0" max="59" value="%ALARM_MIN%">

            <div class="checkbox-group">
                <input type="checkbox" id="buzzer" name="buzzer" %BUZZER_CHECKED%>
                <label for="buzzer">Enable Buzzer</label>
            </div>

            <div class="checkbox-group" style="margin-top: 10px;">
                <input type="checkbox" id="alarm_en" name="alarm_en" %ALARM_CHECKED%>
                <label for="alarm_en">Enable Wake-up Alarm</label>
            </div>

            <button type="submit">Save & Reboot</button>
        </form>

        <a href="/update" style="display: block; text-align: center; margin-top: 20px;">Update Firmware</a>

        <p class="note">Smart-Nav-Mitra will reboot to apply the new settings.</p>
    </div>
</body>
</html>
)raw";

// ------------------------------------
// 4. EMBEDDED HTML FOR FIRMWARE UPDATE PAGE
// ------------------------------------
const char* UPDATE_HTML = R"raw(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Firmware Update</title>
    <style>
        :root { --primary: #6A5ACD; --bg: #F0F4F8; --card-bg: #FFFFFF; --text-color: #333; }
        body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background-color: var(--bg); color: var(--text-color); display: flex; justify-content: center; align-items: center; min-height: 100vh; }
        .container { background: var(--card-bg); padding: 30px; border-radius: 16px; box-shadow: 0 10px 30px rgba(0, 0, 0, 0.1); width: 100%; max-width: 500px; text-align: center; }
        h1 { color: var(--primary); }
        form { margin-top: 20px; }
        input[type="file"] { border: 2px dashed #ddd; padding: 20px; border-radius: 8px; width: 100%; box-sizing: border-box; }
        button { width: 100%; padding: 12px; margin-top: 20px; background-color: var(--primary); color: white; border: none; border-radius: 8px; font-size: 1.1em; cursor: pointer; }
        button:hover { background-color: #5949B2; }
        .progress-bar { width: 100%; background-color: #ddd; border-radius: 4px; margin-top: 20px; display: none; }
        .progress { width: 0%; height: 20px; background-color: var(--primary); border-radius: 4px; text-align: center; color: white; line-height: 20px; }
        #status { margin-top: 10px; font-weight: bold; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Firmware Update</h1>
        <p>Select a .bin file to upload and update the device.</p>
        <form id="upload_form" method="POST" action="/update" enctype="multipart/form-data">
            <input type="file" name="update" id="file" accept=".bin" required>
            <button type="submit">Update Firmware</button>
        </form>
        <div class="progress-bar" id="progress_bar">
            <div class="progress" id="progress">0%</div>
        </div>
        <div id="status"></div>
    </div>
    <script>
        const form = document.getElementById('upload_form');
        const progressBar = document.getElementById('progress_bar');
        const progress = document.getElementById('progress');
        const status = document.getElementById('status');

        form.addEventListener('submit', function(e) {
            e.preventDefault();
            const fileInput = document.getElementById('file');
            const file = fileInput.files[0];
            if (!file) {
                status.textContent = 'Please select a file.';
                return;
            }

            const xhr = new XMLHttpRequest();
            xhr.open('POST', '/update', true);

            xhr.upload.addEventListener('progress', function(e) {
                if (e.lengthComputable) {
                    const percentComplete = (e.loaded / e.total) * 100;
                    progressBar.style.display = 'block';
                    progress.style.width = percentComplete.toFixed(2) + '%';
                    progress.textContent = percentComplete.toFixed(2) + '%';
                }
            });

            xhr.onload = function() {
                if (xhr.status === 200) {
                    status.textContent = 'Update successful! Rebooting...';
                    setTimeout(() => window.location.href = '/', 5000); // Redirect to home page after 5s
                } else {
                    status.textContent = 'Update failed! ' + xhr.responseText;
                }
            };

            const formData = new FormData();
            formData.append('update', file);
            xhr.send(formData);
        });
    </script>
</body>
</html>
)raw";

// ------------------------------------
// 4. FIRMWARE LOGIC (Implementation)
// ------------------------------------

// Load configuration from NVS
void loadConfig() {
  preferences.begin(PREFS_NAMESPACE, true); // Read-only

  staSsid = preferences.getString(KEY_SSID, "");
  staPass = preferences.getString(KEY_PASS, "");
  // Default deviceName is "mochi" if not set in NVS
  deviceName = preferences.getString(KEY_DEV_NAME, "mochi"); 
  // Load temperature thresholds, with defaults
  tempAlertHigh = preferences.getFloat(KEY_TEMP_HIGH, 30.0);
  tempAlertLow = preferences.getFloat(KEY_TEMP_LOW, 18.0);
  // Load new settings
  buzzerEnabled = preferences.getBool(KEY_BUZZER_EN, true);
  gmtOffset_sec = preferences.getLong(KEY_TZ_OFFSET, 0);
  oledTimeoutMins = preferences.getUShort(KEY_OLED_TO, 10);
  // Load quiet hours and alarm settings
  quietHourStart = preferences.getUChar(KEY_QUIET_START, 22);
  quietHourEnd = preferences.getUChar(KEY_QUIET_END, 7);
  alarmEnabled = preferences.getBool(KEY_ALARM_EN, false);
  alarmHour = preferences.getUChar(KEY_ALARM_HR, 7);
  alarmMinute = preferences.getUChar(KEY_ALARM_MIN, 30);

  preferences.end();
}

// Save new configuration to NVS
void saveConfig(const String& ssid, const String& pass, const String& name) {
  preferences.begin(PREFS_NAMESPACE, false); // Read/Write

  preferences.putString(KEY_SSID, ssid);
  preferences.putString(KEY_PASS, pass);
  String sanitizedName = name;
  sanitizedName.replace(" ", "-"); // Sanitize for mDNS
  preferences.putString(KEY_DEV_NAME, sanitizedName);

  preferences.end();
  deviceName = sanitizedName; // Update in memory
}

// Save new settings to NVS
void saveSettings(float high, float low, bool buzzer, long tz, uint16_t timeout, uint8_t qStart, uint8_t qEnd, bool almEn, uint8_t almHr, uint8_t almMin) {
  preferences.begin(PREFS_NAMESPACE, false); // Read/Write
  preferences.putFloat(KEY_TEMP_HIGH, high);
  preferences.putFloat(KEY_TEMP_LOW, low);
  preferences.putBool(KEY_BUZZER_EN, buzzer);
  preferences.putLong(KEY_TZ_OFFSET, tz);
  preferences.putUShort(KEY_OLED_TO, timeout);
  preferences.putUChar(KEY_QUIET_START, qStart);
  preferences.putUChar(KEY_QUIET_END, qEnd);
  preferences.putBool(KEY_ALARM_EN, almEn);
  preferences.putUChar(KEY_ALARM_HR, almHr);
  preferences.putUChar(KEY_ALARM_MIN, almMin);
  preferences.end();
}

// Setup the Captive Portal AP and DNS
void startCaptivePortal() {
  Serial.println("Starting Captive Portal...");

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, NET_MASK);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
  Serial.printf("AP Name: %s\n", AP_SSID);

  // DNS Server Setup (Redirect all requests to AP_IP)
  dnsServer.start(53, "*", AP_IP);

  // Web Server Setup
  server.onNotFound([](AsyncWebServerRequest *request) {
    // Necessary for Captive Portal: redirect all traffic to /config
    request->redirect("/config");
  });

  server.on("/config", HTTP_GET, handleConfig);
  server.on("/saveconfig", HTTP_POST, handleSaveConfig);

  server.begin();
  Serial.println("HTTP and DNS Server started.");
  
  // Show UPDATING state on OLED during configuration (as it's busy)
  currentState = UPDATING; 
  drawMochiFace(currentState);
}

// Try to connect to saved Wi-Fi credentials
bool connectToWiFi() {
  Serial.printf("Connecting to Wi-Fi: %s\n", staSsid.c_str());
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Connecting to:");
  display.setCursor(0, 10);
  display.println(staSsid.c_str());
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(staSsid.c_str(), staPass.c_str());

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) { // 15s timeout
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected successfully!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());

    // Initialize mDNS with the saved device name
    if (!MDNS.begin(deviceName.c_str())) {
      Serial.println("Error starting mDNS");
    } else {
      Serial.printf("mDNS responder started at: http://%s.local\n", deviceName.c_str());
      MDNS.addService("http", "tcp", 80);
    }

    // Set up the main web server endpoints
    server.on("/", HTTP_GET, handleRoot);
    server.on("/data", HTTP_GET, handleData); // API endpoint for JS updates
    server.on("/history", HTTP_GET, handleHistory); // API for chart data
    server.on("/settings", HTTP_GET, handleSettings);
    server.on("/save-settings", HTTP_POST, handleSaveSettings);
    server.on("/reboot", HTTP_POST, handleReboot);
    server.on("/update", HTTP_GET, handleUpdate);
    server.on("/update", HTTP_POST, handleUpdateSuccess, handleUpdateUpload);
    server.begin();
    
    // Resume HAPPY state after connection
    currentState = HAPPY;
    setupOTA();
    return true;
  } else {
    Serial.println("Wi-Fi connection failed or timed out.");
    return false;
  }
}

// Handler for the main operational web page (Mochi Interface)
void handleRoot(AsyncWebServerRequest *request) {
  readSensors(); // Get fresh data for initial page load
  
  String html = MAIN_HTML;

  // Create dynamic greeting
  String greeting = "";
  struct tm timeinfo;
  if(getLocalTime(&timeinfo)){
    int currentHour = timeinfo.tm_hour;
    if (currentHour >= 5 && currentHour < 12) {
      greeting = "Good morning.";
    } else if (currentHour >= 12 && currentHour < 18) {
      greeting = "Good afternoon.";
    } else {
      greeting = "Good evening.";
    }
  }

  // Server-Side Placeholder Replacement for the first load
  html.replace("%DEVICENAME%", deviceName);
  html.replace("%GREETING%", greeting);
  html.replace("%LOCAL_IP%", WiFi.localIP().toString());
  html.replace("%WIFI_SSID%", WiFi.SSID());
  html.replace("%RSSI%", String(WiFi.RSSI()));
  html.replace("%MAC_ADDRESS%", WiFi.macAddress());
  html.replace("%FREE_HEAP%", String(ESP.getFreeHeap()));
  // html.replace("%LUX%", String(lux, 1)); // Uncomment when lux sensor is added
  
  // Replace sensor/state data
  html.replace("%TEMP_C%", String(tempC, 1));
  html.replace("%HUMIDITY%", String(humidity, 0));
  html.replace("%PRESSURE%", String(pressure_hPa, 0));
  
  // State to string (for the System Health card)
  String stateStr;
  switch (currentState) {
      case HAPPY: stateStr = "Happy/Monitoring"; break;
      case ALERT_HIGH: stateStr = "Alert: High Temp"; break;
      case ALERT_LOW: stateStr = "Alert: Low Temp"; break;
      case TOUCHED: stateStr = "Touched"; break;
      case UPDATING: stateStr = "OTA Updating"; break;
      default: stateStr = "Unknown";
  }
  html.replace("%STATE%", stateStr);

  request->send(200, "text/html", html);
}

// API endpoint to return JSON for dynamic JS updates
void handleData(AsyncWebServerRequest *request) {
    // Read sensors just before sending the response
    // No need to call readSensors() here if the main loop does it periodically.    
    StaticJsonDocument<384> doc;

    char timeStr[20];
    struct tm timeinfo;
    if(getLocalTime(&timeinfo)){
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
      strcpy(timeStr, "No Time Sync");
    }

    doc["tempC"] = tempC;
    doc["humidity"] = humidity;
    doc["pressure_hPa"] = pressure_hPa;
    doc["state"] = currentState;
    doc["uptime"] = millis();
    doc["heap"] = ESP.getFreeHeap();
    doc["time"] = timeStr;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
}

// API endpoint to return historical data for chart
void handleHistory(AsyncWebServerRequest *request) {
    StaticJsonDocument<2048> doc; // Larger doc for history
    JsonArray labels = doc.createNestedArray("labels");
    JsonArray temps = doc.createNestedArray("temps");
    JsonArray hums = doc.createNestedArray("hums");

    for (int i = 0; i < DATA_HISTORY_SIZE; i++) {
        if (dataHistory[i].time > 0) { // Only add valid points
            labels.add(dataHistory[i].time);
            temps.add(dataHistory[i].temp);
            hums.add(dataHistory[i].humidity);
        }
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
}

// Handler for the configuration page (Captive Portal)
void handleConfig(AsyncWebServerRequest *request) {
  String html = CONFIG_HTML;
  html.replace("%DEVICENAME%", deviceName);
  request->send(200, "text/html", html);
}

// Handler for saving configuration data
void handleSaveConfig(AsyncWebServerRequest *request) {
  String newSsid, newPass, newDeviceName;

  if (request->hasParam("ssid", true)) {
    newSsid = request->getParam("ssid", true)->value();
  }
  if (request->hasParam("password", true)) {
    newPass = request->getParam("password", true)->value();
  }
  if (request->hasParam("devicename", true)) {
    newDeviceName = request->getParam("devicename", true)->value();
  }

  saveConfig(newSsid, newPass, newDeviceName);

      // Send success message and reboot command
      request->send(200, "text/html", "<h1>Configuration Saved!</h1><p>Smart-Nav-Mitra is rebooting and attempting to connect to <strong>" + newSsid + "</strong>.</p><p>Please wait 10 seconds and try accessing it at <strong>http://" + newDeviceName + ".local</strong></p>");

  Serial.println("Configuration saved. Rebooting...");
  delay(2000);
  ESP.restart();
}

// Handler for the settings page
void handleSettings(AsyncWebServerRequest *request) {
  String html = SETTINGS_HTML;
  html.replace("%TEMP_HIGH%", String(tempAlertHigh));
  html.replace("%TEMP_LOW%", String(tempAlertLow));
  html.replace("%BUZZER_CHECKED%", buzzerEnabled ? "checked" : "");
  html.replace("%OLED_TO%", String(oledTimeoutMins));
  html.replace("%QUIET_START%", String(quietHourStart));
  html.replace("%QUIET_END%", String(quietHourEnd));
  html.replace("%ALARM_CHECKED%", alarmEnabled ? "checked" : "");
  html.replace("%ALARM_HR%", String(alarmHour));
  html.replace("%ALARM_MIN%", String(alarmMinute));
  // This JS snippet will select the correct timezone in the dropdown
  String js = "<script>document.getElementById('timezone').value = '" + String(gmtOffset_sec) + "';</script>";
  html += js;
  request->send(200, "text/html", html);
}

// Handler for saving settings
void handleSaveSettings(AsyncWebServerRequest *request) {
  if (request->hasParam("temp_high", true) && request->hasParam("temp_low", true) && request->hasParam("timezone", true) && request->hasParam("oled_timeout", true) &&
      request->hasParam("quiet_start", true) && request->hasParam("quiet_end", true) && request->hasParam("alarm_hr", true) && request->hasParam("alarm_min", true)) {

    float high = request->getParam("temp_high", true)->value().toFloat();
    float low = request->getParam("temp_low", true)->value().toFloat();
    bool buzzer = request->hasParam("buzzer", true);
    long tz = request->getParam("timezone", true)->value().toInt();
    uint16_t timeout = request->getParam("oled_timeout", true)->value().toInt();
    uint8_t qStart = request->getParam("quiet_start", true)->value().toInt();
    uint8_t qEnd = request->getParam("quiet_end", true)->value().toInt();
    bool almEn = request->hasParam("alarm_en", true);
    uint8_t almHr = request->getParam("alarm_hr", true)->value().toInt();
    uint8_t almMin = request->getParam("alarm_min", true)->value().toInt();
    saveSettings(high, low, buzzer, tz, timeout, qStart, qEnd, almEn, almHr, almMin);

    request->send(200, "text/html", "<h1>Settings Saved!</h1><p>Smart-Nav-Mitra is rebooting to apply changes.</p>");
    delay(2000);
    ESP.restart();
  } else {
    request->send(400, "text/plain", "Bad Request: Missing parameters.");
  }
}

// --- WEB-BASED FIRMWARE UPDATE HANDLERS ---

// Handler to serve the /update page
void handleUpdate(AsyncWebServerRequest *request) {
  request->send(200, "text/html", UPDATE_HTML);
}

// Handler for the file upload process
void handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (index == 0) {
    Serial.printf("Update Start: %s\n", filename.c_str());
    currentState = UPDATING; // Show updating state on OLED
    // If authentication is not used, it's important to check the filename extension
    if (!filename.endsWith(".bin")) {
      request->send(400, "text/plain", "Not a .bin file");
      return;
    }
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start OTA update
      Update.printError(Serial);
    }
  }
  if (len) {
    Update.write(data, len); // Write the received chunk to flash
  }
  if (final) {
    if (Update.end(true)) { // Finish the update
      Serial.println("Update Success");
    } else {
      Update.printError(Serial);
    }
  }
}

// Handler for when the update is successfully finished
void handleUpdateSuccess(AsyncWebServerRequest *request) {
  if(Update.isFinished()){
    request->send(200, "text/plain", "OK");
    ESP.restart();
  } else {
    request->send(500, "text/plain", "Update failed");
  }
}

// Handler for reboot command
void handleReboot(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Rebooting...");
    delay(1000);
    ESP.restart();
}

void setupOTA() {
  ArduinoOTA.setHostname(deviceName.c_str());
  ArduinoOTA.setPassword("mochipass"); // CHANGE THIS IN PRODUCTION!

  ArduinoOTA
    .onStart([]() {
      currentState = UPDATING;
      Serial.println("Start updating...");
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      // OLED/Serial progress display
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
      // Revert to HAPPY state on error
      currentState = HAPPY; 
    });

  ArduinoOTA.begin();
  Serial.println("OTA Initialized.");
}

// Read and update global sensor variables
void readSensors() {
  sensors_event_t humidity_event, temp_event;
  // Use the AHT20 for Temperature and Humidity
  if (aht.getEvent(&humidity_event, &temp_event)) {
    tempC = temp_event.temperature;
    humidity = humidity_event.relative_humidity;
  } else {
    Serial.println("Failed to read from AHT20");
  }

  // Use the BMP280 for Pressure
  pressure_hPa = bmp.readPressure() / 100.0F;

  Serial.printf("T: %.2f C, H: %.2f %%, P: %.2f hPa\n", tempC, humidity, pressure_hPa);
}

void checkTouchAndEnvironment() {
  // 1. Touch Input Check (Highest priority, overrides all states temporarily)
  if (digitalRead(TOUCH_PIN) == HIGH) {
    if (currentState != TOUCHED) {
      currentState = TOUCHED;
      touchTimer = millis();
      buzzAlert(100); // Quick buzz feedback
      Serial.println("Touch detected!");
    }
  }

  // 2. Environment Alert Check (Only proceed if not TOUCHED or UPDATING)
  // The main loop now ensures this is only called when not in a temporary state.
  // It also ensures sensors are read just before this check.
  
    // High Temperature Alert
    if (tempC > tempAlertHigh) {
      if (currentState != ALERT_HIGH) { // Only change state and print once
        currentState = ALERT_HIGH;
        Serial.println("High Temperature Alert!");
      }
      buzzAlert(50); // Small repeating buzz while in alert state
    } 
    // Low Temperature Alert
    else if (tempC < tempAlertLow) {
      if (currentState != ALERT_LOW) { // Only change state and print once
        currentState = ALERT_LOW;
        Serial.println("Low Temperature Alert!");
      }
      buzzAlert(50); // Small repeating buzz while in alert state
    }
    // No Alert
    else {
      if (currentState != HAPPY) { // If it was in any alert state, return to happy
        currentState = HAPPY;
        Serial.println("Temperature returned to normal.");
      }
    }
}

void buzzAlert(int durationMs) {
  if (!buzzerEnabled || isQuietHours()) return; // Check if buzzer is enabled and not quiet hours

  digitalWrite(BUZZER_PIN, HIGH);
  delay(durationMs);
  digitalWrite(BUZZER_PIN, LOW);
}

bool isQuietHours() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false; // Can't determine time, so not quiet hours
  }
  int currentHour = timeinfo.tm_hour;

  // Handle overnight period (e.g., 22:00 to 07:00)
  if (quietHourStart > quietHourEnd) {
    return (currentHour >= quietHourStart || currentHour < quietHourEnd);
  } else { // Handle same-day period (e.g., 09:00 to 17:00)
    return (currentHour >= quietHourStart && currentHour < quietHourEnd);
  }
}

void checkAlarm() {
  if (!alarmEnabled) return;

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  if (timeinfo.tm_hour == alarmHour && timeinfo.tm_min == alarmMinute && !alarmHasTriggeredToday) {
    Serial.println("ALARM! WAKE UP!");
    alarmHasTriggeredToday = true; // Prevent it from triggering again today
    lastActivityTime = millis(); // Wake up screen
    // Play a distinct alarm sound
    for (int i = 0; i < 5; i++) {
      digitalWrite(BUZZER_PIN, HIGH); delay(200); digitalWrite(BUZZER_PIN, LOW); delay(100);
    }
  }

  // Reset the alarm trigger flag just after midnight
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && alarmHasTriggeredToday) {
    alarmHasTriggeredToday = false;
  }
}

// --------------------------------------------------------------------------------
// OLED DISPLAY (Facial Expressions)
// --------------------------------------------------------------------------------

void drawMochiFace(MochiState state) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Draw the main Mochi shape
  display.fillCircle(64, 32, 30, SSD1306_WHITE);
  display.fillCircle(64, 32, 28, SSD1306_BLACK);

  switch (state) {
    case HAPPY:
      // Happy Eyes and Smile
      display.fillCircle(50, 25, 3, SSD1306_WHITE);
      display.fillCircle(78, 25, 3, SSD1306_WHITE);
      display.drawCircle(64, 32, 15, SSD1306_WHITE);
      display.fillRect(49, 32, 30, 15, SSD1306_BLACK); 
      break;

    case ALERT_HIGH:
      // Hot Alert (Sweaty/Panting look)
      display.drawLine(45, 20, 55, 30, SSD1306_WHITE); // Angled eyes
      display.drawLine(55, 20, 45, 30, SSD1306_WHITE);
      display.drawLine(73, 20, 83, 30, SSD1306_WHITE);
      display.drawLine(83, 20, 73, 30, SSD1306_WHITE);
      display.fillRect(62, 38, 4, 4, SSD1306_WHITE); // Open Mouth (Square)
      break;
      
    case ALERT_LOW:
      // Cold Alert (Squinting/Shivering look)
      display.drawPixel(50, 25, SSD1306_WHITE); // Tiny eyes
      display.drawPixel(78, 25, SSD1306_WHITE);
      display.drawFastHLine(50, 40, 28, SSD1306_WHITE); // Flat line mouth
      break;

    case TOUCHED:
      // Winking Face and Big Smile
      display.fillCircle(50, 25, 3, SSD1306_WHITE); 
      display.drawLine(75, 25, 81, 25, SSD1306_WHITE); 
      display.drawCircle(64, 35, 18, SSD1306_WHITE);
      display.fillRect(46, 35, 36, 18, SSD1306_BLACK);
      break;

    case UPDATING:
      // OTA Text and Progress
      display.fillCircle(64, 32, 30, SSD1306_BLACK); 
      display.setCursor(10, 10);
      display.println("OTA UPDATE");
      display.drawRect(5, 45, 118, 10, SSD1306_WHITE);
      display.fillRect(7, 47, (millis()/100)%114, 6, SSD1306_WHITE);
      break;
  }

  // Draw environment data on the outer frame
  display.setCursor(0, 0);
  display.print("T:"); display.print(tempC, 1); display.println("C");
  display.setCursor(0, 56);
  display.print("H:"); display.print(humidity, 0); display.println("%");
  display.setCursor(90, 56);
  display.print("P:"); display.print(pressure_hPa, 0); display.println("hPa");

  display.display();
}

// --------------------------------------------------------------------------------
// MAIN SETUP & LOOP
// --------------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n--- Smart-Nav-Mitra Firmware Starting ---");

  // 1. Hardware Initialization
  pinMode(TOUCH_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;); 
  }
  display.setRotation(2); // Rotate 180 degrees if your screen is upside down
  display.clearDisplay();

  // Initialize BME280
  // Initialize AHT20
  if (!aht.begin()) {
    Serial.println("Could not find AHT20 sensor, check wiring!");
  }
  // Initialize BMP280
  // The I2C address is often 0x77 or 0x76. If 0x76 doesn't work, try 0x77.
  if (!bmp.begin(0x76)) {
    Serial.println("Could not find BMP280 sensor, check wiring!");
  }

  // Initialize history data
  for(int i=0; i<DATA_HISTORY_SIZE; i++) dataHistory[i].time = 0;

  // 2. Load Configuration and Connect
  loadConfig();
  
  if (staSsid.length() > 0) {
    if (!connectToWiFi()) {
      // Failed to connect (wrong password/router down), fallback to Captive Portal
      startCaptivePortal();
    }
  } else {
    // No saved credentials, start Captive Portal immediately
    startCaptivePortal();
  }
  
  // 3. Configure Time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Read sensors initially
  readSensors();
  Serial.println("Smart-Nav-Mitra is ready!");
  lastActivityTime = millis(); // Initialize activity timer
}

void loop() {
  // Always handle OTA
  ArduinoOTA.handle(); 

  // In Captive Portal Mode, just handle DNS requests
  if (WiFi.getMode() == WIFI_AP) {
    dnsServer.processNextRequest();
    // Redraw the face based on the current state (e.g., UPDATING)
    drawMochiFace(currentState);
    delay(10);
    return; // Don't proceed to other logic
  }

  // --- Main logic for connected mode ---
  if (WiFi.status() != WL_CONNECTED) {
    // If connection is lost, you might want to handle it, e.g., try reconnecting.
    // For now, we just stop processing.
    return;
  }

  // Handle touch state timeout
  if (currentState == TOUCHED && millis() - touchTimer >= touchDisplayDuration) {
    currentState = HAPPY; // Revert to a neutral state, environment check will fix it next
  }

  // If there's any touch activity, reset the screen timeout timer
  if (digitalRead(TOUCH_PIN) == HIGH) {
    if (isDisplayOff) {
      isDisplayOff = false; // Wake up screen
      // Force a redraw on wake-up
      lastState = (MochiState)-1; // An invalid state to force redraw
    }
    lastActivityTime = millis(); // Reset activity timer
  }

  // Check for alarm trigger (once per minute)
  if (millis() - lastMinuteCheck > 60000) {
    lastMinuteCheck = millis();
    checkAlarm();
  }

  // Periodically check sensors and environment state
  if (millis() - lastSensorReadTime >= sensorInterval) {
    lastSensorReadTime = millis();
    readSensors(); // Read sensor data

    // Store data for charting
    dataHistory[historyIndex].time = millis(); // Using millis as a simple timestamp for the chart
    dataHistory[historyIndex].temp = tempC;
    dataHistory[historyIndex].humidity = humidity;
    historyIndex = (historyIndex + 1) % DATA_HISTORY_SIZE;

    
    // Only check environment if not in a temporary state like TOUCHED or UPDATING
    if (currentState != TOUCHED && currentState != UPDATING) {
      checkTouchAndEnvironment();
    }
  }

  // OLED Timeout Logic
  // Check if the screen should be off due to timeout or quiet hours
  bool shouldBeOff = (oledTimeoutMins > 0 && (millis() - lastActivityTime > (unsigned long)oledTimeoutMins * 60 * 1000)) || isQuietHours();
  // But, give it a grace period after a touch
  if (millis() - lastActivityTime < 10000) shouldBeOff = false; // Keep screen on for at least 10s after touch

  if (shouldBeOff && !isDisplayOff) {
    if (millis() - lastActivityTime > (unsigned long)oledTimeoutMins * 60 * 1000) {
      isDisplayOff = true;
      display.clearDisplay();
      display.display();
    }
  }

  // Redraw the face based on the current state
  // Only redraw if the state has changed to prevent flicker and save resources
  // Also, don't draw if the display is supposed to be off
  if (!isDisplayOff) {    
    if (currentState != lastState) {
      drawMochiFace(currentState);
      lastState = currentState;
    }
  }
  
  delay(10); // Small delay to prevent the loop from running too fast
}

#elif DATA_COLLECTION_MODE == 1 // This block runs if DATA_COLLECTION_MODE is 1

// ================================================================================
// PHASE 1: DATA COLLECTION MODE
// In this mode, the device's only job is to print the state of the touch pin
// to the Serial Monitor. This data can be copied and used to train a gesture
// recognition model.
// ================================================================================
void setup() {
  Serial.begin(115200);
  pinMode(TOUCH_PIN, INPUT);
  Serial.println("--- Smart-Nav-Mitra: Data Collection Mode ---");
  Serial.println("Continuously printing touch pin state. Press the touch sensor to generate data.");
}

void loop() {
  // Print the value of the touch pin (0 for not touched, 1 for touched)
  Serial.println(digitalRead(TOUCH_PIN));
  delay(10); // Sample every 10ms
}

#endif // End of DATA_COLLECTION_MODE check