#include "Wire.h" //default
#include <WiFi.h> //default
#include <PubSubClient.h> // v2.8.0 by Nick O'Leary https://pubsubclient.knolleary.net/
#include "esp_wpa2.h" //default
//must use additional board manager link: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
//must install additional board "esp32" v2.0.6 by Espressif: https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/getting_started.html#about-arduino-esp32

#define I2C_DEV_ADDR 0x00 //I2C ADDRESS OF ESP SLAVE
#define EAP_IDENTITY "eduroamuser"//eduroam login user
#define EAP_USERNAME "eduroamuser"//eduroam login user
#define EAP_PASSWORD "eduroampass"//eduroam login password

uint16_t mustTemp, capTemp, jacketTemp, brixRead, pumpOnTime;
uint8_t mustTempDecimal, capTempDecimal, jacketTempDecimal, tempSetPoint, tempStatusFlags, pumpStatusFlags, brixStatusFlags;
uint8_t RF_Packet[14], readBuffer[25], PumpBrixConfigBuff[6], TempContOpBuff[4], PumpContOpBuff[3];
uint16_t newJacketTemp, oldJacketTemp;
int tempCount, requestPacketPointer;
char valveStatusFlag;
float brixFinal;

/////////////////////WIFI CONNECTION///////////////////////////////////////////////
const char *ssid = "wifiname"; // Enter your WiFi name
const char *password = "wifipass";  // Enter WiFi password
//const char *ssid = "eduroam"; //uncomment if on campus

/////////////////////MQTT Broker SETUP///////////////////////////////////////////////
const char *mqtt_broker = "54.151.63.90";
const char *mqtt_username = "admin";
const char *mqtt_password = "TeamPinot123";
const int mqtt_port = 1883;
const char *topic1 = "/esp32_2/Temp/Mode";
const char *topic2 = "/esp32_2/Temp/Setpoint";
const char *topic3 = "/esp32_2/Temp/ValveControl";
const char *topic4 = "/esp32_2/Brix/Vmax";
const char *topic5 = "/esp32_2/Brix/Vmin";
const char *topic6 = "/esp32_2/Brix/CyclesPerDay";
const char *topic7 = "/esp32_2/Brix/MinutesPerCycle";
const char *topic8 = "/esp32_2/Pump/Mode";
const char *topic9 = "/esp32_2/Pump/PumpControl";
const char *topic10 = "/esp32_2/LED1";
const char *topic11 = "/esp32_2/LED2";

const int ledPin1 = 26;//test led pin for bidirectionality
const int ledPin2 = 27;//test led pin for bidirectionality

WiFiClient espClient;
PubSubClient client(espClient);

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////WIFI DISCONNECT/RECONNECT FUNCTIONS/////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void WiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Wifi reconnected.");
}

void WiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Disconnected from WiFi, attempting to reconnect");
  //WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);//eduroam connectivity
  WiFi.begin(ssid, password);//comment out if off campus

}
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////I2C COMMANDS REQUEST/RECEIVE////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void onRequest() { //request to write to master
  if (RF_Packet[1] == 0x18 && requestPacketPointer == 0) {
    Wire.write(PumpBrixConfigBuff, 14);
    Serial.printf("packetPointer: 0 \n\r");
    requestPacketPointer = requestPacketPointer + 1;
  }
  else if (RF_Packet[1] == 0x18 && requestPacketPointer == 1) {
    Wire.write(TempContOpBuff, 14);
    Serial.printf("packetPointer: 1 \n\r");
    requestPacketPointer = requestPacketPointer + 1;
  }
  else if (RF_Packet[1] == 0x18 && requestPacketPointer == 2) {
    Wire.write(PumpContOpBuff, 14);
    Serial.printf("packetPointer: 2 \n\r");
    requestPacketPointer = 0;
  }
  Serial.printf("requestPacketPointer: %d\n\r", requestPacketPointer);
}

