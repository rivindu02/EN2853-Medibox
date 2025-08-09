/*
 * Medibox - ESP32 Project (Fixed Version)
 * 
 * This program creates a medicine reminder system with the following features:
 * - Gets accurate time from NTP server
 * - Allows setting time zone with 30-minute increments
 * - Supports setting and managing multiple alarms
 * - Monitors temperature and humidity
 * - Provides visual and audio alerts
*/

/*
 * Medibox - Medicine Reminder System
 * Copyright (c) 2025 Kumarage R V
 * Licensed under the MIT License
 */

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT22 Configuration
#define DHTPIN 12
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Button Pin Definitions
#define BTN_UP 33
#define BTN_OK 32
#define BTN_DOWN 35
#define BTN_CANCEL 25

// Output Pin Definitions
#define LED_PIN 15
#define BUZZER_PIN 5

// Button debounce time in milliseconds
#define DEBOUNCE_TIME 250
#define BUTTON_DELAY 50    // Add a small delay after button press


// Button States - Renamed to avoid conflicts
enum Button {NONE, UP, OK_BTN, DOWN, CANCEL_BTN};

// Menu States
enum MenuState {
  MAIN_MENU,
  SET_TIMEZONE,
  SET_ALARM_1,
  SET_ALARM_2,
  VIEW_ALARMS,
  DELETE_ALARM,      // Keep this as the main delete menu entry
  DELETE_ALARM_1,    // New state for deleting alarm 1
  DELETE_ALARM_2,    // New state for deleting alarm 2
  NORMAL_DISPLAY
};

// Submenu States for alarm setting
enum AlarmSettingState {
  SETTING_HOUR,
  SETTING_MINUTE,
  CONFIRM_ALARM
};

// --- Mode selection globals ---
enum AppMode { MODE_NONE, MODE_ALARM, MODE_IOT };
AppMode selectedMode = MODE_NONE;



/* === Node-RED IoT Mode Additions === */

#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// --- MQTT and Wi-Fi ---
bool wifiConnected = false;
WiFiClient espClient;
PubSubClient client(espClient);
const char* mqtt_server = "test.mosquitto.org";


// --- LDR & Servo Pins ---
#define LDR_PIN    34    // Analog pin for LDR
#define SERVO_PIN  13    // PWM pin for Servo
Servo lightServo;

// --- Sampling Buffers & Intervals ---
const int MAX_LDR_SAMPLES = 60;          // Maximum buffer size
float ldrSamples[MAX_LDR_SAMPLES];
int sampleIndex     = 0;
unsigned long lastSampleTime = 0;
unsigned long lastSendTime   = 0;
int ts = 5;       // sampling interval (s)
int tu = 120;     // sending interval (s)

// --- Servo-Control Parameters ---
float thetaOffset = 30.0;    // θoffset in degrees
float gammaFactor = 0.75;    // γ factor
float Tmed        = 30.0;    // ideal storage temp



// Global Variables
MenuState currentState = NORMAL_DISPLAY;
AlarmSettingState alarmSettingState = SETTING_HOUR;
int menuPosition = 0;
float timeZoneOffset = 0.0; // Changed to float to support 30min increments
bool alarm1Active = false;
bool alarm2Active = false;
int alarm1Hour = 0, alarm1Minute = 0;
int alarm2Hour = 0, alarm2Minute = 0;
int settingHour = 0, settingMinute = 0;
bool alarmRinging = false;
int alarmRingingNum = 0;
unsigned long alarmStartTime = 0;
bool alarmSnoozing = false;
unsigned long snoozeStartTime = 0;
const int SNOOZE_DURATION = 5 * 60 * 1000; // 5 minutes in milliseconds



// Button debouncing variables
unsigned long lastButtonPressTime = 0;
bool menuInitialized = false;
int deletePosition = 0;

// Wi-Fi Credentials
const char* ssid = "Wokwi-GUEST";   
const char* password = ""; 

// NTP Configuration
const char* ntpServer = "pool.ntp.org";

// Temperature and humidity healthy ranges
const float MIN_HEALTHY_TEMP = 24.0;
const float MAX_HEALTHY_TEMP = 32.0;
const float MIN_HEALTHY_HUMIDITY = 65.0;
const float MAX_HEALTHY_HUMIDITY = 80.0;


