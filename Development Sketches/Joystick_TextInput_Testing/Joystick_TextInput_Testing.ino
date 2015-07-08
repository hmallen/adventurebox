#include <LiquidCrystal_I2C.h>
#include <Wire.h>

#define lcdAddress 0x27

const int joystickXPin = A1;
const int joystickYPin = A2;
const int joystickSelectPin = 2;

byte charMarker = 32;  // 32 = Space
int charPosition = 0;
int charLine = 0;

LiquidCrystal_I2C lcd(lcdAddress, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

void setup() {
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.backlight();
  delay(500);
  /*for (int x = 0; x < 3; x++) {
    lcd.noBacklight();
    delay(500);
    lcd.backlight();
    delay(500);
  }*/
  lcd.setCursor(0, 0);
  lcd.clear();
}

void loop() {
  /*lcd.print("Position Test");
  delay(5000);
  lcd.noBacklight();
  while (true) {
    delay(10000);
  }*/
  charMarker = 65;
  byte charLast = charMarker;
  lcd.print(char(charMarker));
  while (true) {
    switch(joystickSelect()) {
      case 0:
        break;
      case 1:
        if (charMarker == 90) charMarker = 65;
        else charMarker++;
        break;
      case 2:
        // Move to next character set
        break;
      case 3:
        if (charMarker == 65) charMarker = 90;
        else charMarker--;
        break;
      case 4:
        // Move to previous character set
        break;
      case 5:
        // Select character and move over a space
        break;
      default:
        break;
    }
    
    /*byte joystickCommand = joystickSelect();
    if (joystickCommand == 0) continue;
    else if (joystickCommand == 1) {  // Up
      if (charMarker == 90) charMarker = 65;
      else charMarker++;
    }
    else if (joystickCommand == 2) {  // Right
    }
    else if (joystickCommand == 3) {  // Down
      if (charMarker == 65) charMarker = 90;
      else charMarker--;
    }
    else if (joystickCommand == 4) {  // Left
    }
    else if (joystickCommand == 5) {  // Select
    }
    else {
      lcd.clear();
      lcd.print("Error!");
      for (int x = 0; x < 3; x++) {
        lcd.noBacklight();
        delay(500);
        lcd.backlight();
        delay(500);
      }
      delay(5000);
      lcd.clear();
    }*/
    if (charMarker != charLast) {
      lcd.clear();
      lcd.print(char(charMarker));
      charLast = charMarker;
      delay(250);
    }
    delay(10);
  }
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
