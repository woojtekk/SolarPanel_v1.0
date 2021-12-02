// #include <WiFi.h>
#include <Arduino.h>
//bumtarara
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
// #include <HTTPClient.h>
#include <ESP8266HTTPClient.h>
#include "RTClib.h"
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
unsigned long sys_start_time_epoch=0;

int PinPUMP = 14;
RTC_DS3231 rtc;
WiFiUDP ntpUDP;
WiFiClient client;

NTPClient timeClient(ntpUDP,"europe.pool.ntp.org", 3600*2, 60000);
//
const char* ssid = "dzialka";
const char* password = "20.06.2019";

int ERR_TEMP_COUNT=0;

String serverName = "http://woojtekk.pl/update.php";
//
#define EEPROM_write(address, p) {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) EEPROM.write(address+i, pp[i]);EEPROM.commit();}
#define EEPROM_read(address, p)  {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) pp[i]=EEPROM.read(address+i);}

int minmaxlastday;// = rtc.now().day();

unsigned long data_send_period = 60;
unsigned long data_send_last = 100 - data_send_period ;
unsigned long Time_zero;
unsigned long TotalPumpTime = 0;
int EPROMADDR = 3;


unsigned long pump_check_last = 0; // time since last pump solar panel check
unsigned long pump_start_time =0;
int pump_check_time = 300;  // seconds ==> 5 min
int pump_check = 12;


float temp_out     = 0;
float temp_out_max = 0;

float temp_in      = 0;
float temp_in_max  = 0;

float temp_int      = 0;
float temp_int_max  = 0;

float temp_delta   = 5;
bool pompa         = true;

String formattedDate;
String dayStamp;
String timeStamp;
String DateTimeNow;

long lastadjusttime=0;
String tryb;
// TaskHandle_t Task1;
// TaskHandle_t Task2;
// TaskHandle_t Task3;
DateTime now;
bool Is_Day = false;
bool Last_Is_Day = false;

void wifi_connect(){

  if (WiFi.status() != WL_CONNECTED){
    WiFi.disconnect();

    unsigned int st = millis();
    WiFi.begin(ssid, password);
    Serial.println("\n-----\nConnecting to WiFi:");
    Serial.print(" :: SSID:     ");  Serial.println(ssid);
    Serial.print(" :: PASSWORD: ");  Serial.println(password);

    while (WiFi.status() != WL_CONNECTED && ( (millis() - st) <=15*1000) ) {
      delay(250);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WiFi Status: ");
      Serial.println(WiFi.status());
      timeClient.begin();
      delay(500);
    }
  }
};

void RTCadjust(){

  // wifi_connect();

  if (WiFi.status() == WL_CONNECTED) {
    timeClient.begin();
    timeClient.update();

    if ( abs( (long)(rtc.now().unixtime() - timeClient.getEpochTime()) ) >= 10 ){
        rtc.adjust(DateTime(timeClient.getEpochTime()));
        Serial.println(":: >> RTC updated");
    }
  }


  now = rtc.now();
  DateTimeNow = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + "_" + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second() );
  Serial.print("Date time now: ");
  Serial.println(DateTimeNow);
}

void get_temp() {

  sensors.requestTemperatures();
  delay(100);
  temp_in  = sensors.getTempCByIndex(0);
  temp_int = sensors.getTempCByIndex(2);
  temp_out = sensors.getTempCByIndex(1);

  if ( temp_in == -127 || temp_out == -127 ) if ( ERR_TEMP_COUNT++ >= 50)  ESP.restart();

  if(temp_in  > temp_in_max  && minmaxlastday == rtc.now().day() ) {temp_in_max  = temp_in;  minmaxlastday = rtc.now().day(); }
  if(temp_int > temp_int_max && minmaxlastday == rtc.now().day() ) {temp_int_max = temp_int; minmaxlastday = rtc.now().day(); }
  if(temp_out > temp_out_max && minmaxlastday == rtc.now().day() ) {temp_out_max = temp_out; minmaxlastday = rtc.now().day(); }
}

void data_send(){
  Serial.println("send-data");
  data_send_last = rtc.now().secondstime();

  if (WiFi.status() != WL_CONNECTED)
    wifi_connect();

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
    String serverPath = serverName +"?LocalTime="    + String(DateTimeNow)
                                  + "&temp_in="      + String(temp_in)
                                  + "&temp_in_max="  + String(temp_in_max)
                                  + "&temp_int="     + String(temp_int)
                                  + "&temp_int_max=" + String(temp_int_max)
                                  + "&temp_out="     + String(temp_out)
                                  + "&temp_out_max=" + String(temp_out_max)
                                  + "&pompa="        + String(pompa)
                                  + "&TPT="          + String(TotalPumpTime)
                                  + "&SysUp="        + String(rtc.now().secondstime() - sys_start_time_epoch )
                                  + "&TRYB="         + String(tryb)+" "+String(pump_check_time)+" "+String(pump_check)+" "+String(ERR_TEMP_COUNT);

    serverPath.replace(' ', '_');
    Serial.println(serverPath);

    // Your Domain name with URL path or IP address with path
    http.begin(client,serverPath.c_str());

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    // if (httpResponseCode > 0) {
    //   Serial.print("HTTP Response code: ");
    //   Serial.println(httpResponseCode);
    //   String payload = http.getString();
    //   // Serial.println(payload);
    // }
  }
