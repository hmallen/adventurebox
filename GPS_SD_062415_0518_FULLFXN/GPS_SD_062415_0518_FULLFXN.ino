/*

Adventure Box v1.0

GPS and data tracking/logging device to record trip data on hikes, expeditions, etc.

TO DO:
- Satellites
- HDOP
- Sensiron SHT11x

*/

#include <SD.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <TinyGPS.h>

const int UPDATERATE = 5000;  // Logging frequency (ms)
const int TIMEOFFSETUTC = -4;  // EST

TinyGPS gps;
SoftwareSerial gpsSerial(8, 7);

File logFile;

const int chipSelect = 10;

int satellites, hdop;
float gpsLat, gpsLon;
float gpsAltitudeFt, gpsCourse, gpsSpeedMPH;
String gpsDate, gpsTime;

void setup() {
  Serial.begin(57600);
  gpsSerial.begin(9600);

  pinMode(chipSelect, OUTPUT);

  Serial.print(F("Initializing SD card..."));
  if (!SD.begin(chipSelect, 11, 12, 13)) {
    Serial.println(F("initialization failed!"));
    sdFileError(1);
  }
  Serial.println(F("success!"));

  Serial.print(F("Creating log file..."));
  char filename[15];
  strcpy(filename, "GPSLOG00.TXT");
  for (uint8_t i = 0; i < 100; i++) {
    filename[6] = '0' + i / 10;
    filename[7] = '0' + i % 10;
    // create if does not exist, do not open existing, write, sync after write
    if (!SD.exists(filename)) {
      break;
    }
  }
  Serial.println(F("success!"));

  Serial.print(F("Initializing log file for writing..."));
  logFile = SD.open(filename, FILE_WRITE);
  if (!logFile) {
    Serial.println(F("failed!"));
    sdFileError(2);
  }
  Serial.println(F("success!"));
  Serial.print(F("Writing to "));
  Serial.println(filename);

  Serial.println(F("Setup complete."));
  Serial.println();
  Serial.print(F("Waiting for GPS..."));
  if (!feedgps()) {
    while (!feedgps()) {
      delay(1000);
    }
  }
  Serial.println(F("signal acquired."));
  Serial.println();
  Serial.println(F("Beginning GPS data log."));
  Serial.println();
  //digitalWrite(gpsSignalPin, LOW);
}

void loop() {
  unsigned long timerStart = millis();
  for (unsigned long timerStart = millis(); (millis() - timerStart) < (UPDATERATE - 1000); ) {
    // Executed between GPS reads
  }
  gpsGetData();
  sdWriteData();
  serialPrintData();
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
  else {  // Poorly devised seitch/case. Fxn below fails to deliver outside of EST
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
  if (!logFile) sdFileError(2);
  String logDateTimeString = gpsDate + "," + gpsTime;
  String logDataString = String(satellites) + "," + String(hdop) + "," + String(gpsAltitudeFt) + "," + String(gpsSpeedMPH) + "," + String(gpsCourse);
  logFile.print(logDateTimeString);
  logFile.print(F(","));
  logFile.print(gpsLat, 6);
  logFile.print(F(","));
  logFile.print(gpsLon, 6);
  logFile.print(F(","));
  logFile.println(logDataString);
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
  Serial.println(logDataString);
  Serial.flush();
}

void sdFileError(int errorCode) {
  if (errorCode == 1) Serial.println(F("SD card initialization failed. Please restart..."));
  else if (errorCode == 2) Serial.println(F("Failed to open log file for data writing data...halting."));

  while (true) {
    delay(10000);
  }
}
