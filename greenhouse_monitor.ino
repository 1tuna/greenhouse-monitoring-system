#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <DS1302.h>
#include <SD.h>
#include <Keypad.h>

// DHT11 Setup
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// RTC Setup
#define DS1302_CLK 22
#define DS1302_DAT 23
#define DS1302_RST 24
DS1302 rtc(DS1302_RST, DS1302_DAT, DS1302_CLK);

// SD Card
#define SD_CS 53
File logFile;

// LEDs and Buzzer
#define RED_LED 7
#define YELLOW_LED 6
#define GREEN_LED 5
#define BUZZER_PIN 8

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {39, 40, 41, 42}; // Row pins
byte colPins[COLS] = {36, 37, 38};     // Column pins
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Threshold values
float lowerThreshold = 25.0;
float upperThreshold = 30.0;

String inputValue = "";
byte inputMode = 0; // 0: idle, 1: lower threshold, 2: upper threshold
unsigned long lastSensorRead = 0;

void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  dht.begin();
  rtc.halt(false);
  rtc.writeProtect(false);

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Starting");
  delay(2000);
  lcd.clear();

  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
  } else {
    logFile = SD.open("datalog.csv", FILE_WRITE);
    if (logFile && logFile.size() == 0) {
      logFile.println("Date,Time,Temperature,MinThreshold,MaxThreshold");
      logFile.close();
    }
  }

  displayThresholds();
}

void loop() {
  // Keypad check
  char key = keypad.getKey();
  if (key) {
    handleKeypadInput(key);
  }

  // Read every 2 seconds
  if (inputMode == 0 && millis() - lastSensorRead > 2000) {
    lastSensorRead = millis();
    readAndLogTemperature();
  }
}

void readAndLogTemperature() {
  float temperature = dht.readTemperature();
  if (isnan(temperature)) {
    Serial.println("[WARNING] Temperature could not be read!");
    return;
  }

  String timeStr = rtc.getTimeStr();
  String dateStr = rtc.getDateStr();

  // LCD display
  lcd.setCursor(0, 0);
  lcd.print(dateStr);
  lcd.setCursor(0, 1);
  lcd.print(timeStr);
  lcd.print(" ");
  lcd.print(temperature, 1);
  lcd.print((char)223);
  lcd.print("C ");

  // Serial Monitor
  Serial.println("---------------");
  Serial.print("Date      : "); Serial.println(dateStr);
  Serial.print("Time      : "); Serial.println(timeStr);
  Serial.print("Temperature: "); Serial.print(temperature); Serial.println(" C");
  Serial.print("Min Threshold: "); Serial.println(lowerThreshold);
  Serial.print("Max Threshold: "); Serial.println(upperThreshold);
  Serial.println("---------------");

  // Bluetooth via Serial1
  String dataLine = "Date: " + dateStr + "  Time: " + timeStr + "  Temp: " + String(temperature) + " C";
  Serial1.println(dataLine);

  // SD card write
  logFile = SD.open("datalog.csv", FILE_WRITE);
  if (logFile) {
    logFile.print(dateStr); logFile.print(",");
    logFile.print(timeStr); logFile.print(",");
    logFile.print(temperature); logFile.print(",");
    logFile.print(lowerThreshold); logFile.print(",");
    logFile.println(upperThreshold);
    logFile.close();
  }

  // LED and Buzzer logic
  if (temperature > upperThreshold) {
    digitalWrite(RED_LED, HIGH);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER_PIN, HIGH);
  } else if (temperature >= lowerThreshold && temperature <= upperThreshold) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    digitalWrite(RED_LED, LOW);
    digitalWrite(YELLOW_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

void displayThresholds() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Min: ");
  lcd.print(lowerThreshold);
  lcd.setCursor(0, 1);
  lcd.print("Max: ");
  lcd.print(upperThreshold);
  delay(2000);
  lcd.clear();
}

void handleKeypadInput(char key) {
  if (key == '*') {
    if (inputMode == 1 && inputValue.length() > 0) {
      lowerThreshold = inputValue.toFloat();
      lcd.clear();
      lcd.print("Min Set:");
      lcd.setCursor(0, 1);
      lcd.print(lowerThreshold);
      delay(2000);
      lcd.clear();
      displayThresholds();
      inputMode = 0;
      inputValue = "";
    } else {
      inputMode = 1;
      inputValue = "";
      lcd.clear();
      lcd.print("Enter Min:");
      lcd.setCursor(0, 1);
    }
    return;
  }

  if (key == '#') {
    if (inputMode == 2 && inputValue.length() > 0) {
      upperThreshold = inputValue.toFloat();
      lcd.clear();
      lcd.print("Max Set:");
      lcd.setCursor(0, 1);
      lcd.print(upperThreshold);
      delay(2000);
      lcd.clear();
      displayThresholds();
      inputMode = 0;
      inputValue = "";
    } else {
      inputMode = 2;
      inputValue = "";
      lcd.clear();
      lcd.print("Enter Max:");
      lcd.setCursor(0, 1);
    }
    return;
  }

  if (key >= '0' && key <= '9' && inputMode != 0) {
    inputValue += key;
    lcd.print(key);
  }
}
