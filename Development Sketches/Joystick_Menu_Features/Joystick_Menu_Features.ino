const int joystickXPin = A2;
const int joystickYPin = A3;
const int buttonPin = 2;

boolean xUp, xDown, yUp, yDown;

void setup() {
  Serial.begin(9600);
  pinMode(buttonPin, INPUT);
}

void loop() {
  int xState = analogRead(joystickXPin);
  int yState = analogRead(joystickYPin);
  int buttonState = digitalRead(buttonPin);

  int xMapped = map(analogRead(joystickXPin), 0, 1023, 0, 100);
  int yMapped = map(analogRead(joystickYPin), 0, 1023, 0, 100);

  /*Serial.print(F("X State: "));
  Serial.println(xState);
  Serial.print(F("Y State: "));
  Serial.println(yState);
  Serial.print(F("Button: "));
  Serial.println(buttonState);*/

  //Serial.print(F("X Mapped: "));
  //Serial.println(xMapped);
  //Serial.print(F("Y Mapped: "));
  //Serial.println(yMapped);
  //Serial.print(F("Button: "));
  //Serial.println(buttonState);

  switchTrigger(xMapped, yMapped);

  Serial.print(F("X: "));
  if (xUp == true) Serial.println(F("Up"));
  else if (xDown == true) Serial.println(F("Down"));
  else Serial.println(F("0"));
  delay(10);
}

void switchTrigger(int xMapped, int yMapped) {
  const int upperThresh = 75;
  const int lowerThresh = 25;

  xUp = false;
  xDown = false;
  yUp = false;
  yDown = false;

  if (xMapped < lowerThresh) xDown = true;
  else if (xMapped > upperThresh) xUp = true;
  if (yMapped < lowerThresh) yDown = true;
  else if (yMapped > upperThresh) yUp = true;
}
