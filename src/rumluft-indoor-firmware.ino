/*
 * Project rumluft-indoor-firmware
 * Description:
 */
#include "ClosedCube_SHT31D.h"
#include "SensirionSGP30.h"

const uint8_t I2C_ADDR_SHT31D = 0x44U;

struct MeasurementData {
    MeasurementData() {
        temperature = NAN;
        humidity = NAN;
        voc = NAN;
        co2 = NAN;
        absoluteHumidity = NAN;
        eco2_base = 0;
        tvoc_base = 0;
    };

    float temperature; // [°C]
    float humidity; // [%RH]
    float voc; // [ppb]
    float co2; // [ppm]
    float absoluteHumidity; // [mg/m^3]
    uint16_t eco2_base; // SGP30 baseline correction algorithm value for CO2 [raw]
    uint16_t tvoc_base; // SGP30 baseline correction algorithm value for VOC [raw]
};

struct EepromStorage {
    uint8_t version; // helper value to detect if EEPROM needs to be initialized (default 0xFF for empty EEPROM)
    bool baselineValid; // flag set once SGP30 baseline calibration sequence is completed (12 hours)
    long timeStamp; // timestamp when baseline calibration sequence has been started or last valid baseline has been stored [Unix time format]
    uint16_t eco2_base; // SGP30 baseline value for CO2
    uint16_t tvoc_base; // SGP30 baseline value for TVOC
};

// object instances
SHT31D_CC::ClosedCube_SHT31D sht31d;
SensirionSGP30 sgp30;
EepromStorage persistency;
bool baselineCalibrationInProgress = false;

