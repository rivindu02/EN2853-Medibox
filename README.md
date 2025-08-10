# 🏥 Medibox: Smart Medicine Reminder System

![Medibox Wokwi Simulation](simulation%20on%20wokwi.png)

## 🌟 Project Overview

Medibox is an advanced ESP32-based medicine reminder system that operates in two distinct modes, designed to help users manage their medication schedules efficiently while monitoring environmental conditions and providing IoT connectivity.

## 🎛️ Operating Modes

### Mode 1: 📱 Medicine Alarm System (LED Display)
Traditional medicine reminder with local display interface

### Mode 2: 🌐 IoT Node-RED Integration (MQTT)
Internet-connected system with remote monitoring and control via Node-RED dashboard

---

## 🚀 Mode 1: Medicine Alarm Features

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

## 🌐 Mode 2: IoT Node-RED Features

- **MQTT Connectivity**
  - Real-time data transmission to Node-RED dashboard
  - Remote monitoring and control capabilities
  - Cloud-based data logging

- **Light-Dependent Control**
  - LDR sensor for ambient light monitoring
  - Servo motor control based on light intensity
  - Configurable sampling intervals

- **Web Dashboard Access**
  - Node-RED web interface for remote monitoring
  - Real-time sensor data visualization
  - Remote parameter configuration

- **IoT Integration**
  - MQTT broker communication (test.mosquitto.org)
  - JSON data format for sensor readings
  - Configurable sampling rates (1-60 seconds)

### 🔧 Hardware Components

**Core Components:**
- ESP32 Microcontroller
- OLED Display (128x64)
- DHT22 Temperature/Humidity Sensor
- Buzzer
- LED Indicator
- Tactile Buttons (Up, Down, OK, Cancel)

**IoT Mode Additional Components:**
- LDR (Light Dependent Resistor)
- Servo Motor
- WiFi connectivity for MQTT communication

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
  - PubSubClient (for MQTT)
  - ESP32Servo
  - ArduinoJson

### Installation Steps

1. Clone the repository
2. Open in PlatformIO
3. Install required libraries (automatically handled by `platformio.ini`)
4. Configure WiFi credentials in the code
5. Upload to ESP32
6. **For IoT Mode:** Set up Node-RED dashboard using `flows.json` from `Demo version 2/`

### 🧰 Simulation

- Tested with Wokwi ESP32 Simulator
- Compatible with VS Code PlatformIO
- Simulation file available for testing

### 🌐 Node-RED Setup (IoT Mode)

1. Install Node-RED on your system
2. Import the flow from `Demo version 2/flows.json`
3. Configure MQTT broker settings
4. Access the dashboard via web browser
5. Monitor and control Medibox remotely

## 📊 Technical Highlights

- Non-blocking design
- Efficient button debouncing
- Timezone-aware time management
- Robust error handling
- Modular code structure

## 🔍 Detailed Functionality

### Mode Selection
- Boot-time mode selection between Alarm System and IoT Node-RED modes
- Button navigation to choose operating mode

### Mode 1: Time Management & Alarms
- NTP-based time synchronization
- Timezone adjustment (±12 hours, 30-minute increments)
- Two independent alarms
- Hour and minute level precision
- Snooze (5-minute intervals)
- Visual and audio alerts

### Mode 2: IoT Connectivity & Remote Access
- MQTT protocol for real-time communication
- Web-based Node-RED dashboard
- Remote sensor monitoring
- Configurable sampling intervals (1-60 seconds)
- Light-based servo control automation

### Environmental Monitoring (Both Modes)
- Temperature range: 24-32°C
- Humidity range: 65-80%
- Warning alerts for out-of-range conditions

## � Usage Instructions

### Getting Started
1. Power on the device
2. Select operating mode:
   - **Medicine Alarm**: Use button navigation on OLED display
   - **IoT Node-RED**: Access web dashboard for remote control

### Mode 1: Medicine Alarm Usage
- Use UP/DOWN buttons to navigate menus
- OK button to select/confirm
- CANCEL button to go back
- Set timezone, alarms, and monitor environment

### Mode 2: IoT Node-RED Usage
- Ensure WiFi connectivity
- Access Node-RED dashboard via web browser
- Monitor real-time sensor data
- Adjust sampling intervals remotely
- Control servo motor based on light conditions

## 🚧 Future Enhancements

- Enhanced mobile app integration
- Advanced medication logging with database
- Multiple medication reminders with custom schedules
- Voice control integration
- Advanced IoT features with cloud analytics
- Bluetooth configuration interface

## 📄 License

MIT License

## 📧 Contact

kumaragerv.22@uom.lk
