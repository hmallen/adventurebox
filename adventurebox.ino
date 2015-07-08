/*

Adventure Box v2 Development Tree

GPS and data tracking/logging device to record trip data on hikes, expeditions, etc.

TO DO:
- Sensiron SHT11x
- Add button to mark favorite locations with text input for description
-- Save in separate file
-- On next trip, alert when close
- If trip folder exists for certain date, increment tripNumber by one to prevent conflict

*/

#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
//#include <SdFat.h>
#include <SD.h>
#include <SHT1x.h>
#include <SoftwareSerial.h>
#include <SPI.h>
#include <TinyGPS.h>
#include <Wire.h>  // May not need to include this directly

#define lcdAddress 0x27

/*
- LCD
- SD
- SHT1x
- SoftwareSerial
- GPS
- Analog
- Digital
*/

// Constants
const int UPDATERATE = 5000;
const int TIMEOFFSETUTC = -4;  // Eastern Standard Time
const int MINSATELLITES = 4;  // Minimum satellite locks acquired before proceeding
const int MENUTIMEOUT = 10000; // Main menu backlight timout (milliseconds) before returning to sleep mode

// Pins
const int sdSlaveSelect = 10;
const int sdaPin = 20;
const int sclPin = 21;
const int joystickXPin = A1;
const int joystickYPin = A2;
const int joystickSelectPin = 2;

// Objects
LiquidCrystal_I2C lcd(lcdAddress, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
File logFile, infoFile;
SHT1x sht1x(sdaPin, sclPin);  // Temperature & humidity (Sensiron)
SoftwareSerial gpsSerial(8, 7);
TinyGPS gps;

// Globals
int satellites, hdop;
float gpsLat, gpsLon;
String gpsTime, gpsDate;
float gpsAltitudeFt, gpsSpeedMPH, gpsCourse;
boolean positionMarker;
boolean pathSetup = false;
int tripNumber = 1;
String directoryNameRaw;

void setup() {
  Serial.begin(9600);

  pinMode(sdSlaveSelect, OUTPUT);
  pinMode(joystickSelectPin, INPUT);

  setupFunctions();

  gpsGetData();
  if (satellites < MINSATELLITES) {
    lcd.print("Waiting for GPS.");
    while (satellites < MINSATELLITES) {
      lcd.setCursor(0, 1);
      for (int x = 0; x < 16; x++) {
        lcd.print(".");
        delay(250);
      }
      lcd.setCursor(0, 1);
      for (int x = 0; x < 16; x++) {
        lcd.print(" ");
        delay(250);
      }
      gpsGetData();
    }
  }
  lcd.setCursor(0, 1);
  lcd.print("Lock acquired.");
  delay(2500);
  lcd.clear();

  // Check if trip in progress
  boolean resumeTrip = false;
  if (EEPROM.read(255) == 1) {
    lcd.print("Resume trip?");
    lcd.setCursor(1, 1);
    lcd.print("YES");
    lcd.setCursor(5, 1);
    lcd.print("NO");
    lcd.setCursor(0, 1);
    lcd.write(254);  // "Dot" character
    boolean optionSelect = true;
    boolean exitMenu = false;
    while (true) {
      switch (joystickSelect()) {
        case 0:
          break;
        case 2:
          if (optionSelect == true) {
            optionSelect = false;
            lcd.setCursor(0, 1);
            lcd.print(" ");
            lcd.setCursor(5, 1);
            lcd.write(254);  // "Dot" character
          }
          break;
        case 4:
          if (optionSelect == false) {
            optionSelect = true;
            lcd.setCursor(5, 1);
            lcd.print(" ");
            lcd.setCursor(0, 1);
            lcd.write(254);  // "Dot" character
          }
          break;
        case 5:
          if (optionSelect == true) resumeTrip = true;
          else resumeTrip = false;
          exitMenu = true;
          break;
        default:
          break;
      }
      if (exitMenu == true) {
        lcd.clear();
        break;
      }
      delay(10);
    }
  }

  if (resumeTrip == false) mainMenu();
  else {
    lcd.print("Resuming trip.");
    // INSERT NAME OF RESUMED TRIP HERE ON LCD
    delay(5000);
    lcd.clear();
  }
}

void loop() {
  lcd.noBacklight();
  EEPROM.write(255, 1);
  while (true) {
    // Trip logging
  }
}

void gpsGetData() {
  boolean newdata = false;
  unsigned long start = millis();
  while (millis() - start < 1000) {
    if (feedgps()) newdata = true;
  }
  if (newdata) gpsdump(gps);
}

// Get and process GPS data
void gpsdump(TinyGPS &gps) {
  float flat, flon;
  unsigned long age;

  String monthFormatted = "";
  String dayFormatted = "";
  String hourFormatted = "";
  String minuteFormatted = "";
  String secondFormatted = "";

  satellites = gps.satellites();
  hdop = gps.hdop();

  gps.f_get_position(&flat, &flon, &age);

  gpsLat = flat;
  gpsLon = flon;

  int year;
  byte month, day, hour, minute, second, hundredths;

  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);

  if (hour == 0) hour = 20;
  else if (hour > 3) hour = hour + TIMEOFFSETUTC;
  else {  // Poorly devised switch/case. Fxn below fails to deliver outside of EST
    switch (hour) {
      case 1:
        hour = 21;
        break;
      case 2:
        hour = 22;
        break;
      case 3:
        hour = 23;
        break;
      default:
        break;
    }
  }

  // Time String
  if (hour < 10) hourFormatted = "0" + String(hour);
  else hourFormatted = String(hour);
  if (minute < 10) minuteFormatted = "0" + String(minute);
  else minuteFormatted = String(minute);
  if (second < 10) secondFormatted = "0" + String(second);
  else secondFormatted = String(second);

  gpsTime = hourFormatted + ":" + minuteFormatted + ":" + secondFormatted;

  // Date String
  if (month < 10) monthFormatted = "0" + String(month);
  else monthFormatted = String(month);
  if (day < 10) dayFormatted = "0" + String(day);
  else dayFormatted = String(day);

  if (pathSetup == true) {
    pathSetup = false;
    directoryNameRaw = monthFormatted + dayFormatted + String(year) + "_" + String(tripNumber);
  }

  gpsDate = monthFormatted + "/" + dayFormatted + "/" + year;

  gpsAltitudeFt = gps.f_altitude();
  gpsCourse = gps.f_course();
  gpsSpeedMPH = gps.f_speed_mph();
}

