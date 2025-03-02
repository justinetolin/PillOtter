#include <LiquidCrystal_I2C.h>
#include <RTClib.h>
#include <SD.h>
#include <SPI.h>
#include <Servo.h>
#include <SoftwareSerial.h>
#include <Wire.h>

// ----- Pin Definitions -----
#define SD_CS_PIN 10     // SD card chip select
#define SERVO1_PIN 6     // Servo for compartment 1 (Medicine 1)
#define SERVO2_PIN 7     // Servo for compartment 2 (Medicine 2)
#define IR_SENSOR_PIN 8  // IR sensor input (LOW when detected)
#define BUZZER_PIN 9     // Buzzer output

// Bluetooth module (adjust pins if necessary)
#define BT_RX 2
#define BT_TX 3

// GSM module (adjust pins if necessary)
#define GSM_RX 4
#define GSM_TX 5

// ----- Global Objects -----
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Adjust the I2C address if needed
RTC_DS3231 rtc;
Servo servo1, servo2;
SoftwareSerial btSerial(BT_RX, BT_TX);     // For Bluetooth communication
SoftwareSerial gsmSerial(GSM_RX, GSM_TX);  // For GSM communication

// Files on SD card
const char *scheduleFile = "schedule.txt";
const char *logFile = "log.csv";

// Phone number for SMS alert (update with real number)
const char *practitionerNumber = "+1234567890";

// ----- Medicine Schedule Structure -----
// For each medicine we keep track of:
// - name, interval (received but not used in math below), iterations (2 or 3)
// - the base time (first scheduled dose) used to reset each day,
// - the next dispensing time (which may be adjusted after a delay),
// - currentDose (0 for first dose, 1 for second, etc.)
// - active flag to indicate if data was received
// - a flag to ensure we trigger dispensing only once per scheduled minute.
struct Medicine {
  String name;
  int interval;             // Received value (not used in calculation here)
  int iterations;           // 2 or 3
  int currentDose;          // 0-indexed dose of the day
  int baseHour;             // First scheduled dose hour (from app)
  int baseMinute;           // First scheduled dose minute (from app)
  int nextHour;             // Next scheduled dispensing hour
  int nextMinute;           // Next scheduled dispensing minute
  bool active;              // true if medicine data received
  int lastDispensedMinute;  // to avoid multiple triggers in same minute
};

Medicine med1;
Medicine med2;

// ----- Function Prototypes -----
void loadSchedule();
void saveScheduleToSD();
void checkBluetooth();
void dispenseMedicine(Medicine &med, int compartment);
void sendSMS(DateTime eventTime);
void logEvent(Medicine &med, DateTime actualTime, bool taken);
String formatDateTime(DateTime dt);

void setup() {
  Serial.begin(9600);
  btSerial.begin(9600);
  gsmSerial.begin(9600);

  // Initialize RTC
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("RTC error!");
    while (1);  // halt
  }

  // Initialize SD card
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card init failed!");
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("SD init error!");
    // Optionally halt if SD is required
  }

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("System Starting");

  // Initialize servos
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(0);  // initial position
  servo2.write(0);

  // Set pin modes
  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initialize medicine data as inactive
  med1.active = false;
  med2.active = false;
  med1.lastDispensedMinute = -1;
  med2.lastDispensedMinute = -1;

  // Attempt to load schedule from SD card
  loadSchedule();

  delay(2000);
}

void loop() {
  // Check for incoming Bluetooth data (sent every second)
  checkBluetooth();

  // Get current time
  DateTime now = rtc.now();

  // For each active medicine, check if it’s time to dispense.
  // We trigger if the current hour and minute match the next scheduled time
  // and we haven’t already dispensed in this minute.
  if (med1.active) {
    if (now.hour() == med1.nextHour && now.minute() == med1.nextMinute &&
        now.minute() != med1.lastDispensedMinute) {
      med1.lastDispensedMinute = now.minute();
      dispenseMedicine(med1, 1);
    }
  }
  if (med2.active) {
    if (now.hour() == med2.nextHour && now.minute() == med2.nextMinute &&
        now.minute() != med2.lastDispensedMinute) {
      med2.lastDispensedMinute = now.minute();
      dispenseMedicine(med2, 2);
    }
  }

  // (Optionally, reset lastDispensedMinute after the minute passes—here we use
  // current minute check.)

  delay(1000);
}

