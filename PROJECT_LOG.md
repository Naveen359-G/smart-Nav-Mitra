# Smart-Nav-Mitra: Project Development Log

This document summarizes the development journey, key decisions, and feature implementation for the Smart-Nav-Mitra project. It serves as a context file to resume development in future sessions.

## 1. Project Inception & Core Firmware (Phase 0)

The project began as a smart environmental monitor. The initial stable firmware established the foundational features:

-   **Hardware Support:** ESP32-C3 with AHT20 (Temperature/Humidity) and BMP280 (Pressure) sensors, plus an SSD1306 OLED display, touch sensor, and buzzer.
-   **Web Server:** An asynchronous web server was implemented to host a live data dashboard.
-   **Live Dashboard:** The web page displays real-time sensor data, system status, and a historical data chart using Chart.js.
-   **Networking:**
    -   A Captive Portal (`Smart-Nav-Mitra-Setup`) was created for easy first-time Wi-Fi configuration.
    -   mDNS was set up to allow access via a friendly URL (e.g., `http://nav-mitra.local`).
-   **Persistent Storage:** The ESP32's NVS is used to save all user settings (Wi-Fi credentials, device name, etc.).
-   **OTA Updates:** The initial firmware included support for Over-the-Air updates via the command line (`espota.py`).

## 2. Daily Life & UI Enhancements (Phase 2)

This phase focused on making the device more user-friendly and integrated into a daily routine.

-   **Wake-up Alarm:** A configurable daily alarm was added, which triggers the buzzer at a set time.
-   **Scheduled Quiet Hours:** A feature to automatically disable the buzzer and OLED screen during user-defined hours (e.g., overnight).
-   **OLED Screen Timeout:** An inactivity timer was implemented to turn off the OLED screen to prevent burn-in, with a touch-to-wake function.
-   **Time Zone Support:** A dropdown menu was added to the settings page to allow users to select their local time zone for accurate time display.
-   **Web-Based OTA:** A dedicated `/update` page was created, allowing new `firmware.bin` files to be uploaded directly from the web browser.
-   **Dynamic UI:**
    -   The web dashboard was enhanced with a dynamic greeting ("Good morning," "Good afternoon," etc.) based on the time of day.
    -   A live date and time display was added to the top right corner of the web page.

## 3. Project Branding and Documentation

The project underwent a naming evolution and was fully documented.

-   **Naming:** The project was officially named **Smart-Nav-Mitra**, incorporating the user's name ("Nav") and the Sanskrit word for "friend" ("Mitra"). All firmware code and user-facing text were updated.
-   **README.md:** A comprehensive `README.md` file was created, detailing all features, hardware components, pin configurations, setup instructions, and the development roadmap.
-   **.gitignore:** A `.gitignore` file was created to exclude PlatformIO build artifacts and local VS Code settings from the Git repository.
-   **GitHub Setup:** The project was successfully pushed to a GitHub repository, and issues with the local Git author configuration were resolved.

## 4. On-Hold and Future Phases

-   **AI/TinyML Integration (On Hold):** An attempt was made to integrate TensorFlow Lite for gesture recognition. This was paused due to persistent, complex build errors with the library on the target hardware. This remains a future goal.
-   **Advanced Sensing (Next Up):** The `README.md` has been updated to plan for the integration of a **BH1750 light sensor** and an **INMP441 microphone** in the next phase of development.
-   **Smart Home & Usability (Planned):** Future plans include implementing **MQTT support** for integration with platforms like Home Assistant and adding a **Wi-Fi scanner** to the setup page.