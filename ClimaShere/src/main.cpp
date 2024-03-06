#include <Wire.h>
#include "Adafruit_LTR390.h"
#include "Adafruit_HTU21DF.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdlib.h>
#include <cmath>

#define LEDPIN D9

// Initialize the LTR390 UV sensor
Adafruit_LTR390 uv;

// Initialize the HTU21D-F sensor
Adafruit_HTU21DF htu;

// Define the digital pin for the water sensor
#define WATER_SENSOR_PIN D7

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long previousMillis = 0;
const long interval = 1000;
#define SERVICE_UUID        "e52a72b3-5969-4ec8-a4a5-d308de94b252"
#define CHARACTERISTIC_UUID "a1d4b93e-ab02-421b-a786-9b1e2ae011d3"

struct weatherData {
  int temperature;
  String weather;
};

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};

// TODO: add DSP algorithm functions here

void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE work!");

    // TODO: name your device to avoid conflictions
    BLEDevice::init("MoniMoni sensing");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharacteristic->addDescriptor(new BLE2902());
    pCharacteristic->setValue("Device connected!");
    pService->start();
    // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.println("Waiting for a client connection to notify...");
  
  // Initialize UV sensor
  if (!uv.begin()) {
    Serial.println("Failed to find LTR390 sensor");
    while (1){
      Serial.print(".");
      delay(10);
    }
  }
  
  // Initialize HTU21D-F sensor
  if (!htu.begin()) {
    Serial.println("Couldn't find HTU21D-F sensor");
    while (1){
      Serial.print(".");
      delay(10);
    }
  }

  // Set up the water sensor pin as input
  pinMode(WATER_SENSOR_PIN, INPUT);
}

void loop() {
  // Read water sensor
  String weather;
  int waterStatus = digitalRead(WATER_SENSOR_PIN);
  
  // Read UV sensor
  float uvs = uv.readUVS(); // corrected method to read UV
  if (uvs > 1000) { // Example threshold, adjust based on your needs
    Serial.println("Orange - Strong UV Rays Detected");
    weather = "strong_uv";
  } else {
    Serial.println("Yellow - Low UV Rays Detected");
    weather = "low_uv";
  }

  if (waterStatus == HIGH) {
    Serial.println("Blue - Water Detected");
    weather = "rain";
  }

  // Read temperature and humidity
  float temp = htu.readTemperature();
  int temperature = static_cast<int>(std::round(temp));

  float humi = htu.readHumidity();
  
  // Just as an example, print temperature and humidity to serial
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.print(" °C, Humidity: ");
  Serial.print(humi);
  Serial.println(" %");

  if (deviceConnected){
    weatherData data = {temperature, weather};
    
    uint8_t message[sizeof(weatherData)];
      memcpy(message, &data, sizeof(weatherData));
      
      unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
      // pCharacteristic->setValue(message);
      pCharacteristic->setValue(message, sizeof(weatherData));
      pCharacteristic->notify();
      // Serial.println("Notify value: " + data.total_jumps);
      Serial.print("Notify value: ");
      Serial.println(data.temperature);
      Serial.println(data.weather);
      previousMillis = currentMillis;
      }
  }

  delay(2000); // Wait for 2 seconds before next read
}
