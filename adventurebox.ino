/*

Adventure Box v2 Development Tree

GPS and data tracking/logging device to record trip data on hikes, expeditions, etc.

TO DO:

- If trip folder exists for certain date, increment tripNumber by one to prevent conflict

- Add text input library and menus

- Add EEPROM directory name storage for resume of trip on reboot
- Add EEPROM trip number storage?
-- Use/reset after initial directory check and creation??
- Add text input library (CAPS ONLY [KISS prototyping strategy])
- Add HIGH/LOW's to EEPROM long-term and for individual trips
- Add option to name trip
- Add ability to exit menus without input
- Check for redundant calls to lcd.clear()

EEPROM Storage:
- 0-8 = directoryName storage
- 255 = Trip in progress (0/1)
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

boolean debugMode = true;  //  Set to false to eliminate data serial prints

// Constants
const int UPDATERATE = 5000;
const int GPSREADTIME = 1000;
const int TIMEOFFSETUTC = -4;  // Eastern Standard Time
const int MINSATELLITES = 4;  // Minimum satellite locks acquired before proceeding
const int MENUTIMEOUT = 10000; // Main menu backlight timout (milliseconds) before returning to sleep or trip data logging

// Pins
const int sdSlaveSelect = 10;
const int sdaPin = 20;
const int sclPin = 21;
const int joystickXPin = A2;
const int joystickYPin = A3;
const int lightPin = A7;
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
float tempF, humidityRH;
int lightLevel;
boolean pathSetup = false;
boolean externalSensorsPresent;
String directoryNameRaw;
char directoryName[9];
boolean positionMarker;
String infoString;

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
  boolean endTrip = false;
  if (EEPROM.read(255) == 1) {
    if (menuConfirm(1) == false) {
      EEPROM.write(255, 0);
      mainMenu();
    }
    else {
      lcd.print("Resuming trip.");
      for (int x = 0; x < 9; x++) {
        char c = EEPROM.read(x);
        directoryName[x] = c;
      }
      lcd.print("Resuming trip:");
      lcd.setCursor(0, 1);
      lcd.print(directoryName);
      delay(5000);
      lcd.clear();
      createPaths(false);
    }
  }
}

void loop() {
  lcd.noBacklight();
  while (true) {
    // Trip logging
    for (unsigned long x = millis(); (millis() - x) < (UPDATERATE - GPSREADTIME); ) {
      if (joystickSelect() == 5) {
        tripMenu();
        break;
      }
    }
    gpsGetData();
    if (externalSensorsPresent == true) externalSensorsGetData();
    sdWriteData();
    if (debugMode == true) serialPrintData();
    if (positionMarker == true) {
      sdWriteInfo();
      infoString = "";
      positionMarker = false;
    }
  }
}

void gpsGetData() {
  boolean newdata = false;
  unsigned long start = millis();
  while (millis() - start < GPSREADTIME) {
    if (feedgps()) newdata = true;
  }
  if (newdata) gpsdump(gps);
}

// Get and process GPS data
void gpsdump(TinyGPS & gps) {
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
    directoryNameRaw = monthFormatted + dayFormatted + String(year) + "_";
    pathSetup = false;
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

void externalSensorsGetData() {
  tempF = sht1x.readTemperatureF();
  humidityRH = sht1x.readHumidity();
  lightLevel = map(analogRead(lightPin), 0, 1023, 0, 100);
}

void textInput(int inputType) {
  lcd.clear();
  if (inputType == 1) lcd.print("Trip Title:");
  else if (inputType == 2) lcd.print("Location Info:");
  lcd.setCursor(0, 1);
  lcd.print("_");
  int charPos = 0;  // Zero-indexed position of cursor
  lcd.setCursor(charPos, 1);
  byte charValues[17];
  for (int x = 0; x < 16; x++) {
    charValues[x] = 32;
  }
  byte charNum = 32;
  boolean exitMenu = false;
  while (true) {
    switch (joystickSelect()) {
      case 0:
        break;
      case 1:
        lcd.setCursor(charPos, 1);
        if (charNum == 65) charNum = 32;
        else if (charNum == 32) charNum = 90;
        else charNum--;
        lcd.write(charNum);
        break;
      case 2:
        if (charPos < 15) {
          charValues[charPos] = charNum;
          charPos++;
          charNum = 32;
          lcd.print("_");
        }
        break;
      case 3:
        lcd.setCursor(charPos, 1);
        if (charNum == 90) charNum = 32;
        else if (charNum == 32) charNum = 65;
        else charNum++;
        lcd.write(charNum);
        break;
      case 4:
        if (charPos >= 0) {
          charValues[charPos] = charNum;
          charPos--;
          charNum = 32;
          lcd.print("_");
        }
        break;
      case 5:
        if (menuConfirm(3) == true) exitMenu = true;
        else {
          lcd.setCursor(0, 0);
          if (inputType == 1) lcd.print("Trip Title:");
          else if (inputType == 2) lcd.print("Location info:");
          lcd.setCursor(0, 1);
          // Add print of info text in progress
          lcd.setCursor(charPos, 1);
          if (charPos != 15) {
            lcd.print("_");
            lcd.setCursor(charPos, 1);
          }
        }
        break;
      default:
        break;
    }
    if (exitMenu == true) {
      lcd.clear();
      byte newSize = 17;
      for (int x = 15; x >= 0; x--) {
        byte b = charValues[x];
        if (b == 32) newSize--;
        if (b != 32) break;
      }
      for (int x = 0; x < newSize; x++) {
        char c = charValues[x];
        infoString += c;
      }
      break;
    }
    delay(10);
  }
}

// Write all data compiled into string into main log file of SD card
void sdWriteData() {
  if (!logFile) programError(7);
  String logDateTimeString = gpsDate + "," + gpsTime;
  String logDataString = String(satellites) + "," + String(hdop) + "," + String(gpsAltitudeFt) + "," + String(gpsSpeedMPH) + "," + String(gpsCourse);
  String shtDataString = String(tempF) + "," + String(humidityRH);
  logFile.print(logDateTimeString);
  logFile.print(F(","));
  logFile.print(gpsLat, 6);
  logFile.print(F(","));
  logFile.print(gpsLon, 6);
  logFile.print(F(","));
  logFile.print(logDataString);
  logFile.print(F(","));
  if (externalSensorsPresent == true) {
    logFile.print(shtDataString);
    logFile.print(F(","));
  }
  logFile.print(positionMarker);
  logFile.print(F(","));
  if (positionMarker == true) logFile.println(infoString);
  else logFile.println(F("---"));
  logFile.flush();
  logFile.close();
}

// Write any data of choice into info file on SD card
void sdWriteInfo() {
  if (!infoFile) programError(8);
  String infoDateTimeString = gpsDate + ", " + gpsTime;
  infoFile.println(infoDateTimeString);
  infoFile.print(gpsLat, 6);
  infoFile.print(F(","));
  infoFile.println(gpsLon, 6);
  infoFile.println(infoString);
  infoFile.println();
  infoFile.flush();
  infoFile.close();
}

// Print data from sdWriteData() if desired for debugging
void serialPrintData() {
  String logDateTimeString = gpsDate + "," + gpsTime;
  String logDataString = String(satellites) + "," + String(hdop) + "," + String(gpsAltitudeFt) + "," + String(gpsSpeedMPH) + "," + String(gpsCourse);
  String shtDataString = String(tempF) + "," + String(humidityRH) + "," + String(lightLevel);
  Serial.print(logDateTimeString);
  Serial.print(F(","));
  Serial.print(gpsLat, 6);
  Serial.print(F(","));
  Serial.print(gpsLon, 6);
  Serial.print(F(","));
  Serial.print(logDataString);
  Serial.print(F(","));
  if (externalSensorsPresent == true) {
    Serial.print(shtDataString);
    Serial.print(F(","));
  }
  Serial.print(positionMarker);
  Serial.print(F(","));
  if (positionMarker == true) Serial.println(F("COMMENT"));
  else Serial.println(F("---"));
  Serial.flush();
}

// Detect and buffer digital/analog input from joystick and return positional value
byte joystickSelect() {
  int buttonState = digitalRead(joystickSelectPin);
  int xMapped = map(analogRead(joystickXPin), 0, 1023, 0, 100);
  int yMapped = map(analogRead(joystickYPin), 0, 1023, 0, 100);

  if (yMapped > 75) return 1;
  else if (xMapped > 75) return 2;
  else if (yMapped < 25) return 3;
  else if (xMapped < 25) return 4;
  else if (buttonState == 1) {
    while (digitalRead(joystickSelectPin) == 1) {
      delay(10);
    }
    return 5;
  }
  else return 0;
}

// Main menu presented when not currently in trip mode
void mainMenu() {
  lcd.clear();
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
      case 1:
        x = millis();
        break;
      case 2:
        if (optionSelect == true) {
          optionSelect = false;
          lcd.setCursor(0, 1);
          lcd.print(" ");
          lcd.setCursor(5, 1);
          lcd.write(254);  // "Dot" character
          x = millis();
        }
        break;
      case 3:
        x = millis();
        break;
      case 4:
        if (optionSelect == false) {
          optionSelect = true;
          lcd.setCursor(5, 1);
          lcd.print(" ");
          lcd.setCursor(0, 1);
          lcd.write(254);  // "Dot" character
          x = millis();
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
    EEPROM.write(255, 1);
    createPaths(true);
  }
}

// Menu presented while currently in trip mode
void tripMenu() {
  /*
  Options:
  - Current position
  - Location comment [textInput(2)]
  - End trip

  Change optionSelect boolean to menu option integer
  Need to add menu scrolling once >2 options available.
  */
  lcd.setCursor(1, 0);
  lcd.print("Mark location");
  lcd.setCursor(1, 1);
  lcd.print("End trip");
  lcd.setCursor(0, 0);
  lcd.write(254);  // "Dot" character
  int menuOption = false;
  boolean exitMenu = false;

  for (unsigned long x = millis(); (millis() - x) < MENUTIMEOUT; ) {
    switch (joystickSelect()) {
      case 0:
        break;
      case 1:
        if (menuOption == 2) {
          lcd.setCursor(0, 1);
          lcd.print(" ");
          lcd.setCursor(0, 0);
          lcd.write(254);
          menuOption = 1;
        }
        x = millis();
        break;
      case 2:
        x = millis();
        break;
      case 3:
        if (menuOption == 1) {
          lcd.setCursor(0, 0);
          lcd.print(" ");
          lcd.setCursor(0, 1);
          lcd.write(254);
          menuOption = 2;
        }
        x = millis();
        break;
      case 4:
        x = millis();
        break;
      case 5:
        exitMenu = true;
        x = millis();
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
  if (exitMenu == true) {
    switch (menuOption) {
      case 1:  // Current position
        //displayPosition();
        break;
      case 2:  // Mark position
        textInput(2);
        break;
      case 3:  // End trip
        if (menuConfirm(2) == true) {
          EEPROM.write(255, 0);
          lcd.print("Trip ended.");
          delay(5000);
          // Display trip stats here!!!!
          // displayTripStats();
          mainMenu();
        }
        else break;
      default:
        break;
    }
  }
}

boolean menuConfirm(int confirmationType) {
  lcd.clear();
  if (confirmationType == 1) lcd.print("Resume trip?");
  else if (confirmationType == 2) lcd.print("End trip?");
  else if (confirmationType == 3) lcd.print("Done with input?");
  lcd.setCursor(1, 1);
  lcd.print("YES");
  lcd.setCursor(5, 1);
  lcd.print("NO");
  lcd.setCursor(0, 1);
  lcd.write(254);  // "Dot" character
  boolean optionSelect = true;
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
        lcd.clear();
        if (optionSelect == true) return true;
        else return false;
      default:
        break;
    }
    delay(10);
  }
}

