//MH-Z19
#include <MHZ.h>
//ENS160
#include <DFRobot_ENS160.h>
//BME680
#include <Adafruit_BME680.h>
#include <bme68x.h>
#include <bme68x_defs.h>
//I2C
#include <Wire.h>
//Web comm.
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

// SD card
#include <SPI.h>
#include <SD.h>

//-------------------------------------------------DECLARATIONS
// WiFi
const char* ssid = "";
const char* password = "";
const char* serverURL = "";

#define CO2_IN 16      //D0 - MH-Z19 PWM IN
#define CALIB_PIN 2   //D4 - MH-Z19 Calibration pin
#define SWITCH_PIN 0  //D3 - Calibration button
#define CS_PIN 15  //D8 - Chip select pin

//SD card file
File myFile;

//Wifi connect timer
int connectTimer = 0;

//Time since the start of the program, will be used with millis() and saved into csv/json data, can calculate time of measurement if the start time of program is known
//millis() overflows after approximately 50 daysm
//TODO: replace with NTP if wifi available, if not buy external RTC module and use that
unsigned long measurementTime;

//Calibration ISR
volatile bool calibrateRequested = false;
volatile unsigned long lastInterruptTime = 0;
const unsigned long debounceDelay = 300;  // milliseconds
bool calibrating = false;
int calibrateCount = 0;


// Sensor warmup
const int interval = 180000;  // 3 minutes
unsigned long currentMillis;
unsigned long previousMillis = 0;

//Arrays for measurements
float tempArr[10];
float humArr[10];
float CO2Arr[10];
float AQIArr[10];
float TVOCArr[10];

//Avgs of measured values
float tempAvg = 0;
float humAvg = 0;
float CO2Avg = 0;
float AQIAvg = 0;
float TVOCAvg = 0;

bool isWarm = false;

//Sensors objects
//I2C on pins: D2:SDA D1:SCL
Adafruit_BME680 bme(&Wire);
DFRobot_ENS160_I2C ENS160(&Wire, 0x53);
MHZ co2(CO2_IN, MHZ19B);

//Calibration trigger ISR
void IRAM_ATTR handleCalibrationInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime > debounceDelay) {
    calibrateRequested = true;
    lastInterruptTime = currentTime;
  }
}


