#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <SD.h>

byte status = 0, sdStatus = 0; //error codes
//pins:
#define JOYSTICK_RX_PIN A1
#define JOYSTICK_RY_PIN A0
#define EXHAUST_PIN 2
#define DHT_PIN 3
#define HUMIDIFIER_PIN 4
#define SD_CHIP_SELECT 5
#define CIRCULATING_PIN 6

//humidity variables:
float humidity; //%rh
int targetHumidity = 90; //%rh

//scheduled exhaust cycle:
bool exhaustCycleActive = false;

DHT dht(DHT_PIN, DHT22); //humidity sensor

LiquidCrystal_I2C lcd(0x27,20,4); //i2c address, screen width, screen height

char filename[13]; //8.3 file name format, 8 character name and 3 character extension maximum

void setup() {
  pinMode(EXHAUST_PIN, OUTPUT); //connected to MOSFET, pull-down resistor, LOW turns off, HIGH turns on
  pinMode(CIRCULATING_PIN, OUTPUT); //connected to MOSFET, pull-down resistor, LOW turns off, HIGH turns on
  pinMode(HUMIDIFIER_PIN, OUTPUT); //LOW turns on, HIGH turns off
  dht.begin(); //initialize humidity sensor
  initializeDisplay(); //initialize display:
  createLogFile(); //initialize SD card and creates log file
  delay(50);
}

void loop() {
  static unsigned long lastTime = 0UL; //ms
  const unsigned long interval = 2000UL; //ms

  setTargetHumidity();
  updateExhaustCycle(); //checks if it's ready to exhaust

  unsigned long currentTime = millis();
  if (currentTime - lastTime < interval) return; //all code below this line happens every 2000 ms
  lastTime = currentTime;
  humidity = getHumidity();

  switch (status) { //error codes
    case 0: //functioning normally
      updateDisplay();
      logSdCard();
      if (!exhaustCycleActive) updateHumidityOutputs(); //skip this if exhaust cycle is running
      break;
    case 1: //humidity sensor read error
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(F("sensor read error"));
      break;
  }
  status = 0;
}

float getHumidity() {
  static byte dhtErrors = 0;
  const int dhtMaxErrors = 5;

  float sensorReading = dht.readHumidity();
  if (isnan(sensorReading)) { //humidity sensor does not return a valid value
    dhtErrors++;
    status = 1;
    if (dhtErrors >= dhtMaxErrors) { //too many consecutive failures, force outputs off
      digitalWrite(HUMIDIFIER_PIN, HIGH); //HIGH turns off
      if (!exhaustCycleActive) digitalWrite(EXHAUST_PIN, LOW); //don't override the exhaust cycle
    }
    return humidity;
  }
  else { //output humidity and time
    dhtErrors = 0; //reset on a good read
    return sensorReading;
  }
}

void updateDisplay() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(F("Humidity: "));
  lcd.print(humidity);
  lcd.print(F("%"));
  lcd.setCursor(0,1);
  lcd.print(F("Target: "));
  lcd.print(targetHumidity);
  lcd.print(F("%"));
}
void logSdCard() {
  lcd.setCursor(0,2);
  switch (sdStatus) {
    case 0: {
      File dataLog = SD.open(filename, FILE_WRITE);
      if (dataLog) {
        sdStatus = 0;
        dataLog.print(millis()); //milliseconds since powered on
        dataLog.print(F(","));
        dataLog.println(humidity);
        dataLog.close();
      }
      else lcd.print(F("SD write error"));
      break;
    }
    case 1:
      lcd.print(F("SD start error"));
      break;
    case 2:
      lcd.print(F("SD file error"));
      break;
  }
}

void setTargetHumidity() {
  static unsigned long lastJoystickTime = 0UL; //ms
  const unsigned long joystickInterval = 500UL; //ms

  unsigned long currentTime = millis();
  int yValue = analogRead(JOYSTICK_RY_PIN);

  if (yValue < 800 && yValue > 200) { //joystick centered
    lastJoystickTime = 0;
    return;
  }

  if (currentTime - lastJoystickTime < joystickInterval) return;
  lastJoystickTime = currentTime;

  if (yValue >= 800) { //joystick pushed up
    targetHumidity += 5;
    if (targetHumidity > 100) targetHumidity = 0;
  }
  else { //joystick pushed down
    targetHumidity -= 5;
    if (targetHumidity < 0) targetHumidity = 100;
  }
  updateDisplay();
}

void initializeDisplay(){
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.clear();
}

void createLogFile(){
  if (!SD.begin(SD_CHIP_SELECT)) sdStatus = 1; //ensures that the card is connected properly
  else {
    int fileNumber = 0;
    while (true) {
      sprintf(filename, "log%04d.csv", fileNumber); //formats file name to log[increment (4 digits)].csv
      if (!SD.exists(filename)) break; //if the file doesn't exist, break the loop
      fileNumber++; //increment
    }
    File dataLog = SD.open(filename, FILE_WRITE); //opens the file with the filename selected
    if (dataLog) {
      lcd.print(F("opening "));
      lcd.print(filename);
      dataLog.println(F("Time_Milliseconds,Relative_Humidity_%")); //csv header
      dataLog.close();
    } 
    else sdStatus = 2;
  }
}

void updateHumidityOutputs(){
  const int humidityMargin = 5;
  if (humidity <= targetHumidity - humidityMargin) increaseHumidity(true); //humidity too low, turn on humidifier 
  if (humidity >= targetHumidity - humidityMargin/2) increaseHumidity(false); //humidity back up to target, turn off humidifier
  if (humidity >= targetHumidity + humidityMargin) decreaseHumidity(true); //humidity too high, turn on exhaust
  if (humidity <= targetHumidity + humidityMargin/2) decreaseHumidity(false); //humidity back down to target, turn off exhaust
}

void increaseHumidity(bool enable){
  digitalWrite(HUMIDIFIER_PIN, enable ? LOW : HIGH);
  digitalWrite(CIRCULATING_PIN, enable ? HIGH : LOW);
}
void decreaseHumidity(bool enable){
  digitalWrite(EXHAUST_PIN, enable ? HIGH : LOW);
}
void updateExhaustCycle(){
  static unsigned long lastExhaustStart = 0; //ms
  const unsigned long exhaustCycleInterval = 3600000UL; //ms
  const unsigned long exhaustCycleDuration = 300000UL; //ms

  unsigned long currentTime = millis();
  if (!exhaustCycleActive && currentTime - lastExhaustStart >= exhaustCycleInterval) { //starts the exhaust cycle
    //code within this scope runs every hour:
    exhaustCycleActive = true;
    lastExhaustStart = currentTime; //mark when this cycle started
    digitalWrite(EXHAUST_PIN, HIGH);
    digitalWrite(HUMIDIFIER_PIN, HIGH);
  }
  else if (exhaustCycleActive && currentTime - lastExhaustStart >= exhaustCycleDuration) { //ends the exhaust cycle
    exhaustCycleActive = false;
    digitalWrite(EXHAUST_PIN, LOW);
  }
}
