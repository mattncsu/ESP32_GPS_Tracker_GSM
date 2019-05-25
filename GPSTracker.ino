#define TINY_GSM_MODEM_SIM7000
// Increase the buffer
#define TINY_GSM_RX_BUFFER 512

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG Serial

#include <TinyGsmClient.h>
#include <WiFi.h>
#include <ThingsBoard.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Your GPRS credentials
// Leave empty, if missing user or pass
const char apn[]  = "hologram";
const char user[] = "";
const char pass[] = "";

//GPS Bounding Box (fail over to cellular outside)
const float northLimit=xxx;
const float southLimit=xxx;
const float eastLimit=xxx;
const float westLimit=xxx;

float gps_latitude,gps_longitude,gps_speed,gps_course,gps_altitude;
int gps_view_satellites,gps_used_satellites;
bool gps_fixstatus;

int counter=1195; //Start high to do a 4G test after ~15s
unsigned long timer=0;

#define WIFI_SSID       "xxx"
#define WIFI_PASSWORD   "xxx"

char payload[512]; //message to publish

#define THINGSBOARD_SERVER "demo.thingsboard.io"
#define MQTT_PORT 1883
#define TOKEN "xxx"
#define REFRESH_RATE 3000

// Set serial for debug console (to the Serial Monitor, speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
#define SerialAT Serial2

// Uncomment this if you want to see all AT commands
//#define DUMP_AT_COMMANDS
#ifdef DUMP_AT_COMMANDS
  #include <StreamDebugger.h>
  StreamDebugger debugger(SerialAT, SerialMon);
  TinyGsm modem(debugger);
#else
  TinyGsm modem(SerialAT);
#endif

// Server details
const char server[] = "demo.thingsboard.io"; //access token in middle, 104.196.24.70=demo.thingsboard.io
const char resource[]= "/api/v1/xxxx/telemetry";
const int  port = 80;
const String contentType="application/json";

TinyGsmClient client_gsm(modem);
WiFiClient client_wifi;
ThingsBoard tb_gsm(client_gsm);
ThingsBoard tb_wifi(client_wifi);

bool inYard(){
  return((gps_latitude > southLimit) && (gps_latitude < northLimit) && (gps_longitude > westLimit) && (gps_longitude < eastLimit));
}

void connectToWifi() {
  SerialMon.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    SerialMon.print(".");
  }
    SerialMon.println("");
    SerialMon.println("WiFi connected");
    SerialMon.println("IP address: ");
    SerialMon.println(WiFi.localIP());
}

void setupModem(){
  SerialMon.print("Initializing modem...");
  if (!modem.restart()) {
    SerialMon.println(F(" [fail]"));
    SerialMon.println(F("************************"));
    SerialMon.println(F(" Is your modem connected properly?"));
    SerialMon.println(F(" Is your serial speed (baud rate) correct?"));
    SerialMon.println(F(" Is your modem powered on?"));
    SerialMon.println(F(" Do you use a good, stable power source?"));
    SerialMon.println(F(" Try useing File -> Examples -> TinyGSM -> tools -> AT_Debug to find correct configuration"));
    SerialMon.println(F("************************"));
    delay(10000);
    return;
  }
  SerialMon.println(F(" [OK]"));
  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem: ");
  SerialMon.println(modemInfo);

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork()) {
    SerialMon.println(F(" [fail]"));
    SerialMon.println(F("************************"));
    SerialMon.println(F(" Is your sim card locked?"));
    SerialMon.println(F(" Do you have a good signal?"));
    SerialMon.println(F(" Is antenna attached?"));
    SerialMon.println(F(" Does the SIM card work with your phone?"));
    SerialMon.println(F("************************"));
    delay(10000);
    return;
  }
  SerialMon.println(F(" [OK]"));

  SerialMon.print("Connecting to ");
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    SerialMon.println(F(" [fail]"));
    SerialMon.println(F("************************"));
    SerialMon.println(F(" Is GPRS enabled by network provider?"));
    SerialMon.println(F(" Try checking your card balance."));
    SerialMon.println(F("************************"));
    delay(10000);
    return;
  }
  SerialMon.println(F(" [OK]"));
  IPAddress local = modem.localIP();
  SerialMon.print("Local IP: ");
  SerialMon.println(local);
  modem.enableGPS();
}

