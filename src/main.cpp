#include <LiquidCrystal_I2C.h>
#include <RtcDS1302.h>
#include <SoftwareSerial.h>
#include <ThreeWire.h>
#include <Wire.h>

// ! OBJECTS DEFINITIONS
LiquidCrystal_I2C lcd(0x27, 16, 2);
ThreeWire myWire(7, 6, 8);  // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);
SoftwareSerial BTSerial(3, 5);  // RX, TX

// ! VARIABLES, DEFINITIONS AND STRUCTURES
enum State { SETUP = 0, DISPENSE = 1 };
State currentState = SETUP;

struct Medicine {
  String name;
  int interval;
  int iterations;
  int baseHour;
  int baseMinute;
  int nextHour;
  int nextMinute;
  bool active;
  int lastDispensedHour;
  int lastDispensedMinute;
};

Medicine med1, med2;

// ! FUNCTIONS
String receiveData(bool expectInt = false) {
  unsigned long startTime = millis();
  while (millis() - startTime < 300000) {  // 5-minute timeout
    if (BTSerial.available()) {
      String buffer = BTSerial.readStringUntil('\n');
      buffer.trim();

      if (expectInt) {
        return String(buffer.toInt());  // ✅ Convert `int` back to `String`
      } else {
        return buffer;  // ✅ Return the received string
      }
    }
  }
  return "";  // Return empty string if timeout
}

int NewInstance() {
  BTSerial.println("1");

  Serial.println("Waiting for Med1 Name...");
  med1.name = receiveData();

  Serial.println("Waiting for Med1 Interval...");
  med1.interval =
      receiveData(true).toInt();  // ✅ Convert received String to int

  Serial.println("Waiting for Med1 Iterations...");
  med1.iterations = receiveData(true).toInt();

  Serial.println("Waiting for Med1 Base Hour...");
  med1.baseHour = receiveData(true).toInt();

  Serial.println("Waiting for Med1 Base Minute...");
  med1.baseMinute = receiveData(true).toInt();

  Serial.println("Waiting for Med1 Next Hour...");
  med1.nextHour = receiveData(true).toInt();

  Serial.println("Waiting for Med1 Next Minute...");
  med1.nextMinute = receiveData(true).toInt();

  Serial.println("Waiting for Med1 Active State...");
  med1.active = (receiveData() == "1");

  Serial.println("Waiting for Med1 Last Dispensed Hour...");
  med1.lastDispensedHour = receiveData(true).toInt();

  Serial.println("Waiting for Med1 Last Dispensed Minute...");
  med1.lastDispensedMinute = receiveData(true).toInt();

  // Check if Med2 is active
  Serial.println("Waiting for Med2 Active State...");
  med2.active = (receiveData() == "1");

  if (med2.active) {
    Serial.println("Waiting for Med2 Name...");
    med2.name = receiveData();

    Serial.println("Waiting for Med2 Interval...");
    med2.interval = receiveData(true).toInt();

    Serial.println("Waiting for Med2 Iterations...");
    med2.iterations = receiveData(true).toInt();

    Serial.println("Waiting for Med2 Base Hour...");
    med2.baseHour = receiveData(true).toInt();

    Serial.println("Waiting for Med2 Base Minute...");
    med2.baseMinute = receiveData(true).toInt();

    Serial.println("Waiting for Med2 Next Hour...");
    med2.nextHour = receiveData(true).toInt();

    Serial.println("Waiting for Med2 Next Minute...");
    med2.nextMinute = receiveData(true).toInt();

    Serial.println("Waiting for Med2 Last Dispensed Hour...");
    med2.lastDispensedHour = receiveData(true).toInt();

    Serial.println("Waiting for Med2 Last Dispensed Minute...");
    med2.lastDispensedMinute = receiveData(true).toInt();
  }

  Serial.println("Setup Successful");
  currentState = DISPENSE;
  return 1;
}

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();
  Rtc.Begin();
  Serial.begin(9600);
  BTSerial.begin(9600);

  Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  lcd.setCursor(0, 0);
  lcd.print("RTC DS1302 Ready");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);

  // LOAD USER FROM SD HERE

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   PillOtter");
  lcd.setCursor(0, 1);
  lcd.print("Connect 2 setup");
}

void loop() {
  switch (currentState) {
    case SETUP:
      Serial.println("State: SETUP");

      if (BTSerial.available()) {
        String receivedData = BTSerial.readStringUntil('\n');
        receivedData.trim();
        Serial.println("RECEIVED: ");
        Serial.println(receivedData);

        if (receivedData == "check") {
          BTSerial.println("1");
        } else if (receivedData == "NewInstance") {
          int confirmation = NewInstance();
          BTSerial.println(confirmation);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Setup Successful");

        } else if (receivedData == "Clear") {
          BTSerial.println("CLEAR NA CLEAR");
          Serial.println("USER CLEARED");
          currentState = SETUP;
        }
      }
      break;

    case DISPENSE:
      // Serial.println("State: DISPENSE");
      // Serial.println("What data do you need?");
      if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input == "med1") {
          Serial.println(med1.name);
        } else if (input == "med2") {
          Serial.println(med2.name);
        }
      }

      break;
  }
}