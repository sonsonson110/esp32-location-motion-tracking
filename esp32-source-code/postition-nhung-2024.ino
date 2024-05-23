#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <TinyGPS++.h>

#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// Gyroscope + Accelerometer + Temperature
Adafruit_MPU6050 mpu;
#define MPU6050_BAUDRATE 115200

// Wifi credentials and odds
const char* WIFI_SSID = "WIFI_NAME";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";
unsigned long previousMillis = 0;
unsigned long interval = 5000; // period of time to check for wifi status

// GPS
TinyGPSPlus gps;
#define GPS_BAUDRATE 9600


// Firebase 
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
#define API_KEY "FIREBASE_PROJECT_APPID"
#define DATABASE_URL "REALTIME_DATABASE_URL"
bool signupOK = false;

void initMPU6050() {
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  // Set up sensor range
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void initWifi() {
  // go to station mode (connect to another network)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD); // pass the network credentials
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.print("Connected! Local IP address: ");
  Serial.println(WiFi.localIP());
}

void initFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }
  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void setup(void) {
  Serial.begin(MPU6050_BAUDRATE);
  while (!Serial)
    delay(10); 

  initMPU6050();
  initWifi();
  initFirebase();

  // GPS
  Serial2.begin(GPS_BAUDRATE);


  delay(3000);
}

void loop() {
  Serial.println("----start loop----\n");

  // if WiFi is down, try reconnecting
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis >=interval) && (WiFi.status() != WL_CONNECTED)) {
    reconnectWifi();
    previousMillis = currentMillis;
  } 

  // GPS validation
  while (Serial2.available() > 0)
    gps.encode(Serial2.read());
  
  if (!gps.location.isValid()){
    delay(500);
    return;
  }


  /* Get new sensor events with the readings */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  /* send sensor data to Realtime Database */
  sendSensorData(a, g, temp);

  // logging
  logNEO6mGPS();
  logMPU6050(a, g, temp);


  Serial.println("----end loop----\n");
  delay(3000);
}

void reconnectWifi() {
  Serial.print(millis());
  Serial.println("Reconnecting to WiFi...");
  WiFi.disconnect();
  WiFi.reconnect();
}

void utcToVietnameseTz(char* tmbuffer, int buffersize) {
  int vietnameseHour = (gps.time.hour() + 7)  % 24;
  snprintf(tmbuffer, buffersize, "%d-%02d-%02dT%02d:%02d:%02d", gps.date.year(), gps.date.month(), gps.date.day(), vietnameseHour, gps.time.minute(), gps.time.second());
}

void logNEO6mGPS() {
  Serial.println("--start logNEO6mGPS--");

  Serial.print("LAT=");  Serial.println(gps.location.lat(), 6);
  Serial.print("LONG="); Serial.println(gps.location.lng(), 6);
  Serial.print("ALT=");  Serial.println(gps.altitude.meters());
  Serial.print("VELOCITY=");  Serial.println(gps.speed.kmph(), 6);
  Serial.print("SATELLITES=");  Serial.println(gps.satellites.value());

  char tmbuffer[20];
  utcToVietnameseTz(tmbuffer, 20);
  Serial.print("TIMESTAMP=");  Serial.println(tmbuffer);

  Serial.println("--end logNEO6mGPS--\n");
}

void logMPU6050(sensors_event_t a, sensors_event_t g, sensors_event_t temp) {
  Serial.println("--start logMPU6050--");

  Serial.printf("ACCELEROMETER X: %.5f, Y: %.5f, Z: %.5f m/s^2\n", a.acceleration.x, a.acceleration.y, a.acceleration.z);
  Serial.printf("GYROSCOPE X: %.5f, Y: %.5f, Z: %.5f rad/s\n", g.gyro.x, g.gyro.y, g.gyro.z);
  Serial.printf("TEMPERATURE: %.4f degC\n", temp.temperature);

  Serial.println("--end logMPU6050--\n");
}

void sendSensorData(sensors_event_t a, sensors_event_t g, sensors_event_t temp) {
  Serial.println("--start sendSensorData--");
  if (Firebase.ready() && signupOK) {
    FirebaseJson accelerometer;
    accelerometer.set("x", a.acceleration.x);
    accelerometer.set("y", a.acceleration.y);
    accelerometer.set("z", a.acceleration.z);

    FirebaseJson gyro;
    gyro.set("x", g.gyro.x);
    gyro.set("y", g.gyro.y);
    gyro.set("z", g.gyro.z);

    FirebaseJson realtimeJson;
    realtimeJson.set("accelerometer", accelerometer);
    realtimeJson.set("gyro", gyro);
    realtimeJson.set("temperature", temp.temperature);
    realtimeJson.set("lat", gps.location.lat());
    realtimeJson.set("lon", gps.location.lng());
    realtimeJson.set("alt", gps.altitude.meters());
    realtimeJson.set("velocity", gps.speed.kmph());
    
    char tmbuffer[20]; utcToVietnameseTz(tmbuffer, 20);
    realtimeJson.set("lastUpdated", tmbuffer);

    // create string path
    char* rtpath = "realtime/esp32";
    char histpath[50];
    strcpy(histpath, "location-history/esp32/");
    strcat(histpath, tmbuffer);

    // send
    Serial.printf("Set json for realtime...: %s\n", Firebase.RTDB.setJSON(&fbdo, rtpath, &realtimeJson) ? "realtime path updated!" : fbdo.errorReason().c_str());
    Serial.printf("Set json for history...: %s\n", Firebase.RTDB.setJSON(&fbdo, histpath, &realtimeJson) ? "history added!" : fbdo.errorReason().c_str());

  } else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
    Serial.println("--start sendSensorData--\n");
}
