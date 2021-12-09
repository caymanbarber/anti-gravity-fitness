#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>
#include <ArduinoJson.h>

#define LED_PIN 23
#define IMU_SENSOR_UPDATE_INTERVAL (25)
#define BLE_UPDATE_INTERVAL (150)

const char* deviceServiceUuid = "fb234ef4-c523-4af5-b161-c12ab497ea8c";
const char* deviceServiceCharacteristicUuid = "0ca01305-76b4-4856-b325-aa733e6f9be9";
const char* deviceServiceCommandCharacteristicUuid = "f810f934-9f66-416d-bc5b-493d04b2bb48";

const int value_size = 124; //

BLEService movementService(deviceServiceUuid); 
BLECharacteristic movementCharacteristic(deviceServiceCharacteristicUuid, BLERead | BLENotify, value_size);
BLECharCharacteristic commandCharacteristic(deviceServiceCommandCharacteristicUuid, BLERead | BLEWrite);

enum command { NaN = 0x00, on = 0x01, off = 0x02, reset = 0x03};
bool command_read = false;
char command_val = on;

const int arr_len = 10;

typedef struct __attribute__((packed))
{
  float accelX[arr_len];
  float accelY[arr_len];
  float accelZ[arr_len];
  bool accel_updated = false;
  
  float gyroX [arr_len];
  float gyroY [arr_len];
  float gyroZ [arr_len];
  bool gyro_updated = false;
  
  unsigned long timestamp [arr_len];
} sensor_data;

sensor_data SensorData;

typedef struct __attribute__((packed))
{
  float accelX[arr_len];
  float accelY[arr_len];
  float accelZ[arr_len];
  
  float gyroX [arr_len];
  float gyroY [arr_len];
  float gyroZ [arr_len];
} filtered_data;

filtered_data FilteredData;

typedef struct __attribute__((packed))
{
  float rel_posX = 0;
  float rel_posY = 0;
  float rel_posZ = 0;

  float velX = 0;
  float velY = 0;
  float velZ = 0;

  unsigned long timestamp = 0;
} send_data;

send_data sendData;

typedef struct __attribute__((packed))
{
  float X=0;
  float Y=0;
  float Z=0;

  float I=0;
  float J=0;
  float K=0;

  unsigned long timestamp;
} imu_bias;

static imu_bias bias;

char sendBuffer[value_size];

void setup() {
  ////Serial.println("Starting Setup");
  if (!arduino_setup()) {
    ////Serial.println("Arduino setup failed");
    while (1) {}
  }
  if (!imu_setup()) {
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
  
        if (imu_task()) {
          
        }
    }
  } else {
    ////Serial.println(".");
  }
}

bool arduino_setup() {
  ////Serial.begin(115200);
  //while( !////Serial);
  pinMode( LEDG, OUTPUT );
  pinMode( LEDR, OUTPUT );
  digitalWrite( LEDG, HIGH );
  digitalWrite( LEDR, LOW );
  delay(10);
  return true;
}

bool imu_setup() {
  if (!IMU.begin()) {
    ////Serial.println("Failed to initialize IMU!");
    return false;
  }
  for (int i = 0; i < arr_len; i++) {
    SensorData.timestamp[i] = 0;
  }
  
  
  delay(10);
  return true;
}

bool ble_setup() {
  if (!BLE.begin()) {
    ////Serial.println("* Starting BLE module failed!");
    return false;
  }
  delay(10);
  
  BLE.setDeviceName("Arduino Dumbbell");
  BLE.setLocalName("Arduino Dumbbell");
  BLE.setAdvertisedService(movementService);
  
  movementService.addCharacteristic(movementCharacteristic);
  movementService.addCharacteristic(commandCharacteristic);
  
  
  BLE.addService(movementService);

  //////Serial.println("Added Service");

  char init_value[100];
  movementCharacteristic.writeValue(init_value);
  commandCharacteristic.writeValue(off);

  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  
  BLE.advertise();
  ////Serial.println("Advertising");
  return true;
}

void bump_float_array(float new_val, float *arr, int arr_len) {
    //bumps array down by one. New value takes place of arr[0]
    memmove(&arr[1],&arr[0],(arr_len-1)*sizeof(float));
    arr[0] = new_val;
}

void bump_long_array(unsigned long new_val, unsigned long *arr, int arr_len) {
    //bumps array down by one. New value takes place of arr[0]
    memmove(&arr[1],&arr[0],(arr_len-1)*sizeof(long));
    arr[0] = new_val;
}

