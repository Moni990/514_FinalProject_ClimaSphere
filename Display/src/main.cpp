#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLE2902.h>
#include <stdlib.h>
#include <AccelStepper.h>
#include <FastLED.h>
#include <SwitecX25.h>
#define LEDPIN D9

// standard X25.168 ranges from 0 to 315 degrees, at 1/3 degree increments
#define STEPS 945
static int nextPos = 0;
bool setFlag = false;


// OLED display TWI address
#define OLED_ADDR   0x3C
// Reset pin # (or -1 if sharing Arduino reset pin)
#define RESET_PIN   -1
// For motors connected to D0, D1, D2, D3
SwitecX25 motor1(STEPS, D0, D1, D2, D3);

static BLEUUID serviceUUID("e52a72b3-5969-4ec8-a4a5-d308de94b252");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("a1d4b93e-ab02-421b-a786-9b1e2ae011d3");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

struct weatherData {
  int temperature;
  String weather;
};

Adafruit_SSD1306 display(128, 64, &Wire, RESET_PIN);

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    weatherData receivedSensorData;
    memcpy(&receivedSensorData, pData, sizeof(weatherData));

    int temperature = receivedSensorData.temperature;
    String weather = receivedSensorData.weather;

    Serial.print("Temperature: ");
    Serial.println(temperature);
    Serial.print("Weather: ");
    Serial.println(weather);

    // Update the display with the new temperature
    display.clearDisplay(); // Clear the display
    display.setCursor(0,0); // Set cursor to top-left
    display.println("Temp:");
    display.println(temperature);
    display.println("UV: ");
    if(weather == "strong_uv"){
      display.println("strong");
    } else{
      display.println("low");
    }
    if(weather == "rain"){
      display.println("rain");
      digitalWrite(D9, HIGH);
    } else{
      digitalWrite(D9, LOW);
    }
    display.display();

    // Calculate steps for the temperature change
    // Assuming 0 degrees = 0 steps (horizontal right position)
    int stepsPerDegree = 3; // since 945 steps for 315 degrees
    int tempChangeSteps = (temperature / 5) * (30 * stepsPerDegree); // temperature is assumed to be in increments of 5

    // Set motor position
    motor1.setPosition(tempChangeSteps);
    while(motor1.currentStep != motor1.targetStep){
      motor1.update(); // This call updates the motor position
      delay(10);
    }
}

class MyClientCallback : public BLEClientCallbacks {
   void onConnect(BLEClient* pclient){}

   void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");
    pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);

    connected = true;
    return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks


void setup() {
  Serial.begin(115200);
  // Wait for serial port to connect

  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("514");

  pinMode(D9, OUTPUT);
  digitalWrite(LEDPIN,HIGH);
  delay(1000);
  digitalWrite(LEDPIN,LOW);
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);

  // Initialize with the I2C addr 0x3C (for the 128x64)
  if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;){
      Serial.print('.');
      delay(100);
    } // Don't proceed, loop forever
  }

  display.clearDisplay();

  // Display static text
  display.setTextSize(1);      
  display.setTextColor(SSD1306_WHITE);  
  display.setCursor(0,0);     
  display.display();   
  motor1.setPosition(0); // Set initial position to 0 steps (0 degrees)
  motor1.update(); // This will move the motor to the initial position}
}

void loop() {
  // put your main code here, to run repeatedly:
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }
 
  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {
    String newValue = "Time since boot: " + String(millis()/1000);
    Serial.println("Setting new characteristic value to \"" + newValue  + "\"");

    // Set the characteristic's value to be the array of bytes that is actually a string.
    pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
  }else if(doScan){
    BLEDevice::getScan()->start(0);  // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
  }
 
    /*motor1.setPosition(300);
      while(motor1.currentStep != motor1.targetStep){
      motor1.update(); // This call updates the motor position
      delay(10);
    }*/

    delay(1000); // Delay a second between loops.
}