// ----- Dispensing Routine -----
// Dispense the pill from the given compartment and update the schedule.
// For Medicine 1, the event is logged and an SMS is sent if the pill is missed.
void dispenseMedicine(Medicine &med, int compartment) {
  // Display dispensing info
  lcd.clear();
  lcd.print("Dispensing ");
  lcd.print(med.name);
  lcd.setCursor(0, 1);
  lcd.print("Comp ");
  lcd.print(compartment);

  // Rotate the correct servo
  if (compartment == 1) {
    servo1.write(90);  // dispense action
    delay(1000);
    servo1.write(0);
  } else if (compartment == 2) {
    servo2.write(90);
    delay(1000);
    servo2.write(0);
  }

  // Ring buzzer
  digitalWrite(BUZZER_PIN, HIGH);
  delay(500);
  digitalWrite(BUZZER_PIN, LOW);

  // Prompt user on LCD
  lcd.clear();
  lcd.print("Take pill now!");

  // Wait up to 2 minutes for pill intake
  // (IR sensor now reads LOW when a hand is detected)
  unsigned long startTime = millis();
  bool pillTaken = false;
  while (millis() - startTime < 120000) {
    if (digitalRead(IR_SENSOR_PIN) == LOW) {
      pillTaken = true;
      break;
    }
  }
  DateTime nowTime = rtc.now();

  // If pill taken, confirm; if not, alert (only for Medicine 1)
  if (pillTaken) {
    lcd.clear();
    lcd.print("Pill taken!");
    Serial.print("Pill taken at: ");
    Serial.println(formatDateTime(nowTime));
  } else {
    lcd.clear();
    lcd.print("Missed pill!");
    Serial.print("Missed pill at: ");
    Serial.println(formatDateTime(nowTime));
    if (compartment == 1) {
      sendSMS(nowTime);
    }
  }

  // For Medicine 1 (compartment 1) log the event.
  if (compartment == 1) {
    logEvent(med, nowTime, pillTaken);
  }

  // -------- Update next scheduled time --------
  // We assume a fixed gap of 6 hours between doses.
  // For iteration 2:
  //    • Dose 0: first dose at base time.
  //    • Dose 1: next scheduled = (base time + 6 hrs) adjusted by any delay.
  // For iteration 3:
  //    • Dose 0: base time,
  //    • Dose 1: base time + 6 hrs,
  //    • Dose 2: base time + 12 hrs.
  // After the final dose, reset to next day (using the base time).
  // For iteration 2, if the dose was delayed then the delay carries over.
  if (med.iterations == 2) {
    if (med.currentDose == 0) {
      // Compute delay in minutes: difference between actual intake and
      // scheduled time.
      DateTime scheduledDose =
          DateTime(nowTime.year(), nowTime.month(), nowTime.day(), med.nextHour,
                   med.nextMinute, 0);
      int delayMinutes = (nowTime.unixtime() - scheduledDose.unixtime()) / 60;
      // Next scheduled = (base time + 6 hrs) plus delay.
      int newHour = med.baseHour + 6;
      int newMinute = med.baseMinute;
      if (newHour >= 24) newHour -= 24;
      int totalMins = newHour * 60 + newMinute + delayMinutes;
      newHour = (totalMins / 60) % 24;
      newMinute = totalMins % 60;
      med.nextHour = newHour;
      med.nextMinute = newMinute;
      med.currentDose = 1;
    } else {  // dose 1 was just given; reset for next day.
      med.nextHour = med.baseHour;
      med.nextMinute = med.baseMinute;
      med.currentDose = 0;
    }
  } else if (med.iterations == 3) {
    if (med.currentDose == 0) {
      // Next dose fixed at base time + 6 hrs (ignoring delay)
      int newHour = med.baseHour + 6;
      int newMinute = med.baseMinute;
      if (newHour >= 24) newHour -= 24;
      med.nextHour = newHour;
      med.nextMinute = newMinute;
      med.currentDose = 1;
    } else if (med.currentDose == 1) {
      // Next dose = base time + 12 hrs
      int newHour = med.baseHour + 12;
      int newMinute = med.baseMinute;
      if (newHour >= 24) newHour -= 24;
      med.nextHour = newHour;
      med.nextMinute = newMinute;
      med.currentDose = 2;
    } else {  // After dose 2, reset for next day.
      med.nextHour = med.baseHour;
      med.nextMinute = med.baseMinute;
      med.currentDose = 0;
    }
  }

  // Save updated schedule to SD
  saveScheduleToSD();

  delay(3000);  // allow time for user to read LCD
}

