# 🏥 Medibox: Smart Medicine Reminder System

![Medibox Wokwi Simulation]((https://github.com/rivindu02/EN2853-Medibox/blob/main/simulation%20on%20wokwi.png))

## 🌟 Project Overview

Medibox is an advanced ESP32-based medicine reminder system designed to help users manage their medication schedules efficiently while monitoring environmental conditions.

### 🚀 Key Features

- **Precise Time Synchronization**
  - Automatic time sync via NTP server
  - Configurable time zone support (with 30-minute increments)

- **Flexible Alarm Management**
  - Set and manage multiple medicine alarms
  - Easy alarm configuration and deletion
  - Snooze and stop alarm functionality

- **Environmental Monitoring**
  - Real-time temperature and humidity tracking
  - Health range alerts for medication storage conditions

- **User-Friendly Interface**
  - OLED display for clear information
  - Intuitive button-based navigation
  - Visual and audio alerts

### 🔧 Hardware Components

- ESP32 Microcontroller
- OLED Display (128x64)
- DHT22 Temperature/Humidity Sensor
- Buzzer
- LED Indicator
- Tactile Buttons

## 🛠 Setup and Installation

### Prerequisites

- PlatformIO IDE
- ESP32 Development Board
- Required Libraries:
  - Arduino
  - WiFi
  - Wire
  - Adafruit_GFX
  - Adafruit_SSD1306
  - DHT Sensor Library

### Installation Steps

1. Clone the repository
2. Open in PlatformIO
3. Install required libraries
4. Configure WiFi credentials
5. Upload to ESP32

### 🧰 Simulation

- Tested with Wokwi ESP32 Simulator
- Compatible with VS Code PlatformIO

## 📊 Technical Highlights

- Non-blocking design
- Efficient button debouncing
- Timezone-aware time management
- Robust error handling
- Modular code structure

## 🔍 Detailed Functionality

### Time Management
- NTP-based time synchronization
- Timezone adjustment (±12 hours, 30-minute increments)

### Alarm System
- Two independent alarms
- Hour and minute level precision
- Snooze (5-minute intervals)
- Visual and audio alerts

### Environmental Monitoring
- Temperature range: 24-32°C
- Humidity range: 65-80%
- Warning alerts for out-of-range conditions

## 🚧 Next version

- Bluetooth/WiFi configuration interface
- Medication logging
- Multiple medication reminders
- Mobile app integration

## 📄 License

MIT License

## 📧 Contact

kumaragerv.22@uom.lk
