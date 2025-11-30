# Smart-Nav-Mitra - The Smart Environmental Companion

![Smart-Nav-Mitra](https://via.placeholder.com/600x300.png?text=Smart-Nav-Mitra+In+Action!)
*(Replace this with a real image of your Smart-Nav-Mitra device)*

Smart-Nav-Mitra is a small, friendly IoT device designed to be your smart desk companion. It monitors its environment, displays its "mood" on an OLED screen, and provides a rich web interface for live data, charting, and configuration. Built on the ESP32-C3, it is a powerful, local-first platform for home sensing and automation.

---

## ✨ Features

The current firmware provides a rich set of features right out of the box:

#### **Sensing & Display**
- **Live Environment Monitoring:** Real-time data for Temperature, Humidity (from AHT20), and Pressure (from BMP280).
- **Expressive OLED Face:** Mochi's face changes based on its state (Happy, Too Hot, Too Cold, Touched, Updating).
- **At-a-Glance Data:** The OLED screen displays key sensor readings around the border.

#### **Web Interface & Control**
- **Live Data Dashboard:** A modern, mobile-friendly web page showing all sensor and system data.
- **Historical Charting:** A live-updating chart plots the history of temperature and humidity.
- **Web-Based Settings:** A dedicated `/settings` page to configure all device options.
- **Remote Reboot:** A reboot button on the dashboard for easy troubleshooting.

#### **Customization & Daily Life**
- **Configurable Alerts:** Set custom temperature thresholds for high and low alerts.
- **Wake-up Alarm:** Set a daily alarm that will trigger a buzzing sound.
- **Scheduled Quiet Hours:** Automatically disables the buzzer and OLED screen during user-defined hours (e.g., overnight).
- **Buzzer Control:** A master switch to enable or disable all audible alerts.
- **OLED Screen Timeout:** The screen automatically turns off after a period of inactivity to save power and prevent burn-in. It wakes up on touch.
- **Time Zone Support:** Configure your local time zone for accurate time display.

#### **System & Networking**
- **User-Friendly Setup:** A Captive Portal creates a "Smart-Nav-Mitra-Setup" Wi-Fi network for easy first-time configuration.
- **Persistent Memory (NVS):** All your settings (Wi-Fi, device name, alerts, etc.) are saved and persist through reboots.
- **mDNS Discovery:** Access Mochi on your local network with a friendly URL like `http://mochi.local`.
- **Over-the-Air (OTA) Updates:** Update the firmware wirelessly over your Wi-Fi network.

---

## 🛠️ Hardware Components

To build Smart-Nav-Mitra, you will need the following components:

| Component | Description |
| :--- | :--- |
| **ESP32-C3 Super Mini** | The main microcontroller for the project. |
| **AHT20 + BMP280 Board** | A breakout board containing both sensors for I2C communication. |
| **SSD1306 OLED Display** | A 0.96" or 1.3" I2C OLED screen (128x64 pixels). |
| **Active Buzzer** | A 5V or 3.3V active buzzer for sound feedback. |
| **TTP223 Touch Sensor** | A capacitive touch sensor module. |
| **BH1750 Light Sensor** | An I2C ambient light sensor (for the next phase). |
| **INMP441 Microphone** | An I2S digital microphone (for future AI features). |
| **Breadboard & Wires** | For connecting all the components. |

### Pin Configuration & Connections

Wire the components to the ESP32-C3 according to the table below. These pins are defined in the firmware.

| Component Pin | ESP32-C3 Pin | Description |
| :--- | :--- | :--- |
| **AHT20/BMP280 VDD** | `3V3` | Power for sensors |
| **AHT20/BMP280 GND** | `GND` | Common Ground |
| **AHT20/BMP280 SCL** | `GPIO 9` | I2C Clock |
| **AHT20/BMP280 SDA** | `GPIO 8` | I2C Data |
| **BH1750 VCC** | `3V3` | Power for light sensor |
| **BH1750 GND** | `GND` | Common Ground |
| **BH1750 SCL** | `GPIO 9` | I2C Clock (shared) |
| **BH1750 SDA** | `GPIO 8` | I2C Data (shared) |
| **OLED VDD** | `3V3` | Power for OLED |
| **OLED GND** | `GND` | Common Ground |
| **OLED SCL** | `GPIO 9` | I2C Clock (shared) |
| **OLED SDA** | `GPIO 8` | I2C Data (shared) |
| **Touch Sensor VCC** | `3V3` | Power for touch sensor |
| **Touch Sensor GND** | `GND` | Common Ground |
| **Touch Sensor I/O** | `GPIO 7` | Touch signal output |
| **Buzzer VCC (+)** | `3V3` | Power for buzzer |
| **Buzzer GND (-)** | `GND` | Common Ground |
| **Buzzer I/O** | `GPIO 6` | Buzzer control signal |
| **INMP441 VDD** | `3V3` | Power for microphone |
| **INMP441 GND** | `GND` | Common Ground |
| **INMP441 SD** | `GPIO 2` | I2S Serial Data |
| **INMP441 SCK** | `GPIO 3` | I2S Serial Clock |
| **INMP441 WS** | `GPIO 1` | I2S Word Select |

---

## 🚀 Setup & Installation (For Developers)

Follow these steps to compile and upload the firmware.

1.  **Prerequisites:**
    *   Install Visual Studio Code.
    *   Install the PlatformIO IDE extension in VS Code.

2.  **Clone the Repository:**
    ```bash
    git clone <your-repository-url>
    cd mochi-firmware
    ```

3.  **Build & Upload:**
    *   Open the project folder in VS Code.
    *   PlatformIO will automatically detect the `platformio.ini` file and download the required libraries.
    *   Use the PlatformIO controls in the status bar to **Build** and then **Upload** the firmware to your connected ESP32-C3.

---

## ⚙️ First-Time User Setup

Once Smart-Nav-Mitra is powered on for the first time, follow these steps to connect it to your network.

1.  **Connect to Smart-Nav-Mitra's Wi-Fi:** On your phone or computer, look for a Wi-Fi network named **`Smart-Nav-Mitra-Setup`** and connect to it. The password is **`mochisetup`**.
2.  **Captive Portal:** Your device should automatically open a configuration page. If it doesn't, open a web browser and go to `http://192.168.4.1`.
3.  **Enter Your Credentials:** On the configuration page, enter a name for your device (e.g., "nav-mitra"), select your home Wi-Fi network SSID, and enter its password.
4.  **Save & Reboot:** Click "Connect & Save". Smart-Nav-Mitra will save the settings and reboot.
5.  **Access Smart-Nav-Mitra:** After about 30 seconds, it will be connected to your home network. You can now access its dashboard by navigating to the mDNS address you set, for example, **`http://nav-mitra.local`**.

---

## 🗺️ Navigating the Web Interface

-   **Dashboard (Home Page):** This is the main view where you can see all live data, system status, and the historical data chart.
-   **Settings Page (`/settings`):** Click the "Settings" button on the dashboard to access the configuration page. Here you can change:
    -   Temperature alert thresholds.
    -   Your local time zone.
    -   The OLED screen timeout.
    -   Quiet hours start and end times.
    -   Alarm time and enable/disable the alarm.
    -   Enable or disable the buzzer.
-   **Reboot Button:** Safely restarts the device from the web interface.

---

## 🛣️ Project Development Roadmap

This project is developed in phases to ensure stability.

-   **Phase 0: The Foundation** - ✅ **COMPLETE**
    -   This initial version established a fully functional, web-connected environmental monitor. Key features included:
        -   Live sensor monitoring (Temperature, Humidity, Pressure).
        -   Web server with a live data dashboard and historical charting.
        -   Captive portal for easy Wi-Fi setup.
        -   Persistent storage for configuration (Wi-Fi, device name, alerts).
        -   Expressive OLED display with state-based faces.
        -   Over-the-Air (OTA) firmware updates.
        -   NTP for time synchronization.

-   **Phase 1: AI/TinyML Integration** - 🧊 **ON HOLD**
    -   This phase, intended to add gesture recognition, is currently on hold due to build complexities with the AI libraries.

-   **Phase 2: Daily Life Features** - ✅ **COMPLETE**
    -   This phase enhanced the device with features for daily routines and user customization. Key features included:
        -   A configurable daily wake-up alarm.
        -   Scheduled "Quiet Hours" to automatically silence the buzzer.
        -   A master toggle to enable/disable the buzzer.
        -   An adjustable OLED screen timeout to prevent burn-in.
        -   Time zone selection for accurate local time display.

-   **Phase 3: Advanced Sensing** - 📝 **NEXT UP**
    -   The next planned phase is to integrate new hardware to give Smart-Nav-Mitra new senses. This includes:
        -   **BH1750 Light Sensor:** To measure ambient light and enable features like auto-screen-off in the dark.
        -   **INMP441 Microphone:** To prepare the hardware foundation for future voice command capabilities.

-   **Phase 4: Smart Home & Usability** - ⏳ **PLANNED**
    -   This phase will focus on integrating with other smart devices and improving the user setup experience. Key features will be **MQTT support** and a **Wi-Fi scanner**.

---

## 💡 Future Development Ideas

Here are some exciting features planned or considered for future versions of Smart-Nav-Mitra.

### Smart Home & Usability (Next Steps)

-   **MQTT Integration:** Allow Mochi to publish its data to a local MQTT broker like Mosquitto or Home Assistant. This is the key to creating powerful home automations (e.g., "If Mochi is too hot, turn on the fan").
-   **Wi-Fi Network Scanner:** Add a "Scan" button to the setup page to automatically find and list nearby Wi-Fi networks, preventing typos.

### Hardware-Based Feature Enhancements

To add new senses and capabilities, we can integrate additional components:

-   **Light Sensing (BH1750 / LDR):**
    -   **Component:** A BH1750 (I2C) or a simple LDR (analog) light sensor.
    -   **Feature:** Allow Mochi to sense the ambient light level. It could automatically enter a "sleepy" state (😴) and turn off its screen when the room gets dark, and wake up when the lights come on. The light level could also be displayed on the dashboard.

-   **Voice Command Recognition (INMP441):**
    -   **Component:** An INMP441 I2S digital microphone.
    -   **Feature:** By re-visiting the AI/TinyML phase, we could train a small model to recognize a few keywords like "Hello Mochi," "What's the temperature?," or "Good night." This would allow for hands-free interaction.

-   **RGB Status LED (NeoPixel):**
    -   **Component:** A single WS2812B "NeoPixel" RGB LED.
    -   **Feature:** Provide a clear, colorful status indicator. The LED could glow blue when cold, red when hot, green when happy, or pulse purple during an OTA update. This provides instant feedback without needing to look at the screen.