// ----- SMS Routine -----
// Uses the GSM module to send an SMS alert if a pill is missed.
void sendSMS(DateTime eventTime) {
  String message =
      "Alert: Missed pill for Medicine 1 scheduled earlier. Event time: " +
      formatDateTime(eventTime);
  gsmSerial.println("AT+CMGF=1");  // set SMS text mode
  delay(1000);
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(practitionerNumber);
  gsmSerial.println("\"");
  delay(1000);
  gsmSerial.println(message);
  delay(100);
  gsmSerial.write(26);  // Ctrl+Z to send SMS
  delay(5000);
}

// ----- Logging Routine -----
// For Medicine 1 only, log the scheduled (base) time and the actual intake time
// (if taken) in CSV format.
void logEvent(Medicine &med, DateTime actualTime, bool taken) {
  File logF = SD.open(logFile, FILE_WRITE);
  if (logF) {
    // For logging, we log the base (first dose) time and actual intake time (or
    // blank if missed)
    String baseTimeStr = (med.baseHour < 10 ? "0" : "") + String(med.baseHour) +
                         ":" + (med.baseMinute < 10 ? "0" : "") +
                         String(med.baseMinute);
    String actualStr = taken ? formatDateTime(actualTime) : "";
    String logLine = baseTimeStr + "," + actualStr;
    logF.println(logLine);
    logF.close();
  } else {
    Serial.println("Error opening log file");
  }
}

// ----- Format DateTime to String -----
// Returns a string in the format "YYYY-MM-DD HH:MM"
String formatDateTime(DateTime dt) {
  char buffer[17];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d", dt.year(), dt.month(), dt.day(),
          dt.hour(), dt.minute());
  return String(buffer);
}

// ----- Bluetooth Parsing -----
// The app sends a line for each medicine. We check for "Medicine 1:" and
// "Medicine 2:", parse the comma-separated data, and update the corresponding
// Medicine structure. Expected format: "Medicine X:
// Name,Interval,Iteration,HH:MM"
void checkBluetooth() {
  if (btSerial.available()) {
    String data = btSerial.readStringUntil('\n');
    data.trim();
    if (data.length() == 0) return;

    Serial.println("BT data: " + data);
    // Check if data is for Medicine 1
    if (data.startsWith("Medicine 1:")) {
      String payload = data.substring(String("Medicine 1:").length());
      payload.trim();
      // Expected tokens: name, interval, iteration, firstSchedule
      int idx1 = payload.indexOf(',');
      int idx2 = payload.indexOf(',', idx1 + 1);
      int idx3 = payload.indexOf(',', idx2 + 1);
      if (idx1 > 0 && idx2 > idx1 && idx3 > idx2) {
        med1.name = payload.substring(0, idx1);
        med1.interval = payload.substring(idx1 + 1, idx2).toInt();
        med1.iterations = payload.substring(idx2 + 1, idx3).toInt();
        String timeStr = payload.substring(idx3 + 1);
        timeStr.trim();
        int colonIndex = timeStr.indexOf(':');
        if (colonIndex > 0) {
          med1.baseHour = timeStr.substring(0, colonIndex).toInt();
          med1.baseMinute = timeStr.substring(colonIndex + 1).toInt();
          // Set the next dispensing time initially to the base time
          med1.nextHour = med1.baseHour;
          med1.nextMinute = med1.baseMinute;
          med1.currentDose = 0;
          med1.active = true;
          med1.lastDispensedMinute = -1;
        }
      }
      saveScheduleToSD();  // save after receiving Medicine 1 data
      lcd.clear();
      lcd.print("Med1 updated");
      delay(1000);
    }
    // Check for Medicine 2 data
    else if (data.startsWith("Medicine 2:")) {
      String payload = data.substring(String("Medicine 2:").length());
      payload.trim();
      int idx1 = payload.indexOf(',');
      int idx2 = payload.indexOf(',', idx1 + 1);
      int idx3 = payload.indexOf(',', idx2 + 1);
      if (idx1 > 0 && idx2 > idx1 && idx3 > idx2) {
        med2.name = payload.substring(0, idx1);
        med2.interval = payload.substring(idx1 + 1, idx2).toInt();
        med2.iterations = payload.substring(idx2 + 1, idx3).toInt();
        String timeStr = payload.substring(idx3 + 1);
        timeStr.trim();
        int colonIndex = timeStr.indexOf(':');
        if (colonIndex > 0) {
          med2.baseHour = timeStr.substring(0, colonIndex).toInt();
          med2.baseMinute = timeStr.substring(colonIndex + 1).toInt();
          med2.nextHour = med2.baseHour;
          med2.nextMinute = med2.baseMinute;
          med2.currentDose = 0;
          med2.active = true;
          med2.lastDispensedMinute = -1;
        }
      }
      saveScheduleToSD();  // save after receiving Medicine 2 data
      lcd.clear();
      lcd.print("Med2 updated");
      delay(1000);
    }
  }
}