void make_send_buffer() {
    //creates char buffer to send

    StaticJsonDocument<value_size> doc;
    // //doc["pX"] = sendData.rel_posX*10;
    //////Serial.print("rel posX: ");
    //////Serial.print(sendData.rel_posX, 4);
    // //doc["pY"] = sendData.rel_posY*10;
    doc["pZ"] = sendData.rel_posZ*10;

    // //doc["vX"] = sendData.velX*10;
    // //doc["vY"] = sendData.velY*10;
    doc["vZ"] = sendData.velZ*10;
    
    doc["time"] = sendData.timestamp;
    serializeJson(doc, sendBuffer);
    ////Serial.print(sendData.rel_posZ*10);
    ////Serial.print("\t");
    ////Serial.println(sendData.velZ*10);
    delay(1);
    //////Serial.println(sendBuffer);
}

void find_vel_and_pos() {

  sendData.rel_posX = FilteredData.accelX[3] + bias.X;
  sendData.rel_posY = FilteredData.accelY[3] + bias.Y;
  sendData.rel_posZ = FilteredData.accelZ[3] + bias.Z;

  float last_velX = sendData.velX;
  float last_velY = sendData.velY;
  float last_velZ = sendData.velZ;

  unsigned long last_time = sendData.timestamp;

  int k = arr_len + 1;
  //find timestamp in SensorData.timestamp[j]
  for (int i = 0; i < arr_len; i++) {
    //////Serial.println(SensorData.timestamp[i]);
    if (last_time == SensorData.timestamp[i]) {
      k = i;
      //////Serial.println(k);
      break;
    }
  }

  if (k > arr_len) {
    //////Serial.println("time not found");
    sendData.velX = 0.0;
    sendData.velY = 0.0;
    sendData.velZ = 0.0;
    sendData.timestamp = SensorData.timestamp[3];
  } else {
      /*
      ////Serial.print("Last time:");
      ////Serial.println(last_time);

      ////Serial.print("Newest time:");
      ////Serial.println(SensorData.timestamp[3]);
      
      ////Serial.print("Index of last_sample:");
      ////Serial.println(k);
      */
      // say k = 4
      float sumX, sumY, sumZ = 0.0;
      float time_sum = 0;
      
      /*
      ////Serial.print("time 3:");
      ////Serial.print(SensorData.timestamp[3]);
      ////Serial.print(" - time 4:");
      ////Serial.println(SensorData.timestamp[4]);
      ////Serial.print("= ");
      ////Serial.println(SensorData.timestamp[3] - SensorData.timestamp[4]);
      ////Serial.print("*1000 = ");
      ////Serial.println(0.001*(SensorData.timestamp[3] - SensorData.timestamp[4]));
      */
      
      for (int j = 3; j <k; j++) {
        float delta_t = 0.001*(SensorData.timestamp[j] - SensorData.timestamp[j+1]); //in seconds not ms
        time_sum += delta_t;
        sumX += delta_t * (FilteredData.accelX[j] + bias.X);
        sumY += delta_t * (FilteredData.accelY[j] + bias.Y);
        sumZ += delta_t * (FilteredData.accelZ[j] + bias.Z);
        //sumZ += delta_t * (FilteredData.accelZ[j-1] + bias.Z-0.9806);
        //timestamp at 3+1 - timestamp at 3
      }
      /*
      ////Serial.print("sum time: ");
      ////Serial.println(time_sum);
      
      ////Serial.print("sum Z: ");
      ////Serial.println(sumZ*10,5);
      */
      float drift_correction = 0.05;
      
      sendData.velX = last_velX + sumX - drift_correction*sendData.velX;
      sendData.velY = last_velY + sumY - drift_correction*sendData.velY;
      sendData.velZ = last_velZ + sumZ - drift_correction*sendData.velZ;

      sendData.timestamp = SensorData.timestamp[3];
    
      //search filtered data for last_time -> index k
      //from index k to index 3 sum delta t * accel value via (rieman sum) for integration for each x,y,z
      //probably just last value (k = 4)
      

  }
}


//just sends acceleration values as pos and gyro as vel
void test_find_vel_and_pos() {
  //filtered_data + send_data = send_data
  //TODO: use array structs, filter, and use calculus to get vel and rel_pos

  //////Serial.println("\n Filtered X[0]: ");
  //////Serial.println(FilteredData.accelX[0],5);
  sendData.rel_posX = FilteredData.accelX[3]+bias.X;
  sendData.rel_posY = FilteredData.accelY[3]+bias.Y;
  sendData.rel_posZ = FilteredData.accelZ[3]+bias.Z;

  sendData.velX = FilteredData.gyroX[3]+bias.I;
  sendData.velY = FilteredData.gyroY[3]+bias.J;
  sendData.velZ = FilteredData.gyroZ[3]+bias.K;  

  sendData.timestamp = SensorData.timestamp[3];
  /*
  ////Serial.println("\n time stamp ");
  ////Serial.println(SensorData.timestamp[0]);
  */
  
}

