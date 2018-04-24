/*
 * Project rumluft-indoor-firmware
 * Description:
 */
#include "ClosedCube_SHT31D.h"
#include "SensirionSGP30.h"

const uint8_t I2C_ADDR_SHT31D = 0x44U;

SHT31D_CC::ClosedCube_SHT31D sht31d;
SensirionSGP30 sgp30;

struct MeasurementData {
    MeasurementData() {
        temperature = NAN;
        humidity = NAN;
        voc = NAN;
        co2 = NAN;
        absoluteHumidity = NAN;
    };

    float temperature; // [°C]
    float humidity; // [%RH]
    float voc; // [ppb]
    float co2; // [ppm]
    float absoluteHumidity; // [mg/m^3]
};

/* return absolute humidity [mg/m^3] with approximation formula
* @param temperature [°C]
* @param humidity [%RH]
*/
uint32_t getAbsoluteHumidity(const MeasurementData& sample) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((sample.humidity / 100.0f) * 6.112f * exp((17.62f * sample.temperature) / (243.12f + sample.temperature)) / (273.15f + sample.temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}

void readTemperatureHumidity(MeasurementData& sample) {
    SHT31D_CC::SHT31D sht31dresult = sht31d.readTempAndHumidity(SHT31D_CC::REPEATABILITY_LOW, SHT31D_CC::MODE_CLOCK_STRETCH, 50);
    if (sht31dresult.error == SHT31D_CC::NO_ERROR) {
        sample.temperature = sht31dresult.t;
        sample.humidity = sht31dresult.rh;
        Serial.print("Temp = "); Serial.print(sample.temperature); Serial.println(" °C");
        Serial.print("Humidity = "); Serial.print(sample.humidity); Serial.println(" %RH");
    }
    else {
        Serial.println("SHT31D measurement failed");
    }
}

void setHumidityCompensation(MeasurementData& sample) {
    uint32_t absoluteHumidity = getAbsoluteHumidity(sample);
    if (sgp30.setHumidity(absoluteHumidity)) {
        sample.absoluteHumidity = absoluteHumidity;
        Serial.print("Absolute Humidity = "); Serial.print(sample.absoluteHumidity); Serial.println(" mg/m^3");
    }
    else {
        Serial.println("SGP30 setting absolute humidity failed");
    }
}

void readAirQuality(MeasurementData& sample) {
    if (sgp30.IAQmeasure()) {
        sample.voc = sgp30.TVOC;
        sample.co2 = sgp30.eCO2;
        Serial.print("TVOC = "); Serial.print(sample.voc); Serial.println(" ppb");
        Serial.print("CO2 = "); Serial.print(sample.co2); Serial.println(" ppm");
    }
    else {
        Serial.println("SGP30 measurement failed");
    }
}

void publishMeasurementData(const MeasurementData& sample) {
    // Send as text representation in JSON format so it can easily be processed
    char data[255];
    memset(data, sizeof(data), 0);
    snprintf(data, sizeof(data), "{\"temp\":%.2f,\"humid\":%.2f,\"voc\":%.2f,\"co2\":%.2f,\"absHumid\":%.2f}", sample.temperature, sample.humidity, sample.voc, sample.co2, sample.absoluteHumidity);

    // Trigger the integration (limited to 255 bytes)
    Particle.publish("v1-indoor-data", data, PRIVATE);
}

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
    MeasurementData sample;
    // SHT31D
    readTemperatureHumidity(sample);

    // SGP30
    setHumidityCompensation(sample);
    readAirQuality(sample);

    // Pubish data to consumers (cloud and other mesh nodes)
    publishMeasurementData(sample);

    // Wait until next sample
    delay(10000);
}
