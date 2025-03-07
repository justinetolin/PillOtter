#include <LiquidCrystal_I2C.h>
#include <RtcDS1302.h>
#include <SD.h>
#include <SPI.h>
#include <Servo.h>
#include <SoftwareSerial.h>
#include <ThreeWire.h>
#include <Wire.h>

// ! OBJECTS DEFINITIONS
LiquidCrystal_I2C lcd(0x27, 16, 2);
ThreeWire myWire(7, 6, 8);  // DAT, CLK, RST for RTC DS1302
RtcDS1302<ThreeWire> Rtc(myWire);

// Two servo motors for dispensing pills
Servo servo1;  // for med1
Servo servo2;  // for med2

// Define additional hardware pins
#define CSpin 53       // SD card chip select
#define IR_PIN 4       // IR sensor input pin (reads LOW when pill is taken)
#define BUZZER_PIN A7  // Buzzer output pin
#define LED_PIN 3
#define servo1pin 32
#define servo2pin 38

// ! VARIABLES, DEFINITIONS AND STRUCTURES
enum State { SETUP = 0, DISPENSE = 1 };
State currentState = SETUP;

struct Medicine {
  String name;
  int interval;    // in minutes (from hours)
  int iterations;  // doses per day (not used in this snippet)
  int baseHour;    // scheduled base hour (from app)
  int baseMinute;  // scheduled base minute
  int nextHour;    // next dispensing hour
  int nextMinute;  // next dispensing minute
  bool active;     // true if medicine is active
  int lastDispensedHour;
  int lastDispensedMinute;
  bool dispensed;  // flag to indicate if the medicine has been dispensed
  int dosesTaken;  // doses taken today
};

Medicine med1, med2;
String MedContact = "+639915176440";  // For GSM alerts

// ! FUNCTIONS

// Receive data from Serial1 (Bluetooth/GSM) with optional integer expectation.
// This function returns a String (even when expecting a number, which you then
// convert).
String receiveData(bool expectInt = false) {
  unsigned long startTime = millis();
  while (millis() - startTime < 300000) {  // 5-minute timeout
    if (Serial1.available()) {
      String buffer = Serial1.readStringUntil('\n');
      buffer.trim();
      if (expectInt) {
        // Return the integer value as a String
        return String(buffer.toInt());
      } else {
        return buffer;
      }
    }
  }
  return "";  // Return empty string if timeout
}

// ! HELPER FUNCTION: Format DateTime into "YYYY-MM-DD HH:MM"
String formatDateTime(const RtcDateTime &dt) {
  char buf[18];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d", dt.Year(), dt.Month(), dt.Day(),
          dt.Hour(), dt.Minute());
  return String(buf);
}

// ! LOG SCHED FUNCTION: Logs scheduled and actual intake times
void logSched(Medicine &med, const RtcDateTime &scheduled,
              const RtcDateTime &actual) {
  String schedStr = formatDateTime(scheduled);
  String actualStr = formatDateTime(actual);
  File logFile = SD.open("schedlog.txt", FILE_WRITE);
  if (logFile) {
    logFile.println(schedStr + "," + actualStr);
    logFile.close();
    Serial.println("Logged schedule: " + schedStr + " , " + actualStr);
  } else {
    Serial.println("Error opening schedlog.txt for writing.");
  }
}

// ! Functions for buzzer control
void beepBuzzer(int times, int duration) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
    delay(duration);
  }
}

void continuousBuzzer() { digitalWrite(BUZZER_PIN, HIGH); }

void stopBuzzer() { digitalWrite(BUZZER_PIN, LOW); }

// ! Dispense Pill Function using servo motor
void dispensePill(int compartment) {
  if (compartment == 1) {
    servo1.write(90);
    delay(3000);
    servo1.write(0);
  } else if (compartment == 2) {
    servo2.write(90);
    delay(3000);
    servo2.write(0);
  }
}