float lastLdrAvg = 0;
float lastTemperature = 0;
float lastHumidity = 0;
int   lastServoAngle = 0;



// Function Prototypes
void print_line(String message, int x = 0, int y = 0, int size = 1, bool clear = true);
void print_time_now();
void update_time();
void update_time_with_check_alarm();
void ring_alarm(int alarmNum);
Button check_button_press();
void go_to_menu();
void run_mode();
void set_timezone();
void set_alarm(int alarmNum);
void view_alarms();
void check_temp();
void stop_alarm(bool snooze = false);
void check_snooze();
void display_alarm_setting(int alarmNum);
void handle_alarm_setting(int alarmNum, Button value);
String format_timezone(float tz);
void display_delete_alarm_menu();
void delete_alarm_1();
void delete_alarm_2();
void show_mode_selection_menu();
void connect_wifi_once();
void reconnect_mqtt();
void callback(char* topic, byte* payload, unsigned int length);
void update_light_and_servo_control();
void display_iot_readings();

void setup() {
  Serial.begin(115200);

  // Initialize the OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Medibox starting...");
  display.display();
  delay(1000);

  // Initialize DHT sensor
  dht.begin();
  
  // Initialize pins
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_CANCEL, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Turn off LED and buzzer
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Connect to Wi-Fi
  print_line("Connecting to WiFi..");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(500);
    display.print(".");
    display.display();
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    print_line("WiFi connected!");
    
    Serial.print("WiFi connected!  IP = ");
    Serial.println(WiFi.localIP());
    
    // Initialize and get time from NTP server
    configTime(timeZoneOffset * 3600, 0, ntpServer);
    print_line("Time synchronized");
  } else {
    print_line("WiFi connection failed");
  }
  
  delay(1000);
  print_line("Medibox ready!");
  delay(1000);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  lightServo.attach(SERVO_PIN);
  // after all pin/display initialization...
  show_mode_selection_menu();

}

void loop() {
  if (selectedMode == MODE_ALARM) {
    check_snooze();
    
    // Only run normal display when alarm is not ringing
    if (!alarmRinging) {
      if (currentState == NORMAL_DISPLAY) {
        update_time_with_check_alarm();
        check_temp();
        
        Button pressedButton = check_button_press();
        if (pressedButton == OK_BTN) {
          go_to_menu();
        }
      } else {
        run_mode();
      }
    } 
    // Special handling when alarm is ringing - keep showing alarm and check buttons
    else {
      Button pressedButton = check_button_press();
      if (pressedButton == CANCEL_BTN) {
        stop_alarm(false); // Stop
      } else if (pressedButton == UP) {
        stop_alarm(true); // Snooze
      }
      
      // Keep displaying alarm message
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(10, 10);
      display.println("MEDICINE");
      display.setCursor(10, 30);
      display.println("TIME!");
      display.setTextSize(1);
      display.setCursor(30, 50);
      display.println("Alarm " + String(alarmRingingNum));
      display.setCursor(0, 55);
      display.println("UP=Snooze, CANCEL=Stop");
      display.display();
      
      // Pulse the LED and buzzer periodically
      if (millis() % 2000 < 200) {
        digitalWrite(LED_PIN, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);
      } else if (millis() % 2000 < 400) {
        digitalWrite(LED_PIN, LOW);
        digitalWrite(BUZZER_PIN, LOW);
      }
    }
  }
  else if (selectedMode == MODE_IOT) {
    connect_wifi_once();
    reconnect_mqtt();
    client.loop();
    update_light_and_servo_control();
    display_iot_readings();

    // If user presses CANCEL, go back to mode menu
    Button pressedButton = check_button_press();
    if (pressedButton == CANCEL_BTN) {
      
      selectedMode = MODE_NONE;
      show_mode_selection_menu();
      delay(DEBOUNCE_TIME);
      pressedButton = NONE; // Reset button state
    }
  }

}




void display_iot_readings() {
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.print("Light: "); 
  display.print(lastLdrAvg, 3);

  display.setCursor(0, 10);
  display.print("Temp: "); 
  display.print(lastTemperature, 1);
  display.print(" C");

  display.setCursor(0, 20);
  display.print("Hum:  "); 
  display.print(lastHumidity, 1);
  display.print(" %");

  display.setCursor(0, 30);
  display.print("Servo: ");
  display.print(lastServoAngle);

  display.setCursor(0, 50);
  display.println("CANCEL => Menu");

  display.display();
}

