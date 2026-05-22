
/*********************************************************************
 * Desktop Station Serial Gateway - ESP32 Port
 * 
 * Original by Yaasan (2013)
 * Ported for ESP32 by Uwe Monreal
 * License: CC0 1.0 (Public Domain)
 *********************************************************************/
#include <Arduino.h>
#include "TrackController.h"
#include "TrackReporterS88_DS.h"

#include "M5Atom.h"

//#include <esp_task_wdt.h> // optional, falls du den Watchdog nutzen willst
#define MAX_S88DECODER 1
#define RELPYERROR_300 "300 Command error"
#define RELPYERROR_301 "301 Syntax error"
#define RELPYERROR_302 "302 receive timeout"
#define RELPYERROR_303 "303 Unknown error"
#define RELPYERROR_NONE ""

const bool DEBUG = false;
const uint64_t TIMEOUT = 500; // ms
const uint16_t HASH = 0x00;
const bool LOOPBACK = false;

TrackController ctrl(HASH, DEBUG, TIMEOUT, LOOPBACK); // Instance de la classe TrackController, création de l'objet ctrl.
TrackReporterS88_DS reporter(MAX_S88DECODER);
String request;
String function;
word arguments[8];
word numOfArguments;
boolean result;
void decodePacket(TrackMessage &inMessage);