// ! Update Schedule: Adds the interval (in hours) to the actual dispensing
// time.
// Updated saveSched() function: removes the old file and rewrites new schedule
void saveSched() {
  // Remove the old schedule file if it exists.
  if (SD.exists("USERINFO.txt")) {
    SD.remove("USERINFO.txt");
  }

  File schedF = SD.open("USERINFO.txt", FILE_WRITE);
  if (schedF) {
    schedF.print(MedContact);
    schedF.print(",");
    if (med1.active) {
      schedF.print(med1.name);
      schedF.print(",");
      schedF.print(med1.interval);
      schedF.print(",");
      schedF.print(med1.iterations);
      schedF.print(",");
      schedF.print(med1.baseHour);
      schedF.print(",");
      schedF.print(med1.baseMinute);
      schedF.print(",");
      schedF.print(med1.nextHour);
      schedF.print(",");
      schedF.print(med1.nextMinute);
      schedF.print(",");
      schedF.print(med1.active);
      schedF.print(",");
      schedF.print(med1.lastDispensedHour);
      schedF.print(",");
      schedF.print(med1.lastDispensedMinute);
      schedF.print(",");
      schedF.print(med2.active);
    }

    if (med2.active) {
      schedF.print(",");
      schedF.print(med2.name);
      schedF.print(",");
      schedF.print(med2.interval);
      schedF.print(",");
      schedF.print(med2.iterations);
      schedF.print(",");
      schedF.print(med2.baseHour);
      schedF.print(",");
      schedF.print(med2.baseMinute);
      schedF.print(",");
      schedF.print(med2.nextHour);
      schedF.print(",");
      schedF.print(med2.nextMinute);
      schedF.print(",");
      schedF.print(med2.lastDispensedHour);
      schedF.print(",");
      schedF.print(med2.lastDispensedMinute);
    }
    schedF.println();
    schedF.close();
    Serial.println("Schedule saved to SD.");
  } else {
    Serial.println("Failed to open USERINFO.txt for writing.");
  }
}

// Updated updateSchedule() function: updates the Medicine struct and then
// rewrites USERINFO.txt
// void updateSchedule(Medicine &med, int actualHour, int actualMinute) {
//   Serial.println("Updating schedule...");

//   int actualMins = actualHour * 60 + actualMinute;
//   int intervalMins = med.interval * 60;

//   // Increase the count of doses taken today
//   med.dosesTaken++;

//   // Check if we need another dose today
//   if (med.dosesTaken < med.iterations) {
//     // Schedule next dose today
//     int newTime = actualMins + intervalMins;
//     med.nextHour = (newTime / 60) % 24;
//     med.nextMinute = newTime % 60;
//   } else {
//     // Reset for the next day
//     med.dosesTaken = 0;
//     med.nextHour = med.baseHour;
//     med.nextMinute = med.baseMinute;
//   }

//   // Update last dispensed time
//   med.lastDispensedHour = actualHour;
//   med.lastDispensedMinute = actualMinute;
//   med.dispensed = false;  // Ready for next cycle

//   // Debugging Output
//   Serial.print("Next dose for ");
//   Serial.print(med.name);
//   Serial.print(" at ");
//   Serial.print(med.nextHour);
//   Serial.print(":");
//   Serial.println(med.nextMinute);

//   saveSched();  // Save updated schedule to SD
// }

void updateSchedule(Medicine &med, int actualHour, int actualMinute) {
  Serial.println("Updating schedule...");

  int actualMins =
      actualHour * 60 + actualMinute;  // Convert current time to minutes
  int intervalMins = med.interval;     // Interval is already in minutes

  // Increase the count of doses taken today
  med.dosesTaken++;

  // Check if we need another dose today
  if (med.dosesTaken < med.iterations) {
    // Schedule next dose today
    int newTime = actualMins + intervalMins;  // Add interval to current time
    med.nextHour = (newTime / 60) % 24;       // Convert back to hours
    med.nextMinute = newTime % 60;            // Convert back to minutes
  } else {
    // Reset for the next day
    med.dosesTaken = 0;
    med.nextHour = med.baseHour;
    med.nextMinute = med.baseMinute;
  }

  // Update last dispensed time
  med.lastDispensedHour = actualHour;
  med.lastDispensedMinute = actualMinute;
  med.dispensed = false;  // Ready for next cycle

  // Debugging Output
  Serial.print("Next dose for ");
  Serial.print(med.name);
  Serial.print(" at ");
  Serial.print(med.nextHour);
  Serial.print(":");
  Serial.println(med.nextMinute);

  saveSched();  // Save updated schedule to SD
}

void simulateTime(RtcDateTime &now, int minutesToAdd) {
  int totalMinutes = now.Hour() * 60 + now.Minute() + minutesToAdd;
  int newHour = (totalMinutes / 60) % 24;
  int newMinute = totalMinutes % 60;
  now = RtcDateTime(now.Year(), now.Month(), now.Day(), newHour, newMinute,
                    now.Second());
}

void resetDailyDoses() {
  RtcDateTime now = Rtc.GetDateTime();
  if (now.Hour() == 0 && now.Minute() == 0) {  // Midnight Reset
    med1.dosesTaken = 0;
    med2.dosesTaken = 0;
    Serial.println("Daily doses reset!");
  }
}