// Shows OLED menu so user picks Alarm vs IoT mode
void show_mode_selection_menu() {
  int sel = 0;
  bool done = false;
  while (!done) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SELECT MODE:");
    display.println(sel == 0 ? "> Medibox Alarm" : "  Medibox Alarm");
    display.println(sel == 1 ? "> IoT Node-RED"    : "  IoT Node-RED");
    display.println("\nUP/DOWN + OK");
    display.display();

    Button b = check_button_press();
    if (b == UP || b == DOWN) {
      sel = 1 - sel;          // toggle between 0 and 1
      
       // WAIT until button is physically released
      while (digitalRead(BTN_UP) == LOW || digitalRead(BTN_DOWN) == LOW) {
        delay(10);
      }
      // FULL debounce delay
      delay(DEBOUNCE_TIME);
      

    } else if (b == OK_BTN) {
      while (digitalRead(BTN_OK) == LOW) {
        delay(10);
      }
      delay(DEBOUNCE_TIME);
      done = true;
      selectedMode = (sel == 0 ? MODE_ALARM : MODE_IOT);
    }
    else{delay(20);}
  }
}

// Connect to Wi‑Fi (non-blocking after first success)
void connect_wifi_once() {
  if (wifiConnected) return;
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
    // small delay to allow other tasks
    delay(100);
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    configTime(timeZoneOffset * 3600, 0, ntpServer);
    Serial.println("WiFi + NTP ready");
  } else {
    Serial.println("WiFi connect failed");
  }
}

// Reconnect to MQTT broker if needed
void reconnect_mqtt() {
  if (client.connected() || !wifiConnected) return;
  Serial.print("MQTT connecting...");
  if (client.connect("ESP32Client85939022")) {
    Serial.println("connected");
    // subscribe to control topics
    client.subscribe("medibox/ts");
    client.subscribe("medibox/tu");
    client.subscribe("medibox/thetaOffset");
    client.subscribe("medibox/gamma");
    client.subscribe("medibox/Tmed");
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
}

// MQTT callback: update parameters
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  String msg = String((char*)payload);
  if (strcmp(topic, "medibox/ts") == 0)        ts = msg.toInt();
  else if (strcmp(topic, "medibox/tu") == 0)   tu = msg.toInt();
  else if (strcmp(topic, "medibox/thetaOffset") == 0) thetaOffset = msg.toFloat();
  else if (strcmp(topic, "medibox/gamma") == 0)       gammaFactor = msg.toFloat();
  else if (strcmp(topic, "medibox/Tmed") == 0)        Tmed = msg.toFloat();
}

// Called every loop in IoT mode
// Called every loop in IoT mode
void update_light_and_servo_control() {
  unsigned long now = millis();

  // 1) Sample LDR every ts seconds
  if (now - lastSampleTime >= (unsigned long)ts * 1000) {
    lastSampleTime = now;
    int raw = analogRead(LDR_PIN);
    float intensity_ = 1.0f - (raw / 4095.0f);

    // publish raw LDR immediately
    char buf[16];
    dtostrf(intensity_, 1, 3, buf);
    client.publish("medibox/ldr", buf);

    // store for averaging…
    int count = tu / ts;
    if (count > MAX_LDR_SAMPLES) count = MAX_LDR_SAMPLES;
    ldrSamples[sampleIndex] = intensity_;
    sampleIndex = (sampleIndex + 1) % count;
  }

  // 2) Publish average and update servo every tu seconds
  if (now - lastSendTime >= (unsigned long)tu * 1000) {
    lastSendTime = now;

    // — compute avg LDR and publish it again —
    int count = tu / ts;
    if (count > MAX_LDR_SAMPLES) count = MAX_LDR_SAMPLES;
    float sum = 0;
    for (int i = 0; i < count; i++) sum += ldrSamples[i];
    float avg = sum / count;
    lastLdrAvg = avg;

    char buf[16];
    dtostrf(avg, 1, 3, buf);
    client.publish("medibox/ldr", buf);

    // — 1) READ DHT22 —
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) {
      lastTemperature = temp;   // for your OLED
      lastHumidity    = hum;    // for your OLED

      // — PUBLISH TEMPERATURE & HUMIDITY —
      char tBuf[16], hBuf[16];
      dtostrf(temp, 1, 1, tBuf);
      dtostrf(hum,  1, 1, hBuf);
      client.publish("medibox/temperature", tBuf);
      client.publish("medibox/humidity",    hBuf);

      // — 2) COMPUTE & MOVE SERVO —
      float theta = thetaOffset
                    + (180 - thetaOffset)
                      * avg
                      * gammaFactor
                      * log((float)tu / (float)ts)
                      * (temp / Tmed);
      theta = constrain(theta, thetaOffset, 180);
      lightServo.write((int)theta);
      lastServoAngle = (int)theta;  // for your OLED

      // — PUBLISH SERVO ANGLE —
      char aBuf[8];
      snprintf(aBuf, sizeof(aBuf), "%d", lastServoAngle);
      client.publish("medibox/servoAngle", aBuf);

      Serial.print("Servo angle: ");
      Serial.println(theta);
    }
  }
}


