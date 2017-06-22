/*
  LightController.cpp - Raw light driver on an OpenTempescope
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

#include "LightController.h"

LightController::LightController(int pinR, int pinG, int pinB){
    this->pinR=pinR;
    this->pinG=pinG;
    this->pinB=pinB;
    pinMode(pinR,OUTPUT);
    pinMode(pinG,OUTPUT);
    pinMode(pinB,OUTPUT);
    this->_r=0;
    this->_g=0;
    this->_b=0;
    analogWrite(this->pinR,0);
    analogWrite(this->pinG,0);
    analogWrite(this->pinB,0);
}
void LightController::setRGB(uint16_t r, uint16_t g, uint16_t b) {
  if(r!= this->_r){
     analogWrite(this->pinR,r);
     this->_r=r;
  }
  if(g!= this->_g){
     analogWrite(this->pinG,g);
     this->_g=g;
  }
  if(b!= this->_b){
     analogWrite(this->pinB,b);
     this->_b=b;
  }
}