void setup() {
  Serial.begin(115200);
  // Watchdog optional (nur falls benötigt)
  // esp_task_wdt_init(8, true);
  // esp_task_wdt_add(NULL);

    M5.begin(true, false, true);    //Init Atom-Matrix(Initialize serial port, LED).  初始化 ATOM-Matrix(初始化串口、LED点阵)
    delay(50);   //delay 50ms.  延迟50ms
    M5.dis.drawpix(0, 0xff0000);    //Light the LED with the specified RGB color 00ff00(Atom-Matrix has only one light).  以指定RGB颜色0x00ff00点亮第0个LED

		M5.update();	

 #if defined ARDUINO_ARCH_ESP32
   ctrl.begin(19, 22);
 #elif defined ARDUINO_ARCH_AVR
   ctrl.begin(255, 255);
 #endif

  //ctrl.begin();
  reporter.refresh();
  Serial.println("--------------------------------------");
  Serial.println("Desktop Station Interface with ESP32");
  Serial.println("--------------------------------------");
  Serial.println("100 Ready");
  //pinMode(8, OUTPUT);
  //pinMode(9, OUTPUT);
}
// Empfangsfunktion nahezu unverändert
int receiveRequest() {
  char buffer[128];
  int i = 0;
  char aByte;
  int aResult = -1;
  unsigned long time = millis();
  
  if (Serial.available() > 0) {
    while (1) {
      if (Serial.available() > 0) {
        aByte = Serial.read();
        buffer[i] = aByte;
        if (aByte == '\n') {
          aResult = i;
          break;
        }
        i++;
        if (i > 127) break;
      } else {
        if (millis() > time + 4000) {
          aResult = -1;
          break;
        }
      }
    }
  } else {
    aResult = 0;
  }
  if (aResult > 0) {
    buffer[i] = '\0';
    request = String(buffer);
  } else {
    request = "";
  }
  return aResult;
}
word stringToWord(String s) {
  word result = 0;
  for (int i = 0; i < s.length(); i++) {
    if (isdigit(s.charAt(i))) {
      result = 10 * result + (s.charAt(i) - '0');
    }
  }
  return result;
}
boolean parse() {
  int lpar = request.indexOf('(');
  if (lpar == -1) return false;
  function = request.substring(0, lpar);
  function.trim();
  int offset = lpar + 1;
  int comma = request.indexOf(',', offset);
  numOfArguments = 0;
  while (comma != -1) {
    String tmp = request.substring(offset, comma);
    tmp.trim();
    arguments[numOfArguments++] = stringToWord(tmp);
    offset = comma + 1;
    comma = request.indexOf(',', offset);
  }
  int rpar = request.indexOf(')', offset);
  if (rpar == -1) return false;
  if (rpar > offset) {
    String tmp = request.substring(offset, rpar);
    tmp.trim();
    arguments[numOfArguments++] = stringToWord(tmp);
  }
  return true;
}
boolean dispatch() {
  byte aValue, aValueHigh;
  boolean aResult;
  //if (function == "accelerateLoco") return ctrl.accelerateLoco(arguments[0]);
  //else if (function == "decelerateLoco") return ctrl.decelerateLoco(arguments[0]);
  if (function == "toggleLocoDirection") return ctrl.toggleLocoDirection(arguments[0]);
  else if (function == "setLocoDirection") return ctrl.setLocoDirection(arguments[0], arguments[1]);
  else if (function == "toggleLocoFunction") return ctrl.toggleLocoFunction(arguments[0], arguments[1]);
  else if (function == "setLocoFunction") return ctrl.setLocoFunction(arguments[0], arguments[1], arguments[2]);
  else if (function == "setTurnout") return ctrl.setTurnout(arguments[0], arguments[1]);
  else if (function == "setPower") 
	{
		if(arguments[0] == 1)
			{
				M5.dis.drawpix(0, 0x0000f0);  //Blue  绿色 
			}
		else if (arguments[0] == 0)
			{
				M5.dis.drawpix(0, 0xff0000);  //Red
			}
		M5.update();
		return ctrl.setPower(arguments[0]);
	}
  else if (function == "setLocoSpeed") return ctrl.setLocoSpeed(arguments[0], arguments[1]);
  else if (function == "getVersion") {
			aResult = ctrl.getVersion(&aValueHigh, &aValue);
				Serial.print("@VER,");
				Serial.print(aValueHigh, HEX);
				Serial.print(aValue, HEX);
				Serial.println(",");

		}
  else if (function == "getLocoConfig") {
  
		aResult = ctrl.readConfig(arguments[0], arguments[1], &aValue);
		Serial.print("@CV,");
		Serial.print(arguments[0], DEC);
		Serial.print(",");
		Serial.print(arguments[1], DEC);
		Serial.print(",");
		Serial.print(aValue, DEC);
		Serial.println(",");
		
		return true;
		}
 	else if (function == "setLocoConfig") {
		return ctrl.writeConfig(arguments[0], arguments[1], arguments[2]);
		}
	else if (function == "setPing") {
		return setPing();
		}
  
   else if (function == "getS88")
		{
		int aMaxS88Num = MAX_S88DECODER;
		
		if( arguments[0] > 0)
		{
			aMaxS88Num = arguments[0];
		}

		reporter.refresh(aMaxS88Num);

					//Send a S88 sensor reply 
		Serial.print("@S88,");

		word aFlags = 0;

		for( int j = 0; j < aMaxS88Num; j++)
		{
			aFlags = (reporter.getByte((j << 1) + 1) << 8) + reporter.getByte(j << 1);
			
			Serial.print(aFlags, HEX);
			Serial.print(",");
		}
			
		Serial.println("");
		
			return true;
		
		} /* getS88 */
  else if (function == "reset")
		{
		
			ctrl.end();
			ctrl.begin();
			Serial.println("100 Ready");
			
			return true;
		} /* reset */
  else if (function == "mfxDiscovery")
		{
		setMfxDiscovery();
		
		return true;
		} /* mfxDiscovery */
  else if (function == "mfxBind")
		{
		setMfxBind(arguments[0], arguments[1], arguments[2]);
		
		return true;
		} /* mfxBind */
  else if (function == "mfxVerify")
		{
		setMfxVerify(arguments[0], arguments[1], arguments[2]);
		
		return true;
		} /* mfxVerify */ 

  else return false;
}

void loop() {
  TrackMessage message;
  String aReplyText = RELPYERROR_NONE;
  int aReceived = receiveRequest();
  if (aReceived > 0) {
    if (parse()) {
      if (dispatch()) aReplyText = "200 Ok";
      else aReplyText = RELPYERROR_300;
    } else aReplyText = RELPYERROR_301;
  } else if (aReceived == 0) {
    if (ctrl.receiveMessage(message)) decodePacket(message);
  } else if (aReceived < 0) {
    Serial.flush();
    aReplyText = RELPYERROR_302;
  }
  if (aReplyText != "") Serial.println(aReplyText);
  // Optional Watchdog-Reset
  // esp_task_wdt_reset();
}


boolean setPing() {
	TrackMessage message;
	
	
	message.clear();
	message.command = 0x18;
	message.length = 0x00;

	ctrl.sendMessage(message);
	
	return true;
}