// Print a message on the OLED display
void print_line(String message, int x, int y, int size, bool clear) {
  if (clear) {
    display.clearDisplay();
  }
  display.setTextSize(size);
  display.setCursor(x, y);
  display.println(message);
  display.display();
}

// Print the current time on the OLED
void print_time_now() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    print_line("Failed to get time");
    return;
  }
  
  char timeStr[50];
  strftime(timeStr, sizeof(timeStr), "%A %d %B\n%H:%M:%S", &timeinfo);
  print_line(String(timeStr));
}

// Update and display the time
void update_time() {
  print_time_now();
}

// Update time and check alarms
void update_time_with_check_alarm() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    print_line("Failed to get time");
    return;
  }
  
  if (!alarmRinging && !alarmSnoozing) {
    // Check if alarm 1 should ring
    if (alarm1Active && timeinfo.tm_hour == alarm1Hour && timeinfo.tm_min == alarm1Minute && timeinfo.tm_sec == 0) {
      ring_alarm(1);
      return; // Exit to prevent screen refresh
    }
    // Check if alarm 2 should ring
    else if (alarm2Active && timeinfo.tm_hour == alarm2Hour && timeinfo.tm_min == alarm2Minute && timeinfo.tm_sec == 0) {
      ring_alarm(2);
      return; // Exit to prevent screen refresh
    }
  }
  
  char timeStr[50];
  strftime(timeStr, sizeof(timeStr), "%A %d %B\n%H:%M:%S", &timeinfo);
  print_line(String(timeStr));
}