bool imu_task() {

  static long previousMillis = 0;
  unsigned long currentMillis = millis();
  
  static float x = 0.00, y = 0.00, z = 0.00;
  static float i = 0.00, j = 0.00, k = 0.00;
  
  if (currentMillis - previousMillis < IMU_SENSOR_UPDATE_INTERVAL) {
    //////Serial.println("imu time failed");
    return false;
  }
  
  previousMillis = currentMillis;
  
  if(IMU.accelerationAvailable()){
    //////Serial.println("Available");
    
    IMU.readAcceleration(x, y, z);
    //IMU.readGyroscope(i, j, k);

    bump_long_array(millis(),SensorData.timestamp,arr_len);

    bump_float_array(x, SensorData.accelX, arr_len);
    bump_float_array(y, SensorData.accelY, arr_len);
    bump_float_array(z, SensorData.accelZ, arr_len);
    SensorData.accel_updated = true;

    //bump_float_array(j, SensorData.gyroX, arr_len);
    //bump_float_array(j, SensorData.gyroY, arr_len);
    //bump_float_array(k, SensorData.gyroZ, arr_len);
    SensorData.gyro_updated = true;
    return true;
    
  } else {
    ////Serial.println("Not Available");
    ////Serial.print("Accel available?: ");
    ////Serial.println(IMU.accelerationAvailable());
    ////Serial.print("Gyro available?: ");
    ////Serial.println(IMU.gyroscopeAvailable());
    
    
    SensorData.accel_updated = false;
    SensorData.gyro_updated = false;
    return false;
  }
    
  return (SensorData.accel_updated && SensorData.gyro_updated);
}

bool ble_task() {
  static uint32_t previousTime = 0;
  uint32_t currentTime = millis();
  //remove later
  SensorData.accel_updated = true;
  SensorData.gyro_updated = true;
  
  if (currentTime - previousTime >= BLE_UPDATE_INTERVAL) {
    previousTime = currentTime;
    BLE.poll();
  } else {
    return false;
  }

  command_val = commandCharacteristic.value();
      
  if (command_val == reset){
      ////Serial.println("Reset sensors");
      reset_sensor(); 
      command_val = on;
      commandCharacteristic.writeValue(on);
  }
  
  
  if (SensorData.accel_updated && SensorData.gyro_updated && command_val == on) {
    //////Serial.println("filtering data");
    filter_data();
    //////Serial.println("data filtered");
    find_vel_and_pos();

    memset(sendBuffer, '\0', sizeof(sendBuffer));
    make_send_buffer();
    //////Serial.println(sendBuffer);

    movementCharacteristic.writeValue(sendBuffer);

    SensorData.accel_updated = false;
    SensorData.gyro_updated = false;
    return true;
  }else {
    return false;
  }
}

float average(float *arr) {
  float avg, sum = 0;
  for (int i = 0; i < arr_len; i++) {
    sum += arr[i];
  }
  avg = sum/arr_len;
  return avg;
}

void reset_sensor() {
  //reset velocity to 0,0,0
  
  
  //////Serial.println("Resetting");
  sendData.velX = 0.0;
  sendData.velY = 0.0;
  sendData.velZ = 0.0; 
  
  bias.X = (-1)*average(FilteredData.accelX);
  bias.Y = (-1)*average(FilteredData.accelY);
  bias.Z = (-1)*average(FilteredData.accelZ);
  //bias.Z = (0.9806)+(-1)*average(FilteredData.accelZ);

  //bias.I = (-1)*average(FilteredData.gyroX);
  //bias.J = (-1)*average(FilteredData.gyroY);
  //bias.K = (-1)*average(FilteredData.gyroZ);

  command_read = true;
}