void tbWifiConnect(){
  // Connect to the ThingsBoard
        SerialMon.print("Connecting to: ");
        SerialMon.print(THINGSBOARD_SERVER);
        SerialMon.print(" over WIFI with token ");
        SerialMon.println(TOKEN);
        if (!tb_wifi.connect(THINGSBOARD_SERVER, TOKEN)) {
          SerialMon.println("Failed to connect");
          return;
        }
}

void tbGSMConnect(){
  // Connect to the ThingsBoard
        SerialMon.print("Connecting to: ");
        SerialMon.print(THINGSBOARD_SERVER);
        SerialMon.print(" over CELLULAR with token ");
        SerialMon.println(TOKEN);
        if (!tb_gsm.connect(THINGSBOARD_SERVER, TOKEN)) {
          SerialMon.println("Failed to connect");
          return;
        }
}

void initArduinoOTA(){
  ArduinoOTA.setHostname("RoboESP");
  ArduinoOTA.setPassword("xxx");
  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
  
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void setup() {
  SerialMon.begin(115200); // Set serial console baud rate
  delay(10);
  SerialAT.begin(115200); // Set GSM module baud rate
  connectToWifi();
  setupModem();
  initArduinoOTA();
  timer=millis();
}

void loop() {
  if((millis()-timer)>REFRESH_RATE){
    gps_fixstatus = modem.getGPS(&gps_latitude, &gps_longitude, &gps_speed, &gps_altitude, &gps_course, &gps_view_satellites, &gps_used_satellites);
    if ( gps_fixstatus ) {
      gps_altitude=gps_altitude*3.2808;
      SerialMon.println("#GPS Data: ");  
    }
    sprintf(payload, "{\"lat\":%8f,\"long\":%8f,\"speed\":%2f,\"head\":%2f,\"alt\":%2f}", gps_latitude,gps_longitude, gps_speed, gps_course, gps_altitude); 
    SerialMon.println(payload);
    
    int post_stat;
    if((int)gps_latitude!=0){
      SerialMon.print("In Yard:");SerialMon.println(inYard());
      if(inYard() && (counter<1200)){ //Use cell link once/hour
        SerialMon.print("***Trying Wifi*** ");
        unsigned long timer1=millis();
        if (!tb_wifi.connected()) {
          tbWifiConnect();
        }
        bool stat=tb_wifi.sendTelemetryJson(payload);
        if(stat){
          SerialMon.println("Wifi send success"); 
        } else{
          SerialMon.println("Wifi send FAILED"); 
          counter=1200; //force cellular attempt on next pass
        }
        SerialMon.print(millis()-timer1);
        SerialMon.println("ms");
  
      } else {
        if(counter>=10){ //Don't use cell link more than once/30s
          SerialMon.print("***Trying GSM*** ");
          unsigned long timer1=millis();
          if (!tb_gsm.connected()) {
            tbGSMConnect();
          }
          bool stat=tb_gsm.sendTelemetryJson(payload);  
          SerialMon.print(millis()-timer1);
          SerialMon.print("ms: ");
          if(stat){
            SerialMon.println("4G send success");
            counter=0; 
          } else{
            SerialMon.println("4G send FAILED"); 
          }
        } else {
          SerialMon.print("Waiting ");
          SerialMon.print((10-counter)*(REFRESH_RATE/1000));
          SerialMon.print(" seconds to post over cellular.");
        }
      }
    }
    tb_wifi.loop();
    if ((WiFi.status() != WL_CONNECTED) && (inYard)) {
      connectToWifi();
    }
    SerialMon.print("Wait 3 seconds...");
    counter++;
    SerialMon.println(counter);
  }
  ArduinoOTA.handle();
}
