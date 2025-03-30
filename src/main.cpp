#include <Arduino.h>

// Include necessary libraries
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <time.h>

// Define pins based on your connections
#define OLED_SDA 21
#define OLED_SCL 22
#define DHT_PIN 4
#define BUZZER_PIN 25
#define ALARM_LED_PIN 26  // Red LED
#define WARNING_LED_PIN 27  // Yellow LED
#define SELECT_BTN_PIN 13  // Green button
#define UP_BTN_PIN 12  // Blue button
#define DOWN_BTN_PIN 14  // Red button
#define SNOOZE_BTN_PIN 15  // Black button

// Define constants
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define DHT_TYPE DHT22

// Healthy environmental ranges
#define MIN_TEMP 24.0
#define MAX_TEMP 32.0
#define MIN_HUMIDITY 65.0
#define MAX_HUMIDITY 80.0

// WiFi credentials - using Wokwi's default WiFi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// NTP Server settings
const char* ntpServer = "pool.ntp.org";
int gmtOffset_sec = 0;  // This will be set by the user
const int daylightOffset_sec = 0;

// Initialize objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHT_PIN, DHT_TYPE);

// Global variables
struct Alarm {
  int hour;
  int minute;
  bool active;
};

Alarm alarms[2] = {{0, 0, false}, {0, 0, false}};
bool alarmRinging = false;
bool snoozeActive = false;
unsigned long alarmStartTime = 0;
unsigned long snoozeStartTime = 0;
const unsigned long snoozeDuration = 5 * 60 * 1000; // 5 minutes in milliseconds

int selectedMenuItem = 0;
int menuState = 0;  // 0: Main Menu, 1: Setting Time Zone, 2: Setting Alarm, 3: View Alarms, 4: Delete Alarm
int currentAlarmIndex = 0;  // For setting or deleting a specific alarm

// Button state tracking
bool lastSelectState = HIGH;
bool lastUpState = HIGH;
bool lastDownState = HIGH;
bool lastSnoozeState = HIGH;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(ALARM_LED_PIN, OUTPUT);
  pinMode(WARNING_LED_PIN, OUTPUT);
  pinMode(SELECT_BTN_PIN, INPUT_PULLUP);
  pinMode(UP_BTN_PIN, INPUT_PULLUP);
  pinMode(DOWN_BTN_PIN, INPUT_PULLUP);
  pinMode(SNOOZE_BTN_PIN, INPUT_PULLUP);
  
  // Initialize OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing Medibox...");
  display.display();
  delay(1000);
  
  // Initialize DHT sensor
  dht.begin();
  
  // Connect to WiFi
  connectToWifi();
  
  // Configure time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Medibox Ready!");
  display.println("Press SELECT to start");
  display.display();
  delay(2000);
}

void loop() {
  // Check buttons
  checkButtons();
  
  // Update display based on current state
  switch(menuState) {
    case 0:
      displayMainMenu();
      break;
    case 1:
      setTimeZone();
      break;
    case 2:
      setAlarm(currentAlarmIndex);
      break;
    case 3:
      viewAlarms();
      break;
    case 4:
      deleteAlarm();
      break;
    case 5:
      displayTime();
      checkAlarms();
      checkEnvironment();
      break;
  }
  
  delay(100); // Small delay to prevent display flickering
}

void connectToWifi() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Connecting to WiFi");
  display.println(ssid);
  display.display();
  
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    display.print(".");
    display.display();
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connected!");
    display.println(WiFi.localIP());
    display.display();
    delay(2000);
  } else {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi Connection Failed");
    display.println("Check credentials");
    display.display();
    delay(2000);
  }
}

