#define TINY_GSM_MODEM_SIM7000
// Increase the buffer
#define TINY_GSM_RX_BUFFER 512

// Define the serial console for debug prints, if needed
#define TINY_GSM_DEBUG Serial

#include <TinyGsmClient.h>
#include <WiFi.h>
#include <ThingsBoard.h>

// Your GPRS credentials
// Leave empty, if missing user or pass
const char apn[]  = "hologram";
const char user[] = "";
const char pass[] = "";

//GPS Bounding Box (fail over to cellular outside)
const float northLimit=xx.xxx;
const float southLimit=xx.xxx;
const float eastLimit=-xx.xxx;
const float westLimit=-xx.xx;

float gps_latitude,gps_longitude,gps_speed,gps_course,gps_altitude;
int gps_view_satellites,gps_used_satellites;
bool gps_fixstatus;


#define WIFI_SSID       "xxx"
#define WIFI_PASSWORD   "xxx"

char payload[512]; //message to publish

#define THINGSBOARD_SERVER "demo.thingsboard.io"
#define TOKEN "xxx"



// Set serial for debug console (to the Serial Monitor, speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
#define SerialAT Serial2

#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon); //prints AT commands to serial terminal
TinyGsm modem(debugger);

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

void setup() {
  SerialMon.begin(115200); // Set serial console baud rate
  delay(10);
  SerialAT.begin(115200); // Set GSM module baud rate
  connectToWifi();
  setupModem();
}


int counter=1195; //Start high to do a 5G test after ~15s
void loop() {

  gps_fixstatus = modem.getGPS(&gps_latitude, &gps_longitude, &gps_speed, &gps_altitude, &gps_course, &gps_view_satellites, &gps_used_satellites);
  if ( gps_fixstatus ) {
    gps_altitude=gps_altitude*3.2808;
    SerialMon.print("#GPS Location: ");
    SerialMon.print(gps_latitude,6);
    SerialMon.print(",");
    SerialMon.print(gps_longitude,6);
    SerialMon.print(" SPEED: ");
    SerialMon.print(gps_speed,1);
    SerialMon.print(" COURSE: ");
    SerialMon.print(gps_course,1);
    SerialMon.print(" ALT: ");
    SerialMon.println(gps_altitude,1);

//    int year, month, day, hour, minute, second;
//    if ( modem.getGPSTime(&gps_year, &gps_month, &gps_day, &gps_hour, &gps_minute, &gps_second) ) {
//      //Sync time if it's diffrent.
//      set_time(gps_year, gps_month, gps_day, gps_hour, gps_minute, gps_second);
//    }
    
  }
  sprintf(payload, "{\"lat\":%8f,\"long\":%8f,\"speed\":%2f,\"head\":%2f,\"alt\":%2f}", gps_latitude,gps_longitude, gps_speed, gps_course, gps_altitude); 
  SerialMon.println(payload);
  
  int post_stat;
  if((int)gps_latitude!=0){
    SerialMon.print("In Yard:");SerialMon.println(inYard());
    if(inYard() && (counter<1200)){ //Use cell link once/hour
      SerialMon.print("***Trying Wifi*** ");
      unsigned long timer=millis();
      if (!tb_wifi.connected()) {
        // Connect to the ThingsBoard
        SerialMon.print("Connecting to: ");
        SerialMon.print(THINGSBOARD_SERVER);
        SerialMon.print(" with token ");
        SerialMon.println(TOKEN);
        if (!tb_wifi.connect(THINGSBOARD_SERVER, TOKEN)) {
          SerialMon.println("Failed to connect");
          return;
        }
      }

      // Uploads new telemetry to ThingsBoard using MQTT.
      // See https://thingsboard.io/docs/reference/mqtt-api/#telemetry-upload-api
      // for more details
      bool stat=tb_wifi.sendTelemetryJson(payload);
      if(stat){
        SerialMon.println("Wifi send success"); 
      } else{
        SerialMon.println("Wifi send FAILED"); 
      }
      SerialMon.print(millis()-timer);
      SerialMon.println("ms");

    } else {
      if(counter>=10){ //Don't use cell link more than once/30s
        SerialMon.print("***Trying GSM*** ");
        unsigned long timer=millis();
        if (!tb_gsm.connected()) {
          // Connect to the ThingsBoard
          SerialMon.print("Connecting to: ");
          SerialMon.print(THINGSBOARD_SERVER);
          SerialMon.print(" with token ");
          SerialMon.println(TOKEN);
          if (!tb_gsm.connect(THINGSBOARD_SERVER, TOKEN)) {
            SerialMon.println("Failed to connect");
            return;
          }
        }
       
        // Uploads new telemetry to ThingsBoard using MQTT.
        // See https://thingsboard.io/docs/reference/mqtt-api/#telemetry-upload-api
        // for more details
        bool stat=tb_gsm.sendTelemetryJson(payload);  
        SerialMon.print(millis()-timer);
        SerialMon.print("ms: ");
        if(stat){
          SerialMon.println("4G send success");
          counter=0; 
        } else{
          SerialMon.println("4G send FAILED"); 
        }
        

      } else {
        SerialMon.print("Waiting ");
        SerialMon.print((10-counter)*3);
        SerialMon.print(" seconds to post over cellular.");
      }
    }
   
  }
  tb_wifi.loop();
  if ((WiFi.status() != WL_CONNECTED) && (inYard)) {
    connectToWifi();
  }
  SerialMon.print("Wait 3 seconds...");
  delay(3000);
  counter++;
  SerialMon.println(counter);
}