void filter_data(){
  
  //median_filter(SensorData.gyroX, 5, FilteredData.gyroX);
  //median_filter(SensorData.gyroY, 5, FilteredData.gyroY);
  //median_filter(SensorData.gyroZ, 5, FilteredData.gyroZ);

  
  
  //////Serial.println("median filtered 1");
  //copy(SensorData.accelX, FilteredData.accelX);
  //copy(SensorData.accelY, FilteredData.accelY);
  //copy(SensorData.accelZ, FilteredData.accelZ);
  median_filter(SensorData.accelX, 5, FilteredData.accelX);
  median_filter(SensorData.accelY, 5, FilteredData.accelY);
  median_filter(SensorData.accelZ, 5, FilteredData.accelZ);

 

  //////Serial.println("median filtered 2");

  //low_pass_filter(FilteredData.gyroX);
  //low_pass_filter(FilteredData.gyroY);
  //low_pass_filter(FilteredData.gyroZ);

  low_pass_filter(FilteredData.accelX);
  low_pass_filter(FilteredData.accelY);
  low_pass_filter(FilteredData.accelZ);
  
}

void copy(float *src_array, float *dest_array) {
  for (int i = 0; i < arr_len; i++) {
    dest_array[i] = src_array[i];
  }
}

void low_pass_filter(float *arr) {
  float temp_arr[arr_len];
  
  float filter_taps[5] = {-0.07801,
                           0.27071,
                           0.63580,
                           0.27071,
                          -0.07801};
                          
  /*float filter_taps[5] = {0.0,
                           0.0,
                           1.0,
                           0.0,
                           0.0}; 
                        */

  /*float filter_taps[5] = {0.2,
                           0.2,
                           0.2,
                           0.2,
                           0.2}; 
                        */
  float sum = 0;

  for (int i = 0; i < arr_len; i++) {
    sum = 0;
    if (i == arr_len -1) {
        temp_arr[i] = arr[i]*filter_taps[0];
    } else if (i == arr_len -2) {
        for (int j = 0; j < 2; j++) {
            sum += arr[i+j]*filter_taps[j];
        }
    } else if (i == arr_len -3) {
        for (int j = 0; j < 3; j++) {
            sum += arr[i+j]*filter_taps[j];
        }
    } else if (i == arr_len -4) {
        for (int j = 0; j < 4; j++) {
            sum += arr[i+j]*filter_taps[j];
        }
    } else {
        for (int j = 0; j < 5; j++) {
             sum += arr[i+j]*filter_taps[j];
        }
    }
    temp_arr[i] = sum;
  }
  copy(arr, temp_arr);
}

int cmpfunc (const void * a, const void * b) {
   return ( *(int*)a - *(int*)b );
}

void median_filter(float *arr, int win_len, float *to_arr) {
  float temp_arr[arr_len];
  //change 3 to win len
  float window[3]; 
  
  if (win_len < 3) {
    win_len = 3;
  }
  
  int forward_bound;
  int rear_bound;
  int forward_mod;
  int rear_mod;
  
  if(win_len%2==0) { //is even
    forward_bound = win_len/2-1;
    rear_bound = win_len/2;
  } else {
    forward_bound = win_len/2;
    rear_bound = win_len/2;
  }

  //////Serial.println("Going into for");
  for (int i = 0; i < arr_len; i++) {
    if(i-rear_bound < 0) {
      rear_mod = i-rear_bound;
    } else {
      rear_mod = 0;
    }

    if(i + forward_bound > arr_len-1) {
      forward_mod = arr_len-1 - (i + forward_bound);
    } else {
      forward_mod = 0;
    }
    //////Serial.println("before for");
    for (int j = i-(rear_mod+rear_bound); j<=i+forward_mod+forward_bound; j++) {
        window[j-(i-(rear_mod+rear_bound))]=arr[j];
    }

    int new_win_len = (forward_mod+forward_bound)+1+(rear_mod+rear_bound);
    delay(1);
    //////Serial.println("before q sort");
    qsort(window, new_win_len, sizeof(float), cmpfunc);
    //////Serial.println("after q sort");
    
    if (new_win_len % 2 != 0)
        temp_arr[i] = (float)window[new_win_len / 2];
 
    temp_arr[i] = (float)(window[(new_win_len - 1) / 2] + window[new_win_len / 2]) / 2.0;
  }
  //////Serial.println("end of for");
  copy(temp_arr, to_arr);
}

void blePeripheralConnectHandler(BLEDevice central) {
  digitalWrite( LEDG, LOW );
  digitalWrite( LEDR, HIGH );
  ////Serial.print(F( "Connected to central: " ));
  ////Serial.println(central.address());
}

void blePeripheralDisconnectHandler( BLEDevice central ) {
  digitalWrite( LEDG, HIGH );
  digitalWrite( LEDR, LOW );
  ////Serial.print(F("Disconnected from central: "));
  ////Serial.println(central.address());
}
