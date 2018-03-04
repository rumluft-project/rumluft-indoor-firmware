/*

This is example for SHT3X-DIS Digital Humidity & Temperature Sensors Arduino Library
ClosedCube SHT31-D [Digital I2C] Humidity and Temperature Sensor Breakout

Initial Date: 06-Oct-2015
Last Updated: 12-Oct-2016

Hardware connections for Arduino Uno:
	VDD to 3.3V DC
	SDA to D0
	SCL to D1
	GND to common ground

MIT License

*/

#include <ClosedCube_SHT31D.h>

SHT31D_CC::ClosedCube_SHT31D sht31d;

void setup()
{
	Serial.begin(9600);
	Serial.println("ClosedCube SHT31-D Periodic Mode Example");

	sht31d.begin(0x44);
	Serial.print("sht31d Serial #");
	Serial.println(sht31d.readSerialNumber());

	if (sht31d.periodicStart(SHT31D_CC::REPEATABILITY_HIGH, SHT31D_CC::FREQUENCY_10HZ) != SHT31D_CC::NO_ERROR)
		Serial.println("[ERROR] Cannot start periodic mode");

}

void loop()
{
	printResult("Periodic Mode", sht31d.periodicFetchData());
	delay(250);
}

void printResult(String text, SHT31D_CC::SHT31D result) {
	if (result.error == SHT31D_CC::NO_ERROR) {
		Serial.print(text);
		Serial.print(": T=");
		Serial.print(result.t);
		Serial.print("C, RH=");
		Serial.print(result.rh);
		Serial.println("%");
	}
	else {
		Serial.print(text);
		Serial.print(": [ERROR] Code #");
		Serial.println(result.error);
	}
}
