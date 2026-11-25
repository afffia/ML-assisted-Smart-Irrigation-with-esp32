#include <Wire.h>
#include <RTClib.h>
#include <DHTesp.h>
#include "IrrigationModel.h"

#define SOIL_PIN 34
#define DHT_PIN 4
#define FLOAT_PIN 27
#define RELAY_PIN 25

RTC_DS3231 rtc;
DHTesp dht;
Eloquent::ML::Port::DecisionTreeRegressor mlModel;

const int AIR_VALUE = 2560;
const int WATER_VALUE = 1200;

// Safety rule limits (from AgriSense Slide 5)
const int MAX_RUN_SECONDS = 10;
const unsigned long COOLDOWN_MS = 15000; // 15s cooldown for demo (increase for real garden)
unsigned long lastWaterTime = 0;
bool rtcReady = false;

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(FLOAT_PIN, INPUT_PULLUP);
  dht.setup(DHT_PIN, DHTesp::DHT22);

  Wire.begin(21, 22);
  if (rtc.begin()) {
    rtcReady = true;
    if (rtc.lostPower()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }
  Serial.println("soil_moisture,temperature,humidity,hour,pump_seconds,status");
}

void loop() {
  TempAndHumidity data = dht.getTempAndHumidity();
  float temp = isnan(data.temperature) ? 28.0 : data.temperature;
  float hum = isnan(data.humidity) ? 55.0 : data.humidity;

  int rawSoil = analogRead(SOIL_PIN);
  int soilPercent = constrain(map(rawSoil, AIR_VALUE, WATER_VALUE, 0, 100), 0, 100);
  bool tankHasWater = (digitalRead(FLOAT_PIN) == LOW);

  int currentHour = rtcReady ? rtc.now().hour() : 8; // Defaults to 8 AM if RTC unplugged

  // 1. TinyML Offline Inference: predicts run-time in seconds
  float features[] = { (float)soilPercent, temp, hum, (float)currentHour };
  int runSeconds = round(mlModel.predict(features));

  // Fallback to fixed threshold if ML output is invalid
  if (runSeconds < 0 || runSeconds > 30) {
    runSeconds = (soilPercent < 35) ? 5 : 0;
  }

  // 2. Safety Rules Always Win (Slide 4 & 5)
  bool inWateringWindow = (currentHour >= 6 && currentHour <= 10) || (currentHour >= 16 && currentHour <= 20);
  bool cooldownPassed = (millis() - lastWaterTime >= COOLDOWN_MS);
  String status = "HOLD";

  if (runSeconds > 0) {
    if (!tankHasWater) {
      runSeconds = 0;
      status = "LOCKED_TANK_EMPTY";
    } else if (!inWateringWindow) {
      runSeconds = 0;
      status = "LOCKED_TIME_WINDOW";
    } else if (!cooldownPassed) {
      runSeconds = 0;
      status = "LOCKED_COOLDOWN";
    } else {
      runSeconds = min(runSeconds, MAX_RUN_SECONDS);
      status = "WATERING";
    }
  }

  // 3. Log CSV row over USB Serial for live laptop learning
  Serial.printf("%d,%.1f,%.1f,%d,%d,%s\n", soilPercent, temp, hum, currentHour, runSeconds, status.c_str());

  // 4. Actuate pump for predicted seconds
  if (runSeconds > 0) {
    digitalWrite(RELAY_PIN, LOW);
    delay(runSeconds * 1000);
    digitalWrite(RELAY_PIN, HIGH);
    lastWaterTime = millis();
  }

  delay(2000);
}