// Feed data as it becomes available
boolean feedgps() {
  while (gpsSerial.available()) {
    if (gps.encode(gpsSerial.read()))
      return true;
  }
  return false;
}

void sdWriteData() {
  if (!logFile) programError(7);
  String logDateTimeString = gpsDate + "," + gpsTime;
  String logDataString = String(satellites) + "," + String(hdop) + "," + String(gpsAltitudeFt) + "," + String(gpsSpeedMPH) + "," + String(gpsCourse);
  logFile.print(logDateTimeString);
  logFile.print(F(","));
  logFile.print(gpsLat, 6);
  logFile.print(F(","));
  logFile.print(gpsLon, 6);
  logFile.print(F(","));
  logFile.print(logDataString);
  logFile.print(F(","));
  logFile.print(positionMarker);
  logFile.print(F(","));
  if (positionMarker == true) {
    // Execute text input function for marker comment
    logFile.println(F("COMMENT"));
  }
  else logFile.println(F("---"));
  logFile.flush();
  logFile.close();
}

void sdWriteInfo(String infoString) {
  if (!infoFile) programError(8);
  String infoDateTimeString = gpsDate + "," + gpsTime;
  infoFile.close();
}

void serialPrintData() {
  String logDateTimeString = gpsDate + "," + gpsTime;
  String logDataString = String(satellites) + "," + String(hdop) + "," + String(gpsAltitudeFt) + "," + String(gpsSpeedMPH) + "," + String(gpsCourse);
  Serial.print(logDateTimeString);
  Serial.print(F(","));
  Serial.print(gpsLat, 6);
  Serial.print(F(","));
  Serial.print(gpsLon, 6);
  Serial.print(F(","));
  Serial.print(logDataString);
  Serial.print(F(","));
  Serial.print(positionMarker);
  Serial.print(F(","));
  if (positionMarker == true) {
    // Execute text input function for marker comment
    Serial.println(F("COMMENT"));
  }
  else Serial.println(F("---"));
  Serial.flush();
}

byte joystickSelect() {
  int buttonState = digitalRead(joystickSelectPin);
  int xMapped = map(analogRead(joystickXPin), 0, 1023, 0, 100);
  int yMapped = map(analogRead(joystickYPin), 0, 1023, 0, 100);

  if (yMapped > 75) return 1;
  else if (xMapped > 75) return 2;
  else if (yMapped < 25) return 3;
  else if (xMapped < 25) return 4;
  else if (buttonState == 1) return 5;
  else return 0;
}

