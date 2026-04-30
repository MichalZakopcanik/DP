//MH-Z19
#include <MHZ.h>
//ENS160
#include <DFRobot_ENS160.h>
//BME680
#include <Adafruit_BME680.h>
//HM3301 - PM1.0,2.5,10
#include "Seeed_HM330X.h"
//I2C
#include <Wire.h>
//Web comm.
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

//SD card
#include <SPI.h>
#include <SD.h>

//Timer and MQTT
#include <Ticker.h>
#include <AsyncMqttClient.h>


//-------------------------------------------------DECLARATIONS
// WiFi
const char* serverURL = "";
const char* wifiSsids[] = {"wifi1", "wifi2", "wifi3"}; //replace with your wifi credentials
const char* wifiPasswords[] = {"pass1", "pass2", "pass3"};
size_t wifiCount = sizeof(wifiSsids)/sizeof(wifiSsids[0]);
int currentWifi = 0;

// MQTT setup
//Length of ips should match the wifiCount
#define MQTT_HOST IPAddress(example_num1, example_num2, example_num3, example_num4) //replace with your MQTT IP address
#define MQTT_PORT example_num //replace with your MQTT port
#define MQTT_DATA_TOPIC "/example/data" //replace with your topic

AsyncMqttClient mqttClient;

// Event handlers
Ticker mqttReconnectTimer;
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;

// Wifi reconnecting
bool shouldReconnectWifi = false;
// MQTT reconnecting count
int mqttReconnectCount = 0;
#define MQTT_MAX_REC_COUNT 3

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
bool isWarm = false;

//Measuring variables
bool isMeasuring = true;
static unsigned long lastMeasurementMillis = 0;
int measurementIndex = 0;

//SD card file
File myFile;
int lineCounter = 0;

#define CO2_IN 16      //D0 - MH-Z19 PWM IN
#define CALIB_PIN 2   //D4 - MH-Z19 Calibration pin
#define SWITCH_PIN 0  //D3 - Calibration button
#define CS_PIN 15  //D8 - Chip select pin

//Arrays for measurements
float tempArr[10];
float humArr[10];
float CO2Arr[10];
float AQIArr[10];
float TVOCArr[10];
float PM1Arr[10];
float PM2Arr[10]; //PM2,5
float PM10Arr[10];


//Avgs of measured values
float tempAvg = 0;
float humAvg = 0;
float CO2Avg = 0;
float AQIAvg = 0;
float TVOCAvg = 0;
float PM1Avg = 0;
float PM2Avg = 0;
float PM10Avg = 0;


//Sensors objects
//I2C on pins: D2:SDA D1:SCL
Adafruit_BME680 bme(&Wire);
DFRobot_ENS160_I2C ENS160(&Wire, 0x53);
MHZ co2(CO2_IN, MHZ19B);
HM330X pm;
u8 buf[30]; //for HM3301


//---------FUNCTIONS----------
//Wifi
void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n; i++){
    if(WiFi.SSID(i)=="Pispot"){
      Serial.println("Pispot found, connecting");
      WiFi.begin("Pispot",wifiPasswords[0]);
      currentWifi = 0;
      return;
    }
  }
  for (int i = 0; i < n; i++){
    for(int j = 0; j < (int)wifiCount;j++){
      if(WiFi.SSID(i)==wifiSsids[j]){
        Serial.print("SSID: ");
        Serial.println(wifiSsids[j]);
        currentWifi = j;
        WiFi.begin(wifiSsids[j],wifiPasswords[j]);
        return;
      }
    }
  }
  Serial.println("No saved WiFi found.");
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("\nConnected to Wi-Fi.");
  mqttClient.setServer(ips[currentWifi], MQTT_PORT);
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  Serial.print("Disconnect reason: ");
  Serial.println(event.reason);
  shouldReconnectWifi = true;
}