// ! Check and Dispense: Called every 10 seconds in DISPENSE state.
void checkAndDispense() {
  // Get current time from RTC
  RtcDateTime now = Rtc.GetDateTime();
  int currentHour = now.Hour();
  int currentMinute = now.Minute();

  // Reset dispensed flag if the next scheduled time is reached
  if (med1.active && currentHour == med1.nextHour &&
      currentMinute == med1.nextMinute) {
    med1.dispensed = false;
  }

  // Process Med1 if active, scheduled time matches, and not yet dispensed
  if (med1.active && currentHour == med1.nextHour &&
      currentMinute == med1.nextMinute && !med1.dispensed) {
    Serial.println("Dispensing Med1...");
    beepBuzzer(2, 200);
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED HIGHHH");
    dispensePill(1);
    Serial.println("servo doneee");
    beepBuzzer(2, 200);
    continuousBuzzer();
    Serial.println("BUZZZ CONTINUOS");
    // Keep buzzing until IR sensor indicates pill taken (IR sensor reads LOW)
    // Wait for the IR sensor to detect that the pill has been taken
    while (digitalRead(IR_PIN) != LOW) {  // Wait for LOW (pill detected)
      RtcDateTime now = Rtc.GetDateTime();
      int currentHour = now.Hour();
      int currentMinute = now.Minute();
      int currentSecond = now.Second();
      Serial.print("Current time: ");
      Serial.print(currentHour);
      Serial.print(":");
      Serial.println(currentMinute);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Current Time: ");
      lcd.setCursor(0, 1);
      lcd.print(currentHour);
      lcd.print(":");
      lcd.print(currentMinute);
      lcd.print(":");
      lcd.print(currentSecond);
      delay(1000);

      // Send alert if the pill is not taken within the specified time
      int currentTime = millis();
      int interval = 18000;  // 18 seconds (for testing)
      bool texted = false;
      if (currentTime >= interval && !texted) {
        sendAlert("Ayaw uminom ni patient maamsir");
        texted = true;
      }
    }

    // Pill has been taken (IR sensor reads HIGH)
    stopBuzzer();
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED LOWWW");
    sendAlert("Nakainom na si patient mo beh!");
    stopBuzzer();
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED LOWWW");
    sendAlert("Nakainom na si patient mo beh!");

    // Log scheduled and actual intake times
    RtcDateTime scheduledTime(now.Year(), now.Month(), now.Day(), med1.nextHour,
                              med1.nextMinute, 0);
    RtcDateTime actualTime = Rtc.GetDateTime();
    logSched(med1, scheduledTime, actualTime);
    updateSchedule(med1, currentHour, currentMinute);
    med1.dispensed = true;
  }

  // Process Med2 if active, scheduled time matches, and not yet dispensed
  if (med2.active && currentHour == med2.nextHour &&
      currentMinute == med2.nextMinute && !med2.dispensed) {
    Serial.println("Dispensing Med2...");
    beepBuzzer(2, 200);
    digitalWrite(LED_PIN, HIGH);
    Serial.println("LED LIGHT UP");
    dispensePill(2);
    Serial.println("Servo Droppp");
    beepBuzzer(2, 200);
    continuousBuzzer();
    Serial.println("Continuous Buzzer");
    // Wait until the IR sensor detects the pill has been taken
    // Wait for the IR sensor to detect that the pill has been taken
    while (digitalRead(IR_PIN) != LOW) {  // Wait for LOW (pill detected)
      RtcDateTime now = Rtc.GetDateTime();
      int currentHour = now.Hour();
      int currentMinute = now.Minute();
      int currentSecond = now.Second();
      Serial.print("Current time: ");
      Serial.print(currentHour);
      Serial.print(":");
      Serial.println(currentMinute);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Current Time: ");
      lcd.setCursor(0, 1);
      lcd.print(currentHour);
      lcd.print(":");
      lcd.print(currentMinute);
      lcd.print(":");
      lcd.print(currentSecond);
      delay(1000);

      // Send alert if the pill is not taken within the specified time
      int currentTime = millis();
      int interval = 18000;  // 18 seconds (for testing)
      bool texted = false;
      if (currentTime >= interval && !texted) {
        sendAlert("Ayaw uminom ni patient maamsir");
        texted = true;
      }
    }

    // Pill has been taken (IR sensor reads HIGH)
    stopBuzzer();
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED LOWWW");
    sendAlert("Nakainom na si patient mo beh!");
    stopBuzzer();
    digitalWrite(LED_PIN, LOW);
    Serial.println("LED LOWWW");
    sendAlert("Nakainom na si patient mo beh!");

    RtcDateTime scheduledTime(now.Year(), now.Month(), now.Day(), med2.nextHour,
                              med2.nextMinute, 0);
    RtcDateTime actualTime = Rtc.GetDateTime();
    logSched(med2, scheduledTime, actualTime);
    Serial.println("LOGGEDDDDD SCHED2");
    updateSchedule(med2, currentHour, currentMinute);
    Serial.println("SCHEDD UPFDTAEDDD");
    med2.dispensed = true;  // Mark this event as handled
  }
}

