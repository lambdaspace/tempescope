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

#include "Weather.h"
#include "PinController.h"
#include "LightController.h"
#include "FanStateController.h"
#include "PumpStateController.h"
#include "MistStateController.h"
#include "LightStateController.h"

//pins
#define PIN_R 5
#define PIN_G 6
#define PIN_B 9
#define PIN_MIST 12
#define PIN_FAN 7
#define PIN_PUMP 2

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
  Serial.begin(9600);
  
  //Serial.println("Serial begin");
  
  //low level controllers
  mistController=new PinController(PIN_MIST);
  fanController=new PinController(PIN_FAN);
  pumpController=new PinController(PIN_PUMP);
  lightController=new LightController(PIN_R,PIN_G,PIN_B);
  
  //initializing 
  for(int i=0;i<3;i++){
   lightController->setRGB(255,0,0);
   delay(80);
   lightController->setRGB(0,0,0);
   delay(30);
   }
  for(int i=0;i<3;i++){
      lightController->setRGB(0,255,0);
   delay(80);
   lightController->setRGB(0,0,0);
   delay(30);
   }
  for(int i=0;i<3;i++){
      lightController->setRGB(0,0,255);
   delay(80);
   lightController->setRGB(0,0,0);
   delay(30);
   }
   
  Serial.println("Hello, serial?");
 
  //state controllers
  fanStateController=new FanStateController(fanController);
  pumpStateController=new PumpStateController(pumpController);
  mistStateController=new MistStateController(mistController);
  //Serial.println("11x");
  lightStateController=new LightStateController(lightController);
  
  //do nothing to start up
  currentWeather = new Weather(0,kClear,false);
  doWeather(*currentWeather);
  delay(1000);
  lightController->setRGB(0,0,0);
  
  Serial.println("Setup ende");
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


byte readBuf[21];
int readPacket(byte buf[],int length){
  int idx=0;
  long tEnd=millis()+1000;
  while(millis()<tEnd && idx<length){
    if(Serial.available()>0){
      int k=Serial.read();
      buf[idx++]=k;
    }
  }
  return idx==length;
}

void loop(){

  
  //delay(300);
  //Serial.print("state ");
  //Serial.println(lightStateController->state());
  //delay(1000);
  //Serial.println("RAM: " + String(freeRam(), DEC));
  if (Serial.available()>0)
  {
    char func=Serial.read();
    //Serial.write(func);
    Serial.print(func);
    boolean err=0;
    
    switch(func){
      case 'C':
        //connecting!
        
        //read and ignore (C)ONNECT
        readPacket(readBuf, 7);
        break;
      case 'D':
        //disconnecting!
        //read and ignore (D)ISCONNECT
        readPacket(readBuf, 10);
        break;
      case 'g':
        //set rgb led
         //play single weather
        if(readPacket(readBuf, 6)){
          double pNoon= readBuf[0]/255.;
          int weatherType= readBuf[1];
          int lightning=readBuf[2];
          double red= readBuf[3]/255.;
          double green= readBuf[4]/255.;
          double blue= readBuf[5]/255.0;
          //Serial.println(pNoon);
          //Serial.println(weatherType);
          //Serial.println(lightning);
          
          Weather newWeather(pNoon, (WeatherType)weatherType, lightning, red, green, blue);
          
          newWeather.validateAndFix();
          newWeather.print();
    
          *currentWeather = newWeather;
          doWeather(*currentWeather);          
          
          //Serial.print("state ");
          //Serial.println(lightStateController->state());
          
        }else{
          err=1;
        }
        break;      
      default:
        err=1;
    }
    if(err){
      while(Serial.available()>0) //kill everything in Socket
        Serial.read();
    }
  } 
  
  doWeather(*currentWeather);
  //digitalWrite(11,HIGH);
  //analogWrite(11,55); 
}