void onReceive(int len) { //request to read from Master
  Serial.printf("onReceive[%d]: \n\r", len);//print to serial monitor that read request was made from master and # of bytes
  int i = 1;
  int y = 1;
  while (Wire.available()) { //reads from master 1 byte at a time, if master sends less that 14 bytes, remaining will be 0xFF
    readBuffer[i] = Wire.read();
    if ( i > 10 && i < 25) {
      RF_Packet[y] = readBuffer[i];
      Serial.printf("RF_Packet[%d]: 0x%x \n\r", y, RF_Packet[y]);
      y = y + 1;
    }
    i = i + 1;
  }
  if (RF_Packet[1] == 0 && len == 25) { //if process values reported, execute determine process values function
    DetermineProcessValues();
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////CODE FOR INTERPRETING PROCESS VALUES////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void DetermineProcessValues() {
  Serial.printf("RF_Packet = 0x0 \n\r");

  //interpret must temp
  char Mtemp[30];
  mustTemp = (RF_Packet[2] << 1) | (RF_Packet[3] >> 7) & 511;
  mustTempDecimal = mustTemp % 10;
  mustTemp = mustTemp / 10;
  Serial.printf("Must Temp: %d.%d degrees C \n\r", mustTemp, mustTempDecimal);
  sprintf(Mtemp, "{\"mustTemp\" :%d.%d}", mustTemp, mustTempDecimal);
  client.publish("/esp32_2/Temp", Mtemp);
  delay(300);

  //interpret cap temp //
  char Ctemp[30];
  capTemp = ((RF_Packet[3] << 2) | (RF_Packet[4] >> 6)) & 511;
  capTempDecimal = capTemp % 10;
  capTemp = capTemp / 10;
  Serial.printf("Cap Temp: %d.%d degrees C \n\r", capTemp, capTempDecimal);
  sprintf(Ctemp, "{\"capTemp\" :%d.%d}", capTemp, capTempDecimal);
  client.publish("/esp32_2/Temp", Ctemp);
  delay(150);

  //interpret jacket temp
  char Jtemp[30];
  jacketTemp = ((RF_Packet[4] << 3) | (RF_Packet[5] >> 5)) & 511;
  jacketTempDecimal = jacketTemp % 10;
  jacketTemp = jacketTemp / 10;
  Serial.printf("Jacket Temp: %d.%d degrees C \n\r", jacketTemp, jacketTempDecimal);
  sprintf(Jtemp, "{\"jacketTemp\" :%d.%d}", jacketTemp, jacketTempDecimal);
  client.publish("/esp32_2/Temp", Jtemp);
  delay(100);

  //interpret Temp controller set point
  char tempSP[30];
  tempSetPoint = RF_Packet[9];
  Serial.printf("Temp Control Setpoint: %d degrees C \n\r", tempSetPoint);
  sprintf(tempSP, "{\"tempSetPoint\" :%d}", tempSetPoint);
  client.publish("/esp32_2/Temp", tempSP);
  delay(100);

  //interpret TempStatusFlags
  tempStatusFlags = RF_Packet[10];

  //Alarm Flags [7:4]
  char tempAlarmChar[64];
  uint8_t tempAlarmFlag = tempStatusFlags >> 4;

  switch (tempAlarmFlag) {
    case 0x00:
      Serial.print("TempAlarmFlag: No Alarms \n\r");
      strcpy (tempAlarmChar, "{\"TempAlarmFlags\" :0}");
      break;
    case 0x01:
      Serial.print("TempAlarmFlag: Thermistor Open \n\r");
      strcpy (tempAlarmChar, "{\"TempAlarmFlags\" :1}");
      break;
    case 0x02:
      Serial.print("TempAlarmFlag: Thermistor Below Low Limit \n\r");
      strcpy (tempAlarmChar, "{\"TempAlarmFlags\" :2}");
      break;
    case 0x04:
      Serial.print("TempAlarmFlag: Thermistor Above High Limit \n\r");
      strcpy (tempAlarmChar, "{\"TempAlarmFlags\" :4}");
      break;
  }
  client.publish("/esp32_2/Temp", tempAlarmChar);
  delay(100);

  //Valve Status [3]
  char tempValveChar[64];
  uint8_t tempValveStatus = (tempStatusFlags >> 3) & 0x01;

  switch (tempValveStatus) {
    case 0x00:
      //tempValveChar = "Valve Off";
      Serial.print("TempValveStatus: Valve Off \n\r");
      strcpy (tempValveChar, "{\"TempValveStatus\" :0}");
      break;
    case 0x01:
      //  tempValveChar = "Valve On";
      Serial.print("TempValveStatus: Valve On \n\r");
      strcpy (tempValveChar, "{\"TempValveStatus\" :1}");
      break;
  }
  client.publish("/esp32_2/Temp", tempValveChar);
  delay(100);

  //Temperture Controller Mode [2:0]
  char tempConChar[64];
  uint8_t tempConMode = tempStatusFlags & 0x07;

  switch (tempConMode) {
    case 0x00:
      Serial.print("Temp Control Mode: Cold Soak \n\r");
      strcpy (tempConChar, "{\"TempControllerMode\" :0}");
      break;
    case 0x01:
      Serial.print("Temp Control Mode: Heat Soak \n\r");
      strcpy (tempConChar, "{\"TempControllerMode\" :1}");
      break;
    case 0x02:
      Serial.print("Temp Control Mode: Cold Tmax \n\r");
      strcpy (tempConChar, "{\"TempControllerMode\" :2}");
      break;
    case 0x03:
      Serial.print("Temp Control Mode: Heat Tmin \n\r");
      strcpy (tempConChar, "{\"TempControllerMode\" :3}");
      break;
    case 0x04:
      Serial.print("Temp Control Mode: Manual \n\r");
      strcpy (tempConChar, "{\"TempControllerMode\" :4}");
      break;
  }
  client.publish("/esp32_2/Temp", tempConChar);
  delay(100);

  //interpret PumpStatusFlags
  pumpStatusFlags = RF_Packet[11];

  //Pump Mode [1]
  char pumpModeChar[64];
  uint8_t pumpPumpMode = (pumpStatusFlags >> 1) & 0x01;

  switch (pumpPumpMode) {
    case 0x00:
      Serial.print("Pump Mode: Auto");
      strcpy (pumpModeChar, "{\"PumpMode\" :0}");
      break;
    case 0x01:
      Serial.print("Pump Mode: Manual");
      strcpy (pumpModeChar, "{\"PumpMode\" :1}");
      break;
  }
  client.publish("/esp32_2/Pump", pumpModeChar);
  delay(100);

  //Pump Status [0]
  char pumpStatusChar[64];
  uint8_t pumpPumpStatus = pumpStatusFlags & 0x01;
  switch (pumpPumpStatus) {
    case 0x00:
      Serial.print("Pump Status: Off");
      strcpy (pumpStatusChar, "{\"PumpStatus\" :0}");
      break;
    case 0x01:
      Serial.print("Pump Status: On");
      strcpy (pumpStatusChar, "{\"PumpStatus\" :1}");
      break;
  }
  client.publish("/esp32_2/Pump", pumpStatusChar);
  delay(100);


  //interpret PumpOnTimeAccumulator
  char PTAchar[30];
  pumpOnTime = (RF_Packet[12] << 8) | RF_Packet[13];
  Serial.printf("Pump on for: %d seconds \n\r", pumpOnTime);
  sprintf(PTAchar, "{\"pumpOnTime\" :%d}", pumpOnTime);
  client.publish("/esp32_2/Pump", PTAchar);
  delay(100);

  //interpret BrixStatusFlags
  brixStatusFlags = RF_Packet[14];


  //Brix Fresh Data Flag [7]
  char BRIXchar[64];
  uint8_t brixFreshDataFlag = (brixStatusFlags >> 7) & 0x01;
  switch (brixFreshDataFlag) {
    case 0x00:
      Serial.print("Brix Data in This Packet is Stale");
      break;
    case 0x01:
      Serial.print("Brix Data in This Packet is Fresh");
      //interpret brix reading
      char BRIXchar[30];
      uint16_t brixRead = ((RF_Packet[7] << 8) | RF_Packet[8]) & 2047;
      int brixDecimal = (brixRead % 50) * 2;
      int  brixOnes = (brixRead / 50) - 5;
      Serial.printf("Brix Meter Reading %d.%d \n\r", brixOnes, brixDecimal);
      sprintf(BRIXchar, "{\"Brix\" :%d.%d}", brixOnes, brixDecimal);
      client.publish("/esp32_2/Brix", BRIXchar);
      delay(150);
      break;
  }

  //Brix Card Error/Warning [4:0]
  uint8_t brixCardError = brixStatusFlags & 0x07;
  char brixErrorChar[64];
  switch (brixCardError) {
    case 0x00:
      Serial.println("Brix Card Error: NO ERROR");
      strcpy (brixErrorChar, "{\"BrixCardError\" :0}");
      break;
    case 0x01:
      Serial.println("Brix Card Error: WDT RESET");
      strcpy (brixErrorChar, "{\"BrixCardError\" :1}");
      break;
    case 0x02:
      Serial.println("Brix Card Error: PURGE FAILURE");
      strcpy (brixErrorChar, "{\"BrixCardError\" :2}");
      break;
    case 0x03:
      Serial.println("Brix Card Error: BRIX HIGH");
      strcpy (brixErrorChar, "{\"BrixCardError\" :3}");
      break;
    case 0x04:
      Serial.println("Brix Card Error: BRIX LOW");
      strcpy (brixErrorChar, "{\"BrixCardError\" :4}");
      break;
    case 0x05:
      Serial.println("Brix Card Error: PURGE HIGH");
      strcpy (brixErrorChar, "{\"BrixCardError\" :5}");
      break;
    case 0x06:
      Serial.println("Brix Card Error: PURGE LOW");
      strcpy (brixErrorChar, "{\"BrixCardError\" :6}");
      break;
    case 0x07:
      Serial.println("Brix Card Error: SENSOR FAILURE");
      strcpy (brixErrorChar, "{\"BrixCardError\" :7}");
      break;
    case 0x08:
      Serial.println("Brix Card Error: ZERO CAL LOW");
      strcpy (brixErrorChar, "{\"BrixCardError\" :8}");
      break;
    case 0x09:
      Serial.println("Brix Card Error: ZERO CAL HIGH");
      strcpy (brixErrorChar, "{\"BrixCardError\" :9}");
      break;
    case 0x0A:
      Serial.println("Brix Card Error: NONZERO CAL LOW");
      strcpy (brixErrorChar, "{\"BrixCardError\" :10}");
      break;
    case 0x0B:
      Serial.println("Brix Card Error: NONZERO CAL HIGH");
      strcpy (brixErrorChar, "{\"BrixCardError\" :11}");
      break;
    case 0x0C:
      Serial.println("Brix Card Error: CAL SLOPE LOW");
      strcpy (brixErrorChar, "{\"BrixCardError\" :12}");
      break;
    case 0x0D:
      Serial.println("Brix Card Error: CAL SLOPE HIGH");
      strcpy (brixErrorChar, "{\"BrixCardError\" :13}");
      break;
    case 0x0E:
      Serial.println("Brix Card Error: CAL ONE POINT");
      strcpy (brixErrorChar, "{\"BrixCardError\" :14}");
      break;
    case 0x10:
      Serial.println("Brix Card Warning: ZERO CAL LOW");
      strcpy (brixErrorChar, "{\"BrixCardError\" :16}");
      break;
    case 0x11:
      Serial.println("Brix Card Warning: ZERO CAL HIGH");
      strcpy (brixErrorChar, "{\"BrixCardError\" :17}");
      break;
    case 0x12:
      Serial.println("Brix Card Warning: NONZERO CAL LOW");
      strcpy (brixErrorChar, "{\"BrixCardError\" :18}");
      break;
    case 0x13:
      Serial.println("Brix Card Warning: NONZERO CAL HIGH");
      strcpy (brixErrorChar, "{\"BrixCardError\" :19}");
      break;
    case 0x14:
      Serial.println("Brix Card Warning: CAL SLOPE LOW");
      strcpy (brixErrorChar, "{\"BrixCardError\" :20}");
      break;
    case 0x15:
      Serial.println("Brix Card Warning: CAL SLOPE HIGH");
      strcpy (brixErrorChar, "{\"BrixCardError\" :21}");
      break;
    case 0x16:
      Serial.println("Brix Card Warning: CAL ONE POINT");
      strcpy (brixErrorChar, "{\"BrixCardError\" :22}");
      break;
    case 0x17:
      Serial.println("Brix Card Warning: CAL NO POINTS");
      strcpy (brixErrorChar, "{\"BrixCardError\" :23}");
      break;
  }
  client.publish("/esp32_2/Brix", brixErrorChar);
  delay(5000);

  /////////////////////////Valve Setup Flags///////////////////////////
  char valveFlagChar[64];
  newJacketTemp = jacketTemp;
  Serial.printf("first Old jack temp %d, New jack temp %d, tempConMode %d\n\r", oldJacketTemp, newJacketTemp, tempConMode);
  if ((oldJacketTemp << newJacketTemp) && (tempConMode == 0x00) && (tempCount == 0) ) {
    Serial.printf("loop 1\n\r");
    tempCount = 1;
  }
  else if ((oldJacketTemp << newJacketTemp) && (tempConMode == 0x00) && (tempCount == 1) ) {
    Serial.printf("Check Valve Setup, Temp Rising\n\r");
    strcpy (valveFlagChar, "{\"ValveSetupFlag\" :1}");
    client.publish("/esp32_2/Temp", valveFlagChar);
  }
  else if ((oldJacketTemp >> newJacketTemp) && (tempConMode == 0x01) && (tempCount == 0)) {
    Serial.printf("loop 2\n\r");
    tempCount = 1;
  }
  else if ((oldJacketTemp >> newJacketTemp) && (tempConMode == 0x01) && (tempCount == 1)) {
    Serial.printf("Check Valve Setup, Temp Falling\n\r");
    strcpy (valveFlagChar, "{\"ValveSetupFlag\" :2}");
    client.publish("/esp32_2/Temp", valveFlagChar);
  }
  else {
    Serial.printf("Valve Setup: No Flags\n\r");
    tempCount = 0;
    strcpy (valveFlagChar, "{\"ValveSetupFlag\" :0}");
    client.publish("/esp32_2/Temp", valveFlagChar);
  }
  oldJacketTemp = newJacketTemp;
  Serial.printf("Old jack temp %d, New jack temp %d, tempConMode %d\n\r", oldJacketTemp, newJacketTemp, tempConMode);

}
//////////////////////////////////////////////////////////////////////////////////////////////
////////HANDLE INCOMING MQTT MESSAGES/////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
//Callback Function
void callback(char *topic, byte *payload, unsigned int length) {
  char payload_output[length + 1];
  for (int i = 0; i < length; i++) {
    //Serial.println((char) payload[i]);
    payload_output[i] = (char)payload[i];
  }
  payload_output[length] = '\0';
  //Serial.println(payload_output);
  if (strcmp(topic, topic1) == 0) {
    //Serial.println("Going into handleTopic1");
    handleTopic1(payload_output);
  }
  else if (strcmp(topic, topic2) == 0) {
    handleTopic2(payload_output);
  }
  else if (strcmp(topic, topic3) == 0) {
    handleTopic3(payload_output);
  }
  else if (strcmp(topic, topic4) == 0) {
    handleTopic4(payload_output);
  }
  else if (strcmp(topic, topic5) == 0) {
    handleTopic5(payload_output);
  }
  else if (strcmp(topic, topic6) == 0) {
    handleTopic6(payload_output);
  }
  else if (strcmp(topic, topic7) == 0) {
    handleTopic7(payload_output);
  }
  else if (strcmp(topic, topic8) == 0) {
    handleTopic8(payload_output);
  }
  else if (strcmp(topic, topic9) == 0) {
    handleTopic9(payload_output);
  }
  else if (strcmp(topic, topic10) == 0) {
    handleTopic10(payload_output);
  }
  else if (strcmp(topic, topic11) == 0) {
    handleTopic11(payload_output);
  }
}

//Topic = /esp32_2/Temp/Mode
void handleTopic1(char *payload_output) {

  if (strcmp(payload_output, "Cold Soak") == 0) {
    Serial.printf("Temp Control Mode now: Cold Soak\n\r");
    TempContOpBuff[2] = 0x00;
  }
  else if (strcmp(payload_output, "Heat Soak") == 0) {
    Serial.printf("Temp Control Mode now: Heat Soak\n\r");
    TempContOpBuff[2] = 0x01;
  }
  else if (strcmp(payload_output, "Cool Tmax") == 0) {
    Serial.printf("Temp Control Mode now: Cold Tmax\n\r");
    TempContOpBuff[2] = 0x02;
  }
  else if (strcmp(payload_output, "Heat Tmin") == 0) {
    Serial.printf("Temp Control Mode now: Heat Tmin\n\r");
    TempContOpBuff[2] = 0x03;
  }
  else if (strcmp(payload_output, "Manual") == 0) {
    Serial.printf("Temp Control Mode now: Manual\n\r");
    TempContOpBuff[2] = 0x04;
  }
}

//Topic = /esp32_2/Temp/Setpoint
void handleTopic2(char *payload_output) {
  char *end_string;
  int tempSetPointNew = strtol(payload_output, &end_string, 10);
  Serial.printf("New Set Point: %d degrees C \n\r", tempSetPointNew);
  TempContOpBuff[3] = tempSetPointNew;
}

//Topic = /esp32_2/Temp/ValveControl
void handleTopic3(char *payload_output) {
  if (strcmp(payload_output, "Valve Off") == 0) {
    Serial.printf("Valve now set: Off\n\r");
    TempContOpBuff[4] = 0x00;
  }
  else if (strcmp(payload_output, "Valve On") == 0) {
    Serial.printf("Valve now set: On\n\r");
    TempContOpBuff[4] = 0x01;
  }
}

//Topic = /esp32_2/Brix/Vmax
void handleTopic4(char *payload_output) {
  char *end_string;
  int num = strtol(payload_output, &end_string, 10);
  if (strcmp(end_string, "Maximum Pump Speed In ") == 0) {
    int PumpSpeed_Max = num;
    Serial.printf("New Pump Vmax: %d \n\r", PumpSpeed_Max);
    PumpBrixConfigBuff[2] = PumpSpeed_Max;
  }
}

//Topic = /esp32_2/Brix/Vmin
void handleTopic5(char *payload_output) {
  char *end_string;
  int num = strtol(payload_output, &end_string, 10);
  if (strcmp(end_string, "Minimum Pump Speed In ") == 0) {
    int PumpSpeed_Min = num;
    Serial.printf("New Pump Vmin: %d \n\r", PumpSpeed_Min);
    PumpBrixConfigBuff[3] = PumpSpeed_Min;
  }
}

//Topic = /esp32_2/Brix/CyclesPerDay
void handleTopic6(char *payload_output) {
  char *end_string;
  int num = strtol(payload_output, &end_string, 10);
  if (strcmp(end_string, "Number of Cycles Per Day") == 0) {
    int CyclesPerDay = num;
    Serial.printf("New # of cycles per day: %d \n\r", CyclesPerDay);
    PumpBrixConfigBuff[4] = CyclesPerDay;
  }
}

//needs to be checked how to represent minutes per cycle in 2 bytes to 1/10 of a minutes
//Topic = /esp32_2/Brix/MinutesPerCycle
void handleTopic7(char *payload_output) {
  char *end_string;
  int num = strtol(payload_output, &end_string, 10);
  if (strcmp(end_string, "Minutes Per Pump Cycle") == 0) {
    int MinutesPerCycle = num;
    Serial.printf("New # Minutes Per Pump Cycle: %d \n\r", MinutesPerCycle);
    PumpBrixConfigBuff[5] = num;
  }
}

//Topic = /esp32_2/Pump/Mode
void handleTopic8(char *payload_output) {
  if (strcmp(payload_output, "Auto") == 0) {
    Serial.printf("Pump Mode Now: Auto \n\r");
    PumpContOpBuff[2] = 0x00;
  }
  else if (strcmp(payload_output, "Manual") == 0) {
    Serial.printf("Pump Mode Now: Manual \n\r");
    PumpContOpBuff[2] = 0x01;
  }
}

//Topic = /esp32_2/Pump/PumpControl
void handleTopic9(char *payload_output) {
  if (strcmp(payload_output, "Pump Off") == 0) {
    Serial.printf("Pump Mode Now: Off \n\r");
    PumpContOpBuff[3] = 0x00;
  }
  else if (strcmp(payload_output, "Pump On") == 0) {
    Serial.printf("Pump Mode Now: On \n\r");
    PumpContOpBuff[3] = 0x01;
  }
}

// Topic = /esp32_2/LED1
void handleTopic10(char *payload_output) {
  if (strcmp(payload_output, "LED On") == 0) {
    Serial.printf("LED Now On \n\r");
    digitalWrite(ledPin1, 1);
  }
  else if (strcmp(payload_output, "LED Off") == 0) {
    Serial.printf("LED Now Off \n\r");
    digitalWrite(ledPin1, 0);
  }
}

// Topic = /esp32_2/LED2
void handleTopic11(char *payload_output) {
  if (strcmp(payload_output, "LED On") == 0) {
    Serial.printf("LED Now On \n\r");
    digitalWrite(ledPin2, 1);
  }
  else if (strcmp(payload_output, "LED Off") == 0) {
    Serial.printf("LED Now Off \n\r");
    digitalWrite(ledPin2, 0);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
////////////INITIAL SETUP/////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void setup() {

  delay(40000); // allow IFCS to turn on and setup before ESP begins

  Serial.begin(115200);
  
  //LED pin setup
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);

  //i2c setup below
  Serial.setDebugOutput(true);
  Wire.onReceive(onReceive);
  Wire.onRequest(onRequest);
  Wire.begin((uint8_t)I2C_DEV_ADDR);

  //RF Packet Header Commands for each requested byte
  PumpBrixConfigBuff[1] = 0x24;
  TempContOpBuff[1] = 0x25;
  PumpContOpBuff[1] = 0x26;

  //necessary for esp32 i2c slave
  #if CONFIG_IDF_TARGET_ESP32
    char message[64];
    Wire.slaveWrite((uint8_t *)message, strlen(message));
  #endif

  // connecting to a WiFi network
  WiFi.onEvent(WiFiConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  //WiFi.begin(ssid, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME, EAP_PASSWORD);//eduroam connectivity
  WiFi.begin(ssid, password);//comment out if off campus
  
  //connecting to a mqtt broker
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the mqtt broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Mosquitto mqtt broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  client.subscribe(topic1);
  client.subscribe(topic2);
  client.subscribe(topic3);
  client.subscribe(topic4);
  client.subscribe(topic5);
  client.subscribe(topic6);
  client.subscribe(topic7);
  client.subscribe(topic8);
  client.subscribe(topic9);
  client.subscribe(topic10);
  client.subscribe(topic11);

}
//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  client.loop();//necessary to stay connected & recieve MQTT messages
}