// Disables LCD backlight and waits for outside input to conserve battery life
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

// Performs all serial, software, object, and sensor initiation functions.//
////-->Would have included in main setup, but numerous one-time execution considerations were necessary.
void setupFunctions() {
  // Need to find alternate method of testing initialization of each component and triggering error when necessary
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Adventure Box");
  lcd.setCursor(0, 1);
  lcd.print("Version 2.3");
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
    lcd.print("N/A.");
    externalSensorsPresent = false;
  }
  else {
    lcd.print("done!");
    externalSensorsPresent = true;
  }
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

void createPaths(boolean resumeTrip) {
  char logFileName[15];
  char infoFileName[16];

  if (resumeTrip == false) {
    pathSetup = true;
    gpsGetData();

    while (true) {
      int tripNumber = 1;
      directoryNameRaw += String(tripNumber);
      for (int y = 0; y < 9; y++) {
        char c = directoryNameRaw.charAt(y);
        directoryName[y] = c;
      }
      if (!SD.exists(directoryName)) break;
      else tripNumber++;
    }
  }

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
  infoFile = SD.open(infoFileName, FILE_WRITE);
  if (!infoFile) programError(6);
  infoFile.close();
}

void programError(int errorCode) {
  boolean lcdError = false;
  switch (errorCode) {
    case 0:
      Serial.println(F("SD card failed to initialize."));
      break;
    case 1:
      lcdError = true;
      Serial.println(F("LCD failed to initialize."));
      break;
    /*case 2:
      Serial.println(F("SHT11 failed to initialize."));
      break;*/
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
  if (lcdError == false) {
    lcd.clear();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Program error.");
    lcd.setCursor(0, 1);
    lcd.print("Reset required.");
  }
  while (true) {
    Serial.println(F("Please reset device."));
    delay(30000);
  }
}
