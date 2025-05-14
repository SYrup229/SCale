#include "Color_Sensor.h"
#include <Adafruit_AS7341.h>

static Adafruit_AS7341 _as7341;   // one sensor, one global—simple life

bool AS7341ColorSensor::begin(TwoWire &bus, uint8_t addr)
{
    _bus  = &bus;
    _addr = addr;
    _bus->begin();                       // kick the bus

    // wake the silicon
    return _as7341.begin(addr, _bus);
}

void AS7341ColorSensor::readChannels()
{
    _as7341.readAllChannels(_channels);
}

uint16_t AS7341ColorSensor::getRawChannel(uint8_t index) const
{
    return (index < 10) ? _channels[index] : 0;
}

String AS7341ColorSensor::classify() const
{
    // Simple but surprisingly useful primary-colour detector.
    uint16_t red   = _channels[2] + _channels[3]; // 610–670 nm
    uint16_t green = _channels[4] + _channels[5]; // 510–570 nm
    uint16_t blue  = _channels[0] + _channels[1]; // 405–500 nm

    if (red > green && red > blue)
        return (green > blue) ? "Yellow" : "Red";
    if (green > red && green > blue)
        return "Green";
    if (blue > red && blue > green)
        return "Blue";
    return "Unknown";
}

String AS7341ColorSensor::getColor() const
{
    return classify();
}
