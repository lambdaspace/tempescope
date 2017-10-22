/*
  Tempescope.ino - Main source file for OpenTempescope hardware
  Released as part of the OpenTempescope project - http://tempescope.com/opentempescope/
  Copyright (c) 2013 Ken Kawamoto.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <ESP8266WiFi.h>
#include <NtpClientLib.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Time.h>

#include "config.h"
#include "Weather.h"
#include "PinController.h"
#include "LightController.h"
#include "FanStateController.h"
#include "PumpStateController.h"
#include "MistStateController.h"
#include "LightStateController.h"

//pins
#define PIN_R D5
#define PIN_G D6
#define PIN_B D7
#define PIN_MIST D0
#define PIN_FAN D1
#define PIN_PUMP D2
#define PIN_DEBUG A0

void test_mode();

/*****************
Low level controllers
******************/
PinController  *mistController,
               *fanController,
               *pumpController;
LightController *lightController;

/*****************
State controllers
******************/
FanStateController *fanStateController;
PumpStateController *pumpStateController;
MistStateController *mistStateController;
LightStateController *lightStateController;

Weather *currentWeather;

void setup(){
  Serial.begin(115200);
  Serial.println("Starting up ...");

  // Set debug pin as an input
  pinMode(PIN_DEBUG, INPUT);

  //low level controllers
  mistController=new PinController(PIN_MIST);
  fanController=new PinController(PIN_FAN);
  pumpController=new PinController(PIN_PUMP);
  lightController=new LightController(PIN_R,PIN_G,PIN_B);

  //initializing
  for(int i=0;i<3;i++){
   lightController->setRGB(1023,0,0);
   delay(80);
   lightController->setRGB(0,0,0);
   delay(30);
   }
  for(int i=0;i<3;i++){
   lightController->setRGB(0,1023,0);
   delay(80);
   lightController->setRGB(0,0,0);
   delay(30);
   }
  for(int i=0;i<3;i++){
   lightController->setRGB(0,0,1023);
   delay(80);
   lightController->setRGB(0,0,0);
   delay(30);
   }

  //state controllers
  fanStateController=new FanStateController(fanController);
  pumpStateController=new PumpStateController(pumpController);
  mistStateController=new MistStateController(mistController);
  lightStateController=new LightStateController(lightController);

  //do nothing to start up
  currentWeather = new Weather(0,kClear,false);
  doWeather(*currentWeather);
  delay(1000);
  lightController->setRGB(0,0,0);

  // Check if we must pause the setup end go into test mode
  if (digitalRead(PIN_DEBUG) == HIGH) {
    Serial.println("Entering test mode...");
    test_mode();
  }

  Serial.printf("Connecting to %s ...\n", WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.println("Synchronising clock...");

  NTP.begin();
  while (NTP.getLastNTPSync() == 0) {
    NTP.getTime();
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\nClock synchronized!");
  NTP.setInterval(604800);
}

int freeRam () {
    extern int __heap_start, *__brkval;
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}

uint8_t *_tester;

void doWeather(Weather weather){
  if(weather.lightning())
    lightStateController->doAction(ACTION_LIGHT_LNG_ON);
  else{
    lightStateController->setPNoon(weather.pNoon());
    lightStateController->doAction(ACTION_LIGHT_LNG_OFF);
  }

  switch(weather.weatherType()){
    case kClear:
    {
    pumpStateController->doAction(ACTION_PUMP_OFF);
    fanStateController->doAction(ACTION_FAN_OFF);
    mistStateController->doAction(ACTION_MIST_OFF);
    }break;
    case kRain:
    {
    pumpStateController->doAction(ACTION_PUMP_ON);
    fanStateController->doAction(ACTION_FAN_OFF);
    mistStateController->doAction(ACTION_MIST_OFF);
    }break;
    case kCloudy:
    {
    pumpStateController->doAction(ACTION_PUMP_OFF);
    fanStateController->doAction(ACTION_FAN_ON);
    mistStateController->doAction(ACTION_MIST_ON);
    }break;
    case kStorm:
    {
    pumpStateController->doAction(ACTION_PUMP_ON);
    fanStateController->doAction(ACTION_FAN_ON);
    mistStateController->doAction(ACTION_MIST_ON);
    }break;
    case kRGB:
    {
    pumpStateController->doAction(ACTION_PUMP_OFF);
    fanStateController->doAction(ACTION_FAN_OFF);
    mistStateController->doAction(ACTION_MIST_OFF);
    int result = lightStateController->setRGB(weather.red(),weather.green(),weather.blue());
    //Serial.println(lightStateController->state());
    //Serial.println(result);
    }break;
  }
}

void loop() {
  //delay(300);
  //Serial.print("state ");
  //Serial.println(lightStateController->state());
  //delay(1000);
  //Serial.println("RAM: " + String(freeRam(), DEC));

  String url = "http://api.openweathermap.org/data/2.5/weather?id=";
  url += CITY_ID;
  url += "&APPID=";
  url += OWM_API_KEY;

  HTTPClient apiClient;
  apiClient.begin(url);

  int httpCode = apiClient.GET();

  if (httpCode == 200) {
    WiFiClient *stream = apiClient.getStreamPtr();
    DynamicJsonBuffer jsonBuffer;

    while(apiClient.connected()) {
      if (stream->available()) {
        String response = stream->readStringUntil('\r');
        response.trim();

        JsonObject& root = jsonBuffer.parseObject(response.c_str());

        if (root.success()) {
          JsonObject& sys = root["sys"];
          time_t sunrise = sys["sunrise"];
          time_t sunset = sys["sunset"];
          time_t t = NTP.getTime();
          int pNoon = t > sunrise && t < sunset;

          String ids = root["weather"][0]["id"];
          int id = ids.toInt();
          WeatherType weatherType;
          boolean lightning = false;

          if (id == 800) {
            weatherType = kClear;
          } else if (id > 700 && id < 900) {
            weatherType = kCloudy;
          } else if (id >= 300 && id < 700) {
            weatherType = kRain;
          } else if (id >= 200 && id < 300) {
            weatherType = kStorm;
            lightning = true;
          } else {
            break;
          }

          Weather newWeather(pNoon, weatherType, lightning);
          *currentWeather = newWeather;

          break;
        } else {
          Serial.println("Failed to parse API response");
        }
      }
    }
  } else {
    Serial.println("API request failed");
  }
  apiClient.end();

  doWeather(*currentWeather);
  delay(900000);
}

void test_mode() {
  uint8_t selection;

  do {
    // Print menu
    Serial.print("\
      0) Continue booting\n\
      1) Simulate weather\n\
      2) Set light color\n\
      Select option: \
    ");

    // Wait for user input
    while (Serial.available() == 0) {
      yield();
    }

    selection = Serial.parseInt();
    Serial.println(selection);
    switch (selection) {
      case 0:
        break;
      case 1:
      {
        WeatherType weatherType;
        uint8_t lightning = false;

        Serial.print("\
          1) Clear weather\n\
          2) Rain\n\
          3) Clouds or fog\n\
          4) Storm\n\
          Select option: \
        ");

        while (Serial.available() == 0) {
          yield();
        }

        selection = Serial.parseInt();
        Serial.println(selection);
        switch (selection) {
          case 1:
            weatherType = kClear;
            break;
          case 2:
            weatherType = kRain;
            break;
          case 3:
            weatherType = kCloudy;
            break;
          case 4:
            weatherType = kStorm;
            lightning = true;
            break;
          default:
            Serial.println("Invalid option");
            continue;
        }

        doWeather(Weather(1, weatherType, lightning));
        break;
      }
      case 2:
      {
        Serial.print("Insert value for red: ");
        while (Serial.available() == 0) {
          yield();
        }
        uint16_t red = Serial.parseInt();
        Serial.println(red);

        Serial.print("Insert value for green: ");
        while (Serial.available() == 0) {
          yield();
        }
        uint16_t green = Serial.parseInt();
        Serial.println(green);

        Serial.print("Insert value for blue: ");
        while (Serial.available() == 0) {
          yield();
        }
        uint16_t blue = Serial.parseInt();
        Serial.println(blue);

        if (red > 1023 || green > 10123 || blue > 1023) {
          Serial.println("You have entered invalid values");
        } else {
          lightController->setRGB(red, green, blue);
        }
        break;
      }
      default:
        Serial.println("Invalid option");
    }
  } while (selection != 0);
}