// WiFi.disconnect();
// Serial.println(":: WiFi disconnect");
}

void data_print(){
  Serial.print("> Local Time:  "   ); Serial.print(DateTimeNow);
  Serial.print(" T_IN:  "       ); Serial.print(temp_in);
  Serial.print(" T_IN_MAX:  "   ); Serial.print(temp_in_max);
  Serial.print(" T_INT:  "       ); Serial.print(temp_int);
  Serial.print(" T_INT_MAX:  "   ); Serial.print(temp_int_max);
  Serial.print(" T_OUT: "       ); Serial.print(temp_out);
  Serial.print(" T_OUT_MAX: "   ); Serial.print(temp_out_max);
  Serial.print(" Pompa: "       ); Serial.print(pompa);
  Serial.print(" TPT: "); Serial.print(TotalPumpTime);
  Serial.print(" SysUp: "       ); Serial.println(String(rtc.now().secondstime() - sys_start_time_epoch ));
  // Serial.print(tryb); Serial.println(String(pump_check));
}

void pomp_onoff(){
  digitalWrite(PinPUMP, !(pompa));

  if (pompa==true){
    TotalPumpTime=0;
    EEPROM_read(EPROMADDR,TotalPumpTime);

    if (pump_start_time != 0)   TotalPumpTime += ((rtc.now().secondstime()-pump_start_time)/1000);

    pump_start_time = rtc.now().secondstime();

    EEPROM_write(EPROMADDR,TotalPumpTime);
  }
  else pump_start_time=0;

}

bool getPage() {

  Serial.print("[HTTP] begin...\n");
      HTTPClient http;
  if (http.begin(client, "http://woojtekk.pl/todo.txt")) {  // HTTP


    Serial.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();

    // httpCode will be negative on error
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        Serial.println(payload);
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.printf("[HTTP} Unable to connect\n");
  }


  return true;
}

void parser(String txt){


}


void setup() {
  Serial.begin(115200);
  Serial.print("\n:: >>> FRESH <<<\n");
  ERR_TEMP_COUNT = 0;

  pinMode(PinPUMP, OUTPUT);
  digitalWrite(PinPUMP, HIGH);
  delay(500);
  digitalWrite(PinPUMP, LOW);


  EEPROM.begin(512);
  // TotalPumpTime=0;
  // EEPROM_write(EPROMADDR,TotalPumpTime);
  EEPROM_read(EPROMADDR,TotalPumpTime);

  if (! rtc.begin()) { Serial.println("Couldn't find RTC"); }
  delay(2000);
  sys_start_time_epoch = rtc.now().secondstime();
  delay(200);
  minmaxlastday = rtc.now().day();

  wifi_connect();
  RTCadjust();
  sensors.begin();
  getPage();
  // get_temp();
}



void loop() {
      now = rtc.now();

      DateTimeNow = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + "_" + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second() );

      if ( now.hour() >=7 && now.hour() <= 19 ) Is_Day = true;  else Is_Day = false;

      if ( Last_Is_Day != Is_Day && Is_Day ) ESP.restart();
      Last_Is_Day = Is_Day;

      if ( Is_Day &&  pompa == false ) {
        if ( (int)( rtc.now().secondstime() - pump_check_last ) >= pump_check_time )
        {
          pompa = true;
          pomp_onoff();

          delay(pump_check*1000); //ms

          pompa = false;
          pomp_onoff();

          pump_check_last = rtc.now().secondstime();
        }
      }

      get_temp();

      if ( Is_Day )
      {
        tryb = "\"MODE: A - Day MODE --> NORMAL \"";
        pump_check = 12;

        if (temp_in <= 70){
          if ( temp_in <= (temp_out - temp_delta) && pompa == false) {
            pompa = true;
          }

          if ( temp_out <= (temp_in + temp_delta/2)  && pompa == true) {
            pompa = false;
            pump_check_last = rtc.now().secondstime();
          }
        }
        else{
          tryb = "\"MODE: B - FULL >> BOILER FULL, PREVENT PANEL OVERHEATING\"";
          pump_check = 50;
          if(temp_out >= 100) pompa = true;
          if(temp_out < 100) pompa = false;
        }
      }
      else
      {
        tryb = "\"MODE: C - Night >>>\"";
        pump_check = 10;

        // if (temp_in >= 60) pompa = true;
        // if (temp_in <= 35) pompa = false;
      }

      if (temp_in >= 90){
        pompa = false;
        tryb = "\"MODE: D - OVERHEAT >> POSSIBLE PANEL OVERHEATING\"";
      }

      if (temp_in <= 0  ){
        pompa = true;
        tryb = "\"MODE: E - TEMP. IN SENSOR ERROR  !!!!!!!!\"";
      }



      pomp_onoff();

      data_print();

      if ((rtc.now().secondstime() - data_send_last) > data_send_period)  data_send();
      else      delay(1000);
    }
