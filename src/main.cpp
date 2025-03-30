

/*
 * Medibox - ESP32 Project
 * 
 * This program creates a medicine reminder system with the following features:
 * - Gets accurate time from NTP server
 * - Allows setting time zone
 * - Supports setting and managing multiple alarms
 * - Monitors temperature and humidity
 * - Provides visual and audio alerts
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
#define BTN_CANCEL 34

// Output Pin Definitions
#define LED_PIN 15
#define BUZZER_PIN 5

// Button States - Rename conflicting enum values
enum Button {NONE, UP, OK_BTN, DOWN, CANCEL_BTN};

// Menu States
enum MenuState {
  MAIN_MENU,
  SET_TIMEZONE,
  SET_ALARM_1,
  SET_ALARM_2,
  VIEW_ALARMS,
  DELETE_ALARM,
  NORMAL_DISPLAY
};

// Global Variables
MenuState currentState = NORMAL_DISPLAY;
int menuPosition = 0;
int timeZoneOffset = 0;
bool alarm1Active = false;
bool alarm2Active = false;
int alarm1Hour = 0, alarm1Minute = 0;
int alarm2Hour = 0, alarm2Minute = 0;
bool alarmRinging = false;
int alarmRingingNum = 0;
unsigned long alarmStartTime = 0;
bool alarmSnoozing = false;
unsigned long snoozeStartTime = 0;
const int SNOOZE_DURATION = 5 * 60 * 1000; // 5 minutes in milliseconds

// Wi-Fi Credentials
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// NTP Configuration
const char* ntpServer = "pool.ntp.org";

// Temps and humidity healthy ranges
const float MIN_HEALTHY_TEMP = 24.0;
const float MAX_HEALTHY_TEMP = 32.0;
const float MIN_HEALTHY_HUMIDITY = 65.0;
const float MAX_HEALTHY_HUMIDITY = 80.0;

// Function Prototypes
void print_line(String message, int x = 0, int y = 0, int size = 1, bool clear = true);
void print_time_now();
void update_time();
void update_time_with_check_alarm();
void ring_alarm(int alarmNum);
Button wait_for_button_press();
void go_to_menu();
void run_mode();
void set_timezone();
void set_alarm(int alarmNum);
void view_alarms();
void delete_alarm();
void check_temp();
void stop_alarm(bool snooze = false);
void check_snooze();
unsigned long get_button_press_duration(int pin);

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
    
    // Initialize and get time from NTP server
    configTime(timeZoneOffset * 3600, 0, ntpServer);
    print_line("Time synchronized");
  } else {
    print_line("WiFi connection failed");
  }
  
  delay(1000);
  print_line("Medibox ready!");
  delay(1000);
}

void loop() {
  check_snooze();
  
  if (currentState == NORMAL_DISPLAY) {
    update_time_with_check_alarm();
    check_temp();
    
    Button pressedButton = wait_for_button_press();
    if (pressedButton == OK_BTN) {
      go_to_menu();
    } else if (pressedButton == CANCEL_BTN && alarmRinging) {
      stop_alarm();
    } else if (pressedButton == UP && alarmRinging) {
      stop_alarm(true); // Snooze
    }
  } else {
    run_mode();
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
    if (alarm1Active && timeinfo.tm_hour == alarm1Hour && timeinfo.tm_min == alarm1Minute && timeinfo.tm_sec == 0) {
      ring_alarm(1);
    } else if (alarm2Active && timeinfo.tm_hour == alarm2Hour && timeinfo.tm_min == alarm2Minute && timeinfo.tm_sec == 0) {
      ring_alarm(2);
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
  
  print_line("MEDICINE TIME!", 10, 10, 2);
  print_line("Alarm " + String(alarmNum), 30, 35, 1, false);
  print_line("UP=Snooze, CANCEL=Stop", 0, 50, 1, false);
  
  digitalWrite(LED_PIN, HIGH);
  
  // Buzzer pattern
  for (int i = 0; i < 3; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

// Wait for a button press and return which button was pressed
Button wait_for_button_press() {
  if (digitalRead(BTN_UP) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BTN_UP) == LOW) {
      while (digitalRead(BTN_UP) == LOW); // Wait for release
      return UP;
    }
  }
  
  if (digitalRead(BTN_OK) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BTN_OK) == LOW) {
      while (digitalRead(BTN_OK) == LOW); // Wait for release
      return OK_BTN;
    }
  }
  
  if (digitalRead(BTN_DOWN) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BTN_DOWN) == LOW) {
      while (digitalRead(BTN_DOWN) == LOW); // Wait for release
      return DOWN;
    }
  }
  
  if (digitalRead(BTN_CANCEL) == LOW) {
    delay(50); // Debounce
    if (digitalRead(BTN_CANCEL) == LOW) {
      while (digitalRead(BTN_CANCEL) == LOW); // Wait for release
      return CANCEL_BTN;
    }
  }
  
  return NONE;
}

// Navigate to the main menu
void go_to_menu() {
  currentState = MAIN_MENU;
  menuPosition = 0;
  
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
  Button pressedButton = wait_for_button_press();
  
  if (currentState == MAIN_MENU) {
    if (pressedButton == UP && menuPosition > 0) {
      menuPosition--;
    } else if (pressedButton == DOWN && menuPosition < 5) {
      menuPosition++;
    } else if (pressedButton == OK_BTN) {
      switch (menuPosition) {
        case 0: // Set Time Zone
          currentState = SET_TIMEZONE;
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(0, 0);
          display.println("SET TIME ZONE");
          display.println("Current: UTC" + String(timeZoneOffset >= 0 ? "+" : "") + String(timeZoneOffset));
          display.println("UP/DOWN to change");
          display.println("OK to confirm");
          display.println("CANCEL to go back");
          display.display();
          break;
        case 1: // Set Alarm 1
          currentState = SET_ALARM_1;
          set_alarm(1);
          break;
        case 2: // Set Alarm 2
          currentState = SET_ALARM_2;
          set_alarm(2);
          break;
        case 3: // View Alarms
          currentState = VIEW_ALARMS;
          view_alarms();
          break;
        case 4: // Delete Alarm
          currentState = DELETE_ALARM;
          delete_alarm();
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
  } else if (currentState == SET_TIMEZONE) {
    set_timezone();
  } else if (currentState == SET_ALARM_1) {
    set_alarm(1);
  } else if (currentState == SET_ALARM_2) {
    set_alarm(2);
  } else if (currentState == VIEW_ALARMS) {
    view_alarms();
  } else if (currentState == DELETE_ALARM) {
    delete_alarm();
  }
}

// Set the timezone offset from UTC
void set_timezone() {
  Button pressedButton = wait_for_button_press();
  
  if (pressedButton == UP && timeZoneOffset < 12) {
    timeZoneOffset++;
  } else if (pressedButton == DOWN && timeZoneOffset > -12) {
    timeZoneOffset--;
  } else if (pressedButton == OK_BTN) {
    // Update time with new timezone
    configTime(timeZoneOffset * 3600, 0, ntpServer);
    currentState = MAIN_MENU;
    go_to_menu();
    return;
  } else if (pressedButton == CANCEL_BTN) {
    currentState = MAIN_MENU;
    go_to_menu();
    return;
  }
  
  if (pressedButton == UP || pressedButton == DOWN) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SET TIME ZONE");
    display.println("Current: UTC" + String(timeZoneOffset >= 0 ? "+" : "") + String(timeZoneOffset));
    display.println("UP/DOWN to change");
    display.println("OK to confirm");
    display.println("CANCEL to go back");
    display.display();
  }
}

// Set an alarm time
void set_alarm(int alarmNum) {
  static int settingHour = 0;
  static int settingMinute = 0;
  static bool settingHourState = true;
  
  if (currentState == MAIN_MENU) {
    // Initial setup for setting alarm
    settingHour = (alarmNum == 1) ? alarm1Hour : alarm2Hour;
    settingMinute = (alarmNum == 1) ? alarm1Minute : alarm2Minute;
    settingHourState = true;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SET ALARM " + String(alarmNum));
    display.println("Setting hour: ");
    display.setTextSize(2);
    display.setCursor(40, 25);
    display.println(String(settingHour < 10 ? "0" : "") + String(settingHour) + ":" + 
                    String(settingMinute < 10 ? "0" : "") + String(settingMinute));
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.println("UP/DOWN to change, OK next");
    display.display();
    
    currentState = (alarmNum == 1) ? SET_ALARM_1 : SET_ALARM_2;
    return;
  }
  
  Button pressedButton = wait_for_button_press();
  
  if (settingHourState) {
    // Setting hour
    if (pressedButton == UP) {
      settingHour = (settingHour + 1) % 24;
    } else if (pressedButton == DOWN) {
      settingHour = (settingHour + 23) % 24;
    } else if (pressedButton == OK_BTN) {
      settingHourState = false;
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("SET ALARM " + String(alarmNum));
      display.println("Setting minute: ");
      display.setTextSize(2);
      display.setCursor(40, 25);
      display.println(String(settingHour < 10 ? "0" : "") + String(settingHour) + ":" + 
                      String(settingMinute < 10 ? "0" : "") + String(settingMinute));
      display.setTextSize(1);
      display.setCursor(0, 50);
      display.println("UP/DOWN to change, OK to set");
      display.display();
      return;
    } else if (pressedButton == CANCEL_BTN) {
      currentState = MAIN_MENU;
      go_to_menu();
      return;
    }
  } else {
    // Setting minute
    if (pressedButton == UP) {
      settingMinute = (settingMinute + 1) % 60;
    } else if (pressedButton == DOWN) {
      settingMinute = (settingMinute + 59) % 60;
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
      currentState = MAIN_MENU;
      go_to_menu();
      return;
    } else if (pressedButton == CANCEL_BTN) {
      currentState = MAIN_MENU;
      go_to_menu();
      return;
    }
  }
  
  if (pressedButton == UP || pressedButton == DOWN) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("SET ALARM " + String(alarmNum));
    display.println(settingHourState ? "Setting hour: " : "Setting minute: ");
    display.setTextSize(2);
    display.setCursor(40, 25);
    display.println(String(settingHour < 10 ? "0" : "") + String(settingHour) + ":" + 
                    String(settingMinute < 10 ? "0" : "") + String(settingMinute));
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.println(settingHourState ? "UP/DOWN to change, OK next" : "UP/DOWN to change, OK to set");
    display.display();
  }
}

// View all active alarms
void view_alarms() {
  Button pressedButton = wait_for_button_press();
  
  if (pressedButton == CANCEL_BTN || pressedButton == OK_BTN) {
    currentState = MAIN_MENU;
    go_to_menu();
    return;
  }
  
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

// Delete a specific alarm
void delete_alarm() {
  static int deletePosition = 0;
  
  if (currentState == MAIN_MENU) {
    deletePosition = 0;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("DELETE ALARM");
    display.println("");
    
    if (!alarm1Active && !alarm2Active) {
      display.println("No active alarms to delete");
      display.println("\nPress any button to go back");
      display.display();
      delay(2000);
      currentState = MAIN_MENU;
      go_to_menu();
      return;
    }
    
    display.println(deletePosition == 0 ? "> Alarm 1" : "  Alarm 1");
    display.println(deletePosition == 1 ? "> Alarm 2" : "  Alarm 2");
    display.println("\nUP/DOWN to select");
    display.println("OK to delete");
    display.println("CANCEL to go back");
    display.display();
    
    currentState = DELETE_ALARM;
    return;
  }
  
  Button pressedButton = wait_for_button_press();
  
  if (pressedButton == UP && deletePosition > 0) {
    deletePosition--;
  } else if (pressedButton == DOWN && deletePosition < 1) {
    deletePosition++;
  } else if (pressedButton == OK_BTN) {
    if (deletePosition == 0) {
      alarm1Active = false;
    } else {
      alarm2Active = false;
    }
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Alarm " + String(deletePosition + 1) + " deleted");
    display.display();
    
    delay(2000);
    currentState = MAIN_MENU;
    go_to_menu();
    return;
  } else if (pressedButton == CANCEL_BTN) {
    currentState = MAIN_MENU;
    go_to_menu();
    return;
  }
  
  if (pressedButton == UP || pressedButton == DOWN) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("DELETE ALARM");
    display.println("");
    display.println(deletePosition == 0 ? "> Alarm 1" : "  Alarm 1");
    display.println(deletePosition == 1 ? "> Alarm 2" : "  Alarm 2");
    display.println("\nUP/DOWN to select");
    display.println("OK to delete");
    display.println("CANCEL to go back");
    display.display();
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
  digitalWrite(BUZZER_PIN, HIGH); // Add this: Buzzer on
  delay(500);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);  // Add this: Buzzer off
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

// Get duration of a button press in milliseconds
unsigned long get_button_press_duration(int pin) {
  if (digitalRead(pin) == HIGH) {
    return 0;
  }
  
  unsigned long startTime = millis();
  while (digitalRead(pin) == LOW) {
    delay(10);
  }
  return millis() - startTime;
}