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


//I2C on pins:           D2:SDA D1:SCL
#define LED_PIN 2      //D4 - signal LED
#define CO2_IN 14      //D5 - MH-Z19 PWM IN
#define CALIB_PIN 12   //D6 - MH-Z19 Calibration pin
#define SWITCH_PIN 13  //D7 - Calibration button


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

//Avgs of measured values
float tempAvg = 0;
float humAvg = 0;
float CO2Avg = 0;
float AQIAvg = 0;

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
  blink(500, 2);  //zaciatok seutpu
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // Turn off built-in LED (active LOW)

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
  }
  Serial.println("\nPripojene na WiFi");

  blink(500, 2);  //Setup complete
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

      //blink(100, 1);  // Blink once every 5s for feedback
    }

    if (currentMillis - warmupStartMillis >= interval) {
      isWarm = true;
      Serial.println("Senzory su pripravene.");
    }

    return;
  }

  digitalWrite(LED_PIN, LOW);  // Turn LED on (active LOW)


  for (int i = 0; i < 10; i++) {
    //BME680
    bme.performReading();
    tempArr[i] = bme.temperature;
    humArr[i] = bme.humidity;

    //MH-Z19
    CO2Arr[i] = co2.readCO2PWM();

    //ENS160
    ENS160.setTempAndHum(tempArr[i], humArr[i]);  //set parameters for correct AQI measuremet
    AQIArr[i] = ENS160.getAQI();

    Serial.print(i);
    Serial.print("/10: ");
    Serial.print(tempArr[i]);
    Serial.print(" °C, ");
    Serial.print(humArr[i]);
    Serial.print(" %, ");
    Serial.print(CO2Arr[i]);
    Serial.print(" ppm, ");
    Serial.print(AQIArr[i]);
    Serial.println(" -");

    if (calibrateRequested) {
      Serial.println("Pred dalsim setom merani nastane kalibracia");
    }

    delay(6000);
  }

  tempAvg = average(tempArr);
  humAvg = average(humArr);
  CO2Avg = average(CO2Arr);
  AQIAvg = average(AQIArr);

  if (calibrating) {
    CO2Avg = 0;
    calibrateCount++;
    if (calibrateCount > 20) {
      calibrating = false;
      calibrateCount = 0;
    }
  }

  String formattedData = "{\"CO2\":" + String(CO2Avg, 0) + "," + "\"humidity\":" + String(humAvg, 1) + "," + "\"temperature\":" + String(tempAvg, 2) + "," + "\"index\":" + String(AQIAvg, 0) + "}";
  
  Serial.print("Odoslane data: ");
  Serial.print(tempAvg);
  Serial.print(" °C, ");
  Serial.print(humAvg);
  Serial.print(" %, ");
  Serial.print(CO2Avg);
  Serial.print(" ppm, ");
  Serial.print(AQIAvg);
  Serial.println(" -");
  Serial.println(formattedData);


  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverURL);
    http.addHeader("Content-Type", "application/json");

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

  blink(200, 5);  // Blink after sending data


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

    blink(100, 10);
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

void blink(int time, int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(LED_PIN, LOW);  // Turn on (active LOW)
    delay(time);
    digitalWrite(LED_PIN, HIGH);  // Turn off
    delay(time);
  }
}



