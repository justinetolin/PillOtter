#include <LiquidCrystal_I2C.h>
#include <RtcDS1302.h>
#include <ThreeWire.h>
#include <Wire.h>

// Initialize LCD with I2C address 0x3F (change to 0x27 if needed)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Initialize RTC DS1302 with ThreeWire
ThreeWire myWire(7, 6, 8);  // DAT, CLK, RST
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  Wire.begin();
  lcd.init();
  lcd.backlight();

  Rtc.Begin();

  // Uncomment this once to set the RTC to the current date & time, then comment
  // and re-upload
  Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  lcd.setCursor(0, 0);
  lcd.print("RTC DS1302 Ready");
  delay(2000);
  lcd.clear();
}

void loop() {
  RtcDateTime now = Rtc.GetDateTime();
  // Display Date
  lcd.setCursor(0, 0);
  lcd.print("Date: ");
  lcd.print(now.Day());
  lcd.print("/");
  lcd.print(now.Month());
  lcd.print("/");
  lcd.print(now.Year());

  // Display Time
  lcd.setCursor(0, 1);
  lcd.print("Time: ");
  lcd.print(now.Hour());
  lcd.print(":");
  lcd.print(now.Minute());
  lcd.print(":");
  lcd.print(now.Second());

  delay(1000);  // Update every second
}
