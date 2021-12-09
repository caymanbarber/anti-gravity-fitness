#include <ArduinoBLE.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include <ArduinoJson.h>

//#define LED_PIN 23
#define HR_SENSOR_UPDATE_INTERVAL (10)
#define BLE_UPDATE_INTERVAL (1000)


/*
  Hardware Connections (Breakoutboard to Arduino):
  -5V = 5V (3.3V is allowed)
  -GND = GND
  -SDA = A4 (or SDA)
  -SCL = A5 (or SCL)
  -INT = Not connected
*/


MAX30105 particleSensor;

const char* deviceServiceUuid = "d7c72d3e-c632-4983-ac3f-7d2b98b53caa";
const char* deviceServiceCharacteristicUuid = "b91006d8-78c7-4494-9ec4-fdfe8dfb3cc7";
const char* deviceServiceCommandCharacteristicUuid = "9b1f22f1-2dc6-4bb0-b3c3-49399001b4d1";

const int value_size = 124; //

BLEService heartRateService(deviceServiceUuid); 
BLECharacteristic heartRateCharacteristic(deviceServiceCharacteristicUuid, BLERead | BLENotify, value_size);
BLECharCharacteristic commandCharacteristic(deviceServiceCommandCharacteristicUuid, BLERead | BLEWrite);

enum command { NaN = 0x00, on = 0x01, off = 0x02, reset = 0x03};
bool command_read = false;
char command_val = on;

const int arr_len = 50;

typedef struct __attribute__((packed))
{
  float bpm = 0;

  unsigned long timestamp = 0;
} send_data;

send_data SendData;

typedef struct __attribute__((packed))
{
  byte rates[arr_len];
  float beatsPerMinute[arr_len];

  unsigned long timestamp[arr_len];
  bool updated = false;
} sensor_data;

sensor_data SensorData;

char sendBuffer[value_size];

float beatsPerMinute;
int beatAvg;

byte rateSpot = 0;
long lastBeat = 0;

void setup() {
  ////Serial.println("Starting Setup");
  if (!arduino_setup()) {
    ////Serial.println("Arduino setup failed");
    while (1) {}
  }
  if (!heart_rate_setup()) {
    while (1) {}
    ////Serial.println("IMU setup failed");
  }
  if (!ble_setup()) {
    while (1) {}
    ////Serial.println("BLE setup failed");
  }

}

void loop() {
  BLEDevice central = BLE.central();
  delay(50);

  if (central) {
    //////Serial.println("Central");
    while (central.connected()) {


        if (ble_task()) {
          
        }
  
        if (heart_rate_task()) {
          
        }
    }
  } else {
    ////Serial.println(".");
  }
}

bool arduino_setup() {
  //Serial.begin(115200); //TODO
  //while( !Serial);
  delay(100);
  pinMode( LEDG, OUTPUT );
  pinMode( LEDR, OUTPUT );
  digitalWrite( LEDG, HIGH );
  digitalWrite( LEDR, LOW );
  delay(100);
  return true;
}

bool heart_rate_setup() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    ////Serial.println("Failed to initialize heartrate sensor!");
    return false;
  }

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0);
  delay(10);
  return true;
}

bool ble_setup() {
  if (!BLE.begin()) {
    ////Serial.println("* Starting BLE module failed!");
    return false;
  }
  delay(10);
  
  BLE.setDeviceName("Arduino Heartrate");
  BLE.setLocalName("Arduino Heartrate");
  BLE.setAdvertisedService(heartRateService);
  
  heartRateService.addCharacteristic(heartRateCharacteristic);
  heartRateService.addCharacteristic(commandCharacteristic);
  
  
  BLE.addService(heartRateService);

  //////Serial.println("Added Service");

  char init_value[100];
  heartRateCharacteristic.writeValue(init_value);
  commandCharacteristic.writeValue(off);

  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  
  BLE.advertise();
  ////Serial.println("Advertising");
  return true;
}

void bump_long_array(unsigned long new_val, unsigned long *arr, int arr_len) {
    //bumps array down by one. New value takes place of arr[0]
    memmove(&arr[1],&arr[0],(arr_len-1)*sizeof(long));
    arr[0] = new_val;
}

void bump_byte_array(byte new_val, byte *arr, int arr_len) {
    //bumps array down by one. New value takes place of arr[0]
    memmove(&arr[1],&arr[0],(arr_len-1)*sizeof(byte));
    arr[0] = new_val;
}


void make_send_buffer() {
    //creates char buffer to send

    StaticJsonDocument<value_size> doc;
    doc["bpm"] = SendData.bpm;
    
    doc["time"] = SendData.timestamp;
    serializeJson(doc, sendBuffer);
    //Serial.print(sendBuffer);
    delay(5);
    ////Serial.println(sendBuffer);
}

bool heart_rate_task() {

  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  digitalWrite( LEDG, LOW );
  
  if (currentMillis - previousMillis < HR_SENSOR_UPDATE_INTERVAL) {
    ////Serial.println("imu time failed");
    return false;
  }

  if (true){//particleSensor.available()) {
    ////Serial.println("Available");
    previousMillis = currentMillis;
    
    long irValue = particleSensor.getIR();
    delay(5);

    if (checkForBeat(irValue) == true)
    {
      ////Serial.println("Beat found");
      
      //We sensed a beat!
      long delta = millis() - lastBeat;
      lastBeat = millis();
  
      beatsPerMinute = 60 / (delta / 1000.0);
  
      if (beatsPerMinute < 255 && beatsPerMinute > 20)
      {
        digitalWrite( LEDG, HIGH );
        bump_long_array(millis(),SensorData.timestamp,arr_len);
        bump_byte_array((byte)beatsPerMinute, SensorData.rates, arr_len);

        SendData.bpm = average(SensorData.rates);
        SendData.timestamp = millis();
      }
    }
    SensorData.updated = true;
    return true;
    
  } else { 
    ////Serial.println("Not Available");
    return false;
  }
 
}

bool ble_task() {
  static unsigned long previousTime = 0;
  unsigned long currentTime = millis();
  //remove later
  
  if (currentTime - previousTime >= BLE_UPDATE_INTERVAL) {
    previousTime = currentTime;
    BLE.poll();
    delay(5);
  } else {
    return false;
  }

  command_val = commandCharacteristic.value();
  if (command_val == reset){
      //Serial.println("Reset sensors");
      //reset sensor? 
      command_val = on;
      commandCharacteristic.writeValue(on);
  }
  
  
  
  if (SensorData.updated && command_val == on) {
    SensorData.updated = false;

    memset(sendBuffer, '\0', sizeof(sendBuffer));
    make_send_buffer();
    heartRateCharacteristic.writeValue(sendBuffer);
    
    return true;
  }else {
    return false;
  }
}

float average(byte *arr) {
  float avg, sum = 0;
  for (int i = 0; i < arr_len; i++) {
    sum += (float)arr[i];
  }
  avg = sum/arr_len;
  return avg;
}

void copy(float *src_array, float *dest_array) {
  for (int i = 0; i < arr_len; i++) {
    dest_array[i] = src_array[i];
  }
}

void blePeripheralConnectHandler(BLEDevice central) {
  digitalWrite( LEDG, LOW );
  digitalWrite( LEDR, HIGH );
  delay(5);
  //Serial.print(F( "Connected to central: " )); //TODO
  //Serial.println(central.address());
}

void blePeripheralDisconnectHandler( BLEDevice central ) {
  digitalWrite( LEDG, HIGH );
  digitalWrite( LEDR, LOW );
  delay(5);
  //Serial.print(F("Disconnected from central: "));
  //Serial.println(central.address());
}