//-------------------------------------------------SETUP
void setup() {
  Serial.begin(115200);

  //MH-Z19 setup
  pinMode(CO2_IN, INPUT);
  pinMode(CALIB_PIN, OUTPUT);
  digitalWrite(CALIB_PIN, HIGH);  // HIGH = NOT calibrating
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), handleCalibrationInterrupt, FALLING);

  bme.begin();
  bme.setTemperatureOversampling(BME680_OS_8X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);

  ENS160.begin();
  ENS160.setPWRMode(ENS160_STANDARD_MODE);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Pripajam sa na WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
    if (connectTimer >= 15) {
      break;
    }
    connectTimer++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nPripojene na Wifi");
  } else {
    Serial.println("\nNepodarilo sa pripojit na Wifi");
  }

  //setup SD karty
  Serial.print("Initializing SD card...");
  if (!SD.begin(CS_PIN)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  // only one file can be open at a time, closing necessary after done with it
  myFile = SD.open("test.txt", FILE_WRITE);

  // write to file
  if (myFile) {
    Serial.print("Writing to test.txt...");
    myFile.println("Zaciatok merania");
    myFile.close();
    Serial.println("done.");
  } else {
    // print error if file didnt open
    Serial.println("error opening test.txt");
  }

  // reopen the file for reading
  myFile = SD.open("test.txt");
  if (myFile) {
    Serial.println("test.txt:");
    // read from the file until theres nothing else in it
    while (myFile.available()) { Serial.write(myFile.read()); }
    myFile.close();
  } else {
    // print error if file didnt open
    Serial.println("error opening test.txt");
  }

}
//-------------------------------------------------LOOP
void loop() {
  currentMillis = millis();

  static unsigned long warmupStartMillis = 0;
  static unsigned long lastStatusMillis = 0;

  // Warm-up phase
  if (!isWarm) {
    if (warmupStartMillis == 0) {
      warmupStartMillis = currentMillis;
      Serial.println("Zahrievanie senzorov... (3 minuty)");
    }

    if (currentMillis - lastStatusMillis >= 5000) {
      unsigned long secondsPassed = (currentMillis - warmupStartMillis) / 1000;
      Serial.print("Zahrievanie: ");
      Serial.print(secondsPassed);
      Serial.println("s / 180s");
      lastStatusMillis = currentMillis;

    }

    if (currentMillis - warmupStartMillis >= interval) {
      isWarm = true;
      Serial.println("Senzory su pripravene.");
    }

    return;
  }

  // measuring every 6 seconds, 10 measurement total, then averaging it -> 1 minute average
  for (int i = 0; i < 10; i++) {
    //BME680
    bme.performReading();
    tempArr[i] = bme.temperature;
    humArr[i] = bme.humidity;

    //MH-Z19
    CO2Arr[i] = co2.readCO2PWM();

    //ENS160
    ENS160.setTempAndHum(tempArr[i], humArr[i]);  //set parameters for correct AQI and TVOC measuremet
    AQIArr[i] = ENS160.getAQI();
    TVOCArr[i] = ENS160.getTVOC();

    Serial.print(i);
    Serial.print("/10: ");
    Serial.print(tempArr[i]);
    Serial.print(" °C, ");
    Serial.print(humArr[i]);
    Serial.print(" %, ");
    Serial.print(CO2Arr[i]);
    Serial.print(" ppm, ");
    Serial.print(AQIArr[i]);
    Serial.print(" -, ");
    Serial.print(TVOCArr[i]);
    Serial.println(" ppb");

    if (calibrateRequested) {
      Serial.println("Pred dalsim setom merani nastane kalibracia");
    }

    delay(6000);
  }

  tempAvg = average(tempArr);
  humAvg = average(humArr);
  CO2Avg = average(CO2Arr);
  AQIAvg = average(AQIArr);
  TVOCAvg = average(TVOCArr);
  measurementTime = millis();

  //measuring in the for cycle above takes 1 minute, so when calibrateCount > 20, 21 minutes have passed
  if (calibrating) {
    CO2Avg = 0;
    calibrateCount++;
    if (calibrateCount > 20) {
      calibrating = false;
      calibrateCount = 0;
    }
  }

  //JSON print
  //String formattedData = "{\"time\":" + String(measurementTime) + "," + "\"CO2\":" + String(CO2Avg, 0) + "," + "\"humidity\":" + String(humAvg, 1) + "," + "\"temperature\":" + String(tempAvg, 2) + "," + "\"index\":" + String((int)round(AQIAvg)) + "\"tvoc\":" + String(TVOCAvg, 0) "}";
  
  //CSV print
  String formattedData = String(measurementTime) + "," + String(CO2Avg, 0) + "," + String(humAvg, 1) + "," + String(tempAvg, 2) + "," + String((int)round(AQIAvg)) + "," + String(TVOCAvg, 0);

  Serial.print("Odoslane data: ");
  Serial.print(measurementTime);
  Serial.print(" ms, ");
  Serial.print(tempAvg);
  Serial.print(" °C, ");
  Serial.print(humAvg);
  Serial.print(" %, ");
  Serial.print(CO2Avg);
  Serial.print(" ppm, ");
  Serial.print(AQIAvg);
  Serial.print(" -, ");
  Serial.print(TVOCAvg);
  Serial.println(" ppb");
  Serial.println(formattedData);

  //write data to SD card
  myFile = SD.open("test.txt", FILE_WRITE);
  if (myFile) {
    Serial.print("Writing formatted data to test.txt...");
    myFile.println(formattedData);
    myFile.close();
    Serial.println("done writing formatted data.");
  } else {
    // print error if file didnt open
    Serial.println("error opening test.txt");
  }


  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverURL);

    //pre JSON
    //http.addHeader("Content-Type", "application/json");

    //pre CSV
    http.addHeader("Content-Type", "text/csv");
    int httpResponseCode = http.POST(formattedData);
 
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("Odpoved servera: " + response);
    } else {
      Serial.print("Chyba pri odosielani: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi nie je pripojena");
  }


  //turn on calibration
  if (calibrateRequested) {
    Serial.println("Zahajujem kalibraciu CO2 senzora (7s LOW)...");

    digitalWrite(CALIB_PIN, LOW);  // Trigger calibration
    delay(7000);
    digitalWrite(CALIB_PIN, HIGH);

    Serial.println("Kalibracia spustena - 20min nebudu prichadzat CO2 data.");

    calibrateRequested = false;
    calibrating = true;
    calibrateCount = 0;

  }
}

//-------------------------------------------------FUNCTIONS
float average(float arr[]) {
  float val = 0;
  for (int i = 0; i < 10; i++) {
    val += arr[i];
  }
  return val / 10.0;
}