// NewInstance function to receive new data via Serial1 (Bluetooth/GSM) and
// update medicine data.
int NewInstance() {
  Serial1.println("1");

  Serial.println("Waiting for Med Contact...");
  MedContact = receiveData();

  Serial.println("Waiting for Med1 Name...");
  med1.name = receiveData();

  Serial.println("Waiting for Med1 Interval...");
  med1.interval = receiveData(true).toInt();

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

  saveSched();  // Save the schedule to SD
  Serial.println("Setup Successful");
  currentState = DISPENSE;
  return 1;
}

// SD load function: loads all tokens from one line
void loadUser() {
  File schedF = SD.open("USERINFO.txt", FILE_READ);
  if (!schedF) {
    Serial.println("Failed to open USERINFO.txt for reading.");
    return;
  }
  Serial.println("Loading user data...");
  String line = schedF.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) {
    Serial.println("USERINFO.txt is empty.");
    schedF.close();
    return;
  }
  // Increase token count to 21 for all fields:
  String tokens[21];
  int index = 0;
  int lastIndex = 0;
  for (int i = 0; i < line.length(); i++) {
    if (line[i] == ',' || i == line.length() - 1) {
      if (i == line.length() - 1) i++;
      tokens[index++] = line.substring(lastIndex, i);
      lastIndex = i + 1;
    }
  }
  if (index < 11) {
    Serial.println("Corrupt or incomplete data in USERINFO.txt.");
    schedF.close();
    return;
  }
  // Parse MedContact and Med1
  MedContact = tokens[0];
  med1.name = tokens[1];
  med1.interval = tokens[2].toInt();
  med1.iterations = tokens[3].toInt();
  med1.baseHour = tokens[4].toInt();
  med1.baseMinute = tokens[5].toInt();
  med1.nextHour = tokens[6].toInt();
  med1.nextMinute = tokens[7].toInt();
  med1.active = (tokens[8] == "1");
  med1.lastDispensedHour = tokens[9].toInt();
  med1.lastDispensedMinute = tokens[10].toInt();
  med2.active = (tokens[11] == "1");
  if (med2.active && index >= 19) {
    med2.name = tokens[12];
    med2.interval = tokens[13].toInt();
    med2.iterations = tokens[14].toInt();
    med2.baseHour = tokens[15].toInt();
    med2.baseMinute = tokens[16].toInt();
    med2.nextHour = tokens[17].toInt();
    med2.nextMinute = tokens[18].toInt();
    med2.lastDispensedHour = tokens[19].toInt();
    med2.lastDispensedMinute = tokens[20].toInt();
  }
  schedF.close();
  Serial.println("User data loaded successfully.");
}

// Clear user function: saves current data to USER_LOG.txt then resets.
void clearUser() {
  File userFile = SD.open("USERINFO.txt", FILE_READ);
  if (userFile) {
    Serial.println("Saving current data to USER_LOG.txt...");
    File logFile = SD.open("USER_LOG.txt", FILE_WRITE);
    if (logFile) {
      String userData = userFile.readStringUntil('\n');
      logFile.println(userData);
      logFile.close();
      Serial.println("User data saved to log.");
    } else {
      Serial.println("ERROR: Failed to open USER_LOG.txt.");
    }
    userFile.close();
  } else {
    Serial.println("No existing user data found.");
  }
  if (SD.exists("USERINFO.txt")) {
    SD.remove("USERINFO.txt");
    Serial.println("USERINFO.txt deleted.");
  } else {
    Serial.println("USERINFO.txt does not exist.");
  }
  Serial.println("Restarting Arduino...");
  delay(1000);
  asm volatile("jmp 0");  // Soft reset Arduino
}