void checkButtons() {
  // Read current button states
  bool currentSelectState = digitalRead(SELECT_BTN_PIN);
  bool currentUpState = digitalRead(UP_BTN_PIN);
  bool currentDownState = digitalRead(DOWN_BTN_PIN);
  bool currentSnoozeState = digitalRead(SNOOZE_BTN_PIN);
  
  // Check for SELECT button press (falling edge)
  if (currentSelectState == LOW && lastSelectState == HIGH) {
    // Handle select button based on current state
    if (menuState == 0) {
      // In main menu, select the current option
      switch(selectedMenuItem) {
        case 0: // Set Time Zone
          menuState = 1;
          break;
        case 1: // Set Alarm 1
          currentAlarmIndex = 0;
          menuState = 2;
          break;
        case 2: // Set Alarm 2
          currentAlarmIndex = 1;
          menuState = 2;
          break;
        case 3: // View Alarms
          menuState = 3;
          break;
        case 4: // Delete Alarm
          menuState = 4;
          break;
      }
    } else {
      // In other states, SELECT might have different functionality
      // We'll implement this in each specific function
    }
  }
  
  // Check for UP button press
  if (currentUpState == LOW && lastUpState == HIGH) {
    if (menuState == 0) {
      // In main menu, navigate up
      selectedMenuItem = (selectedMenuItem > 0) ? selectedMenuItem - 1 : 4;
    }
    // UP button functionality in other states will be implemented in specific functions
  }
  
  // Check for DOWN button press
  if (currentDownState == LOW && lastDownState == HIGH) {
    if (menuState == 0) {
      // In main menu, navigate down
      selectedMenuItem = (selectedMenuItem < 4) ? selectedMenuItem + 1 : 0;
    }
    // DOWN button functionality in other states will be implemented in specific functions
  }
  
  // Check for SNOOZE button press when alarm is ringing
  if (currentSnoozeState == LOW && lastSnoozeState == HIGH) {
    if (alarmRinging) {
      // Snooze the alarm
      snoozeAlarm();
    } else if (menuState != 0) {
      // Go back to main menu if not in main menu
      menuState = 0;
    }
  }
  
  // Update last button states
  lastSelectState = currentSelectState;
  lastUpState = currentUpState;
  lastDownState = currentDownState;
  lastSnoozeState = currentSnoozeState;
}

void displayMainMenu() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("===== MEDIBOX =====");
  
  // Display menu items with selection indicator
  const char* menuItems[] = {
    "1. Set Time Zone",
    "2. Set Alarm 1",
    "3. Set Alarm 2",
    "4. View Alarms",
    "5. Delete Alarm"
  };
  
  for (int i = 0; i < 5; i++) {
    if (i == selectedMenuItem) {
      display.print("> ");
    } else {
      display.print("  ");
    }
    display.println(menuItems[i]);
  }
  
  display.display();
}

void setTimeZone() {
  // This will be implemented with the time zone setting functionality
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Set Time Zone");
  display.println("Function not yet implemented");
  display.println("Press SNOOZE to return");
  display.display();
}

void setAlarm(int alarmIndex) {
  // This will be implemented with the alarm setting functionality
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Set Alarm ");
  display.println(alarmIndex + 1);
  display.println("Function not yet implemented");
  display.println("Press SNOOZE to return");
  display.display();
}

void viewAlarms() {
  // This will be implemented with the alarm viewing functionality
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("View Alarms");
  display.println("Function not yet implemented");
  display.println("Press SNOOZE to return");
  display.display();
}

void deleteAlarm() {
  // This will be implemented with the alarm deletion functionality
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Delete Alarm");
  display.println("Function not yet implemented");
  display.println("Press SNOOZE to return");
  display.display();
}

void displayTime() {
  // This will be implemented with the time display functionality
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Current Time");
  display.println("Function not yet implemented");
  display.display();
}

void checkAlarms() {
  // This will be implemented to check if any alarms need to be triggered
}

void checkEnvironment() {
  // This will read temperature and humidity and check if they're within healthy range
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  // For demonstration, just print to serial
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" °C");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  
  // Will implement display and warning logic here
}

void ringAlarm() {
  // This will be implemented with the alarm ringing functionality
  alarmRinging = true;
  alarmStartTime = millis();
  digitalWrite(ALARM_LED_PIN, HIGH);
  // Buzzer logic will be implemented here
}

void stopAlarm() {
  alarmRinging = false;
  snoozeActive = false;
  digitalWrite(ALARM_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

void snoozeAlarm() {
  alarmRinging = false;
  snoozeActive = true;
  snoozeStartTime = millis();
  digitalWrite(ALARM_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}