// Ring the alarm with visual and audio indicators
void ring_alarm(int alarmNum) {
  alarmRinging = true;
  alarmRingingNum = alarmNum;
  alarmStartTime = millis();
  
  // Initial buzzer pattern
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// Format timezone for display
String format_timezone(float tz) {
  String result = "UTC";
  
  // Add sign for both positive and negative
  if (tz >= 0) {
    result += "+";
  }else{
    result += "-";
  }
  
  int whole = (int)std::abs(tz);  // Use absolute value
  int frac = std::abs((int)((tz - (int)tz) * 60));  // Ensure correct fractional part
  
  if (frac == 0) {
    result += String(whole);
  } else {
    result += String(whole) + ":" + (frac == 30 ? "30" : "00");
  }
  
  
  return result;
}

// Check for button press with debouncing
Button check_button_press() {
  // Check if enough time has passed since last button press
  if (millis() - lastButtonPressTime < DEBOUNCE_TIME) {
    return NONE;
  }
  
  Button pressedButton = NONE;
  
  if (digitalRead(BTN_UP) == LOW) {
    pressedButton = UP;
  }
  else if (digitalRead(BTN_OK) == LOW) {
    pressedButton = OK_BTN;
  }
  else if (digitalRead(BTN_DOWN) == LOW) {
    pressedButton = DOWN;
  }
  else if (digitalRead(BTN_CANCEL) == LOW) {
    pressedButton = CANCEL_BTN;
  }
  
  // If a button is pressed
  if (pressedButton != NONE) {
    lastButtonPressTime = millis();
    
    // Additional anti-bounce delay
    delay(BUTTON_DELAY);
    
    return pressedButton;
  }
  
  return NONE;
}

// Navigate to the main menu
void go_to_menu() {
  currentState = MAIN_MENU;
  menuPosition = 0;
  menuInitialized = false;
  
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("MENU:");
  display.println("> Set Time Zone");
  display.println("  Set Alarm 1");
  display.println("  Set Alarm 2");
  display.println("  View Alarms");
  display.println("  Delete Alarm");
  display.println("  Back");
  display.display();
}

// Run the current menu mode
void run_mode() {
  Button pressedButton = check_button_press();
  if (pressedButton == NONE) return;
  
  // Main Menu handling
  if (currentState == MAIN_MENU) {
    if (pressedButton == UP && menuPosition > 0) {
      menuPosition--;
    } else if (pressedButton == DOWN && menuPosition < 5) {
      menuPosition++;
    } else if (pressedButton == OK_BTN) {
      switch (menuPosition) {
        case 0: // Set Time Zone
          currentState = SET_TIMEZONE;
          menuInitialized = false; // Force redisplay of timezone screen
          break;
        case 1: // Set Alarm 1
          currentState = SET_ALARM_1;
          settingHour = alarm1Hour;
          settingMinute = alarm1Minute;
          alarmSettingState = SETTING_HOUR;
          display_alarm_setting(1);
          break;
        case 2: // Set Alarm 2
          currentState = SET_ALARM_2;
          settingHour = alarm2Hour;
          settingMinute = alarm2Minute;
          alarmSettingState = SETTING_HOUR;
          display_alarm_setting(2);
          break;
        case 3: // View Alarms
          currentState = VIEW_ALARMS;
          view_alarms();
          break;
        case 4: // Delete Alarm
          currentState = DELETE_ALARM;
          menuInitialized = false; // Force redisplay of delete screen
          break;
        case 5: // Back
          currentState = NORMAL_DISPLAY;
          break;
      }
    } else if (pressedButton == CANCEL_BTN) {
      currentState = NORMAL_DISPLAY;
    }
    
    if (pressedButton == UP || pressedButton == DOWN) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("MENU:");
      display.println(menuPosition == 0 ? "> Set Time Zone" : "  Set Time Zone");
      display.println(menuPosition == 1 ? "> Set Alarm 1" : "  Set Alarm 1");
      display.println(menuPosition == 2 ? "> Set Alarm 2" : "  Set Alarm 2");
      display.println(menuPosition == 3 ? "> View Alarms" : "  View Alarms");
      display.println(menuPosition == 4 ? "> Delete Alarm" : "  Delete Alarm");
      display.println(menuPosition == 5 ? "> Back" : "  Back");
      display.display();
    }
  } 
  // Timezone Setting screen
  else if (currentState == SET_TIMEZONE) {
    // Display timezone screen if not already displayed
    if (!menuInitialized) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("SET TIME ZONE");
      display.println("Current: " + format_timezone(timeZoneOffset));
      display.println("UP/DOWN to change");
      display.println("OK to confirm");
      display.println("CANCEL to go back");
      display.display();
      menuInitialized = true;
      return;
    }
    
    if (pressedButton == UP && timeZoneOffset < 12.0) {
      timeZoneOffset += 0.5; // Increment by 0.5 hours (30 minutes)
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("SET TIME ZONE");
      display.println("Current: " + format_timezone(timeZoneOffset));
      display.println("UP/DOWN to change");
      display.println("OK to confirm");
      display.println("CANCEL to go back");
      display.display();
    } 
    else if (pressedButton == DOWN && timeZoneOffset > -12.0) {
      timeZoneOffset -= 0.5; // Decrement by 0.5 hours (30 minutes)
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("SET TIME ZONE");
      display.println("Current: " + format_timezone(timeZoneOffset));
      display.println("UP/DOWN to change");
      display.println("OK to confirm");
      display.println("CANCEL to go back");
      display.display();
    } 
    else if (pressedButton == OK_BTN) {
      // Update time with new timezone
      // Convert to seconds
      int seconds = (int)(timeZoneOffset * 3600);
      configTime(seconds, 0, ntpServer);
      
      // Confirm timezone change
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("Time Zone Updated!");
      display.println(format_timezone(timeZoneOffset));
      display.display();
      delay(1500);
      
      currentState = MAIN_MENU;
      menuInitialized = false;
      go_to_menu();
    } 
    else if (pressedButton == CANCEL_BTN) {
      currentState = MAIN_MENU;
      menuInitialized = false;
      go_to_menu();
    }
  } 
  else if (currentState == SET_ALARM_1) {
    handle_alarm_setting(1, pressedButton);
  } 
  else if (currentState == SET_ALARM_2) {
    handle_alarm_setting(2, pressedButton);
  } 
  else if (currentState == VIEW_ALARMS) {
    if (pressedButton == CANCEL_BTN || pressedButton == OK_BTN) {
      currentState = MAIN_MENU;
      menuInitialized = false;
      go_to_menu();
    }
  } 
  else if (currentState == DELETE_ALARM) {
    // Display delete menu screen if not already displayed
    if (!menuInitialized) {
      if (!alarm1Active && !alarm2Active) {
        menuPosition = 2;
      } 
      // If Alarm 1 is active, start there
      else if (alarm1Active) {
        menuPosition = 0;
      } 
      // Otherwise, start at Alarm 2 if it's active
      else if (alarm2Active) {
        menuPosition = 1;
      }
      
      display_delete_alarm_menu();
      menuInitialized = true;
      return;
    }
    
    if (pressedButton == UP) {
      do {
        menuPosition = (menuPosition + 2) % 3;  // Wrap around
      } while ((menuPosition == 0 && !alarm1Active) || 
              (menuPosition == 1 && !alarm2Active));
      display_delete_alarm_menu();
    } 
    else if (pressedButton == DOWN) {
      do {
        menuPosition = (menuPosition + 1) % 3;  // Wrap around
      } while ((menuPosition == 0 && !alarm1Active) || 
              (menuPosition == 1 && !alarm2Active));
      display_delete_alarm_menu();
    }
     else if (pressedButton == OK_BTN) {
    if (menuPosition == 0 && alarm1Active) {
      currentState = DELETE_ALARM_1;
      menuInitialized = false;
    } else if (menuPosition == 1 && alarm2Active) {
      currentState = DELETE_ALARM_2;
      menuInitialized = false;
    } else if (menuPosition == 2) {
      currentState = MAIN_MENU;
      menuInitialized = false;
      go_to_menu();
    }
  } else if (pressedButton == CANCEL_BTN) {
    currentState = MAIN_MENU;
    menuInitialized = false;
    go_to_menu();
  }
} else if (currentState == DELETE_ALARM_1) {
  delete_alarm_1();
} else if (currentState == DELETE_ALARM_2) {
  delete_alarm_2();
}
}

