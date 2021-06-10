// #include <WiFi.h>
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

int PinPUMP = 14;
RTC_DS3231 rtc;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP,"europe.pool.ntp.org", 3600*2, 60000);
//
const char* ssid = "dzialka";
const char* password = "20.06.2019";

// const char* ssid = "pinguin";
// const char* password = "BulkaiMaslo@1";

//const char* ssid = "aaa";
//const char* password = "munia@12";

// const char* ssid = "pingwin";
// const char* password = "Ten2Sor2Fizyka@1";

String serverName = "http://woojtekk.pl/update.php";

#define EEPROM_write(address, p) {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) EEPROM.write(address+i, pp[i]);EEPROM.commit();}
#define EEPROM_read(address, p)  {int i = 0; byte *pp = (byte*)&(p);for(; i < sizeof(p); i++) pp[i]=EEPROM.read(address+i);}


unsigned long data_send_period = 1000 * 60 * 1;
unsigned long data_send_last = 10000 - data_send_period ;
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
    long actualTime = timeClient.getEpochTime();
    rtc.adjust(DateTime(actualTime));
    lastadjusttime = actualTime;
  }
  // else
  // rtc.adjust(DateTime(__DATE__, __TIME__));

  now = rtc.now();
  DateTimeNow = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + "_" + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second() );
  Serial.println(DateTimeNow);
}

void get_temp() {
  sensors.requestTemperatures();
  temp_in  = sensors.getTempCByIndex(1);
  temp_int = sensors.getTempCByIndex(2);
  temp_out = sensors.getTempCByIndex(0);

  if(temp_in  > temp_in_max)  temp_in_max = temp_in;
  if(temp_int > temp_int_max) temp_int_max = temp_int;
  if(temp_out > temp_out_max) temp_out_max = temp_out;


}
void data_send(){
  Serial.println("send-data");
  data_send_last = millis();

  if (WiFi.status() != WL_CONNECTED)
    wifi_connect();

  if (WiFi.status() == WL_CONNECTED) {

    HTTPClient http;
    String serverPath = serverName +"?LocalTime="    + String(DateTimeNow)
                                  + "&temp_in="      + String(temp_in)
                                  + "&temp_in_max="  + String(temp_in_max)
                                  + "&temp_int="      + String(temp_int)
                                  + "&temp_int_max="  + String(temp_int_max)
                                  + "&temp_out="     + String(temp_out)
                                  + "&temp_out_max=" + String(temp_out_max)
                                  + "&pompa="        + String(pompa)
                                  + "&TPT="          + String(TotalPumpTime)
                                  + "&SysUp="        + String(millis()/1000)
                                  + "&TRYB="         + String(tryb);

    serverPath.replace(' ', '_');
    Serial.println(serverPath);

    // Your Domain name with URL path or IP address with path
    http.begin(serverPath.c_str());

    // Send HTTP GET request
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      // Serial.println(payload);
    }
  }
// WiFi.disconnect();
// Serial.println(":: WiFi disconnect");
}
void data_print(){
  Serial.print("> Local Time:  "   ); Serial.print(DateTimeNow);
  Serial.print(" :: T_IN:  "       ); Serial.print(temp_in);
  Serial.print(" :: T_IN_MAX:  "   ); Serial.print(temp_in_max);
  Serial.print(" :: T_INT:  "       ); Serial.print(temp_int);
  Serial.print(" :: T_INT_MAX:  "   ); Serial.print(temp_int_max);
  Serial.print(" :: T_OUT: "       ); Serial.print(temp_out);
  Serial.print(" :: T_OUT_MAX: "   ); Serial.print(temp_out_max);
  Serial.print(" :: Pompa: "       ); Serial.print(pompa);
  Serial.print(" :: TPT: "); Serial.print(TotalPumpTime);
  Serial.print(" :: SysUp: "       ); Serial.println(millis()/1000);
  Serial.println(tryb);
}

void pomp_onoff(){
  digitalWrite(PinPUMP, pompa);
  if (pompa==true){
    TotalPumpTime=0;
    EEPROM_read(EPROMADDR,TotalPumpTime);
    if (pump_start_time != 0)
      TotalPumpTime += ((millis()-pump_start_time)/1000);

    pump_start_time = millis();

    EEPROM_write(EPROMADDR,TotalPumpTime);
  }else pump_start_time=0;
}

void setup() {
  Serial.begin(115200);
  Serial.print("\n:: >>> FRESH <<<\n");
  EEPROM.begin(512);
  // TotalPumpTime=0;
  // EEPROM_write(EPROMADDR,TotalPumpTime);

  EEPROM_read(EPROMADDR,TotalPumpTime);

  if (! rtc.begin()) { Serial.println("Couldn't find RTC"); }

  pinMode(PinPUMP, OUTPUT);
  digitalWrite(PinPUMP, HIGH);

  wifi_connect();
  RTCadjust();
  sensors.begin();
  Serial.println(sensors.getDeviceCount());
  get_temp();

}

void loop() {
      now = rtc.now();
      DateTimeNow = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day()) + "_" + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second() );

      if ( pompa == false  ) {
        if ( ( millis()/1000 - pump_check_last ) >= pump_check_time )
        {
          pompa = true;
          pomp_onoff();

          delay(pump_check*1000); //ms

          pompa = false;
          pomp_onoff();

          pump_check_last = millis()/1000;
        }
      }

      get_temp();

      if (now.hour() >=5 && now.hour() <= 23 )
      {
        tryb = "\"MODE: A - NORMAL\"";
        pump_check=12;
        if (temp_in <= 70){
          if ( (temp_out - temp_delta) >= temp_in && pompa == false) {
            pompa = true;
          }

          if ( temp_out <= (temp_in + temp_delta/2)  && pompa == true) {
            pompa = false;
            pump_check_last = millis()/1000;
          }
        }
        else{
          tryb = "\"MODE: B - FULL >> BILER FULL, PREVENT PANEL OVERHEATING\"";
          pump_check = 50;
          if(temp_out >= 100) pompa = true;
          if(temp_out < 100) pompa = false;
        }
      }
      else
      {
        tryb = "\"MODE: C - Night >> boiler cooling down\"";
        if (temp_in >= 60) pompa = true;
        if (temp_in <= 35) pompa = false;
      }

      if (temp_in >= 90){
        pompa = false;
        tryb = "\"MODE: D - OVERHEAT >> POSSIBLE PANEL OVERHEATING\"";
      }

      if (temp_in <=0  ){
        pompa = false;
        tryb = "\"MODE: E - TEMP. IN SENSOR ERROR  STOP STOP STOP\"";
      }



      pomp_onoff();

      data_print();
      if ((millis() - data_send_last) > data_send_period)  data_send();
      else delay(1000);
    }
