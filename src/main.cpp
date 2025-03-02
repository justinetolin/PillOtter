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

// ! VARIABLES, DEFINITIONS AND STRUCTURES
enum State { SETUP = 0, DISPENSE = 1 };
State currentState = SETUP;
#define CSpin 53

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
    if (Serial1.available()) {
      String buffer = Serial1.readStringUntil('\n');
      buffer.trim();

      if (expectInt) {
        return String(buffer.toInt());
      } else {
        return buffer;
      }
    }
  }
  return "";  // Return empty string if timeout
}

int NewInstance() {
  Serial1.println("1");

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

  saveSched();
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
  Serial1.begin(9600);

  Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
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
    };
  }
  lcd.setCursor(10, 1);
  lcd.print("SD OK");
  Serial.println("SD card is ready to use.");
  delay(2000);

  // **Check if USERINFO.txt exists**
  if (SD.exists("USERINFO.txt")) {
    Serial.println("USERINFO.txt found. Loading user data...");
    loadUser();  // Load saved schedule from SD card

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
      Serial.println("State: SETUP");

      if (Serial1.available()) {
        String receivedData = Serial1.readStringUntil('\n');
        receivedData.trim();
        Serial.println("RECEIVED: ");
        Serial.println(receivedData);

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
      // Serial.println("State: DISPENSE");
      // Serial.println("What data do you need?");
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
          Serial.print(med2.active);
        } else if (input == "med2") {
          Serial.println(med2.name);
        } else if (input == "clear") {
          clearUser();
        }
      }

      break;
  }
}

void saveSched() {
  File schedF = SD.open("USERINFO.txt", FILE_WRITE);
  if (schedF) {
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
  }
}

void loadUser() {
  File schedF = SD.open("USERINFO.txt", FILE_READ);

  if (!schedF) {
    Serial.println("Failed to open USERINFO.txt for reading.");
    return;
  }

  Serial.println("Loading user data...");

  String line = schedF.readStringUntil('\n');  // Read the entire line
  line.trim();  // Remove any trailing spaces or newline characters

  if (line.length() == 0) {
    Serial.println("USERINFO.txt is empty.");
    schedF.close();
    return;
  }

  // Tokenize the CSV string
  String tokens[20];
  int index = 0;
  int lastIndex = 0;

  for (int i = 0; i < line.length(); i++) {
    if (line[i] == ',' || i == line.length() - 1) {
      if (i == line.length() - 1)
        i++;  // Include last character if no comma at end
      tokens[index++] = line.substring(lastIndex, i);
      lastIndex = i + 1;
    }
  }

  if (index < 11) {  // Ensure at least Med1 + Med2 active flag is available
    Serial.println("Corrupt or incomplete data in USERINFO.txt.");
    schedF.close();
    return;
  }

  // Parse Med1
  med1.name = tokens[0];
  med1.interval = tokens[1].toInt();
  med1.iterations = tokens[2].toInt();
  med1.baseHour = tokens[3].toInt();
  med1.baseMinute = tokens[4].toInt();
  med1.nextHour = tokens[5].toInt();
  med1.nextMinute = tokens[6].toInt();
  med1.active = (tokens[7] == "1");  // Convert to bool
  med1.lastDispensedHour = tokens[8].toInt();
  med1.lastDispensedMinute = tokens[9].toInt();

  // Parse Med2 Active State
  med2.active = (tokens[10] == "1");

  // If Med2 is active, parse its data
  if (med2.active && index >= 19) {  // Ensure Med2 fields exist
    med2.name = tokens[11];
    med2.interval = tokens[12].toInt();
    med2.iterations = tokens[13].toInt();
    med2.baseHour = tokens[14].toInt();
    med2.baseMinute = tokens[15].toInt();
    med2.nextHour = tokens[16].toInt();
    med2.nextMinute = tokens[17].toInt();
    med2.lastDispensedHour = tokens[18].toInt();
    med2.lastDispensedMinute = tokens[19].toInt();
  }

  schedF.close();
  Serial.println("User data loaded successfully.");
}

void clearUser() {
  // Open USERINFO.txt to read saved data
  File userFile = SD.open("USERINFO.txt", FILE_READ);
  if (userFile) {
    Serial.println("Saving current data to USER_LOG.txt...");

    // Open USER_LOG.txt in append mode
    File logFile = SD.open("USER_LOG.txt", FILE_WRITE);
    if (logFile) {
      String userData = userFile.readStringUntil('\n');  // Read full line
      logFile.println(userData);  // Save data to log file
      logFile.close();
      Serial.println("User data saved to log.");
    } else {
      Serial.println("ERROR: Failed to open USER_LOG.txt.");
    }

    userFile.close();
  } else {
    Serial.println("No existing user data found.");
  }

  // Delete USERINFO.txt to clear saved schedule
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