// Display the alarm setting screen
void display_alarm_setting(int alarmNum) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("SET ALARM " + String(alarmNum));
  
  if (alarmSettingState == SETTING_HOUR) {
    display.println("Setting hour: ");
  } else if (alarmSettingState == SETTING_MINUTE) {
    display.println("Setting minute: ");
  }
  
  display.setTextSize(2);
  display.setCursor(40, 25);
  display.println(String(settingHour < 10 ? "0" : "") + String(settingHour) + ":" + 
                  String(settingMinute < 10 ? "0" : "") + String(settingMinute));
  
  display.setTextSize(1);
  display.setCursor(0, 50);
  
  if (alarmSettingState == SETTING_HOUR) {
    display.println("UP/DOWN to change, OK next");
  } else if (alarmSettingState == SETTING_MINUTE) {
    display.println("UP/DOWN to change, OK to set");
  }
  
  display.display();
}

// Handle alarm setting for a specific alarm number
void handle_alarm_setting(int alarmNum, Button pressedButton) {
  if (alarmSettingState == SETTING_HOUR) {
    if (pressedButton == UP) {
      settingHour = (settingHour + 1) % 24;
    } else if (pressedButton == DOWN) {
      settingHour = (settingHour + 23) % 24; // Wrap around from 0 to 23
    } else if (pressedButton == OK_BTN) {
      alarmSettingState = SETTING_MINUTE;
    } else if (pressedButton == CANCEL_BTN) {
      currentState = MAIN_MENU;
      menuInitialized = false;
      go_to_menu();
      return;
    }
  } else if (alarmSettingState == SETTING_MINUTE) {
    if (pressedButton == UP) {
      settingMinute = (settingMinute + 1) % 60;
    } else if (pressedButton == DOWN) {
      settingMinute = (settingMinute + 59) % 60; // Wrap around from 0 to 59
    } else if (pressedButton == OK_BTN) {
      // Save alarm
      if (alarmNum == 1) {
        alarm1Hour = settingHour;
        alarm1Minute = settingMinute;
        alarm1Active = true;
      } else {
        alarm2Hour = settingHour;
        alarm2Minute = settingMinute;
        alarm2Active = true;
      }
      
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("Alarm " + String(alarmNum) + " set for");
      display.setTextSize(2);
      display.setCursor(30, 20);
      display.println(String(settingHour < 10 ? "0" : "") + String(settingHour) + ":" + 
                      String(settingMinute < 10 ? "0" : "") + String(settingMinute));
      display.display();
      
      delay(2000);
      alarmSettingState = SETTING_HOUR; // Reset for next time
      currentState = MAIN_MENU;
      menuInitialized = false;
      go_to_menu();
      return;
    } else if (pressedButton == CANCEL_BTN) {
      currentState = MAIN_MENU;
      menuInitialized = false;
      go_to_menu();
      return;
    }
  }
  
  // If we changed something, update the display
  if (pressedButton != NONE) {
    display_alarm_setting(alarmNum);
  }
}

