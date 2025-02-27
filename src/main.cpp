#include <LiquidCrystal_I2C.h>
#include <RtcDS1302.h>
#include <SD.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <ThreeWire.h>
#include <Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
ThreeWire myWire(7, 6, 8);
RtcDS1302<ThreeWire> Rtc(myWire);

const int chipSelect = 9;
File myFile;

SoftwareSerial BTSerial(3, 5);  // RX TX

bool userExists = false;
enum STATE { INACTIVE, ACTIVE };
STATE currentState = INACTIVE;

uint8_t hat[] = {0x00, 0x06, 0x09, 0x1f, 0x09, 0x00, 0x06, 0x00};
lcd.createChar(0, hat);

// Medicine structure
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

void setup() {
  Serial.begin(9600);
  BTSerial.begin(9600);
  Wire.begin();
  lcd.init();
  lcd.backlight();
  Rtc.Begin();

  lcd.setCursor(0, 0);
  lcd.print("Initializing...");
  delay(1000);

  if (!SD.begin(chipSelect)) {
    Serial.println("SD Initialization Failed!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SD Card Error!");
    while (1);  // Stop the program
  }

  lcd.setCursor(0, 1);
  lcd.print("SD=OK");
  delay(1000);

  // Check if USERINFO.txt exists on startup
  if (!isAvailable()) {
    Serial.println("Existing user found, loading data...");
    lcd.clear() lcd.setCursor(0, 0);
    lcd.print("User found");
    lcd.setCursor(0, 1);
    lcd.print("Loading...");
    loadUser();
    currentState = ACTIVE;
  } else {
    Serial.println("No user data found. Waiting for new user...");
    currentState = INACTIVE;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   PillOtter");
    lcd.write(byte(0));
    lcd.setCursor(1, 1);
    lcd.print("Waiting for setup");
  }
}

void loop() {
  switch (currentState) {
    case INACTIVE:

    case ACTIVE:
  }
  delay(500);
}

// ----------------------
bool availabilityCheck() {
  if (!SD.exists("USERINFO.txt")) {
    availability = true;
  } else {
    availability = false;
  }
  return availability;
}

void receiveData() {
  unsigned long startTime = millis();
  while (millis() - startTime < 300000) {  // 5-minute timeout
    if (Serial1.available()) {
      return Serial1.readStringUntil('\n');
    }
  }
  return "";  // Default to empty string if timeout occurs
}

void newInstance() {
  BTSerial.println("1");
  Serial.println("Starting new user setup...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Receiving Data...");

  // Receive medicine 1 data
  med1.name = receiveData();
  med1.interval = receiveData().toInt();
  med1.iterations = receiveData().toInt();
  med1.baseHour = receiveData().toInt();
  med1.baseMinute = receiveData().toInt();
  med1.nextHour = receiveData().toInt();
  med1.nextMinute = receiveData().toInt();
  med1.active = true;

  // Receive medicine 2 flag
  int med2Flag = receiveData().toInt();

  if (med2Flag == 1) {
    med2.name = receiveData();
    med2.interval = receiveData().toInt();
    med2.iterations = receiveData().toInt();
    med2.baseHour = receiveData().toInt();
    med2.baseMinute = receiveData().toInt();
    med2.nextHour = receiveData().toInt();
    med2.nextMinute = receiveData().toInt();
    med2.active = true;
  } else {
    med2.active = false;
  }

  // Save data to SD card
  saveUser();

  // Set state to ACTIVE
  currentState = ACTIVE;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("User Registered");
  lcd.setCursor(0, 1);
  lcd.print("System Ready!");
  Serial.println("New user setup complete.");
}

// ----------------------
// saveUser Function
// ----------------------
void saveUser() {
  File dataFile = SD.open("USERINFO.txt", FILE_WRITE);
  if (dataFile) {
    dataFile.print(med1.name + "," + String(med1.interval) + "," +
                   String(med1.iterations) + "," + String(med1.baseHour) + "," +
                   String(med1.baseMinute) + "," + String(med1.nextHour) + "," +
                   String(med1.nextMinute) + ",");

    if (med2.active) {
      dataFile.print(med2.name + "," + String(med2.interval) + "," +
                     String(med2.iterations) + "," + String(med2.baseHour) +
                     "," + String(med2.baseMinute) + "," +
                     String(med2.nextHour) + "," + String(med2.nextMinute));
    }

    dataFile.close();
    Serial.println("User data saved successfully.");
  } else {
    Serial.println("Error saving user data!");
  }
}

// ----------------------
// loadUser Function
// ----------------------
void loadUser() {
  File dataFile = SD.open("USERINFO.txt");
  if (dataFile) {
    String fileContent = dataFile.readString();
    dataFile.close();

    // Parsing user data
    int start = 0, index = 0;
    String values[14];

    for (int i = 0; i < fileContent.length(); i++) {
      if (fileContent[i] == ',' || i == fileContent.length() - 1) {
        values[index++] = fileContent.substring(start, i + 1);
        start = i + 1;
        if (index >= 14) break;
      }
    }

    med1.name = values[0];
    med1.interval = values[1].toInt();
    med1.iterations = values[2].toInt();
    med1.baseHour = values[3].toInt();
    med1.baseMinute = values[4].toInt();
    med1.nextHour = values[5].toInt();
    med1.nextMinute = values[6].toInt();
    med1.active = true;

    if (index > 7) {
      med2.name = values[7];
      med2.interval = values[8].toInt();
      med2.iterations = values[9].toInt();
      med2.baseHour = values[10].toInt();
      med2.baseMinute = values[11].toInt();
      med2.nextHour = values[12].toInt();
      med2.nextMinute = values[13].toInt();
      med2.active = true;
    }

    Serial.println("User data loaded successfully.");
  }
}

// ----------------------
// receiveData Function
// ----------------------
String receiveData() {
  while (!BTSerial.available());
  String received = BTSerial.readStringUntil('\n');
  received.trim();
  return received;
}