//MQTT
void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  mqttReconnectCount=0;
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  
  if (WiFi.isConnected()) {
    mqttReconnectCount++;
    if (mqttReconnectCount >= MQTT_MAX_REC_COUNT){
      Serial.println("Too many MQTT connection retries. Trying different wifi.");
      mqttReconnectCount=0;
      WiFi.disconnect();
      return;
    }
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

/*void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}*/

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void mqttPublish(const char* topic, const char* data) {
  uint16_t packetIdPub = mqttClient.publish(topic, 1, true, data);
  Serial.printf("Publishing on topic %s at QoS 1, packetId: %i\n", topic, packetIdPub);
  Serial.printf("Message: %s \n", data);
}

//Calibration trigger ISR
void IRAM_ATTR handleCalibrationInterrupt() {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime > debounceDelay) {
    calibrateRequested = true;
    lastInterruptTime = currentTime;
  }
}

/*parse buf with 29 u8-data - function for getting data from HM3301*/
HM330XErrorCode parse_result(int position, u8 *data)
{
  u16 value=0;
  if(NULL==data)
    return ERROR_PARAM;
  uint8_t sum = 0;
  for (int i = 0; i < 28; i++) {
    sum += data[i];
  }
  if (sum != data[28]) {
    Serial.println("HM3301 checksum error!");
    return ERROR_OTHERS;
  }
 
  PM1Arr[position] = (u16)data[4]<<8|data[5];
  PM2Arr[position] = (u16)data[6]<<8|data[7];
  PM10Arr[position] = (u16)data[8]<<8|data[9];

  return NO_ERROR;
}

//Averaging measurements
float average(float arr[]) {
  float sum = 0;
  int count = 0;
  for (int i = 0; i < 10; i++) {
    if(!isnan(arr[i])){
      sum += arr[i];
      count++;
    }
  }
  return count > 0 ? sum / count : 0;
}