// View all active alarms
void view_alarms() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("ACTIVE ALARMS");
  display.println("");
  
  if (!alarm1Active && !alarm2Active) {
    display.println("No active alarms");
  } else {
    if (alarm1Active) {
      display.print("Alarm 1: ");
      display.println(String(alarm1Hour < 10 ? "0" : "") + String(alarm1Hour) + ":" + 
                      String(alarm1Minute < 10 ? "0" : "") + String(alarm1Minute));
    }
    
    if (alarm2Active) {
      display.print("Alarm 2: ");
      display.println(String(alarm2Hour < 10 ? "0" : "") + String(alarm2Hour) + ":" + 
                      String(alarm2Minute < 10 ? "0" : "") + String(alarm2Minute));
    }
  }
  
  display.println("\nPress OK/CANCEL to go back");
  display.display();
}

// Function to display delete alarm menu
// Display the delete alarm menu
void display_delete_alarm_menu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("DELETE ALARM");
  display.println("");
  
  // Adjust menuPosition to first available alarm
  if (!alarm1Active && !alarm2Active) {
    display.println("No active alarms");
    display.println("\nPress any button to exit");
    display.display();
    menuPosition = 2;  // Default to "Back" option
    return;
  }
  
  // Adjust menuPosition if current selection is not valid
  if (menuPosition == 0 && !alarm1Active) {
    menuPosition = 1;
  }
  if (menuPosition == 1 && !alarm2Active) {
    menuPosition = 2;
  }
  
  // Only show options for active alarms
  if (alarm1Active) {
    display.println(menuPosition == 0 ? "> Delete Alarm 1" : "  Delete Alarm 1");
  } else {
    display.println("  Alarm 1 not set");
  }
  
  if (alarm2Active) {
    display.println(menuPosition == 1 ? "> Delete Alarm 2" : "  Delete Alarm 2");
  } else {
    display.println("  Alarm 2 not set");
  }
  
  display.println(menuPosition == 2 ? "> Back to Menu" : "  Back to Menu");
  
  display.println("\nUP/DOWN to select");
  display.println("OK to choose");
  display.println("CANCEL to exit");
  display.display();
}

// Handle deleting alarm 1
void delete_alarm_1() {
  // Display confirmation screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("DELETE ALARM 1?");
  display.println("");
  display.println("Current setting:");
  display.setTextSize(2);
  display.setCursor(30, 20);
  display.println(String(alarm1Hour < 10 ? "0" : "") + String(alarm1Hour) + ":" + 
                  String(alarm1Minute < 10 ? "0" : "") + String(alarm1Minute));
  display.setTextSize(1);
  display.println("");
  display.println("OK to delete");
  display.println("CANCEL to go back");
  display.display();

  // Wait for user input with a dedicated loop
  while (true) {
    // Check for button press
    Button pressedButton = check_button_press();
    
    // Handle OK button - confirm deletion
    if (pressedButton == OK_BTN) {
      // Delete alarm 1
      alarm1Active = false;
      
      // Show deletion confirmation
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("ALARM 1 DELETED");
      display.println("\nPress any button");
      display.display();
      
      // Wait for any button to return to main menu
      while (true) {
        Button exitButton = check_button_press();
        if (exitButton != NONE) {
          currentState = MAIN_MENU;
          menuInitialized = false;
          go_to_menu();
          return;
        }
        // Prevent tight loop from blocking other processes
        delay(50);
      }
    }
    // Handle CANCEL button - go back to delete menu
    else if (pressedButton == CANCEL_BTN) {
      currentState = DELETE_ALARM;
      menuPosition = 0;
      menuInitialized = false;
      return;
    }
    
    // Prevent tight loop from blocking other processes
    delay(50);
  }
}

