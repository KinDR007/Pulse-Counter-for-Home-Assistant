/**
   The MySensors Arduino library handles the wireless radio link and protocol
   between your home built sensors/actuators and HA controller of choice.
   The sensors forms a self healing radio network with optional repeaters. Each
   repeater and gateway builds a routing tables in EEPROM which keeps track of the
   network topology allowing messages to be routed to nodes.

   Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
   Copyright (C) 2013-2015 Sensnology AB
   Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors

   Documentation: http://www.mysensors.org
   Support Forum: http://forum.mysensors.org

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as published by the Free Software Foundation.

 *******************************

   REVISION HISTORY
   Version 1.0 - Henrik Ekblad

   DESCRIPTION
   This sketch provides an example how to implement a LM393 PCB
   Use this sensor to measure kWh and Watt of your house meter
   You need to set the correct pulsefactor of your meter (blinks per kWh).
   The sensor starts by fetching current kWh value from gateway.
   Reports both kWh and Watt back to gateway.

   Unfortunately millis() won't increment when the Arduino is in
   sleepmode. So we cannot make this sensor sleep if we also want
   to calculate/report watt value.
   http://www.mysensors.org/build/pulse_power
*/


//#define MY_DEBUG                               // Enable debug prints

#define MY_NODE_ID 31 //31 FVE 30 CEZ          // define node id or set MY_NODE_ID(AUTO)

#define MY_RF24_CHANNEL 90

// Enable and select radio type attached
#define MY_RADIO_RF24
//#define MY_RADIO_NRF5_ESB
//#define MY_RADIO_RFM69
//#define MY_RADIO_RFM95

#include <MySensors.h>                        // Tested on v2.3.2

#define DIGITAL_INPUT_SENSOR 2                // The digital input you attached your light sensor.  (Only 2 and 3 generates interrupt!)
#define PULSE_FACTOR 2000                     // Number of blinks per of your meter (Hutermann HT-1PM has 2000 pulses per Kwh)
#define SLEEP_MODE false                      // Watt value can only be reported when sleep mode is false.
#define MAX_WATT 7000                         // Max watt value to report. This filters outliers. 

#define WATT_CHILD_ID 1                       // Id of the sensor for Watt
#define KWH_CHILD_ID 2                        // Id of the sensor for Kwh
#define PC_CHILD_ID 3                         // Id of the sensor for pulse counter




uint32_t SEND_FREQUENCY = 20000;              // Minimum time between send (in milliseconds). We don't want to spam the gateway.
double ppwh = ((double)PULSE_FACTOR) / 1000;  // Pulses per watt hour

bool pcReceived = false;
volatile uint32_t pulseCount = 0;
volatile uint32_t lastBlink = 0;
volatile uint32_t watt = 0;
uint32_t oldPulseCount = 0;
uint32_t oldWatt = 0;
double oldkWh;
uint32_t lastSend;
int recrequestcount = 0;
unsigned long lastPulse;
unsigned long currentTime;

MyMessage wattMsg(WATT_CHILD_ID, V_WATT);
MyMessage kWhMsg(KWH_CHILD_ID, V_KWH);
MyMessage pcMsg(PC_CHILD_ID, V_VAR1);


void setup()
{

  request(PC_CHILD_ID, V_VAR1);                 // Fetch last known pulse count value from gw

  // im use resistor between D2 pin and GND and connect +5Vcc from arduino nano to S0+ and D2 pin to S0-
  // or
  // Use the internal pullup to be able to hook up this sketch directly to an energy meter with S0 output
  // If no pullup is used, the reported usage will be too high because of the floating pin
  //pinMode(DIGITAL_INPUT_SENSOR,INPUT_PULLUP);


  attachInterrupt(digitalPinToInterrupt(DIGITAL_INPUT_SENSOR), onPulse, RISING);
  lastSend = millis();
}

void presentation()
{

  sendSketchInfo("Energy Meter FVE", "2.0");                          // Send the sketch version information to the gateway and Controller

  present(WATT_CHILD_ID, S_POWER, "watt", false);
  present(KWH_CHILD_ID, S_POWER, "kwh", false);
  present(PC_CHILD_ID, S_CUSTOM);
}

void loop()
{
  uint32_t now = millis();

  bool sendTime = now - lastSend > SEND_FREQUENCY;
  if (pcReceived && (SLEEP_MODE || sendTime)) {                     // Only send values at a maximum frequency or woken up from sleep
    // New watt value has been calculated
    if (!SLEEP_MODE && watt != oldWatt) {
      if (watt < ((uint32_t)MAX_WATT)) {                            //Check that we don't get unreasonable large watt value. could happen when long wraps or false interrupt triggered

        send(wattMsg.set(watt));                                    // Send watt value to gw
      }
#ifdef MY_DEBUG
      Serial.print(" Watt :");
      Serial.println(watt);
#endif
      oldWatt = watt;
    }

    if (now - lastPulse > 120000) {                                   //If No Pulse count in 2min set watt to zero
      watt = 0;
      if (watt < ((uint32_t)MAX_WATT)) {
#ifdef MY_DEBUG
        Serial.print("2 min delay no pulse :");
        Serial.println("sending 0 Watt to controller ");
#endif
        send(wattMsg.set(watt));                                      // Send 0 watt value to gw
      }
    }

    if (pulseCount != oldPulseCount) {                                // Pulse count value has changed
      send(pcMsg.set(pulseCount));                                    // Send pulse count value to gw
      double kWh = ((double)pulseCount / ((double)PULSE_FACTOR));
      oldPulseCount = pulseCount;
      if (kWh != oldkWh) {
        send(kWhMsg.set(kWh, 4));                                     // Send kWh value to gw
        oldkWh = kWh;
      }
    }
    lastSend = now;
  } else if (sendTime && !pcReceived) {                               // No pulse count value received. Try requesting it again
    request(PC_CHILD_ID, V_VAR1);
    recrequestcount = recrequestcount + 1;
#ifdef MY_DEBUG
    Serial.println("request pulse count from controller");
#endif
    lastSend = now;
    return;
  } else if (recrequestcount == 5 && !pcReceived) {                   //For some controllers, if you dont have any V_VAR1 stored node will not get an answer. Try 5 times, then set V_VAR1 to 0 and update controller
#ifdef MY_DEBUG
    Serial.println("Set pulse count and update controller");
#endif
    pcReceived = true;
    recrequestcount = 0;
    send(pcMsg.set(pulseCount));                                      // Send pulse count 0 value to gw
    double kWh = ((double)pulseCount / ((double)PULSE_FACTOR));
    send(kWhMsg.set(kWh, 4));                                         // Send kWh value 0 to gw
    lastSend = now;
  }

  if (SLEEP_MODE) {
    sleep(SEND_FREQUENCY, false);
  }
}

void receive(const MyMessage &message)
{
  if (message.type == V_VAR1) {
    pulseCount = oldPulseCount = message.getLong();
#ifdef MY_DEBUG
    Serial.print("Received last pulse count value from gw:");
    Serial.println(pulseCount);
#endif
    pcReceived = true;
  }
}

void onPulse()
{
  if (!SLEEP_MODE) {
    uint32_t newBlink = micros();
    lastPulse = millis();                                               //last pulse time
    uint32_t interval = newBlink - lastBlink;
    if (interval < 10000L) {                                            // Sometimes we get interrupt on RISING
      return;
    }
    watt = (3600000000.0 / interval) / ppwh;
    lastBlink = newBlink;
  }
  pulseCount++;
#ifdef MY_DEBUG
  Serial.print("pulseCount : ");
  Serial.println(pulseCount);
#endif
}
