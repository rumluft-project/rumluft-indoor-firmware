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
	Serial.println("ClosedCube SHT31D Library - Reset Example");

	sht31d.begin(0x44);

	Serial.print("SHT31D Serial #");
	Serial.println(sht31d.readSerialNumber());

	SHT31D_ErrorCode resultSoft = sht31d.softReset();
	Serial.print("Soft Reset return code: ");
	Serial.println(resultSoft);

	SHT31D_CC::SHT31D_ErrorCode resultGeneral = sht31d.generalCallReset();
	Serial.print("General Call Reset return code: ");
	Serial.println(resultGeneral);

}

void loop()
{

}