// Handle deleting alarm 2 (same approach as delete_alarm_1)
void delete_alarm_2() {
  // Display confirmation screen
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("DELETE ALARM 2?");
  display.println("");
  display.println("Current setting:");
  display.setTextSize(2);
  display.setCursor(30, 20);
  display.println(String(alarm2Hour < 10 ? "0" : "") + String(alarm2Hour) + ":" + 
                  String(alarm2Minute < 10 ? "0" : "") + String(alarm2Minute));
  display.setTextSize(1);
  display.println("");
  display.println("OK to delete");
  display.println("CANCEL to go back");
  display.display();

  // Wait for user input with a dedicated loop
  while (true) {
    // Check for button press
    Button pressedButton = check_button_press();
    
    // Handle OK button - confirm deletion
    if (pressedButton == OK_BTN) {
      // Delete alarm 2
      alarm2Active = false;
      
      // Show deletion confirmation
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("ALARM 2 DELETED");
      display.println("\nPress any button");
      display.display();
      
      // Wait for any button to return to main menu
      while (true) {
        Button exitButton = check_button_press();
        if (exitButton != NONE) {
          currentState = MAIN_MENU;
          menuInitialized = false;
          go_to_menu();
          return;
        }
        // Prevent tight loop from blocking other processes
        delay(50);
      }
    }
    // Handle CANCEL button - go back to delete menu
    else if (pressedButton == CANCEL_BTN) {
      currentState = DELETE_ALARM;
      menuPosition = 0;
      menuInitialized = false;
      return;
    }
    
    // Prevent tight loop from blocking other processes
    delay(50);
  }
}

// Check temperature and humidity
void check_temp() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  bool tempWarning = (temperature < MIN_HEALTHY_TEMP || temperature > MAX_HEALTHY_TEMP);
  bool humidityWarning = (humidity < MIN_HEALTHY_HUMIDITY || humidity > MAX_HEALTHY_HUMIDITY);

  if (tempWarning || humidityWarning) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    
    if (tempWarning && humidityWarning) {
      display.println("WARNING!");
      display.println("Temp & Humidity Issues");
    } else if (tempWarning) {
      display.println("WARNING!");
      display.println("Temperature Issue");
    } else {
      display.println("WARNING!");
      display.println("Humidity Issue");
    }
    
    display.println("");
    display.print("Temp: ");
    display.print(temperature, 1);
    display.println(" C");
    display.print("Humidity: ");
    display.print(humidity, 1);
    display.println("%");
    
    if (tempWarning) {
      display.print("Healthy temp: ");
      display.print(MIN_HEALTHY_TEMP, 0);
      display.print("-");
      display.print(MAX_HEALTHY_TEMP, 0);
      display.println("C");
    }
    
    if (humidityWarning) {
      display.print("Healthy humidity: ");
      display.print(MIN_HEALTHY_HUMIDITY, 0);
      display.print("-");
      display.print(MAX_HEALTHY_HUMIDITY, 0);
      display.println("%");
    }
    
    display.display();
    
    // Flash LED and sound buzzer
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    delay(500);
    
    delay(4000); // Time to read warning
  }
}

// Stop the currently ringing alarm
void stop_alarm(bool snooze) {
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  if (snooze) {
    alarmSnoozing = true;
    alarmRinging = false;
    snoozeStartTime = millis();
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Alarm Snoozed");
    display.println("Will ring again in 5 min");
    display.display();
    delay(2000);
  } else {
    alarmRinging = false;
    alarmSnoozing = false;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Alarm Stopped");
    display.display();
    delay(1000);
  }
}

// Check if snoozed alarm should ring again
void check_snooze() {
  if (alarmSnoozing && (millis() - snoozeStartTime >= SNOOZE_DURATION)) {
    alarmSnoozing = false;
    ring_alarm(alarmRingingNum);
  }
}


