#pragma once
#include <DallasTemperature.h>


class DS18B20
{
public:
    /**
     * @brief DS18B20Driver sensor class
     *
     * @param pin           OneWire bus pin number
     */
    DS18B20( uint8_t pin );

    /**
     * @brief Get temperature from sensor
     *
     * @param index         Sensor index
     * @return `float`      Temperature in Celsius
     */
    float getTemperature( uint8_t index );

    /**
     * @brief Get device address
     *
     * @param index         Sensor index
     * @return `uint64_t`   Device address
     */
    uint64_t getAddres( uint8_t index );

    /**
     * @brief Get device count
     *
     * @return `uint8_t`    Device count
     */
    uint8_t getDeviceCount() { return sensors.getDeviceCount(); }

private:
    OneWire           oneWire;
    DallasTemperature sensors;
};