//-------------------------------------------------SETUP
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Serial start");

  //HM3301 setup
  if(pm.init() != NO_ERROR)
  {
      Serial.println("HM330X init failed!!!");
      while(1);
  }

  //MH-Z19 setup
  pinMode(CO2_IN, INPUT);
  pinMode(CALIB_PIN, OUTPUT);
  digitalWrite(CALIB_PIN, HIGH);  // HIGH = NOT calibrating
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), handleCalibrationInterrupt, FALLING);

  bme.begin();
  bme.setTemperatureOversampling(BME680_OS_2X);
  bme.setHumidityOversampling(BME680_OS_2X);
  bme.setIIRFilterSize(BME680_FILTER_SIZE_3);

  ENS160.begin();
  ENS160.setPWRMode(ENS160_STANDARD_MODE);

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setCredentials("airqManager", "DPg0al26");

  // Connect to Wi-Fi
  connectToWifi();

  //SD card setup
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
}
//-------------------------------------------------LOOP
void loop() {
  if(shouldReconnectWifi){
    shouldReconnectWifi = false;
    connectToWifi();
  }
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
  
  if (isMeasuring) {
    // measuring every 6 seconds, 10 measurement total, then averaging it -> 1 minute average
    if(currentMillis - lastMeasurementMillis >= 6000) {
      lastMeasurementMillis = currentMillis;
      //BME680
      bme.performReading();
      tempArr[measurementIndex] = bme.temperature;
      humArr[measurementIndex] = bme.humidity;

      //MH-Z19
      CO2Arr[measurementIndex] = co2.readCO2PWM();

      //ENS160
      ENS160.setTempAndHum(tempArr[measurementIndex], humArr[measurementIndex]);  //set parameters for correct AQI and TVOC measuremet
      AQIArr[measurementIndex] = ENS160.getAQI();
      TVOCArr[measurementIndex] = ENS160.getTVOC();

      //HM3301
      if(pm.read_sensor_value(buf,29))//returns 0 if no error so doesnt enter if branch
      {
          Serial.println("HM330X read result failed!!!");
      } 
      else {
        if(parse_result(measurementIndex, buf) != NO_ERROR){
          Serial.println("HM330X data parsing failed!!!");
          PM1Arr[measurementIndex] = NAN;
          PM2Arr[measurementIndex] = NAN;
          PM10Arr[measurementIndex] = NAN;
        }
      }

      Serial.print(measurementIndex+1);
      Serial.print("/10: ");
      Serial.print(tempArr[measurementIndex]);
      Serial.print(" °C, ");
      Serial.print(humArr[measurementIndex]);
      Serial.print(" %, ");
      Serial.print(CO2Arr[measurementIndex]);
      Serial.print(" ppm, ");
      Serial.print(AQIArr[measurementIndex]);
      Serial.print(" -, ");
      Serial.print(TVOCArr[measurementIndex]);
      Serial.print(" ppb, ");
      Serial.print(PM1Arr[measurementIndex]);
      Serial.print(" ug/m3, ");
      Serial.print(PM2Arr[measurementIndex]);
      Serial.print(" ug/m3, ");
      Serial.print(PM10Arr[measurementIndex]);
      Serial.println(" ug/m3");

      if (calibrateRequested) {
        Serial.println("Pred dalsim setom merani nastane kalibracia");
      }

      //Incrementing measurement
      measurementIndex++;
      if(measurementIndex>=10){
        measurementIndex = 0;
        isMeasuring = false;
      }
    }
    return;
  }

  tempAvg = average(tempArr);
  humAvg = average(humArr);
  CO2Avg = average(CO2Arr);
  AQIAvg = average(AQIArr);
  TVOCAvg = average(TVOCArr);
  PM1Avg = average(PM1Arr);
  PM2Avg = average(PM2Arr);
  PM10Avg = average(PM10Arr);
  measurementTime = millis();
  String co2Str = "";
  String co2StrCsv = "";

  if (!calibrating) {
    co2Str = String(CO2Avg, 0);
    co2StrCsv = String(CO2Avg, 0);
  }

  //measuring in the for cycle above takes 1 minute, so when calibrateCount > 20, 21 minutes have passed
  else {
    calibrateCount++;
    co2Str = "null";
    co2StrCsv = "";
    if (calibrateCount > 20) {
      calibrating = false;
      calibrateCount = 0;
    }
  }
  
  String tstmpStr = String(measurementTime);
  String tempStr = String(tempAvg, 2);
  String humStr = String(humAvg, 1);
  String pm1Str = String(PM1Avg, 2);
  String pm2Str = String(PM2Avg, 2);
  String pm10Str = String(PM10Avg, 2);
  String aqiStr = String((int)round(AQIAvg)); //because AQI is not float
  String tvocStr = String(TVOCAvg, 0);

  //CSV print
  String formattedData = tstmpStr + "," + co2StrCsv + "," + humStr + "," + tempStr + "," + aqiStr + "," + tvocStr + "," + pm1Str + "," + pm2Str + "," + pm10Str;

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
  Serial.print(" ppb, ");
  Serial.print(PM1Avg);
  Serial.print(" ug/m3, ");
  Serial.print(PM2Avg);
  Serial.print(" ug/m3, ");
  Serial.print(PM10Avg);
  Serial.println(" ug/m3");
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

  if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
    //JSON-like message for MQTT broker
    String payload = "{"
      "\"millisTimestamp\":" + tstmpStr + ","
      "\"temperature\":" + tempStr + ","
      "\"humidity\":" + humStr + ","
      "\"pm1\":" + pm1Str + ","
      "\"pm2\":" + pm2Str + ","
      "\"pm10\":" + pm10Str + ","
      "\"co2\":" + co2Str + ","
      "\"aqi\":" + aqiStr + ","
      "\"tvoc\":" + tvocStr +
    "}";

    mqttPublish(MQTT_DATA_TOPIC, payload.c_str());

    /* if sending info to a specific server - needs internet
    
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverURL);

    //pre JSON
    //http.addHeader("Content-Type", "application/json");
    //int httpResponseCode = http.POST(payload);

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

    http.end();*/
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
  //Allow measuring
  isMeasuring = true;
}