void sendAlert(String msg) {
  // Set GSM module to text mode
  Serial2.println("AT+CMGF=1");
  delay(1000);

  // Specify the recipient's phone number
  Serial2.print("AT+CMGS=\"");
  Serial2.print(MedContact);
  Serial2.println("\"");
  delay(1000);

  // Send the message content
  Serial2.println(msg);
  delay(100);

  // Send Ctrl+Z to indicate end of message
  Serial2.write(26);
  delay(5000);  // Wait for the message to be sent
  Serial.println("GSM message sent: " + msg);
}

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();
  Rtc.Begin();
  Serial.begin(9600);
  Serial1.begin(9600);
  Serial2.begin(9600);

  // Attach servos to designated pins.
  servo1.attach(servo1pin);
  servo2.attach(servo2pin);
  servo1.write(0);
  servo2.write(0);

  // Set buzzer and LED pins as output; IR sensor pin as input.
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(IR_PIN, INPUT);

  // Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  lcd.setCursor(0, 1);
  lcd.print("RTC OK");
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  delay(1000);

  SPI.begin();
  if (!SD.begin(CSpin)) {
    Serial.println("SD card initialization failed.");
    while (true) {
      lcd.setCursor(10, 1);
      lcd.print("SD FAIL");
    }
  }
  lcd.setCursor(10, 1);
  lcd.print("SD OK");
  Serial.println("SD card is ready to use.");
  delay(2000);

  // Check if USERINFO.txt exists and load user data if it does.
  if (SD.exists("USERINFO.txt")) {
    Serial.println("USERINFO.txt found. Loading user data...");
    loadUser();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("User Data Loaded");
    delay(2000);
    currentState = DISPENSE;
  } else {
    Serial.println("No saved data found. Proceeding to setup...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   PillOtter");
    lcd.setCursor(0, 1);
    lcd.print("Connect 2 setup");
    currentState = SETUP;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   PillOtter");
  lcd.setCursor(0, 1);
  lcd.print("Connect 2 setup");
}

void loop() {
  switch (currentState) {
    case SETUP:
      // Existing SETUP code for handling new instance commands...
      if (Serial1.available()) {
        String receivedData = Serial1.readStringUntil('\n');
        receivedData.trim();
        Serial.println("RECEIVED: " + receivedData);
        if (receivedData == "check") {
          Serial1.println("1");
        } else if (receivedData == "NewInstance") {
          int confirmation = NewInstance();
          Serial1.println(confirmation);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Setup Successful");
        }
      }
      break;

    case DISPENSE:
      resetDailyDoses();
      // Check the RTC and schedule every 10 seconds.
      RtcDateTime now = Rtc.GetDateTime();
      int currentHour = now.Hour();
      int currentMinute = now.Minute();
      int currentSecond = now.Second();
      Serial.print("Current time: ");
      Serial.print(currentHour);
      Serial.print(":");
      Serial.println(currentMinute);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Current Time: ");
      lcd.setCursor(0, 1);
      lcd.print(currentHour);
      lcd.print(":");
      lcd.print(currentMinute);
      lcd.print(":");
      lcd.print(currentSecond);

      checkAndDispense();
      delay(1000);
      // Optionally, process USB Serial commands for testing.
      if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input == "med1") {
          Serial.print(med1.name);
          Serial.print(",");
          Serial.print(med1.interval);
          Serial.print(",");
          Serial.print(med1.iterations);
          Serial.print(",");
          Serial.print(med1.baseHour);
          Serial.print(",");
          Serial.print(med1.baseMinute);
          Serial.print(",");
          Serial.print(med1.nextHour);
          Serial.print(",");
          Serial.print(med1.nextMinute);
          Serial.print(",");
          Serial.print(med1.active);
          Serial.print(",");
          Serial.print(med1.lastDispensedHour);
          Serial.print(",");
          Serial.print(med1.lastDispensedMinute);
          Serial.print(",");
          Serial.println(med2.active);
        } else if (input == "med2") {
          Serial.print(med2.name);
          Serial.print(",");
          Serial.print(med2.interval);
          Serial.print(",");
          Serial.print(med1.iterations);
          Serial.print(",");
          Serial.print(med1.baseHour);
          Serial.print(",");
          Serial.print(med2.baseMinute);
          Serial.print(",");
          Serial.print(med2.nextHour);
          Serial.print(",");
          Serial.print(med2.nextMinute);
          Serial.print(",");
          Serial.print(med2.active);
          Serial.print(",");
          Serial.print(med2.lastDispensedHour);
          Serial.print(",");
          Serial.print(med2.lastDispensedMinute);
        } else if (input == "clear") {
          clearUser();
        }
      }
      break;
  }
}