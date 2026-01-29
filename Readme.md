# 🛰️ Project GARUDA: ESP8266 Security Auditor

**GARUDA** is a high-performance, low-level Wi-Fi security auditing tool built for the ESP8266 platform. It specializes in 802.11 management frame manipulation, featuring a robust "Beacon Spam" engine and a responsive, interrupt-driven user interface.



---

## 🚀 Technical Highlights

### 1. Raw Packet Injection Engine
Garuda bypasses standard Wi-Fi station modes to interact directly with the physical layer (PHY). Using the `wifi_send_pkt_freedom` SDK function, it constructs manual Beacon frames.
* **SSID Virtualization:** Automatically generates 10+ clones of a target network.
* **Stealth Padding:** Uses varying trailing space characters to ensure unique SSID hashes while appearing identical to the human eye.
* **MAC Spoofing:** Increments the BSSID for each clone to bypass modern device deduplication logic.

### 2. High-Performance UI Architecture
Designed for reliability during high-traffic radio activity:
* **Interrupt-Driven Input:** Button handling is managed via `IRAM_ATTR` Interrupt Service Routines (ISRs), ensuring navigation remains fluid even when the CPU is saturated with packet transmission.
* **SSD1306 Integration:** A custom rendering engine for the 128x64 OLED display that balances refresh rates with system stability.



---

## 🛠️ Hardware Mapping

The project is optimized for the **ESP8266 (NodeMCU V3)** but is compatible with any ESP8266-based board.

| Component | ESP8266 Pin | Function |
| :--- | :--- | :--- |
| **OLED SDA** | **D2** (GPIO 4) | I2C Data |
| **OLED SCL** | **D1** (GPIO 5) | I2C Clock |
| **Btn UP** | **D5** (GPIO 14) | Menu Navigation |
| **Btn DOWN** | **D6** (GPIO 12) | Menu Navigation |
| **Btn SELECT**| **D7** (GPIO 13) | Execute / Long-press Exit |

---

## 📦 Project Structure

* `main.ino`: Core state machine and interrupt management.
* `inputnodes.h`: Hardware abstraction layer and pin definitions.
* `beaconSpam.h/.cpp`: The attack logic and 802.11 frame construction.

---

## 🔧 Installation & Usage

1. **Prerequisites:**
   * Install [Arduino IDE](https://www.arduino.cc/en/software).
   * Add the ESP8266 Board Manager URL.
   * Required Libraries: `Adafruit_SSD1306`, `Adafruit_GFX`, `OneButton`.

2. **Deployment:**
   ```bash
   git clone [https://github.com/your-username/garuda-esp8266.git](https://github.com/your-username/garuda-esp8266.git)
   cd garuda-esp8266
   # Open main.ino in Arduino IDE and Flash