void decodePacket(TrackMessage &inMessage) {
  if (inMessage.response && inMessage.command == 0x18 && inMessage.length == 0x08) {
    Serial.print("@PING,");
    for (int i = 0; i < 8; i++) {
      Serial.print(inMessage.data[i], HEX);
      if (i < 7) Serial.print(",");
    }
    Serial.println(",");
  }

	/* Check received packet */
	if( inMessage.response == false)
	{
		return;
	}
	
	/* Decode received packets. */
	if( (inMessage.command == 0x00) && (inMessage.length == 0x05))
	{
		/* Power */
		Serial.print("@PWR,");
		Serial.print(inMessage.data[4], HEX);
		Serial.println(",");

		if(inMessage.data[4] == 1)
			{
				M5.dis.drawpix(0, 0x0000f0);  //Blue  绿色 
			}
		else if (inMessage.data[4] == 0)
			{
				M5.dis.drawpix(0, 0xff0000);  //Red
			}
		M5.update();

	}
	
	if( (inMessage.command == 0x05) && (inMessage.length == 0x05))
	{
		
		/* Locomotive direction */
		Serial.print("@DIR,");
		Serial.print(inMessage.data[2], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[3], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[4], HEX);
		Serial.println(",");
	}
	
	if( (inMessage.command == 0x04) && (inMessage.length == 0x06))
	{
	
		/* Locomotive speed */
		Serial.print("@SPD,");
		Serial.print(inMessage.data[2], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[3], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[4], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[5], HEX);
		Serial.println(",");
	}
	
	if( (inMessage.command == 0x06) && (inMessage.length == 0x06))
	{
		/* Locomotive functions */
		Serial.print("@FNC,");
		Serial.print(inMessage.data[2], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[3], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[4], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[5]);
		Serial.println(",");
	}
	
	if( (inMessage.command == 0x0b) && (inMessage.length == 0x06))
	{
		if( inMessage.data[5] != 0)
		{
			/* Accessories */
			Serial.print("@ACC,");
			Serial.print(inMessage.data[2], HEX);
			Serial.print(",");
			Serial.print(inMessage.data[3], HEX);
			Serial.print(",");
			Serial.print(inMessage.data[4], HEX);
			Serial.print(",");
			Serial.print(inMessage.data[5], HEX);
			Serial.println(",");
		}
	}
	
	if( (inMessage.command == 0x01) && (inMessage.length == 0x05))
	{
		/* mfx discovery */
		Serial.print("@MFX,");
		Serial.print(inMessage.data[0], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[1], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[2], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[3], HEX);
		Serial.println(",");
		
	}
	
	if( (inMessage.command == 0x02) && (inMessage.length == 0x06))
	{
		/* mfx discovery */
		Serial.print("@MFXBIND,");
		Serial.print(inMessage.data[0], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[1], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[2], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[3], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[4], HEX);
		Serial.print(",");
		Serial.print(inMessage.data[5], HEX);
		Serial.println(",");
		
	}
	

}

boolean setMfxDiscovery() {

	TrackMessage message;

	/* mfx bind */
	message.clear();
	message.command = 0x01;
	message.length = 0x01;
	message.data[0] = 0x20;

	return ctrl.sendMessage(message);
}

boolean setMfxBind(word inUID_L, word inUID_H, word inAddress) {

	TrackMessage message;

	/* mfx bind */
	message.clear();
	message.command = 0x02;
	message.length = 0x06;
	message.data[0] = (byte)((inUID_L >> 8) & 0xFF);
	message.data[1] = (byte)(inUID_L & 0xFF);
	message.data[2] = (byte)((inUID_H >> 8) & 0xFF);
	message.data[3] = (byte)(inUID_H & 0xFF);
	message.data[4] = (byte)((inAddress >> 8) & 0xFF);
	message.data[5] = (byte)(inAddress & 0xFF);

	return ctrl.exchangeMessage(message, message, 1000);
}

boolean setMfxVerify(word inUID_L, word inUID_H, word inAddress) {
	TrackMessage message;
	
	/* mfx verify */
	message.clear();
	message.command = 0x03;
	message.length = 0x06;
	message.data[0] = (byte)((inUID_L >> 8) & 0xFF);
	message.data[1] = (byte)(inUID_L & 0xFF);
	message.data[2] = (byte)((inUID_H >> 8) & 0xFF);
	message.data[3] = (byte)(inUID_H & 0xFF);
	message.data[4] = (byte)((inAddress >> 8) & 0xFF);
	message.data[5] = (byte)(inAddress & 0xFF);

	return ctrl.exchangeMessage(message, message, 1000);
	
}