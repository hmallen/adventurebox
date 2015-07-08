/*

Adventure Box v2.0

GPS and data tracking/logging device to record trip data on hikes, expeditions, etc.

TO DO:
- Sensiron SHT11x
- Add button to mark favorite locations with text input for description
-- Save in separate file
-- On next trip, alert when close

*/

#include <EEPROM.h>
#include <LiquidCrystal_I2C.h>
#include <SdFat.h>
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

// Pins
const int sdSlaveSelect = 10;
const int sdaPin = 20;
const int sclPin = 21;
const int joystickXPin = A1;
const int joystickYPin = A2;
const int joystickButtonPin = 2;

// Objects
LiquidCrystal_I2C lcd(lcdAddress, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
SdFat sd;
SdFile infoFile;
SdFile logFile;
SHT1x sht1x(sdaPin, sclPin);  // Temperature & humidity (Sensiron)
SoftwareSerial gpsSerial(8, 7);
TinyGPS gps;

// Globals

void setup() {
  Serial.begin(9600);

  pinMode(chipSelect, OUTPUT);
  pinMode(selectPin, INPUT);

  setupFunctions();

  lcd.noBacklight();
}

void loop() {

}

void gpsGetData() {
  boolean newdata = false;
  unsigned long start = millis();
  while (millis() - start < 1000) { // Update every 5 seconds
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
  if (!logFile) sd.errorHalt("Failed to open log file.");
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

String joystickSelect() {
  while (true) {
    int buttonState = digitalRead(joystickButtonPin);
    int xMapped = map(analogRead(joystickXPin), 0, 1023, 0, 100);
    int yMapped = map(analogRead(joystickYPin), 0, 1023, 0, 100);

    if (buttonState == 1) return "select";
    else if (yMapped > 75) return "up";
    else if (xMapped > 75) return "right";
    else if (yMapped < 25) return "down";
    else if (xMapped < 25) return "left";
    else continue;
  }
}

void mainMenu() {
}

void tripMenu() {
}

void setupFunctions() {
  if (!lcd.begin(16, 2)) programError(0);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Adventure Box v2");
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
  delay(1000);
  lcd.print(".");
  delay(1000);
  lcd.print(".");
  delay(1000);
  lcd.print(".");
  delay(1000);
  if (!sd.begin(sdSlaveSelect, SPI_FULL_SPEED)) {
    lcd.print("fail.");
    sd.initErrorHalt();
  }
  else lcd.print("done!");
  delay(1000);
  for (int x = 0; x < 16; x++) {
    lcd.setCursor(x, 1);
    lcd.print("");
  }

  // SHT11 Initialization
  lcd.setCursor(0, 1);
  lcd.print("SHT11");
  delay(1000);
  lcd.print(".");
  delay(1000);
  lcd.print(".");
  delay(1000);
  lcd.print(".");
  delay(1000);
  if (!sht1x.readTemperatureC() || !sht1x.readTemperatureF() || !sht1x.readHumidity()) {
    lcd.print("fail.");
    programError(1);
  }
  else lcd.print("done!");
  delay(1000);
  for (int x = 0; x < 16; x++) {
    lcd.setCursor(x, 1);
    lcd.print("");
  }

  // GPS Initialization
  lcd.setCursor(0, 1);
  lcd.print("GPS");
  delay(1000);
  lcd.print(".");
  delay(1000);
  lcd.print(".");
  delay(1000);
  lcd.print(".");
  delay(1000);
  if (!gpsSerial.begin(9600)) {
    lcd.print("fail.");
    programError(2);
  }
  else lcd.print("done!");
  delay(1000);
  lcd.clear();
}

void programError(int errorCode) {
  switch (errorCode) {
    case 0:
      Serial.println(F("LCD failed to initialize."));
      break;
    case 1:
      Serial.println(F("SHT11 failed to initialize."));
      break;
    case 2:
      Serial.println(F("GPS failed to initialize."));
      break;
    default:
      break;
  }
  while (true) {
    Serial.println(F("Please reset device."));
    delay(30000);
  }
}
