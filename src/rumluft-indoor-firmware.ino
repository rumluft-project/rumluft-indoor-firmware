/*
 * Project rumluft-indoor-firmware
 * Description:
 */
#include "ClosedCube_SHT31D.h"
#include "SensirionSGP30.h"

const uint8_t I2C_ADDR_SHT31D = 0x44U;

SHT31D_CC::ClosedCube_SHT31D sht31d;
SensirionSGP30 sgp30;

// setup() runs once, when the device is first turned on.
void setup() {
    Serial.println("rumluft firmware");
    if (!sht31d.begin(I2C_ADDR_SHT31D)) {
        Serial.println("SHT31 sensor not found");
    }
    if (!sgp30.begin()) {
        Serial.println("SGP30 sensor not found");
    }
    if (!sgp30.IAQinit()) {
        Serial.println("SGP30 init failed");
    }
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
    float voc = NAN; // ppb
    float co2 = NAN; // ppm
    readAirQuality(voc, co2);

    float temperature = NAN; // °C
    float humidity = NAN; // %RH
    readTemperatureHumidity(temperature, humidity);

    // Pubish data to consumers (cloud and other mesh nodes)
    publishMeasurementData(temperature, humidity, voc, co2);

    // Wait until next sample
    delay(10000);
}

void readAirQuality(float& voc, float& co2) {
    // TODO: take current humidity into account to improve measurement
    if (sgp30.IAQmeasure()) {
        voc = sgp30.TVOC;
        co2 = sgp30.eCO2;
        Serial.print("TVOC = "); Serial.print(voc); Serial.println(" ppb");
        Serial.print("CO2 = "); Serial.print(co2); Serial.println(" ppm");
    }
    else {
        Serial.println("SGP30 measurement failed");
    }
}

void readTemperatureHumidity(float& temperature, float& humidity) {
    SHT31D_CC::SHT31D sht31dresult = sht31d.readTempAndHumidity(SHT31D_CC::REPEATABILITY_LOW, SHT31D_CC::MODE_CLOCK_STRETCH, 50);
    if (sht31dresult.error == SHT31D_CC::NO_ERROR) {
        temperature = sht31dresult.t;
        humidity = sht31dresult.rh;
        Serial.print("Temp = "); Serial.print(temperature); Serial.println(" °C");
        Serial.print("Humidity = "); Serial.print(humidity); Serial.println(" %RH");
    }
    else {
        Serial.println("SHT31D measurement failed");
    }
}

void publishMeasurementData(const float temperature, const float humidity, const float voc, const float co2) {
    // Send as text representation in JSON format so it can easily be processed
    char data[255];
    memset(data, sizeof(data), 0);
    snprintf(data, sizeof(data), "{\"temp\":%.2f,\"humid\":%.2f,\"voc\":%.2f,\"co2\":%.2f}", temperature, humidity, voc, co2);

    // Trigger the integration (limited to 255 bytes)
    Particle.publish("v1-indoor-data", data, PRIVATE);
}