// ----- Save Schedule to SD -----
// Save the medicine schedules to the SD card so they can be reloaded after a
// reset.
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

// ----- Load Schedule from SD -----
// If a schedule file exists, load the values into med1 and med2.
void loadSchedule() {
  File schedF = SD.open(scheduleFile);
  if (schedF) {
    while (schedF.available()) {
      String line = schedF.readStringUntil('\n');
      line.trim();
      if (line.startsWith("Medicine 1,")) {
        // Format: Medicine 1,name,interval,iterations,HH:MM,currentDose,HH:MM
        int pos = 0;
        int comma;
        // Skip "Medicine 1,"
        pos = line.indexOf(',') + 1;
        comma = line.indexOf(',', pos);
        med1.name = line.substring(pos, comma);
        pos = comma + 1;
        comma = line.indexOf(',', pos);
        med1.interval = line.substring(pos, comma).toInt();
        pos = comma + 1;
        comma = line.indexOf(',', pos);
        med1.iterations = line.substring(pos, comma).toInt();
        pos = comma + 1;
        String baseTime = line.substring(pos, pos + 5);
        med1.baseHour = baseTime.substring(0, 2).toInt();
        med1.baseMinute = baseTime.substring(3, 5).toInt();
        pos = line.indexOf(',', pos + 5) + 1;
        comma = line.indexOf(',', pos);
        med1.currentDose = line.substring(pos, comma).toInt();
        pos = comma + 1;
        String nextTime = line.substring(pos);
        med1.nextHour = nextTime.substring(0, 2).toInt();
        med1.nextMinute = nextTime.substring(3, 5).toInt();
        med1.active = true;
        med1.lastDispensedMinute = -1;
      } else if (line.startsWith("Medicine 2,")) {
        int pos = 0;
        int comma;
        pos = line.indexOf(',') + 1;
        med2.name = line.substring(pos, line.indexOf(',', pos));
        pos = line.indexOf(',', pos) + 1;
        comma = line.indexOf(',', pos);
        med2.interval = line.substring(pos, comma).toInt();
        pos = comma + 1;
        comma = line.indexOf(',', pos);
        med2.iterations = line.substring(pos, comma).toInt();
        pos = comma + 1;
        String baseTime = line.substring(pos, pos + 5);
        med2.baseHour = baseTime.substring(0, 2).toInt();
        med2.baseMinute = baseTime.substring(3, 5).toInt();
        pos = line.indexOf(',', pos + 5) + 1;
        comma = line.indexOf(',', pos);
        med2.currentDose = line.substring(pos, comma).toInt();
        pos = comma + 1;
        String nextTime = line.substring(pos);
        med2.nextHour = nextTime.substring(0, 2).toInt();
        med2.nextMinute = nextTime.substring(3, 5).toInt();
        med2.active = true;
        med2.lastDispensedMinute = -1;
      }
    }
    schedF.close();
    Serial.println("Schedule loaded from SD.");
  } else {
    Serial.println("No schedule file found. Waiting for Bluetooth data...");
  }
}