/* Helper to calculate absolute humidity [mg/m^3] with approximation formula based on temperature [°C] and humidity [%RH].
* @param const MeasurementData& sample
* @return absolute humidity [mg/m^3]
*/
uint32_t getAbsoluteHumidity(const MeasurementData& sample) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((sample.humidity / 100.0f) * 6.112f * exp((17.62f * sample.temperature) / (243.12f + sample.temperature)) / (273.15f + sample.temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}

/* Read the temperature and humidity from SHT31-D and store in the given sample
* @param MeasurementData& sample - to store temperature and humidity
*/
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

/* Set the absolute humidity on the SGP30 for more acurate readings of air quality signals (TVOC & eCO2) and store absolute humidity in the given sample.
* @param MeasurementData& sample - to store absolute humidity value
*/
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

/* Read the IAQ signals from SGP30 and store VOC and CO2 in the given sample.
* @param MeasurementData& sample - to store voc and co2 values
*/
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

/* Read the baseline from SGP30 and store eco2_base and tvoc_base in the given sample.
* @param MeasurementData& sample - to store eco2_base and tvoc_base values
*/
void readBaseline(MeasurementData& sample) {
    uint16_t eco2_base, tvoc_base;
    if (sgp30.getIAQBaseline(&eco2_base, &tvoc_base)) {
        sample.eco2_base = eco2_base;
        sample.tvoc_base = tvoc_base;
        Serial.print("eco2_base = "); Serial.println(sample.eco2_base);
        Serial.print("tvoc_base = "); Serial.println(sample.tvoc_base);
    }
    else {
        Serial.println("SGP30 baseline reading failed");
    }
}

/* Restore the baseline if it's valid and less than 1 week old. In case of an old baseline, the timestamp for the baseline calibration sequence is stored.
*/
void restoreBaseline() {
    const bool baselineValid = persistency.baselineValid;
    const long ONE_WEEK_MILLIS = 7 * 24 * 60 * 60 * 1000;
    const bool youngerThan1Week = (Time.now() - persistency.timeStamp) <= ONE_WEEK_MILLIS;
    if (baselineValid && youngerThan1Week) {
        if(sgp30.setIAQBaseline(persistency.eco2_base, persistency.tvoc_base)) {
            Serial.print("Restored baseline from UTC "); Serial.print(Time.format(persistency.timeStamp, TIME_FORMAT_DEFAULT));
            Serial.print(": eco2_base = "); Serial.print(persistency.eco2_base); Serial.print(" / tvoc_base = "); Serial.println(persistency.tvoc_base);
        }
        else {
            Serial.println("SGP30 baseline setting failed");
        }
    }
    else {
        Serial.println("Warning - Baseline calibration (12h) required!");
        persistency.baselineValid = false;
        persistency.timeStamp = Time.now();
        baselineCalibrationInProgress = true;
    }
}

void refreshBaseline(const MeasurementData& sample) {
    const bool baselineValid = persistency.baselineValid;
    const long ONE_HOUR_MILLIS = 60 * 60 * 1000;
    const bool olderThan1Hour = (Time.now() - persistency.timeStamp) >= ONE_HOUR_MILLIS;
    if (baselineValid && olderThan1Hour) {
        // Once the baseline is properly initialized or restored, the current baseline value should be stored approximately once per hour
        persistBaseline(sample.eco2_base, sample.tvoc_base);
    }
    else if (baselineCalibrationInProgress) {
        // Restarting the sensor without restoring a baseline will result in the sensor trying to determine a new baseline.
        // The baseline calibration algorithm will be performed on the SGP30 for 12hrs (maximum time required to find a new baseline).
        const bool baselineCalibrationCompleted = (Time.now() - persistency.timeStamp) >= (12 * ONE_HOUR_MILLIS);
        if (baselineCalibrationInProgress && baselineCalibrationCompleted) {
            // The sensor adjusts to the best value it has been exposed to. So keeping it indoors the sensor thinks this is the best value and sets it to 0ppb tVOC and 400ppm CO2eq.
            // As soon as you expose the sensor to outside air it can adjust to the global H2 Background Signal. For normal Operation exposing the sensor to outside air for 10min cumulative time should be sufficient.
            Serial.println("SGP30 baseline calibration completed");
            persistBaseline(sample.eco2_base, sample.tvoc_base);
            baselineCalibrationInProgress = false;
        }
    }
}

void persistBaseline(uint16_t eco2_base, uint16_t tvoc_base) {
    persistency.version = 0;
    persistency.baselineValid = true;
    persistency.timeStamp = Time.now();
    persistency.eco2_base = eco2_base;
    persistency.tvoc_base = tvoc_base;
    EEPROM.put(0, persistency);

    Serial.print("Persisted baseline: eco2_base = "); Serial.print(eco2_base); Serial.print(" / tvoc_base = "); Serial.println(tvoc_base);
}

void publishMeasurementData(const MeasurementData& sample) {
    // Send as text representation in JSON format so it can easily be processed
    char data[255];
    memset(data, sizeof(data), 0);
    snprintf(data, sizeof(data), "{\"temp\":%.2f,\"humid\":%.2f,\"voc\":%.2f,\"co2\":%.2f,\"absHumid\":%.2f,\"eco2_base\":%u,\"tvoc_base\":%u}", sample.temperature, sample.humidity, sample.voc, sample.co2, sample.absoluteHumidity, sample.eco2_base, sample.tvoc_base);

    // Trigger the integration (limited to 255 bytes)
    Particle.publish("v1-indoor-data", data, PRIVATE);
}

void setupEeprom() {
    EEPROM.get(0, persistency);
    if (persistency.version == 0xFF) {
        // EEPROM was empty -> initialize
        persistency.version = 0;
        persistency.baselineValid = false;
        persistency.timeStamp = 0;
        persistency.eco2_base = 0;
        persistency.tvoc_base = 0;
    }
}

// Cloud functions must return int and take one String
int cloudFunctionSetBaseline(String extra) {
    uint16_t eco2_base, tvoc_base;
    if (sgp30.getIAQBaseline(&eco2_base, &tvoc_base) && sgp30.setIAQBaseline(eco2_base, tvoc_base)) {
        persistBaseline(eco2_base, tvoc_base);
        return 0;
    }
    return -1;
}

// Cloud functions must return int and take one String
int cloudFunctionClearBaseline(String extra) {
    // Note: requires a power sequence to properly reinitialize SGP30 baseline calibration sequence
    persistency.version = 0xFF;
    persistency.baselineValid = false;
    persistency.timeStamp = 0;
    persistency.eco2_base = 0;
    persistency.tvoc_base = 0;
    EEPROM.put(0, persistency);
    return 0;
}

// setup() runs once, when the device is first turned on.
void setup() {
    Serial.println("rumluft firmware");
    setupEeprom();

    if (sht31d.begin(I2C_ADDR_SHT31D) != SHT31D_CC::NO_ERROR) {
        Serial.println("SHT31 sensor not found");
    }
    if (!sgp30.begin()) {
        Serial.println("SGP30 sensor not found");
    }
    if (!sgp30.IAQinit()) {
        Serial.println("SGP30 init failed");
    }
    restoreBaseline();

    // register the cloud function (Up to 15 cloud functions may be registered and each function name is limited to a maximum of 12 characters)
    Particle.function("setBaseline", cloudFunctionSetBaseline);
    Particle.function("clrBaseline", cloudFunctionClearBaseline);

    // Give the SGP30 time to start so that it provides valid gas quality signals
    Serial.println("Waiting for the SGP30 to start");
    delay(30000);
}

// loop() runs over and over again, as quickly as it can execute.
void loop() {
    MeasurementData sample;
    // SHT31D
    readTemperatureHumidity(sample);

    // SGP30
    setHumidityCompensation(sample);
    readAirQuality(sample);
    readBaseline(sample);
    refreshBaseline(sample);

    // Pubish data to consumers (cloud and other mesh nodes)
    publishMeasurementData(sample);

    // Wait until next sample
    delay(10000);
}