void mainMenu() {
  boolean newTrip = false;
  lcd.print("Begin new trip?");
  lcd.setCursor(1, 1);
  lcd.print("YES");
  lcd.setCursor(5, 1);
  lcd.print("NO");
  lcd.setCursor(0, 1);
  lcd.write(254);  // "Dot" character
  boolean optionSelect = true;
  boolean exitMenu = false;
  for (unsigned long x = millis(); (millis() - x) < MENUTIMEOUT; ) {
    switch (joystickSelect()) {
      case 0:
        break;
      case 2:
        if (optionSelect == true) {
          optionSelect = false;
          lcd.setCursor(0, 1);
          lcd.print(" ");
          lcd.setCursor(5, 1);
          lcd.write(254);  // "Dot" character
        }
        break;
      case 4:
        if (optionSelect == false) {
          optionSelect = true;
          lcd.setCursor(5, 1);
          lcd.print(" ");
          lcd.setCursor(0, 1);
          lcd.write(254);  // "Dot" character
        }
        break;
      case 5:
        if (optionSelect == true) newTrip = true;
        else newTrip = false;
        exitMenu = true;
        break;
      default:
        break;
    }
    if (exitMenu == true) {
      lcd.clear();
      break;
    }
    delay(10);
  }
  if (newTrip == false) sleepMode();
  else {
    lcd.print("Beginning trip.");
    delay(5000);
    lcd.clear();
  }
}

void tripMenu() {

}

void sleepMode() {
  lcd.noBacklight();
  while (true) {
    if (joystickSelect() == 5) {
      lcd.backlight();
      break;
    }
    delay(10);
  }
  mainMenu();
}

void setupFunctions() {
  // Need to find alternate method of testing initialization of each component and triggering error when necessary
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Adventure Box");
  lcd.setCursor(0, 1);
  lcd.print("Version 2.1");
  delay(5000);
  lcd.setCursor(0, 1);
  lcd.print("Beginning setup.");
  delay(1000);
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Initializing:");
  delay(1000);

  // SD Initialization
  lcd.setCursor(0, 1);
  lcd.print("SD Card");
  delay(500);
  lcd.print(".");
  delay(500);
  lcd.print(".");
  delay(500);
  lcd.print(".");
  delay(500);
  if (!SD.begin(sdSlaveSelect)) {
    lcd.print("fail.");
    programError(0);
  }
  else lcd.print("done!");
  delay(500);
  lcd.setCursor(0, 1);
  for (int x = 0; x < 16; x++) {
    lcd.print(" ");
  }

  // SHT11 Initialization
  lcd.setCursor(0, 1);
  lcd.print("SHT11");
  delay(500);
  lcd.print(".");
  delay(500);
  lcd.print(".");
  delay(500);
  lcd.print(".");
  delay(500);
  if (!sht1x.readTemperatureC() || !sht1x.readTemperatureF() || !sht1x.readHumidity()) {
    lcd.print("fail.");
    programError(2);
  }
  else lcd.print("done!");
  delay(1000);
  lcd.setCursor(0, 1);
  for (int x = 0; x < 16; x++) {
    lcd.print(" ");
  }

  // GPS Initialization
  lcd.setCursor(0, 1);
  lcd.print("GPS");
  delay(500);
  lcd.print(".");
  delay(500);
  lcd.print(".");
  delay(500);
  lcd.print(".");
  delay(500);
  gpsSerial.begin(9600);
  lcd.print("done!");
  delay(500);
  lcd.clear();
  lcd.print("Setup complete.");
  delay(5000);
  lcd.clear();
}

void createPaths() {
  char logFileName[15];
  char infoFileName[16];
  char directoryName[9];
  pathSetup = true;
  gpsGetData();
  String logFileNameRaw = String(directoryName) + "/" + "log.txt";
  for (int x = 0; x < 15; x++) {
    char c = logFileNameRaw.charAt(x);
    logFileName[x] = c;
  }
  String infoFileNameRaw = String(directoryName) + "/" + "info.txt";
  for (int x = 0; x < 16; x++) {
    char c = infoFileNameRaw.charAt(x);
    infoFileName[x] = c;
  }

  if (!SD.mkdir(directoryName)) programError(3);
  logFile = SD.open(logFileName, FILE_WRITE);
  if (!logFile) programError(5);
  logFile.close();
  logFile = SD.open(logFileName, FILE_WRITE);
  if (!logFile) programError(6);
  infoFile.close();
}

void programError(int errorCode) {
  switch (errorCode) {
    case 0:
      Serial.println(F("SD card failed to initialize."));
      break;
    case 1:
      Serial.println(F("LCD failed to initialize."));
      break;
    case 2:
      Serial.println(F("SHT11 failed to initialize."));
      break;
    case 3:
      Serial.println(F("GPS failed to initialize."));
      break;
    case 4:
      Serial.println(F("Failed to created storage directory."));
      break;
    case 5:
      Serial.println(F("Failed to create log file."));
      break;
    case 6:
      Serial.println(F("Failed to create info file."));
      break;
    case 7:
      Serial.println(F("Failed to open log file."));
      break;
    case 8:
      Serial.println(F("Failed to open info file."));
      break;
    default:
      break;
  }
  while (true) {
    Serial.println(F("Please reset device."));
    delay(30000);
  }
}
