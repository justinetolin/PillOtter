#include <LiquidCrystal_I2C.h>
#include <RtcDS1302.h>
#include <SD.h>
#include <SPI.h>
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
void receiveData(String &formData) {
  unsigned long startTime = millis();
  while (millis() - startTime < 300000) {  // 5-minute timeout
    if (BTSerial.available()) {
      formData = BTSerial.readStringUntil('\n');
      formData.trim();
      return;
    }
  }
  formData = "";
}

int NewInstance() {
  BTSerial.println("1");

  Serial.println("Waiting for Med1 Name...");
  receiveData(med1.name);

  Serial.println("Waiting for Med1 Interval...");
  String intervalStr;
  receiveData(intervalStr);
  med1.interval = intervalStr.toInt();

  Serial.println("Waiting for Med1 Iterations...");
  String iterationsStr;
  receiveData(iterationsStr);
  med1.iterations = iterationsStr.toInt();

  Serial.println("Waiting for Med1 Base Hour...");
  String baseHourStr;
  receiveData(baseHourStr);
  med1.baseHour = baseHourStr.toInt();

  Serial.println("Waiting for Med1 Base Minute...");
  String baseMinuteStr;
  receiveData(baseMinuteStr);
  med1.baseMinute = baseMinuteStr.toInt();

  Serial.println("Waiting for Med1 Next Hour...");
  String nextHourStr;
  receiveData(nextHourStr);
  med1.nextHour = nextHourStr.toInt();

  Serial.println("Waiting for Med1 Next Minute...");
  String nextMinuteStr;
  receiveData(nextMinuteStr);
  med1.nextMinute = nextMinuteStr.toInt();

  Serial.println("Waiting for Med1 Active State...");
  String activeStr;
  receiveData(activeStr);
  med1.active =
      (activeStr == "1" || activeStr == "true");  // Convert to boolean

  Serial.println("Waiting for Med1 Last Dispensed Hour...");
  String lastDispensedHourStr;
  receiveData(lastDispensedHourStr);
  med1.lastDispensedHour = lastDispensedHourStr.toInt();

  Serial.println("Waiting for Med1 Last Dispensed Minute...");
  String lastDispensedMinuteStr;
  receiveData(lastDispensedMinuteStr);
  med1.lastDispensedMinute = lastDispensedMinuteStr.toInt();

  // Repeat for Med2
  Serial.println("Waiting for Med2 Name...");
  receiveData(med2.name);

  Serial.println("Waiting for Med2 Interval...");
  receiveData(intervalStr);
  med2.interval = intervalStr.toInt();

  Serial.println("Waiting for Med2 Iterations...");
  receiveData(iterationsStr);
  med2.iterations = iterationsStr.toInt();

  Serial.println("Waiting for Med2 Base Hour...");
  receiveData(baseHourStr);
  med2.baseHour = baseHourStr.toInt();

  Serial.println("Waiting for Med2 Base Minute...");
  receiveData(baseMinuteStr);
  med2.baseMinute = baseMinuteStr.toInt();

  Serial.println("Waiting for Med2 Next Hour...");
  receiveData(nextHourStr);
  med2.nextHour = nextHourStr.toInt();

  Serial.println("Waiting for Med2 Next Minute...");
  receiveData(nextMinuteStr);
  med2.nextMinute = nextMinuteStr.toInt();

  Serial.println("Waiting for Med2 Active State...");
  receiveData(activeStr);
  med2.active = (activeStr == "1" || activeStr == "true");

  Serial.println("Waiting for Med2 Last Dispensed Hour...");
  receiveData(lastDispensedHourStr);
  med2.lastDispensedHour = lastDispensedHourStr.toInt();

  Serial.println("Waiting for Med2 Last Dispensed Minute...");
  receiveData(lastDispensedMinuteStr);
  med2.lastDispensedMinute = lastDispensedMinuteStr.toInt();

  Serial.println("Setup Successful");

  // SAVING DATA TO SD
  void saveScheduleToSD() {
    File schedF = SD.open(scheduleFile, FILE_WRITE);
    if (schedF) {
      // For Medicine 1
      if (med1.active) {
        schedF.print("Medicine 1,");
        schedF.print(med1.name);
        schedF.print(",");
        schedF.print(med1.interval);
        schedF.print(",");
        schedF.print(med1.iterations);
        schedF.print(",");
        if (med1.baseHour < 10) schedF.print("0");
        schedF.print(med1.baseHour);
        schedF.print(":");
        if (med1.baseMinute < 10) schedF.print("0");
        schedF.print(med1.baseMinute);
        schedF.print(",");
        schedF.print(med1.currentDose);
        schedF.print(",");
        if (med1.nextHour < 10) schedF.print("0");
        schedF.print(med1.nextHour);
        schedF.print(":");
        if (med1.nextMinute < 10) schedF.print("0");
        schedF.println(med1.nextMinute);
      }
      // For Medicine 2
      if (med2.active) {
        schedF.print("Medicine 2,");
        schedF.print(med2.name);
        schedF.print(",");
        schedF.print(med2.interval);
        schedF.print(",");
        schedF.print(med2.iterations);
        schedF.print(",");
        if (med2.baseHour < 10) schedF.print("0");
        schedF.print(med2.baseHour);
        schedF.print(":");
        if (med2.baseMinute < 10) schedF.print("0");
        schedF.print(med2.baseMinute);
        schedF.print(",");
        schedF.print(med2.currentDose);
        schedF.print(",");
        if (med2.nextHour < 10) schedF.print("0");
        schedF.print(med2.nextHour);
        schedF.print(":");
        if (med2.nextMinute < 10) schedF.print("0");
        schedF.println(med2.nextMinute);
      }
      schedF.close();
      Serial.println("Schedule saved to SD.");
    } else {
      Serial.println("Error opening schedule file for writing");
    }
  }

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